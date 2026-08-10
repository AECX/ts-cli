#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/binary_writer.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/voice/voice.hpp>
#include <protocol/voice/voice_codec.hpp>
#include <stdexcept>
#include <vector>

namespace ts::test {
    void RunVoiceCodecTests() {
        const std::array<std::byte, 4> audio { std::byte { 1 }, std::byte { 2 }, std::byte { 3 }, std::byte { 4 } };
        const auto client = protocol::VoiceWireCodec::EncodeClient( 42, protocol::VoiceCodec::OpusVoice, audio );
        ExpectEqual( client.size(), std::size_t { 7 }, "Client Voice payload has the wrong size" );
        ExpectEqual( client[0], std::byte { 0 }, "Client Voice id high byte is wrong" );
        ExpectEqual( client[1], std::byte { 42 }, "Client Voice id low byte is wrong" );
        ExpectEqual( client[2], std::byte { 4 }, "Client Voice codec is wrong" );

        const std::array<std::uint64_t, 1> whisperChannels { 0x0102030405060708ULL };
        const std::array<std::uint16_t, 2> whisperClients { 0x1112, 0x1314 };
        const std::array<std::byte, 2> whisperAudio { std::byte { 0xaa }, std::byte { 0xbb } };
        const auto whisper = protocol::VoiceWireCodec::EncodeClientWhisper( 0x1234,
                                                                            protocol::VoiceCodec::OpusVoice,
                                                                            whisperChannels,
                                                                            whisperClients,
                                                                            whisperAudio );
        const auto expectedWhisper = Hex( "12 34 04 01 02 01 02 03 04 05 06 07 08 11 12 13 14 aa bb" );
        ExpectEqual( whisper, expectedWhisper, "Client VoiceWhisper payload is wrong" );

        const auto groupWhisper =
            protocol::VoiceWireCodec::EncodeClientGroupWhisper( 0x1234,
                                                                protocol::VoiceCodec::OpusVoice,
                                                                protocol::GroupWhisperType::ChannelGroup,
                                                                protocol::GroupWhisperTarget::CurrentChannel,
                                                                0x0102030405060708ULL,
                                                                whisperAudio );
        const auto expectedGroupWhisper = Hex( "12 34 04 01 01 01 02 03 04 05 06 07 08 aa bb" );
        ExpectEqual( groupWhisper, expectedGroupWhisper, "Client group VoiceWhisper payload is wrong" );

        protocol::BinaryWriter writer;
        writer.WriteU16( 42 );
        writer.WriteU16( 7 );
        writer.WriteU8( static_cast<std::uint8_t>( protocol::VoiceCodec::OpusVoice ) );
        writer.WriteBytes( audio );

        const protocol::VoiceFrame frame =
            protocol::VoiceWireCodec::DecodeServer( writer.Take(), protocol::PacketFlags::Compressed, false );
        ExpectEqual( frame.voiceId, std::uint16_t { 42 }, "Server Voice id is wrong" );
        ExpectEqual( frame.clientId, std::uint16_t { 7 }, "Server Voice client id is wrong" );
        Expect( frame.codec == protocol::VoiceCodec::OpusVoice, "Server Voice codec is wrong" );
        Expect( frame.talkStart, "Server Voice talk-start marker was lost" );
        Expect( !frame.talkEnd, "Non-empty Server Voice frame was marked as talk-end" );
        Expect( !frame.whisper, "Server Voice frame was marked as whisper" );
        ExpectEqual( frame.data.size(), audio.size(), "Server Voice audio size is wrong" );

        protocol::BinaryWriter whisperWriter;
        whisperWriter.WriteU16( 43 );
        whisperWriter.WriteU16( 8 );
        whisperWriter.WriteU8( static_cast<std::uint8_t>( protocol::VoiceCodec::OpusVoice ) );
        const protocol::VoiceFrame receivedWhisper =
            protocol::VoiceWireCodec::DecodeServer( whisperWriter.Take(), protocol::PacketFlags::None, true );
        Expect( receivedWhisper.whisper, "Server VoiceWhisper frame was not marked as whisper" );
        Expect( receivedWhisper.talkEnd, "Empty Server VoiceWhisper frame was not marked as talk-end" );

        const std::array<std::byte, protocol::packet_limits::MaxClientPayload - 3> maximum {};
        (void)protocol::VoiceWireCodec::EncodeClient( 1, protocol::VoiceCodec::OpusVoice, maximum );
        ExpectThrows<std::runtime_error>(
            [] {
                const std::array<std::byte, protocol::packet_limits::MaxClientPayload - 2> tooLarge {};
                (void)protocol::VoiceWireCodec::EncodeClient( 1, protocol::VoiceCodec::OpusVoice, tooLarge );
            },
            "Oversized Voice payload was not rejected" );

        const std::array<std::byte, protocol::packet_limits::MaxClientPayload - 13> maximumGroupWhisper {};
        (void)protocol::VoiceWireCodec::EncodeClientGroupWhisper( 1,
                                                                  protocol::VoiceCodec::OpusVoice,
                                                                  protocol::GroupWhisperType::AllClients,
                                                                  protocol::GroupWhisperTarget::AllChannels,
                                                                  0,
                                                                  maximumGroupWhisper );
        ExpectThrows<std::runtime_error>(
            [] {
                const std::array<std::byte, protocol::packet_limits::MaxClientPayload - 12> tooLarge {};
                (void)protocol::VoiceWireCodec::EncodeClientGroupWhisper( 1,
                                                                          protocol::VoiceCodec::OpusVoice,
                                                                          protocol::GroupWhisperType::AllClients,
                                                                          protocol::GroupWhisperTarget::AllChannels,
                                                                          0,
                                                                          tooLarge );
            },
            "Oversized group VoiceWhisper payload was not rejected" );

        ExpectThrows<std::runtime_error>(
            [] {
                const std::vector<std::uint64_t> tooManyChannels( 256 );
                (void)protocol::VoiceWireCodec::EncodeClientWhisper( 1,
                                                                     protocol::VoiceCodec::OpusVoice,
                                                                     tooManyChannels,
                                                                     {},
                                                                     {} );
            },
            "VoiceWhisper channel target count overflow was not rejected" );
        ExpectThrows<std::runtime_error>(
            [] {
                const std::vector<std::uint16_t> tooManyClients( 256 );
                (void)
                    protocol::VoiceWireCodec::EncodeClientWhisper( 1, protocol::VoiceCodec::OpusVoice, {}, tooManyClients, {} );
            },
            "VoiceWhisper client target count overflow was not rejected" );
        ExpectThrows<std::runtime_error>(
            [] {
                (void)protocol::VoiceWireCodec::EncodeClient( 1,
                                                              static_cast<protocol::VoiceCodec>( 6 ),
                                                              std::span<const std::byte> {} );
            },
            "Unsupported outgoing Voice codec was not rejected" );
        ExpectThrows<std::runtime_error>(
            [] {
                (void)protocol::VoiceWireCodec::EncodeClientGroupWhisper( 1,
                                                                          protocol::VoiceCodec::OpusVoice,
                                                                          static_cast<protocol::GroupWhisperType>( 4 ),
                                                                          protocol::GroupWhisperTarget::AllChannels,
                                                                          0,
                                                                          {} );
            },
            "Unsupported group whisper type was not rejected" );
        ExpectThrows<std::runtime_error>(
            [] {
                (void)protocol::VoiceWireCodec::EncodeClientGroupWhisper( 1,
                                                                          protocol::VoiceCodec::OpusVoice,
                                                                          protocol::GroupWhisperType::AllClients,
                                                                          static_cast<protocol::GroupWhisperTarget>( 7 ),
                                                                          0,
                                                                          {} );
            },
            "Unsupported group whisper target was not rejected" );

        protocol::BinaryWriter fragmentedWriter;
        fragmentedWriter.WriteU16( 1 );
        fragmentedWriter.WriteU16( 2 );
        fragmentedWriter.WriteU8( static_cast<std::uint8_t>( protocol::VoiceCodec::OpusVoice ) );
        const auto fragmentedPayload = fragmentedWriter.Take();
        ExpectThrows<std::runtime_error>(
            [&fragmentedPayload] {
                (void)protocol::VoiceWireCodec::DecodeServer( fragmentedPayload, protocol::PacketFlags::Fragmented, false );
            },
            "Fragmented Server Voice payload was not rejected" );
    }

} // namespace ts::test
