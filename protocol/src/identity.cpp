#include <cstddef>
#include <limits>
#include <memory>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <protocol/identity.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::protocol {

    struct Identity::Impl {
        struct KeyDeleter {
            void operator()( EVP_PKEY* key ) const {
                EVP_PKEY_free( key );
            }
        };

        struct KeyContextDeleter {
            void operator()( EVP_PKEY_CTX* context ) const {
                EVP_PKEY_CTX_free( context );
            }
        };

        struct BigNumberDeleter {
            void operator()( BIGNUM* value ) const {
                BN_free( value );
            }
        };

        struct DigestContextDeleter {
            void operator()( EVP_MD_CTX* context ) const {
                EVP_MD_CTX_free( context );
            }
        };

        struct BioDeleter {
            void operator()( BIO* bio ) const {
                BIO_free( bio );
            }
        };

        using Key = std::unique_ptr<EVP_PKEY, KeyDeleter>;

        using KeyContext = std::unique_ptr<EVP_PKEY_CTX, KeyContextDeleter>;

        using BigNumber = std::unique_ptr<BIGNUM, BigNumberDeleter>;

        using DigestContext = std::unique_ptr<EVP_MD_CTX, DigestContextDeleter>;

        using Bio = std::unique_ptr<BIO, BioDeleter>;

        Key key;

        Impl(): key( GenerateKey() ) {
        }

        explicit Impl( Key importedKey ): key( std::move( importedKey ) ) {
            ValidateKey( key.get() );
        }

        [[nodiscard]] static Key GenerateKey() {
            KeyContext context( EVP_PKEY_CTX_new_from_name( nullptr, "EC", nullptr ) );

            if ( !context ) {
                throw std::runtime_error( "Failed to create EC key context" );
            }

            if ( EVP_PKEY_keygen_init( context.get() ) != 1 ) {
                throw std::runtime_error( "Failed to initialize EC key generation" );
            }

            if ( EVP_PKEY_CTX_set_group_name( context.get(), "P-256" ) != 1 ) {
                throw std::runtime_error( "Failed to select P-256 curve" );
            }

            EVP_PKEY* rawKey = nullptr;

            if ( EVP_PKEY_generate( context.get(), &rawKey ) != 1 ) {
                throw std::runtime_error( "Failed to generate identity" );
            }

            return Key( rawKey );
        }

        [[nodiscard]] static Key LoadPrivateKey( std::string_view pem ) {
            if ( pem.empty() ) {
                throw std::runtime_error( "Identity PEM is empty" );
            }

            if ( pem.size() > static_cast<std::size_t>( std::numeric_limits<int>::max() ) ) {
                throw std::runtime_error( "Identity PEM is too large" );
            }

            Bio bio( BIO_new_mem_buf( pem.data(), static_cast<int>( pem.size() ) ) );

            if ( !bio ) {
                throw std::runtime_error( "Failed to create identity PEM reader" );
            }

            EVP_PKEY* rawKey = PEM_read_bio_PrivateKey( bio.get(), nullptr, nullptr, nullptr );

            if ( rawKey == nullptr ) {
                throw std::runtime_error( "Failed to read identity private key" );
            }

            Key key( rawKey );

            ValidateKey( key.get() );

            return key;
        }

        static void ValidateKey( EVP_PKEY* key ) {
            if ( key == nullptr || EVP_PKEY_is_a( key, "EC" ) != 1 ) {
                throw std::runtime_error( "Identity key is not an EC key" );
            }

            char groupName[64] {};

            std::size_t groupNameSize = 0;

            if ( EVP_PKEY_get_utf8_string_param( key,
                                                 OSSL_PKEY_PARAM_GROUP_NAME,
                                                 groupName,
                                                 sizeof( groupName ),
                                                 &groupNameSize ) != 1 ) {
                throw std::runtime_error( "Failed to read identity curve" );
            }

            const std::string_view group( groupName );

            if ( group != "prime256v1" && group != "P-256" && group != "secp256r1" ) {
                throw std::runtime_error( "Identity key is not P-256" );
            }

            BIGNUM* rawPrivateKey = nullptr;

            if ( EVP_PKEY_get_bn_param( key, OSSL_PKEY_PARAM_PRIV_KEY, &rawPrivateKey ) != 1 ) {
                throw std::runtime_error( "Identity does not contain a private key" );
            }

            BN_free( rawPrivateKey );
        }

        [[nodiscard]] std::string PrivateKeyPem() const {
            Bio bio( BIO_new( BIO_s_mem() ) );

            if ( !bio ) {
                throw std::runtime_error( "Failed to create identity PEM writer" );
            }

            if ( PEM_write_bio_PrivateKey( bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr ) != 1 ) {
                throw std::runtime_error( "Failed to serialize identity private key" );
            }

            BUF_MEM* buffer = nullptr;

            BIO_get_mem_ptr( bio.get(), &buffer );

            if ( buffer == nullptr || buffer->data == nullptr ) {
                throw std::runtime_error( "Failed to read serialized identity" );
            }

            return std::string( buffer->data, buffer->length );
        }

        static void WriteDerLength( std::vector<std::byte>& output, std::size_t length ) {
            if ( length >= 128 ) {
                throw std::runtime_error( "Unsupported DER length" );
            }

            output.push_back( static_cast<std::byte>( length ) );
        }

        static void WriteDerInteger( std::vector<std::byte>& output, const BIGNUM& value ) {
            const int size = BN_num_bytes( &value );

            if ( size <= 0 ) {
                output.push_back( std::byte { 0x02 } );

                output.push_back( std::byte { 0x01 } );

                output.push_back( std::byte { 0x00 } );

                return;
            }

            std::vector<unsigned char> buffer( static_cast<std::size_t>( size ) );

            BN_bn2bin( &value, buffer.data() );

            const bool needsPadding = ( buffer.front() & 0x80 ) != 0;

            output.push_back( std::byte { 0x02 } );

            WriteDerLength( output, buffer.size() + ( needsPadding ? 1 : 0 ) );

            if ( needsPadding ) {
                output.push_back( std::byte { 0x00 } );
            }

            for ( const unsigned char byte : buffer ) {
                output.push_back( static_cast<std::byte>( byte ) );
            }
        }

        [[nodiscard]] std::vector<std::byte> PublicKey() const {
            BIGNUM* rawX = nullptr;
            BIGNUM* rawY = nullptr;

            if ( EVP_PKEY_get_bn_param( key.get(), OSSL_PKEY_PARAM_EC_PUB_X, &rawX ) != 1 ) {
                throw std::runtime_error( "Failed to read EC X coordinate" );
            }

            if ( EVP_PKEY_get_bn_param( key.get(), OSSL_PKEY_PARAM_EC_PUB_Y, &rawY ) != 1 ) {
                BN_free( rawX );

                throw std::runtime_error( "Failed to read EC Y coordinate" );
            }

            BigNumber x( rawX );

            BigNumber y( rawY );

            std::vector<std::byte> body;

            body.push_back( std::byte { 0x03 } );

            body.push_back( std::byte { 0x02 } );

            body.push_back( std::byte { 0x07 } );

            body.push_back( std::byte { 0x00 } );

            body.push_back( std::byte { 0x02 } );

            body.push_back( std::byte { 0x01 } );

            body.push_back( std::byte { 0x20 } );

            WriteDerInteger( body, *x );

            WriteDerInteger( body, *y );

            std::vector<std::byte> result;

            result.push_back( std::byte { 0x30 } );

            WriteDerLength( result, body.size() );

            result.insert( result.end(), body.begin(), body.end() );

            return result;
        }

        [[nodiscard]] std::vector<std::byte> Sign( std::span<const std::byte> data ) const {
            DigestContext context( EVP_MD_CTX_new() );

            if ( !context ) {
                throw std::runtime_error( "Failed to create identity signature context" );
            }

            if ( EVP_DigestSignInit( context.get(), nullptr, EVP_sha256(), nullptr, key.get() ) != 1 ) {
                throw std::runtime_error( "Failed to initialize identity signature" );
            }

            if ( !data.empty() && EVP_DigestSignUpdate( context.get(), data.data(), data.size() ) != 1 ) {
                throw std::runtime_error( "Failed to update identity signature" );
            }

            std::size_t signatureSize = 0;

            if ( EVP_DigestSignFinal( context.get(), nullptr, &signatureSize ) != 1 ) {
                throw std::runtime_error( "Failed to determine identity signature size" );
            }

            std::vector<std::byte> signature( signatureSize );

            if ( EVP_DigestSignFinal( context.get(), reinterpret_cast<unsigned char*>( signature.data() ), &signatureSize ) !=
                 1 ) {
                throw std::runtime_error( "Failed to sign identity data" );
            }

            signature.resize( signatureSize );

            return signature;
        }
    };

    Identity::Identity(): m_Impl( std::make_unique<Impl>() ) {
    }

    Identity::Identity( std::unique_ptr<Impl> impl ): m_Impl( std::move( impl ) ) {
    }

    Identity::~Identity() = default;

    Identity::Identity( Identity&& other ) noexcept = default;

    Identity& Identity::operator=( Identity&& other ) noexcept = default;

    Identity Identity::FromPrivateKeyPem( std::string_view pem ) {
        Impl::Key key = Impl::LoadPrivateKey( pem );

        return Identity( std::make_unique<Impl>( std::move( key ) ) );
    }

    std::string Identity::PrivateKeyPem() const {
        return m_Impl->PrivateKeyPem();
    }

    std::vector<std::byte> Identity::PublicKey() const {
        return m_Impl->PublicKey();
    }

    std::vector<std::byte> Identity::Sign( std::span<const std::byte> data ) const {
        return m_Impl->Sign( data );
    }

} // namespace ts::protocol
