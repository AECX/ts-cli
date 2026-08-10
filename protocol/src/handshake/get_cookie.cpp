#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <protocol/handshake/get_cookie.hpp>
#include <random>
#include <utility>

namespace ts::protocol {

    class GetCookieData {
      public:
        [[nodiscard]] static std::uint32_t CurrentUnixTime() {
            const auto now = std::chrono::system_clock::now();

            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>( now.time_since_epoch() );

            return static_cast<std::uint32_t>( seconds.count() );
        }

        [[nodiscard]] static std::array<std::byte, 4> RandomBytes() {
            std::random_device randomDevice;

            std::array<std::byte, 4> result {};

            for ( std::byte& value : result ) {
                value = static_cast<std::byte>( randomDevice() & 0xff );
            }

            return result;
        }
    };

    GetCookie::GetCookie( std::uint32_t clientVersion ): ClientHandshakeMessage<handshake::GetCookieCommand>( clientVersion ) {
    }

    Packet GetCookie::Serialize() const {
        BinaryWriter writer = CreateWriter();

        writer.WriteU32( GetCookieData::CurrentUnixTime() );

        const auto random = GetCookieData::RandomBytes();

        writer.WriteBytes( random );

        writer.WriteU16( 1 );

        writer.WriteZeros( 6 );

        return CreatePacket( std::move( writer ) );
    }

} // namespace ts::protocol
