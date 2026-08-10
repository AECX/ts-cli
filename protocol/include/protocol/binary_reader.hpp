#ifndef TS_PROTOCOL_BINARY_READER_HPP
#define TS_PROTOCOL_BINARY_READER_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace ts::protocol {

    class BinaryReader {
      public:
        explicit BinaryReader( std::span<const std::byte> data );

        [[nodiscard]] std::uint8_t ReadU8();
        [[nodiscard]] std::uint16_t ReadU16();
        [[nodiscard]] std::uint32_t ReadU32();

        [[nodiscard]] std::span<const std::byte> ReadBytes( std::size_t count );

        void Skip( std::size_t count );

        [[nodiscard]] std::size_t Remaining() const;

      private:
        void Require( std::size_t count ) const;

        std::span<const std::byte> m_Data;
        std::size_t m_Offset = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_BINARY_READER_HPP
