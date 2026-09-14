#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/voice/voice.hpp>
#include <protocol/voice/voice_codec.hpp>

namespace ts::test {

    void RunWhisperTargetTests() {
        const std::array<std::byte, 2> audio { std::byte { 0xaa }, std::byte { 0xbb } };

        /*
         * The public WhisperTarget must encode to exactly the same wire bytes
         * as the spans the codec has always taken, so exposing it through
         * Session changes the API and not the protocol.
         */
        const protocol::WhisperTarget target { .channelIds = { 0x0102030405060708ULL }, .clientIds = { 0x1112, 0x1314 } };

        const auto encoded = protocol::VoiceWireCodec::EncodeClientWhisper( 0x1234,
                                                                            protocol::VoiceCodec::OpusVoice,
                                                                            target.channelIds,
                                                                            target.clientIds,
                                                                            audio );

        ExpectEqual( encoded,
                     Hex( "12 34 04 01 02 01 02 03 04 05 06 07 08 11 12 13 14 aa bb" ),
                     "WhisperTarget did not encode to the expected wire bytes" );

        const protocol::GroupWhisper group { .type = protocol::GroupWhisperType::ChannelGroup,
                                             .target = protocol::GroupWhisperTarget::CurrentChannel,
                                             .targetId = 0x0102030405060708ULL };

        const auto encodedGroup = protocol::VoiceWireCodec::EncodeClientGroupWhisper( 0x1234,
                                                                                      protocol::VoiceCodec::OpusVoice,
                                                                                      group.type,
                                                                                      group.target,
                                                                                      group.targetId,
                                                                                      audio );

        ExpectEqual( encodedGroup,
                     Hex( "12 34 04 01 01 01 02 03 04 05 06 07 08 aa bb" ),
                     "GroupWhisper did not encode to the expected wire bytes" );

        /* Defaults must be the conservative ones documented on the structs. */
        const protocol::GroupWhisper defaulted;

        Expect( defaulted.type == protocol::GroupWhisperType::ServerGroup, "GroupWhisper type default changed" );
        Expect( defaulted.target == protocol::GroupWhisperTarget::CurrentChannel, "GroupWhisper target default changed" );
        ExpectEqual( defaulted.targetId, std::uint64_t { 0 }, "GroupWhisper target id default changed" );

        const protocol::WhisperTarget empty;

        Expect( empty.channelIds.empty() && empty.clientIds.empty(),
                "A default WhisperTarget must be empty so Session can reject it" );
    }

} // namespace ts::test
