#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>

namespace ts::client::gtk::test {

    void RunConversationLogTests();
    void RunEventRouterTests();

    struct TestGroup {
        std::string_view name;
        void ( *run )();
    };

} // namespace ts::client::gtk::test

int main() {
    constexpr std::array<ts::client::gtk::test::TestGroup, 2> TestGroups = {
        ts::client::gtk::test::TestGroup { .name = "ConversationLog", .run = ts::client::gtk::test::RunConversationLogTests },
        ts::client::gtk::test::TestGroup { .name = "EventRouter", .run = ts::client::gtk::test::RunEventRouterTests } };

    std::size_t failed = 0;

    for ( const ts::client::gtk::test::TestGroup& testGroup : TestGroups ) {
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
