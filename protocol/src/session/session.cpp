#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <protocol/command/parser.hpp>
#include <protocol/command/writer.hpp>
#include <protocol/handshake/client_init.hpp>
#include <protocol/handshake/init_server.hpp>
#include <protocol/message/channel_created.hpp>
#include <protocol/message/channel_deleted.hpp>
#include <protocol/message/channel_edited.hpp>
#include <protocol/message/channel_list.hpp>
#include <protocol/message/channel_moved.hpp>
#include <protocol/message/channel_subscription.hpp>
#include <protocol/message/client_disconnect.hpp>
#include <protocol/message/client_enter_view.hpp>
#include <protocol/message/client_left_view.hpp>
#include <protocol/message/client_move.hpp>
#include <protocol/message/client_moved.hpp>
#include <protocol/message/client_update.hpp>
#include <protocol/message/client_updated.hpp>
#include <protocol/message/command_result.hpp>
#include <protocol/message/send_text_message.hpp>
#include <protocol/message/set_connection_info.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/session/session.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::protocol {

    Session::Session( Transport& transport, SessionBootstrap bootstrap ):
        m_SessionCrypto( bootstrap.material.sharedIv, bootstrap.material.sharedMac ),
        m_SessionTransport( transport, m_SessionCrypto, std::move( bootstrap.sequences ) ) {
    }

    void Session::Login( const ClientProfile& profile, std::uint64_t keyOffset ) {
        if ( m_ClientId != 0 ) {
            throw std::runtime_error( "Session is already logged in" );
        }

        m_Channels.Clear();
        m_Clients.Clear();
        m_Events.clear();
        m_CurrentChannelId = 0;
        m_CurrentNickname = profile.nickname;
        m_InitialStateComplete = false;
        m_ConnectionInfoRequested = false;
        m_LastConnectionInfoSentAt.reset();
        m_DisconnectPacketId.reset();

        const ClientInit clientInit( profile, keyOffset );

        const std::uint16_t clientInitPacketId = m_SessionTransport.SendCommand( clientInit.Serialize() );

        std::vector<Command> commandsBeforeInitServer;
        std::optional<InitServer> initServer;

        while ( !initServer ) {
            const auto data = m_SessionTransport.ReceiveCommand();
            Command command = CommandParser::Parse( data );

            if ( command.Name() == "initserver" ) {
                initServer = InitServer::Parse( command );
                break;
            }

            if ( command.Name() == "error" ) {
                const CommandResult result = CommandResult::Parse( command );

                for ( const CommandResultEntry& entry : result.Entries() ) {
                    if ( entry.id != 0 ) {
                        throw std::runtime_error( "Login rejected (" + std::to_string( entry.id ) + "): " + entry.message );
                    }
                }

                continue;
            }

            commandsBeforeInitServer.emplace_back( std::move( command ) );
        }

        /*
         * The first server Ack is a protocol quirk and is not consumed as a
         * normal encrypted Ack. Receiving a valid initserver confirms that the
         * server accepted clientinit, so retire that reliable packet here.
         */
        m_SessionTransport.ConfirmCommand( clientInitPacketId );

        const std::uint16_t clientId = initServer->ClientId();
        std::string serverName( initServer->ServerName() );

        m_SessionTransport.SetClientId( clientId );
        m_ClientId = clientId;
        m_ServerName = std::move( serverName );
        m_CodecEncryptionMode = initServer->CodecEncryptionMode();

        for ( const Command& command : commandsBeforeInitServer ) {
            ProcessCommand( command );
        }

        ReceiveInitialState();
        m_SessionTransport.SetConnected();
    }

    void Session::Disconnect( std::string_view reason ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        if ( m_DisconnectPacketId ) {
            return;
        }

        const ClientDisconnect disconnect { std::string( reason ) };

        m_DisconnectPacketId = m_SessionTransport.SendCommand( disconnect.Serialize() );
    }

    void Session::SendTextMessage( TextMessageTarget target, std::string_view text ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        const ::ts::protocol::SendTextMessage message { target, std::string( text ) };
        m_SessionTransport.SendCommand( message.Serialize() );
    }

    void Session::MoveToChannel( std::uint64_t channelId ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        const ClientMove move( m_ClientId, channelId );
        m_SessionTransport.SendCommand( move.Serialize() );
    }

    void Session::ChangeNickname( std::string_view nickname ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        const ClientUpdate update { std::string( nickname ) };
        m_SessionTransport.SendCommand( update.Serialize() );
    }

    void Session::SetAudioState( bool inputHardware, bool outputHardware, bool inputMuted ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        CommandWriter writer( "clientupdate" );
        writer.Write( "client_input_hardware", inputHardware );
        writer.Write( "client_output_hardware", outputHardware );
        writer.Write( "client_input_muted", inputMuted );
        m_SessionTransport.SendCommand( writer.Take() );
    }

    void Session::SendVoice( std::span<const std::byte> data, bool talkStart ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Session is not logged in" );
        }

        m_SessionTransport.SendVoice( CurrentVoiceCodec(), data, VoiceEncrypted(), talkStart );
    }

    void Session::ReceiveInitialState() {
        while ( true ) {
            const auto data = m_SessionTransport.ReceiveCommand();
            const Command command = CommandParser::Parse( data );

            if ( command.Name() == "channellistfinished" ) {
                if ( !command.Rows().empty() ) {
                    throw std::runtime_error( "Unexpected channellistfinished parameters" );
                }

                break;
            }

            ProcessCommand( command );
        }

        m_Channels.Validate();

        /*
         * clientinit leaves client_default_channel empty, so the server places
         * us in its default channel. Our own notifycliententerview is not
         * guaranteed to arrive before channellistfinished, therefore startup
         * must not wait for it. Prefer an already-known self placement, then
         * fall back to the advertised default channel. Later self enter/move
         * notifications keep m_CurrentChannelId authoritative.
         */
        if ( const Client* self = m_Clients.Find( m_ClientId ); self != nullptr && self->channelId != 0 ) {
            m_CurrentChannelId = self->channelId;
        } else {
            for ( const ChannelTreeEntry& entry : m_Channels.Tree() ) {
                if ( entry.channel != nullptr && entry.channel->defaultChannel ) {
                    m_CurrentChannelId = entry.channel->id;
                    break;
                }
            }
        }

        if ( m_CurrentChannelId == 0 || m_Channels.Find( m_CurrentChannelId ) == nullptr ) {
            throw std::runtime_error( "Unable to determine current client channel" );
        }

        /* Entering a channel implicitly subscribes the local client to it. */
        m_Channels.SetSubscribed( m_CurrentChannelId, true );
        SubscribeAllChannels();
        m_Clients.Validate( m_Channels );
        m_InitialStateComplete = true;
    }

    void Session::SubscribeAllChannels() {
        /*
         * Subscription is deliberately asynchronous. Waiting for the generic
         * command result here blocks Connection::Connect() after the server has
         * already admitted the client, which leaves the CLI looking connected
         * but unusable. Subscription notifications and the resulting client
         * visibility snapshot are normal live session traffic and are applied
         * by ProcessCommand() once the network runtime starts.
         */
        m_SessionTransport.SendCommand( ChannelSubscribeAll::Serialize() );
    }

    void Session::SubscribeChannel( std::uint64_t channelId ) {
        m_SessionTransport.SendCommand( ChannelSubscribe( channelId ).Serialize() );
    }

    void Session::ProcessPacket() {
        m_SessionTransport.ProcessPacket();
        ProcessReadyCommands();

        while ( m_SessionTransport.HasReadyVoice() ) {
            m_Events.emplace_back( VoiceEvent { .frame = m_SessionTransport.TakeReadyVoice() } );
        }
    }

    void Session::ProcessTimers() {
        using namespace std::chrono_literals;

        m_SessionTransport.ProcessTimers();

        if ( !m_ConnectionInfoRequested || m_SessionTransport.HasPendingReliableCommands() ) {
            return;
        }

        const auto now = ReliableCommandQueue::Clock::now();

        if ( m_LastConnectionInfoSentAt && now - *m_LastConnectionInfoSentAt < 1s ) {
            return;
        }

        m_ConnectionInfoRequested = false;
        m_LastConnectionInfoSentAt = now;
        SendConnectionInfo();
    }

    std::optional<ReliableCommandQueue::TimePoint> Session::NextDeadline() const {
        return m_SessionTransport.NextDeadline();
    }

    bool Session::DisconnectPending() const {
        if ( !m_DisconnectPacketId ) {
            return false;
        }

        return m_SessionTransport.IsCommandPending( *m_DisconnectPacketId );
    }

    bool Session::HasEvent() const {
        return !m_Events.empty();
    }

    SessionEvent Session::TakeEvent() {
        if ( m_Events.empty() ) {
            throw std::runtime_error( "No session event is available" );
        }

        SessionEvent event = std::move( m_Events.front() );
        m_Events.pop_front();

        return event;
    }

    void Session::ProcessReadyCommands() {
        while ( m_SessionTransport.HasReadyCommand() ) {
            const auto data = m_SessionTransport.TakeReadyCommand();
            const Command command = CommandParser::Parse( data );

            ProcessCommand( command );
        }
    }

    void Session::ProcessCommand( const Command& command ) {
        if ( command.Name() == "notifyconnectioninforequest" ) {
            m_ConnectionInfoRequested = true;
            return;
        }

        if ( command.Name() == "channellist" ) {
            m_Channels.Apply( ChannelList::Parse( command ) );
            return;
        }

        if ( command.Name() == "notifychannelsubscribed" || command.Name() == "notifychannelunsubscribed" ) {
            const ChannelSubscriptionState subscription = ChannelSubscriptionState::Parse( command );
            m_Channels.Apply( subscription );

            if ( !subscription.Subscribed() ) {
                for ( const ChannelSubscriptionEntry& entry : subscription.Entries() ) {
                    m_Clients.RemoveInChannel( entry.channelId, m_ClientId );
                }
            }
            return;
        }

        if ( command.Name() == "notifychannelcreated" ) {
            const ChannelCreated created = ChannelCreated::Parse( command );
            m_Channels.Apply( created );

            if ( m_InitialStateComplete ) {
                SubscribeChannel( created.Entry().id );
            }
            return;
        }

        if ( command.Name() == "notifychanneldeleted" ) {
            const ChannelDeleted deleted = ChannelDeleted::Parse( command );
            m_Clients.RemoveInChannel( deleted.ChannelId(), m_ClientId );
            m_Channels.Remove( deleted.ChannelId() );
            return;
        }

        if ( command.Name() == "notifychannelmoved" ) {
            m_Channels.Apply( ChannelMoved::Parse( command ) );
            return;
        }

        if ( command.Name() == "notifychanneledited" ) {
            m_Channels.Apply( ChannelEdited::Parse( command ) );
            return;
        }

        if ( command.Name() == "notifycliententerview" ) {
            const ClientEnterView entered = ClientEnterView::Parse( command );

            for ( const ClientEnterViewEntry& entry : entered.Entries() ) {
                if ( entry.id == m_ClientId ) {
                    if ( entry.channelId != 0 ) {
                        m_CurrentChannelId = entry.channelId;
                        m_Channels.SetSubscribed( entry.channelId, true );
                    }

                    if ( !entry.nickname.empty() ) {
                        m_CurrentNickname = entry.nickname;
                    }

                    continue;
                }

                const Client* existing = m_Clients.Find( entry.id );
                const std::uint64_t effectiveChannelId =
                    entry.channelId != 0 ? entry.channelId : ( existing == nullptr ? 0 : existing->channelId );
                const bool alreadyKnownInChannel = existing != nullptr && existing->detailsKnown && effectiveChannelId != 0 &&
                                                   existing->channelId == effectiveChannelId;

                if ( m_InitialStateComplete && !alreadyKnownInChannel && effectiveChannelId == m_CurrentChannelId ) {
                    EmitPresence( ClientPresenceKind::Joined, entry.id, effectiveChannelId, entry.nickname );
                }
            }

            m_Clients.Apply( entered );
            return;
        }

        if ( command.Name() == "notifyclientmoved" ) {
            const ClientMoved moved = ClientMoved::Parse( command );

            for ( const ClientMovedEntry& entry : moved.Entries() ) {
                const Client* existing = m_Clients.Find( entry.id );
                const std::uint64_t previousChannelId = existing == nullptr ? 0 : existing->channelId;
                const std::string clientName = existing == nullptr ? std::string {} : existing->nickname;

                if ( entry.id == m_ClientId ) {
                    if ( entry.channelId != 0 ) {
                        m_CurrentChannelId = entry.channelId;
                        m_Channels.SetSubscribed( entry.channelId, true );
                    }

                    continue;
                }

                if ( !m_InitialStateComplete || entry.channelId == previousChannelId ) {
                    continue;
                }

                if ( previousChannelId == m_CurrentChannelId && entry.channelId != m_CurrentChannelId ) {
                    EmitPresence( ClientPresenceKind::Left, entry.id, previousChannelId, clientName );
                } else if ( previousChannelId != m_CurrentChannelId && entry.channelId == m_CurrentChannelId &&
                            existing != nullptr && existing->detailsKnown ) {
                    EmitPresence( ClientPresenceKind::Joined, entry.id, entry.channelId, clientName );
                }
            }

            m_Clients.Apply( moved );
            return;
        }

        if ( command.Name() == "notifyclientleftview" ) {
            const ClientLeftView left = ClientLeftView::Parse( command );

            for ( const ClientLeftViewEntry& entry : left.Entries() ) {
                if ( entry.id == m_ClientId ) {
                    m_CurrentChannelId = entry.toChannelId;
                    if ( entry.toChannelId != 0 ) {
                        m_Channels.SetSubscribed( entry.toChannelId, true );
                    }
                    continue;
                }

                if ( !m_InitialStateComplete ) {
                    continue;
                }

                const Client* existing = m_Clients.Find( entry.id );
                const std::uint64_t previousChannelId =
                    entry.fromChannelId != 0 ? entry.fromChannelId : ( existing == nullptr ? 0 : existing->channelId );
                const std::string clientName = existing == nullptr ? std::string {} : existing->nickname;

                if ( previousChannelId == m_CurrentChannelId && entry.toChannelId != m_CurrentChannelId &&
                     ( existing == nullptr || existing->channelId == previousChannelId ) ) {
                    EmitPresence( ClientPresenceKind::Left, entry.id, previousChannelId, clientName );
                }
            }

            for ( const ClientLeftViewEntry& entry : left.Entries() ) {
                if ( entry.id == m_ClientId ) {
                    continue;
                }

                if ( entry.toChannelId != 0 && m_Channels.IsSubscribed( entry.toChannelId ) ) {
                    m_Clients.Move( entry.id, entry.toChannelId );
                } else {
                    m_Clients.Remove( entry.id );
                }
            }
            return;
        }

        if ( command.Name() == "notifyclientupdated" ) {
            const ClientUpdated updated = ClientUpdated::Parse( command );

            for ( const ClientUpdatedEntry& entry : updated.Entries() ) {
                if ( entry.id == m_ClientId && entry.nickname && !entry.nickname->empty() ) {
                    m_CurrentNickname = *entry.nickname;
                }
            }

            m_Clients.Apply( updated );
            return;
        }

        if ( command.Name() == "error" ) {
            const CommandResult result = CommandResult::Parse( command );

            for ( const CommandResultEntry& entry : result.Entries() ) {
                if ( entry.id != 0 ) {
                    m_Events.emplace_back( CommandErrorEvent { .id = entry.id, .message = entry.message } );
                }
            }

            return;
        }

        if ( command.Name() == "notifytextmessage" ) {
            const TextMessage textMessage = TextMessage::Parse( command );

            for ( const TextMessageEntry& message : textMessage.Entries() ) {
                m_Events.emplace_back( TextMessageEvent { .message = message,
                                                          .replyTarget = ReplyTargetFor( message ),
                                                          .channelName = ChannelNameFor( message ),
                                                          .privatePeerName = PrivatePeerNameFor( message ),
                                                          .outgoing = message.invokerId == m_ClientId } );
            }

            return;
        }
    }

    void Session::EmitPresence( ClientPresenceKind kind,
                                std::uint16_t clientId,
                                std::uint64_t channelId,
                                std::string_view clientName ) {
        if ( channelId == 0 || channelId != m_CurrentChannelId ) {
            return;
        }

        const Channel* channel = m_Channels.Find( channelId );

        if ( channel == nullptr ) {
            return;
        }

        std::string name( clientName );

        if ( name.empty() ) {
            name = "client ";
            name += std::to_string( clientId );
        }

        m_Events.emplace_back( ClientPresenceEvent { .kind = kind,
                                                     .clientId = clientId,
                                                     .channelId = channelId,
                                                     .clientName = std::move( name ),
                                                     .channelName = channel->name } );
    }

    std::optional<TextMessageTarget> Session::ReplyTargetFor( const TextMessageEntry& message ) const {
        switch ( message.targetMode ) {
            case TextMessageTargetMode::Private:
                if ( message.invokerId == 0 || message.invokerId == m_ClientId ) {
                    return std::nullopt;
                }

                return TextMessageTarget { .mode = TextMessageTargetMode::Private, .id = message.invokerId };

            case TextMessageTargetMode::Channel:
                if ( message.targetId && *message.targetId != 0 && m_Channels.Find( *message.targetId ) != nullptr ) {
                    return TextMessageTarget { .mode = TextMessageTargetMode::Channel, .id = *message.targetId };
                }

                if ( message.invokerId != 0 ) {
                    const Client* sender = m_Clients.Find( message.invokerId );

                    if ( sender != nullptr && sender->channelId != 0 ) {
                        return TextMessageTarget { .mode = TextMessageTargetMode::Channel, .id = sender->channelId };
                    }
                }

                if ( const Client* self = m_Clients.Find( m_ClientId ) ) {
                    if ( self->channelId != 0 ) {
                        return TextMessageTarget { .mode = TextMessageTargetMode::Channel, .id = self->channelId };
                    }
                }

                return std::nullopt;

            case TextMessageTargetMode::Server:
                return TextMessageTarget { .mode = TextMessageTargetMode::Server, .id = 0 };
        }

        return std::nullopt;
    }

    std::string Session::ChannelNameFor( const TextMessageEntry& message ) const {
        if ( message.targetMode != TextMessageTargetMode::Channel ) {
            return {};
        }

        std::uint64_t channelId = 0;

        if ( message.targetId && *message.targetId != 0 && m_Channels.Find( *message.targetId ) != nullptr ) {
            channelId = *message.targetId;
        } else if ( message.invokerId != 0 ) {
            const Client* sender = m_Clients.Find( message.invokerId );

            if ( sender != nullptr ) {
                channelId = sender->channelId;
            }
        }

        if ( channelId == 0 ) {
            channelId = m_CurrentChannelId;
        }

        const Channel* channel = m_Channels.Find( channelId );
        return channel == nullptr ? std::string {} : channel->name;
    }

    std::string Session::PrivatePeerNameFor( const TextMessageEntry& message ) const {
        if ( message.targetMode != TextMessageTargetMode::Private ) {
            return {};
        }

        if ( message.invokerId != m_ClientId ) {
            if ( !message.invokerName.empty() ) {
                return message.invokerName;
            }

            if ( const Client* sender = m_Clients.Find( message.invokerId ) ) {
                return sender->nickname;
            }

            return {};
        }

        if ( message.targetId && *message.targetId <= std::numeric_limits<std::uint16_t>::max() ) {
            const auto targetId = static_cast<std::uint16_t>( *message.targetId );

            if ( const Client* target = m_Clients.Find( targetId ) ) {
                return target->nickname;
            }
        }

        return {};
    }

    void Session::SendConnectionInfo() {
        const SetConnectionInfo connectionInfo( m_SessionTransport.Statistics() );
        m_SessionTransport.SendCommand( connectionInfo.Serialize() );
    }

    VoiceCodec Session::CurrentVoiceCodec() const {
        const Channel* channel = m_Channels.Find( m_CurrentChannelId );

        if ( channel == nullptr ) {
            throw std::runtime_error( "Current voice channel is unknown" );
        }

        if ( channel->codec == static_cast<std::uint8_t>( VoiceCodec::OpusVoice ) ) {
            return VoiceCodec::OpusVoice;
        }

        if ( channel->codec == static_cast<std::uint8_t>( VoiceCodec::OpusMusic ) ) {
            return VoiceCodec::OpusMusic;
        }

        throw std::runtime_error( "Current channel does not use an Opus codec" );
    }

    bool Session::VoiceEncrypted() const {
        if ( m_CodecEncryptionMode == 2 ) {
            return true;
        }

        if ( m_CodecEncryptionMode == 1 ) {
            return false;
        }

        const Channel* channel = m_Channels.Find( m_CurrentChannelId );

        if ( channel == nullptr ) {
            throw std::runtime_error( "Current voice channel is unknown" );
        }

        return !channel->codecIsUnencrypted;
    }

    std::uint16_t Session::ClientId() const {
        return m_ClientId;
    }

    std::uint64_t Session::CurrentChannelId() const {
        return m_CurrentChannelId;
    }

    std::string_view Session::CurrentNickname() const {
        return m_CurrentNickname;
    }

    std::string_view Session::ServerName() const {
        return m_ServerName;
    }

    const ChannelStore& Session::Channels() const {
        return m_Channels;
    }

    const ClientStore& Session::Clients() const {
        return m_Clients;
    }

} // namespace ts::protocol
