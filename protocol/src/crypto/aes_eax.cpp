#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <optional>
#include <protocol/crypto/aes_eax.hpp>
#include <span>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    class AesEaxOpenSsl {
      public:
        struct MacDeleter {
            void operator()( EVP_MAC* mac ) const {
                EVP_MAC_free( mac );
            }
        };

        struct MacContextDeleter {
            void operator()( EVP_MAC_CTX* context ) const {
                EVP_MAC_CTX_free( context );
            }
        };

        struct CipherContextDeleter {
            void operator()( EVP_CIPHER_CTX* context ) const {
                EVP_CIPHER_CTX_free( context );
            }
        };

        using Mac = std::unique_ptr<EVP_MAC, MacDeleter>;

        using MacContext = std::unique_ptr<EVP_MAC_CTX, MacContextDeleter>;

        using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, CipherContextDeleter>;
    };

    AesEax::AesEax( const std::array<std::byte, 16>& key ): m_Key( key ) {
    }

    AesEaxEncryptedData AesEax::Encrypt( std::span<const std::byte> nonce,
                                         std::span<const std::byte> associatedData,
                                         std::span<const std::byte> data ) const {
        const auto nonceTag = Omac( std::byte { 0x00 }, nonce );

        AesEaxEncryptedData result;

        result.data = CryptCtr( data, nonceTag );

        result.tag = ComputeTag( nonceTag, associatedData, result.data );

        return result;
    }

    std::optional<std::vector<std::byte>> AesEax::Decrypt( std::span<const std::byte> nonce,
                                                           std::span<const std::byte> associatedData,
                                                           std::span<const std::byte> data,
                                                           std::span<const std::byte> tag ) const {
        if ( tag.empty() || tag.size() > 16 ) {
            throw std::runtime_error( "Invalid AES-EAX tag size" );
        }

        const auto nonceTag = Omac( std::byte { 0x00 }, nonce );

        const auto expectedTag = ComputeTag( nonceTag, associatedData, data );

        if ( CRYPTO_memcmp( tag.data(), expectedTag.data(), tag.size() ) != 0 ) {
            return std::nullopt;
        }

        return CryptCtr( data, nonceTag );
    }

    std::array<std::byte, 16> AesEax::Omac( std::byte domain, std::span<const std::byte> data ) const {
        AesEaxOpenSsl::Mac mac( EVP_MAC_fetch( nullptr, "CMAC", nullptr ) );

        if ( !mac ) {
            throw std::runtime_error( "Failed to create CMAC" );
        }

        AesEaxOpenSsl::MacContext context( EVP_MAC_CTX_new( mac.get() ) );

        if ( !context ) {
            throw std::runtime_error( "Failed to create CMAC context" );
        }

        char cipherName[] = "AES-128-CBC";

        OSSL_PARAM parameters[] = { OSSL_PARAM_construct_utf8_string( OSSL_MAC_PARAM_CIPHER, cipherName, 0 ),

                                    OSSL_PARAM_construct_end() };

        if ( EVP_MAC_init( context.get(), reinterpret_cast<const unsigned char*>( m_Key.data() ), m_Key.size(), parameters ) !=
             1 ) {
            throw std::runtime_error( "Failed to initialize CMAC" );
        }

        std::array<std::byte, 16> prefix {};

        prefix.back() = domain;

        if ( EVP_MAC_update( context.get(), reinterpret_cast<const unsigned char*>( prefix.data() ), prefix.size() ) != 1 ) {
            throw std::runtime_error( "Failed to update CMAC" );
        }

        if ( !data.empty() &&
             EVP_MAC_update( context.get(), reinterpret_cast<const unsigned char*>( data.data() ), data.size() ) != 1 ) {
            throw std::runtime_error( "Failed to update CMAC" );
        }

        std::array<std::byte, 16> result {};

        std::size_t resultSize = 0;

        if ( EVP_MAC_final( context.get(), reinterpret_cast<unsigned char*>( result.data() ), &resultSize, result.size() ) !=
             1 ) {
            throw std::runtime_error( "Failed to finalize CMAC" );
        }

        if ( resultSize != result.size() ) {
            throw std::runtime_error( "Unexpected CMAC size" );
        }

        return result;
    }

    std::array<std::byte, 16> AesEax::ComputeTag( const std::array<std::byte, 16>& nonceTag,
                                                  std::span<const std::byte> associatedData,
                                                  std::span<const std::byte> ciphertext ) const {
        const auto headerTag = Omac( std::byte { 0x01 }, associatedData );

        const auto messageTag = Omac( std::byte { 0x02 }, ciphertext );

        std::array<std::byte, 16> result {};

        for ( std::size_t i = 0; i < result.size(); ++i ) {
            result[i] = nonceTag[i] ^ headerTag[i] ^ messageTag[i];
        }

        return result;
    }

    std::vector<std::byte> AesEax::CryptCtr( std::span<const std::byte> data, const std::array<std::byte, 16>& counter ) const {
        if ( data.size() > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) {
            throw std::runtime_error( "AES-EAX input is too large" );
        }

        AesEaxOpenSsl::CipherContext context( EVP_CIPHER_CTX_new() );

        if ( !context ) {
            throw std::runtime_error( "Failed to create AES context" );
        }

        if ( EVP_EncryptInit_ex( context.get(),
                                 EVP_aes_128_ctr(),
                                 nullptr,
                                 reinterpret_cast<const unsigned char*>( m_Key.data() ),
                                 reinterpret_cast<const unsigned char*>( counter.data() ) ) != 1 ) {
            throw std::runtime_error( "Failed to initialize AES-128-CTR" );
        }

        std::vector<std::byte> result( data.size() + 16 );

        int written = 0;

        if ( !data.empty() && EVP_EncryptUpdate( context.get(),
                                                 reinterpret_cast<unsigned char*>( result.data() ),
                                                 &written,
                                                 reinterpret_cast<const unsigned char*>( data.data() ),
                                                 static_cast<int>( data.size() ) ) != 1 ) {
            throw std::runtime_error( "Failed to process AES-EAX data" );
        }

        int finalWritten = 0;

        if ( EVP_EncryptFinal_ex( context.get(), reinterpret_cast<unsigned char*>( result.data() ) + written, &finalWritten ) !=
             1 ) {
            throw std::runtime_error( "Failed to finalize AES-EAX data" );
        }

        result.resize( static_cast<std::size_t>( written + finalWritten ) );

        return result;
    }

} // namespace ts::protocol
