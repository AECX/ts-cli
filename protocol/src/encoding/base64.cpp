#include <limits>
#include <openssl/evp.h>
#include <protocol/encoding/base64.hpp>
#include <stdexcept>

namespace ts::protocol {

    std::string Base64Encode( std::span<const std::byte> data ) {
        if ( data.empty() ) {
            return {};
        }

        if ( data.size() > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) {
            throw std::runtime_error( "Base64 input is too large" );
        }

        const std::size_t encodedSize = 4 * ( ( data.size() + 2 ) / 3 );

        // EVP_EncodeBlock also writes a trailing NUL.
        std::string result( encodedSize + 1, '\0' );

        const int written = EVP_EncodeBlock( reinterpret_cast<unsigned char*>( result.data() ),
                                             reinterpret_cast<const unsigned char*>( data.data() ),
                                             static_cast<int>( data.size() ) );

        if ( written < 0 ) {
            throw std::runtime_error( "Failed to Base64 encode data" );
        }

        result.resize( static_cast<std::size_t>( written ) );

        return result;
    }

    std::vector<std::byte> Base64Decode( std::string_view data ) {
        if ( data.empty() ) {
            return {};
        }

        if ( data.size() > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) {
            throw std::runtime_error( "Base64 input is too large" );
        }

        std::string normalized( data );

        switch ( normalized.size() % 4 ) {
            case 0:
                break;

            case 2:
                normalized.append( "==" );
                break;

            case 3:
                normalized.push_back( '=' );
                break;

            case 1:
                throw std::runtime_error( "Invalid Base64 length" );
        }

        const std::size_t decodedSize = ( normalized.size() / 4 ) * 3;

        std::vector<std::byte> result( decodedSize );

        const int written = EVP_DecodeBlock( reinterpret_cast<unsigned char*>( result.data() ),
                                             reinterpret_cast<const unsigned char*>( normalized.data() ),
                                             static_cast<int>( normalized.size() ) );

        if ( written < 0 ) {
            throw std::runtime_error( "Failed to Base64 decode data" );
        }

        std::size_t padding = 0;

        if ( !normalized.empty() && normalized.back() == '=' ) {
            ++padding;
        }

        if ( normalized.size() >= 2 && normalized[normalized.size() - 2] == '=' ) {
            ++padding;
        }

        result.resize( static_cast<std::size_t>( written ) - padding );

        return result;
    }

} // namespace ts::protocol
