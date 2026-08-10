#ifndef TS_PROTOCOL_TRANSPORT_HPP
#define TS_PROTOCOL_TRANSPORT_HPP

#include <chrono>
#include <concepts>
#include <net/udp_socket.hpp>
#include <protocol/packet/packet.hpp>

namespace ts::protocol {

    template<typename T>
    concept SerializableMessage = requires( const T& message ) {
        { message.Serialize() } -> std::same_as<Packet>;
    };

    template<typename T>
    concept ParsableMessage = requires( const Packet& packet ) {
        { T::Parse( packet ) } -> std::same_as<T>;
    };

    class Transport {
      public:
        explicit Transport( net::UdpSocket& socket );

        void Send( const Packet& packet );

        [[nodiscard]] Packet Receive();
        [[nodiscard]] bool WaitReadable( std::chrono::milliseconds timeout ) const;

        template<SerializableMessage Message>
        void Send( const Message& message ) {
            Send( message.Serialize() );
        }

        template<ParsableMessage Message>
        [[nodiscard]] Message Receive() {
            return Message::Parse( Receive() );
        }

        template<ParsableMessage Response, SerializableMessage Request>
        [[nodiscard]] Response Exchange( const Request& request ) {
            Send( request );
            return Receive<Response>();
        }

      private:
        net::UdpSocket& m_Socket;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_TRANSPORT_HPP
