#include <protocol/binary_reader.hpp>
#include <stdexcept>

namespace ts::protocol {

    BinaryReader::BinaryReader( std::span<const std::byte> data ): m_Data( data ) {
    }

    std::uint8_t BinaryReader::ReadU8() {
        Require( 1 );

        return std::to_integer<std::uint8_t>( m_Data[m_Offset++] );
    }

    std::uint16_t BinaryReader::ReadU16() {
        Require( 2 );

        const std::uint16_t value = static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset] ) ) << 8 |
                                    static_cast<std::uint16_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset + 1] ) );

        m_Offset += 2;

        return value;
    }

    std::uint32_t BinaryReader::ReadU32() {
        Require( 4 );

        const std::uint32_t value = static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset] ) ) << 24 |
                                    static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset + 1] ) ) << 16 |
                                    static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset + 2] ) ) << 8 |
                                    static_cast<std::uint32_t>( std::to_integer<std::uint8_t>( m_Data[m_Offset + 3] ) );

        m_Offset += 4;

        return value;
    }

    std::span<const std::byte> BinaryReader::ReadBytes( std::size_t count ) {
        Require( count );

        const auto result = m_Data.subspan( m_Offset, count );

        m_Offset += count;

        return result;
    }

    void BinaryReader::Skip( std::size_t count ) {
        Require( count );

        m_Offset += count;
    }

    std::size_t BinaryReader::Remaining() const {
        return m_Data.size() - m_Offset;
    }

    void BinaryReader::Require( std::size_t count ) const {
        if ( m_Offset > m_Data.size() || count > m_Data.size() - m_Offset ) {
            throw std::runtime_error( "BinaryReader exceeds buffer" );
        }
    }

} // namespace ts::protocol
