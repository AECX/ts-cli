#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>

namespace ts::test {

    void RunBinaryReaderTests();
    void RunBootstrapCryptoTests();
    void RunChannelStateTests();
    void RunClientCommandTests();
    void RunClientDisconnectTests();
    void RunClientStateTests();
    void RunCommandFragmenterTests();
    void RunCommandParserTests();
    void RunCommandWriterTests();
    void RunConnectionStatisticsTests();
    void RunIdentityTests();
    void RunPacketSequenceTests();
    void RunQuickLzTests();
    void RunReliableCommandQueueTests();
    void RunSessionCryptoTests();
    void RunSetConnectionInfoTests();
    void RunTextMessageTests();
    void RunVoiceCodecTests();

    struct TestGroup {
        std::string_view name;
        void ( *run )();
    };

} // namespace ts::test

int main() {
    constexpr std::array<ts::test::TestGroup, 18> TestGroups = {
        ts::test::TestGroup { .name = "BinaryReader", .run = ts::test::RunBinaryReaderTests },
        ts::test::TestGroup { .name = "BootstrapCrypto", .run = ts::test::RunBootstrapCryptoTests },
        ts::test::TestGroup { .name = "ChannelState", .run = ts::test::RunChannelStateTests },
        ts::test::TestGroup { .name = "ClientCommand", .run = ts::test::RunClientCommandTests },
        ts::test::TestGroup { .name = "ClientDisconnect", .run = ts::test::RunClientDisconnectTests },
        ts::test::TestGroup { .name = "ClientState", .run = ts::test::RunClientStateTests },
        ts::test::TestGroup { .name = "CommandFragmenter", .run = ts::test::RunCommandFragmenterTests },
        ts::test::TestGroup { .name = "CommandParser", .run = ts::test::RunCommandParserTests },
        ts::test::TestGroup { .name = "CommandWriter", .run = ts::test::RunCommandWriterTests },
        ts::test::TestGroup { .name = "ConnectionStatistics", .run = ts::test::RunConnectionStatisticsTests },
        ts::test::TestGroup { .name = "Identity", .run = ts::test::RunIdentityTests },
        ts::test::TestGroup { .name = "PacketSequence", .run = ts::test::RunPacketSequenceTests },
        ts::test::TestGroup { .name = "QuickLz", .run = ts::test::RunQuickLzTests },
        ts::test::TestGroup { .name = "ReliableCommandQueue", .run = ts::test::RunReliableCommandQueueTests },
        ts::test::TestGroup { .name = "SessionCrypto", .run = ts::test::RunSessionCryptoTests },
        ts::test::TestGroup { .name = "SetConnectionInfo", .run = ts::test::RunSetConnectionInfoTests },
        ts::test::TestGroup { .name = "TextMessage", .run = ts::test::RunTextMessageTests },
        ts::test::TestGroup { .name = "VoiceCodec", .run = ts::test::RunVoiceCodecTests } };

    std::size_t failed = 0;

    for ( const ts::test::TestGroup& testGroup : TestGroups ) {
        try {
            testGroup.run();

            std::cout << "[pass] " << testGroup.name << std::endl;
        } catch ( const std::exception& exception ) {
            ++failed;

            std::cerr << "[fail] " << testGroup.name << ": " << exception.what() << std::endl;
        } catch ( ... ) {
            ++failed;

            std::cerr << "[fail] " << testGroup.name << ": unknown exception" << std::endl;
        }
    }

    if ( failed != 0 ) {
        std::cerr << failed << " test group(s) failed" << std::endl;

        return 1;
    }

    std::cout << TestGroups.size() << " test groups passed" << std::endl;

    return 0;
}
