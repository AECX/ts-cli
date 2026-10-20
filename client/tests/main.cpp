#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>

namespace ts::client::test {

    void RunActionQueueTests();
    void RunChannelTreeViewTests();
    void RunCommandHandlerTests();
    void RunCommandParserTests();
    void RunEventQueueTests();
    void RunPresentationTests();
    void RunTalkerActivityTests();
    void RunTargetResolverTests();

    struct TestGroup {
        std::string_view name;
        void ( *run )();
    };

} // namespace ts::client::test

int main() {
    constexpr std::array<ts::client::test::TestGroup, 8> TestGroups = {
        ts::client::test::TestGroup { .name = "ActionQueue", .run = ts::client::test::RunActionQueueTests },
        ts::client::test::TestGroup { .name = "ChannelTreeView", .run = ts::client::test::RunChannelTreeViewTests },
        ts::client::test::TestGroup { .name = "CommandHandler", .run = ts::client::test::RunCommandHandlerTests },
        ts::client::test::TestGroup { .name = "CommandParser", .run = ts::client::test::RunCommandParserTests },
        ts::client::test::TestGroup { .name = "EventQueue", .run = ts::client::test::RunEventQueueTests },
        ts::client::test::TestGroup { .name = "Presentation", .run = ts::client::test::RunPresentationTests },
        ts::client::test::TestGroup { .name = "TalkerActivity", .run = ts::client::test::RunTalkerActivityTests },
        ts::client::test::TestGroup { .name = "TargetResolver", .run = ts::client::test::RunTargetResolverTests } };

    std::size_t failed = 0;

    for ( const ts::client::test::TestGroup& testGroup : TestGroups ) {
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
