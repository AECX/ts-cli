#include <algorithm>
#include <array>
#include <cstddef>
#include <protocol/command/writer.hpp>
#include <protocol/encoding/base64.hpp>
#include <protocol/handshake/client_ek.hpp>
#include <string>
#include <vector>

namespace ts::protocol {

    ClientEk::ClientEk( const Identity& identity,
                        const std::array<std::byte, 32>& clientPublicKey,
                        const std::array<std::byte, 54>& beta ) {
        std::array<std::byte, 86> proofData {};

        std::copy( clientPublicKey.begin(), clientPublicKey.end(), proofData.begin() );

        std::copy( beta.begin(), beta.end(), proofData.begin() + clientPublicKey.size() );

        const std::vector<std::byte> proof = identity.Sign( proofData );

        CommandWriter writer( "clientek" );

        writer.Write( "ek", Base64Encode( clientPublicKey ) );

        writer.Write( "proof", Base64Encode( proof ) );

        m_Data = writer.Take();
    }

    std::vector<std::byte> ClientEk::Serialize() const {
        return m_Data;
    }

} // namespace ts::protocol
