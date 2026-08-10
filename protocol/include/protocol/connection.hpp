#ifndef TS_PROTOCOL_CONNECTION_HPP
#define TS_PROTOCOL_CONNECTION_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <net/address.hpp>
#include <net/udp_socket.hpp>
#include <protocol/client_profile.hpp>
#include <protocol/handshake/handshake.hpp>
#include <protocol/identity.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <protocol/transport.hpp>
#include <span>
#include <stop_token>
#include <string_view>

namespace ts::protocol {

    class ChannelStore;
    class ClientStore;
    class Session;
    class Connection;

    using SessionEventHandler = std::function<void( SessionEvent )>;
    using ConnectionCycleHandler = std::function<void( Connection& )>;

    class Connection {
      public:
        Connection( std::string_view endpoint, ClientProfile profile, Identity identity, std::uint64_t keyOffset );

        ~Connection();

        Connection( const Connection& ) = delete;
        Connection& operator=( const Connection& ) = delete;

        Connection( Connection&& ) = delete;
        Connection& operator=( Connection&& ) = delete;

        void Connect();

        /*
         * Long-lived established-session loop. Exactly this thread owns all
         * mutable TeamSpeak protocol/session state while Run is active.
         *
         * cycleHandler executes on that same owner thread. The client runtime
         * uses it to drain application actions without exposing Session across
         * threads.
         */
        void Run( std::stop_token stopToken, SessionEventHandler eventHandler = {}, ConnectionCycleHandler cycleHandler = {} );

        /* Thread-safe wakeup for producers that queued work for the network owner thread. */
        void Wake();

        void SendTextMessage( TextMessageTarget target, std::string_view text );
        void MoveToChannel( std::uint64_t channelId );
        void ChangeNickname( std::string_view nickname );
        void SetAudioState( bool inputHardware, bool outputHardware, bool inputMuted );
        void SendVoice( std::span<const std::byte> data, bool talkStart );

        [[nodiscard]] std::uint16_t ClientId() const;
        [[nodiscard]] std::uint64_t CurrentChannelId() const;
        [[nodiscard]] std::string_view CurrentNickname() const;
        [[nodiscard]] std::string_view ServerName() const;
        [[nodiscard]] const ChannelStore& Channels() const;
        [[nodiscard]] const ClientStore& Clients() const;

      private:
        void ProcessOnce( std::chrono::milliseconds maximumWait, SessionEventHandler& eventHandler );

        void DisconnectGracefully( SessionEventHandler& eventHandler );
        void DispatchEvents( SessionEventHandler& eventHandler );

        net::Address m_Address;
        net::UdpSocket m_Socket;

        Transport m_Transport;

        ClientProfile m_Profile;
        Identity m_Identity;

        std::uint64_t m_KeyOffset = 0;

        Handshake m_Handshake;

        std::unique_ptr<Session> m_Session;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CONNECTION_HPP
