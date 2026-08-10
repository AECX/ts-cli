#include <array>
#include <cstddef>
#include <openssl/rand.h>
#include <protocol/command/writer.hpp>
#include <protocol/encoding/base64.hpp>
#include <protocol/handshake/client_init_iv.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ts::protocol {

    ClientInitIv::ClientInitIv( const Identity& identity, std::string serverIp ):
        m_Identity( identity ), m_ServerIp( std::move( serverIp ) ), m_Alpha( GenerateAlpha() ) {
    }

    std::vector<std::byte> ClientInitIv::Serialize() const {
        const std::string alpha = Base64Encode( std::span<const std::byte>( m_Alpha ) );

        const auto publicKey = m_Identity.PublicKey();

        const std::string omega = Base64Encode( std::span<const std::byte>( publicKey ) );

        CommandWriter writer( "clientinitiv" );

        writer.Write( "alpha", alpha );

        writer.Write( "omega", omega );

        writer.Write( "ot", 1 );

        writer.Write( "ip", m_ServerIp );

        return writer.Take();
    }

    const std::array<std::byte, 10>& ClientInitIv::Alpha() const {
        return m_Alpha;
    }

    std::array<std::byte, 10> ClientInitIv::GenerateAlpha() {
        std::array<std::byte, 10> alpha {};

        if ( RAND_bytes( reinterpret_cast<unsigned char*>( alpha.data() ), static_cast<int>( alpha.size() ) ) != 1 ) {
            throw std::runtime_error( "Failed to generate client alpha" );
        }

        return alpha;
    }

} // namespace ts::protocol
