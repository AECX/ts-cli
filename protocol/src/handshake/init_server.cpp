#include <charconv>
#include <cstdint>
#include <protocol/handshake/init_server.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace ts::protocol {

    InitServer InitServer::Parse( const Command& command ) {
        if ( command.Name() != "initserver" ) {
            throw std::runtime_error( "Expected initserver command" );
        }

        if ( command.Rows().size() != 1 ) {
            throw std::runtime_error( "Unexpected initserver row count" );
        }

        const CommandRow& row = command.Rows().front();

        const auto clientIdValue = row.Find( "aclid" );

        if ( !clientIdValue ) {
            throw std::runtime_error( "initserver is missing aclid" );
        }

        std::uint16_t clientId = 0;

        const char* first = clientIdValue->data();

        const char* last = first + clientIdValue->size();

        const auto result = std::from_chars( first, last, clientId );

        if ( result.ec != std::errc {} || result.ptr != last || clientId == 0 ) {
            throw std::runtime_error( "Invalid initserver aclid" );
        }

        InitServer initServer;

        initServer.m_ClientId = clientId;

        if ( const auto serverName = row.Find( "virtualserver_name" ) ) {
            initServer.m_ServerName = *serverName;
        }

        if ( const auto encryptionMode = row.Find( "virtualserver_codec_encryption_mode" ) ) {
            std::uint8_t mode = 0;
            const char* modeFirst = encryptionMode->data();
            const char* modeLast = modeFirst + encryptionMode->size();
            const auto modeResult = std::from_chars( modeFirst, modeLast, mode );

            if ( modeResult.ec != std::errc {} || modeResult.ptr != modeLast || mode > 2 ) {
                throw std::runtime_error( "Invalid virtualserver_codec_encryption_mode" );
            }

            initServer.m_CodecEncryptionMode = mode;
        }

        return initServer;
    }

    std::uint16_t InitServer::ClientId() const {
        return m_ClientId;
    }

    std::string_view InitServer::ServerName() const {
        return m_ServerName;
    }

    std::uint8_t InitServer::CodecEncryptionMode() const {
        return m_CodecEncryptionMode;
    }

} // namespace ts::protocol
