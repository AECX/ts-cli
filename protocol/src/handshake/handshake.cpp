#include <cstddef>
#include <cstdint>
#include <protocol/binary_reader.hpp>
#include <protocol/command/parser.hpp>
#include <protocol/crypto/p256.hpp>
#include <protocol/crypto/puzzle_solver.hpp>
#include <protocol/crypto/session_material.hpp>
#include <protocol/handshake/bootstrap_transport.hpp>
#include <protocol/handshake/client_ek.hpp>
#include <protocol/handshake/client_init_iv.hpp>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/get_cookie.hpp>
#include <protocol/handshake/get_puzzle.hpp>
#include <protocol/handshake/handshake.hpp>
#include <protocol/handshake/init_iv_expand_2.hpp>
#include <protocol/handshake/license.hpp>
#include <protocol/handshake/puzzle.hpp>
#include <protocol/handshake/puzzle_answer.hpp>
#include <protocol/handshake/set_cookie.hpp>
#include <protocol/packet/init_codec.hpp>
#include <protocol/packet/packet.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace ts::protocol {

    Handshake::Handshake( Transport& transport, Identity& identity, std::uint32_t clientVersion, std::string serverIp ):
        m_Transport( transport ), m_Identity( identity ), m_ClientVersion( clientVersion ),
        m_ServerIp( std::move( serverIp ) ) {
    }

    SessionBootstrap Handshake::Run() {
        constexpr std::size_t MaxAttempts = 5;

        for ( std::size_t attempt = 0; attempt < MaxAttempts; ++attempt ) {
            const SetCookie cookie = m_Transport.Exchange<SetCookie>( GetCookie( m_ClientVersion ) );

            m_Transport.Send( GetPuzzle( cookie, m_ClientVersion ) );

            const Packet rawPuzzle = m_Transport.Receive();

            BinaryReader headerReader( rawPuzzle.Data() );

            const ServerInitHeader header = InitCodec::ReadServerHeader( headerReader );

            if ( header.command == handshake::ResetCommand ) {
                continue;
            }

            if ( header.command != handshake::PuzzleCommand ) {
                throw std::runtime_error( "Unexpected Init1 response command: " + std::to_string( header.command ) );
            }

            const Puzzle puzzle = Puzzle::Parse( rawPuzzle );

            const auto solution = SolvePuzzle( puzzle.X(), puzzle.N(), puzzle.Level() );

            const ClientInitIv clientInitIv( m_Identity, m_ServerIp );

            const PuzzleAnswer answer( puzzle, solution, clientInitIv.Serialize(), m_ClientVersion );

            m_Transport.Send( answer );

            BootstrapTransport bootstrap( m_Transport );

            const auto plaintext = bootstrap.ReceiveCommand();

            const Command command = CommandParser::Parse( plaintext );

            if ( command.Name() == "initivexpand" ) {
                throw std::runtime_error( "Legacy initivexpand is not supported" );
            }

            const InitIvExpand2 expand = InitIvExpand2::Parse( command );

            if ( !P256::Verify( expand.Omega(), expand.License(), expand.Proof() ) ) {
                throw std::runtime_error( "Invalid initivexpand2 server proof" );
            }

            const License license = License::Parse( expand.License() );

            const SessionMaterial material = SessionMaterialDeriver::Derive( license, clientInitIv.Alpha(), expand.Beta() );

            const ClientEk clientEk( m_Identity, material.clientPublicKey, expand.Beta() );

            const std::uint16_t clientEkPacketId = bootstrap.SendCommand( clientEk.Serialize() );

            bootstrap.ReceiveAck( clientEkPacketId );

            return SessionBootstrap { .material = material,

                                      .sequences = bootstrap.SequenceState() };
        }

        throw std::runtime_error( "Init1 handshake reset too many times" );
    }

} // namespace ts::protocol
