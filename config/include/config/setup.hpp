#ifndef TS_CONFIG_SETUP_HPP
#define TS_CONFIG_SETUP_HPP

#include "config.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <protocol/identity.hpp>
#include <string>
#include <string_view>

namespace ts::config {

    class ConfigSetup {
      public:
        [[nodiscard]] static Config Run( const std::filesystem::path& configPath, const std::filesystem::path& identityPath );

        [[nodiscard]] static Config Run( const std::filesystem::path& configPath,
                                         const std::filesystem::path& identityPath,
                                         std::istream& input,
                                         std::ostream& output );

        /*
         * Non-interactive first run for front ends without a terminal: writes
         * the default client profile and generates a new identity at the
         * default security level, unless a valid identity already exists, in
         * which case it is preserved exactly like the interactive setup does.
         */
        [[nodiscard]] static Config CreateDefault( const std::filesystem::path& configPath,
                                                   const std::filesystem::path& identityPath );

      private:
        [[nodiscard]] static std::string
            Prompt( std::istream& input, std::ostream& output, std::string_view label, std::string_view defaultValue );

        [[nodiscard]] static bool
            PromptYesNo( std::istream& input, std::ostream& output, std::string_view label, bool defaultValue );

        [[nodiscard]] static std::uint64_t PromptUnsigned( std::istream& input,
                                                           std::ostream& output,
                                                           std::string_view label,
                                                           std::uint64_t defaultValue,
                                                           std::uint64_t maximum );

        [[nodiscard]] static protocol::Identity PromptIdentity( std::istream& input, std::ostream& output );

        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value );
    };

} // namespace ts::config

#endif // TS_CONFIG_SETUP_HPP
