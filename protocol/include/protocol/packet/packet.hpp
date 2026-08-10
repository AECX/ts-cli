#ifndef TS_PROTOCOL_PACKET_HPP
#define TS_PROTOCOL_PACKET_HPP

#include <cstddef>
#include <span>
#include <vector>

namespace ts::protocol {

    class Packet {
      public:
        explicit Packet( std::vector<std::byte> data );

        [[nodiscard]] std::span<const std::byte> Data() const;

        [[nodiscard]] std::size_t Size() const;

      private:
        std::vector<std::byte> m_Data;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_HPP
