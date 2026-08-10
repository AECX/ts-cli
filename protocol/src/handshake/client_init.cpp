#include <cstdint>
#include <protocol/command/writer.hpp>
#include <protocol/handshake/client_init.hpp>
#include <utility>
#include <vector>

namespace ts::protocol {

    ClientInit::ClientInit( ClientProfile profile, std::uint64_t keyOffset ):
        m_Profile( std::move( profile ) ), m_KeyOffset( keyOffset ) {
    }

    std::vector<std::byte> ClientInit::Serialize() const {
        CommandWriter writer( "clientinit" );

        writer.Write( "client_nickname", m_Profile.nickname );

        writer.Write( "client_version", m_Profile.version.version );

        writer.Write( "client_platform", m_Profile.version.platform );

        writer.Write( "client_input_hardware", false );

        writer.Write( "client_output_hardware", false );

        writer.Write( "client_default_channel" );

        writer.Write( "client_default_channel_password" );

        writer.Write( "client_server_password" );

        writer.Write( "client_meta_data" );

        writer.Write( "client_version_sign", m_Profile.version.signature );

        writer.Write( "client_key_offset", m_KeyOffset );

        writer.Write( "client_nickname_phonetic" );

        writer.Write( "client_default_token" );

        writer.Write( "hwid" );

        return writer.Take();
    }

} // namespace ts::protocol
