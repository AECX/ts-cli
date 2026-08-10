#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <openssl/bn.h>
#include <protocol/crypto/puzzle_solver.hpp>
#include <stdexcept>

namespace ts::protocol {

    class PuzzleSolver {
      public:
        [[nodiscard]] static std::array<std::byte, 64>
            Solve( const std::array<std::byte, 64>& x, const std::array<std::byte, 64>& n, std::uint32_t level ) {
            BigNumber value = FromBytes( x );
            const BigNumber modulus = FromBytes( n );

            Context context( BN_CTX_new() );

            if ( !context ) {
                throw std::runtime_error( "Failed to create BIGNUM context" );
            }

            for ( std::uint32_t i = 0; i < level; ++i ) {
                if ( BN_mod_sqr( value.get(), value.get(), modulus.get(), context.get() ) != 1 ) {
                    throw std::runtime_error( "Failed to solve TeamSpeak puzzle" );
                }
            }

            std::array<std::byte, 64> result {};

            const int written = BN_bn2binpad( value.get(),
                                              reinterpret_cast<unsigned char*>( result.data() ),
                                              static_cast<int>( result.size() ) );

            if ( written != static_cast<int>( result.size() ) ) {
                throw std::runtime_error( "Failed to serialize puzzle result" );
            }

            return result;
        }

      private:
        struct BigNumberDeleter {
            void operator()( BIGNUM* value ) const {
                BN_free( value );
            }
        };

        struct ContextDeleter {
            void operator()( BN_CTX* context ) const {
                BN_CTX_free( context );
            }
        };

        using BigNumber = std::unique_ptr<BIGNUM, BigNumberDeleter>;

        using Context = std::unique_ptr<BN_CTX, ContextDeleter>;

        [[nodiscard]] static BigNumber FromBytes( const std::array<std::byte, 64>& data ) {
            BIGNUM* value =
                BN_bin2bn( reinterpret_cast<const unsigned char*>( data.data() ), static_cast<int>( data.size() ), nullptr );

            if ( value == nullptr ) {
                throw std::runtime_error( "Failed to create BIGNUM" );
            }

            return BigNumber( value );
        }
    };

    std::array<std::byte, 64>
        SolvePuzzle( const std::array<std::byte, 64>& x, const std::array<std::byte, 64>& n, std::uint32_t level ) {
        return PuzzleSolver::Solve( x, n, level );
    }

} // namespace ts::protocol
