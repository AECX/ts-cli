#include <cctype>
#include <charconv>
#include <client/config/setup.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::client {

    Config ConfigSetup::Run( const std::filesystem::path& configPath, const std::filesystem::path& identityPath ) {
        return Run( configPath, identityPath, std::cin, std::cout );
    }

    Config ConfigSetup::Run( const std::filesystem::path& configPath,
                             const std::filesystem::path& identityPath,
                             std::istream& input,
                             std::ostream& output ) {
        output << "No ts-cli configuration found." << std::endl << "Starting first-run setup." << std::endl << std::endl;

        protocol::ClientProfile profile = Config::DefaultProfile();

        profile.nickname = Prompt( input, output, "Nickname", profile.nickname );

        output << std::endl;

        const bool useDefaultVersion = PromptYesNo( input, output, "Use the default TeamSpeak client version", true );

        if ( !useDefaultVersion ) {
            output << std::endl
                   << "Enter the TeamSpeak wire client profile." << std::endl
                   << "These values are sent exactly as configured." << std::endl
                   << std::endl;

            profile.version.initVersion =
                static_cast<std::uint32_t>( PromptUnsigned( input,
                                                            output,
                                                            "Init1 client version",
                                                            profile.version.initVersion,
                                                            std::numeric_limits<std::uint32_t>::max() ) );

            profile.version.version = Prompt( input, output, "Client version", profile.version.version );

            profile.version.platform = Prompt( input, output, "Client platform", profile.version.platform );

            std::string signature =
                Prompt( input, output, "Client version signature ('-' for empty)", profile.version.signature );

            if ( signature == "-" ) {
                signature.clear();
            }

            profile.version.signature = std::move( signature );
        }

        output << std::endl;

        if ( std::filesystem::exists( identityPath ) ) {
            /*
             * The client has exactly one local identity. If the ordinary
             * config was removed but the identity survived, preserve it
             * instead of silently replacing the TeamSpeak identity. Loading
             * it here also validates the file before setup continues.
             */
            (void)IdentityStore::Load( identityPath );
            output << "Using existing TeamSpeak identity:" << std::endl << "  " << identityPath << std::endl;
        } else {
            const std::uint8_t securityLevel = static_cast<std::uint8_t>(
                PromptUnsigned( input, output, "Identity security level", IdentityStore::DefaultSecurityLevel(), 32 ) );

            output << std::endl;

            protocol::Identity identity = PromptIdentity( input, output );

            output << std::endl << "Calculating identity key offset..." << std::endl;

            const LocalIdentity localIdentity = IdentityStore::Create( std::move( identity ), securityLevel );
            IdentityStore::Save( identityPath, localIdentity );
        }

        Config config = Config::Create( std::move( profile ) );
        config.Save( configPath );

        output << "Configuration written to:" << std::endl
               << "  " << configPath << std::endl
               << "Identity written to:" << std::endl
               << "  " << identityPath << std::endl
               << std::endl;

        return config;
    }

    std::string ConfigSetup::Prompt( std::istream& input,
                                     std::ostream& output,
                                     std::string_view label,
                                     std::string_view defaultValue ) {
        output << label;

        if ( !defaultValue.empty() ) {
            output << " [" << defaultValue << ']';
        }

        output << ": " << std::flush;

        std::string value;

        if ( !std::getline( input, value ) ) {
            throw std::runtime_error( "Unexpected end of input during configuration" );
        }

        if ( value.empty() ) {
            return std::string( defaultValue );
        }

        return value;
    }

    bool ConfigSetup::PromptYesNo( std::istream& input, std::ostream& output, std::string_view label, bool defaultValue ) {
        while ( true ) {
            output << label << ( defaultValue ? " [Y/n]: " : " [y/N]: " ) << std::flush;

            std::string value;

            if ( !std::getline( input, value ) ) {
                throw std::runtime_error( "Unexpected end of input during configuration" );
            }

            if ( value.empty() ) {
                return defaultValue;
            }

            if ( value.size() == 1 ) {
                const char choice = static_cast<char>( std::tolower( static_cast<unsigned char>( value.front() ) ) );

                if ( choice == 'y' ) {
                    return true;
                }

                if ( choice == 'n' ) {
                    return false;
                }
            }

            output << "Please enter y or n." << std::endl;
        }
    }

    std::uint64_t ConfigSetup::PromptUnsigned( std::istream& input,
                                               std::ostream& output,
                                               std::string_view label,
                                               std::uint64_t defaultValue,
                                               std::uint64_t maximum ) {
        while ( true ) {
            output << label << " [" << defaultValue << "]: " << std::flush;

            std::string value;

            if ( !std::getline( input, value ) ) {
                throw std::runtime_error( "Unexpected end of input during configuration" );
            }

            if ( value.empty() ) {
                return defaultValue;
            }

            try {
                const std::uint64_t parsed = ParseUnsigned( value );

                if ( parsed > maximum ) {
                    output << "Value is too large." << std::endl;

                    continue;
                }

                return parsed;
            } catch ( const std::runtime_error& ) {
                output << "Please enter a decimal or hexadecimal integer." << std::endl;
            }
        }
    }

    protocol::Identity ConfigSetup::PromptIdentity( std::istream& input, std::ostream& output ) {
        const bool generate = PromptYesNo( input, output, "Generate a new TeamSpeak identity", true );

        if ( generate ) {
            output << "Generating new P-256 identity..." << std::endl;

            return protocol::Identity {};
        }

        while ( true ) {
            output << "Private key PEM path: " << std::flush;

            std::string path;

            if ( !std::getline( input, path ) ) {
                throw std::runtime_error( "Unexpected end of input during configuration" );
            }

            if ( path.empty() ) {
                output << "A PEM path is required." << std::endl;

                continue;
            }

            try {
                return LoadPemIdentity( path );
            } catch ( const std::exception& exception ) {
                output << "Could not import identity: " << exception.what() << std::endl;
            }
        }
    }

    protocol::Identity ConfigSetup::LoadPemIdentity( const std::filesystem::path& path ) {
        std::ifstream stream( path, std::ios::binary );

        if ( !stream ) {
            throw std::runtime_error( "Failed to open PEM file: " + path.string() );
        }

        std::ostringstream data;

        data << stream.rdbuf();

        if ( stream.bad() ) {
            throw std::runtime_error( "Failed to read PEM file: " + path.string() );
        }

        return protocol::Identity::FromPrivateKeyPem( data.str() );
    }

    std::uint64_t ConfigSetup::ParseUnsigned( std::string_view value ) {
        int base = 10;

        if ( value.size() >= 2 && value[0] == '0' && ( value[1] == 'x' || value[1] == 'X' ) ) {
            value.remove_prefix( 2 );

            base = 16;
        }

        if ( value.empty() ) {
            throw std::runtime_error( "Empty integer" );
        }

        std::uint64_t result = 0;

        const char* first = value.data();

        const char* last = first + value.size();

        const auto parsed = std::from_chars( first, last, result, base );

        if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
            throw std::runtime_error( "Invalid integer" );
        }

        return result;
    }

} // namespace ts::client
