#ifndef TS_PROTOCOL_BINARY_WRITER_HPP
#define TS_PROTOCOL_BINARY_WRITER_HPP
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ts::protocol {

    class BinaryWriter {
      public:
        void WriteU8( std::uint8_t value );
        void WriteU16( std::uint16_t value );
        void WriteU32( std::uint32_t value );
        void WriteU64( std::uint64_t value );

        void WriteBytes( std::span<const std::byte> data );
        void WriteZeros( std::size_t count );

        [[nodiscard]] std::vector<std::byte> Take();

      private:
        std::vector<std::byte> m_Data;
    };
} // namespace ts::protocol
#endif // TS_PROTOCOL_BINARY_WRITER_HPP
