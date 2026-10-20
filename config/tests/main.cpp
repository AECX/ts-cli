#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>

namespace ts::config::test {

    void RunAudioSettingsTests();
    void RunConfigTests();
    void RunSetupTests();
    void RunUserConfigTests();

    struct TestGroup {
        std::string_view name;
        void ( *run )();
    };

} // namespace ts::config::test

int main() {
    constexpr std::array<ts::config::test::TestGroup, 4> TestGroups = {
        ts::config::test::TestGroup { .name = "AudioSettings", .run = ts::config::test::RunAudioSettingsTests },
        ts::config::test::TestGroup { .name = "Config", .run = ts::config::test::RunConfigTests },
        ts::config::test::TestGroup { .name = "Setup", .run = ts::config::test::RunSetupTests },
        ts::config::test::TestGroup { .name = "UserConfig", .run = ts::config::test::RunUserConfigTests } };

    std::size_t failed = 0;

    for ( const ts::config::test::TestGroup& testGroup : TestGroups ) {
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
