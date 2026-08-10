#ifndef TS_CLIENT_CONFIG_SETUP_HPP
#define TS_CLIENT_CONFIG_SETUP_HPP

#include "config.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <protocol/identity.hpp>
#include <string>
#include <string_view>

namespace ts::client {

    class ConfigSetup {
      public:
        [[nodiscard]] static Config Run( const std::filesystem::path& configPath, const std::filesystem::path& identityPath );

        [[nodiscard]] static Config Run( const std::filesystem::path& configPath,
                                         const std::filesystem::path& identityPath,
                                         std::istream& input,
                                         std::ostream& output );

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

        [[nodiscard]] static protocol::Identity LoadPemIdentity( const std::filesystem::path& path );

        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value );
    };

} // namespace ts::client

#endif // TS_CLIENT_CONFIG_SETUP_HPP
