#ifndef TS_PROTOCOL_TEST_SUPPORT_HPP
#define TS_PROTOCOL_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::test {

    class Failure final: public std::runtime_error {
      public:
        explicit Failure( std::string message ): std::runtime_error( std::move( message ) ) {
        }
    };

    inline void Expect( bool condition, std::string_view message ) {
        if ( !condition ) {
            throw Failure( std::string( message ) );
        }
    }

    template<typename Actual, typename Expected>
    void ExpectEqual( const Actual& actual, const Expected& expected, std::string_view message ) {
        if ( !( actual == expected ) ) {
            throw Failure( std::string( message ) );
        }
    }

    template<typename Exception, typename Function>
    void ExpectThrows( Function&& function, std::string_view message ) {
        try {
            std::forward<Function>( function )();
        } catch ( const Exception& ) {
            return;
        } catch ( const std::exception& exception ) {
            throw Failure( std::string( message ) + ": unexpected exception: " + exception.what() );
        } catch ( ... ) {
            throw Failure( std::string( message ) + ": unexpected non-standard exception" );
        }

        throw Failure( std::string( message ) + ": expected an exception" );
    }

    inline std::vector<std::byte> Bytes( std::string_view value ) {
        std::vector<std::byte> result;

        result.reserve( value.size() );

        for ( const char character : value ) {
            result.push_back( static_cast<std::byte>( static_cast<unsigned char>( character ) ) );
        }

        return result;
    }

    inline std::vector<std::byte> Bytes( std::initializer_list<std::uint8_t> values ) {
        std::vector<std::byte> result;

        result.reserve( values.size() );

        for ( const std::uint8_t value : values ) {
            result.push_back( static_cast<std::byte>( value ) );
        }

        return result;
    }

    inline std::uint8_t HexNibble( char character ) {
        if ( character >= '0' && character <= '9' ) {
            return static_cast<std::uint8_t>( character - '0' );
        }

        if ( character >= 'a' && character <= 'f' ) {
            return static_cast<std::uint8_t>( character - 'a' + 10 );
        }

        if ( character >= 'A' && character <= 'F' ) {
            return static_cast<std::uint8_t>( character - 'A' + 10 );
        }

        throw Failure( "Invalid hexadecimal character" );
    }

    inline bool IsWhitespace( char character ) {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r';
    }

    inline std::vector<std::byte> Hex( std::string_view value ) {
        std::vector<std::byte> result;

        int high = -1;

        for ( const char character : value ) {
            if ( IsWhitespace( character ) ) {
                continue;
            }

            const std::uint8_t nibble = HexNibble( character );

            if ( high < 0 ) {
                high = static_cast<int>( nibble );

                continue;
            }

            result.push_back( static_cast<std::byte>( ( static_cast<std::uint8_t>( high ) << 4 ) | nibble ) );

            high = -1;
        }

        if ( high >= 0 ) {
            throw Failure( "Odd hexadecimal string length" );
        }

        return result;
    }

} // namespace ts::test

#endif // TS_PROTOCOL_TEST_SUPPORT_HPP
