#include <cstddef>
#include <cstdint>
#include <limits>
#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/voice/voice_codec.hpp>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    namespace {
        void ValidateCodec( VoiceCodec codec ) {
            if ( static_cast<std::uint8_t>( codec ) > static_cast<std::uint8_t>( VoiceCodec::OpusMusic ) ) {
                throw std::runtime_error( "Unsupported TeamSpeak voice codec" );
            }
        }

        void ValidatePayloadSize( std::size_t metadataSize, std::size_t dataSize, const char* message ) {
            if ( metadataSize > packet_limits::MaxClientPayload || dataSize > packet_limits::MaxClientPayload - metadataSize ) {
                throw std::runtime_error( message );
            }
        }
    } // namespace

    std::vector<std::byte>
        VoiceWireCodec::EncodeClient( std::uint16_t voiceId, VoiceCodec codec, std::span<const std::byte> data ) {
        constexpr std::size_t MetadataSize = 3;

        ValidateCodec( codec );
        ValidatePayloadSize( MetadataSize, data.size(), "Encoded voice frame is too large for a TeamSpeak datagram" );

        BinaryWriter writer;
        writer.WriteU16( voiceId );
        writer.WriteU8( static_cast<std::uint8_t>( codec ) );
        writer.WriteBytes( data );
        return writer.Take();
    }

    std::vector<std::byte> VoiceWireCodec::EncodeClientWhisper( std::uint16_t voiceId,
                                                                VoiceCodec codec,
                                                                std::span<const std::uint64_t> channelIds,
                                                                std::span<const std::uint16_t> clientIds,
                                                                std::span<const std::byte> data ) {
        constexpr std::size_t FixedMetadataSize = 5;
        constexpr std::size_t ChannelIdSize = 8;
        constexpr std::size_t ClientIdSize = 2;

        ValidateCodec( codec );

        if ( channelIds.size() > std::numeric_limits<std::uint8_t>::max() ) {
            throw std::runtime_error( "Too many TeamSpeak VoiceWhisper channel targets" );
        }
        if ( clientIds.size() > std::numeric_limits<std::uint8_t>::max() ) {
            throw std::runtime_error( "Too many TeamSpeak VoiceWhisper client targets" );
        }

        const std::size_t metadataSize =
            FixedMetadataSize + ( channelIds.size() * ChannelIdSize ) + ( clientIds.size() * ClientIdSize );
        ValidatePayloadSize( metadataSize, data.size(), "Encoded VoiceWhisper frame is too large for a TeamSpeak datagram" );

        BinaryWriter writer;
        writer.WriteU16( voiceId );
        writer.WriteU8( static_cast<std::uint8_t>( codec ) );
        writer.WriteU8( static_cast<std::uint8_t>( channelIds.size() ) );
        writer.WriteU8( static_cast<std::uint8_t>( clientIds.size() ) );
        for ( const std::uint64_t channelId : channelIds ) {
            writer.WriteU64( channelId );
        }
        for ( const std::uint16_t clientId : clientIds ) {
            writer.WriteU16( clientId );
        }
        writer.WriteBytes( data );
        return writer.Take();
    }

    std::vector<std::byte> VoiceWireCodec::EncodeClientGroupWhisper( std::uint16_t voiceId,
                                                                     VoiceCodec codec,
                                                                     GroupWhisperType type,
                                                                     GroupWhisperTarget target,
                                                                     std::uint64_t targetId,
                                                                     std::span<const std::byte> data ) {
        constexpr std::size_t MetadataSize = 13;

        ValidateCodec( codec );
        if ( static_cast<std::uint8_t>( type ) > static_cast<std::uint8_t>( GroupWhisperType::AllClients ) ) {
            throw std::runtime_error( "Unsupported TeamSpeak group whisper type" );
        }
        if ( static_cast<std::uint8_t>( target ) > static_cast<std::uint8_t>( GroupWhisperTarget::Subchannels ) ) {
            throw std::runtime_error( "Unsupported TeamSpeak group whisper target" );
        }
        ValidatePayloadSize( MetadataSize,
                             data.size(),
                             "Encoded group VoiceWhisper frame is too large for a TeamSpeak datagram" );

        BinaryWriter writer;
        writer.WriteU16( voiceId );
        writer.WriteU8( static_cast<std::uint8_t>( codec ) );
        writer.WriteU8( static_cast<std::uint8_t>( type ) );
        writer.WriteU8( static_cast<std::uint8_t>( target ) );
        writer.WriteU64( targetId );
        writer.WriteBytes( data );
        return writer.Take();
    }

    VoiceFrame VoiceWireCodec::DecodeServer( std::span<const std::byte> payload, PacketFlags flags, bool whisper ) {
        constexpr std::size_t MetadataSize = 5;

        if ( HasFlag( flags, PacketFlags::Fragmented ) ) {
            throw std::runtime_error( "Fragmented TeamSpeak Voice packets are not supported" );
        }
        if ( payload.size() < MetadataSize ) {
            throw std::runtime_error( "Server Voice payload is too small" );
        }

        BinaryReader reader( payload );

        VoiceFrame frame;
        frame.voiceId = reader.ReadU16();
        frame.clientId = reader.ReadU16();
        const std::uint8_t codec = reader.ReadU8();
        if ( codec > static_cast<std::uint8_t>( VoiceCodec::OpusMusic ) ) {
            throw std::runtime_error( "Unsupported TeamSpeak voice codec" );
        }

        frame.codec = static_cast<VoiceCodec>( codec );
        frame.talkStart = HasFlag( flags, PacketFlags::Compressed );
        frame.whisper = whisper;
        const auto data = reader.ReadBytes( reader.Remaining() );
        frame.data.assign( data.begin(), data.end() );
        frame.talkEnd = frame.data.empty();
        return frame;
    }

} // namespace ts::protocol
