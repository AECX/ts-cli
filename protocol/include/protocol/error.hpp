#ifndef TS_PROTOCOL_ERROR_HPP
#define TS_PROTOCOL_ERROR_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

namespace ts::protocol {

    /*
     * Base class for errors raised by the protocol layer's public API.
     *
     * It derives from std::runtime_error so callers that only catch
     * std::exception keep working unchanged; catching ProtocolError is what
     * lets a caller separate protocol failures from unrelated runtime errors.
     */
    class ProtocolError: public std::runtime_error {
      public:
        explicit ProtocolError( const std::string& message ): std::runtime_error( message ) {
        }
    };

    /*
     * A Connection or Session operation requiring an established session was
     * called before Connect() succeeded, or after the session closed.
     */
    class NotConnectedError: public ProtocolError {
      public:
        explicit NotConnectedError( const std::string& message ): ProtocolError( message ) {
        }
    };

    /*
     * The server rejected the login. Id is the TeamSpeak error id from the
     * command result, retained so callers can distinguish e.g. a wrong server
     * password from a nickname that is already in use.
     */
    class LoginRejectedError: public ProtocolError {
      public:
        LoginRejectedError( std::uint32_t id, const std::string& message ):
            ProtocolError( "Login rejected (" + std::to_string( id ) + "): " + message ), m_Id( id ), m_Message( message ) {
        }

        [[nodiscard]] std::uint32_t Id() const {
            return m_Id;
        }

        [[nodiscard]] const std::string& Message() const {
            return m_Message;
        }

      private:
        std::uint32_t m_Id = 0;

        std::string m_Message;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_ERROR_HPP
