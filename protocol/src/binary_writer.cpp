#include <protocol/binary_writer.hpp>
#include <utility>

namespace ts::protocol {

    void BinaryWriter::WriteU8( std::uint8_t value ) {
        m_Data.push_back( static_cast<std::byte>( value ) );
    }

    void BinaryWriter::WriteU16( std::uint16_t value ) {
        m_Data.push_back( static_cast<std::byte>( ( value >> 8 ) & 0xff ) );

        m_Data.push_back( static_cast<std::byte>( value & 0xff ) );
    }
    void BinaryWriter::WriteU32( std::uint32_t value ) {
        m_Data.push_back( static_cast<std::byte>( ( value >> 24 ) & 0xff ) );

        m_Data.push_back( static_cast<std::byte>( ( value >> 16 ) & 0xff ) );

        m_Data.push_back( static_cast<std::byte>( ( value >> 8 ) & 0xff ) );

        m_Data.push_back( static_cast<std::byte>( value & 0xff ) );
    }
    void BinaryWriter::WriteU64( std::uint64_t value ) {
        m_Data.push_back( static_cast<std::byte>( ( value >> 56 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 48 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 40 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 32 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 24 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 16 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( ( value >> 8 ) & 0xff ) );
        m_Data.push_back( static_cast<std::byte>( value & 0xff ) );
    }

    void BinaryWriter::WriteBytes( std::span<const std::byte> data ) {
        m_Data.insert( m_Data.end(), data.begin(), data.end() );
    }
    void BinaryWriter::WriteZeros( std::size_t count ) {
        m_Data.insert( m_Data.end(), count, std::byte { 0 } );
    }

    std::vector<std::byte> BinaryWriter::Take() {
        return std::move( m_Data );
    }

} // namespace ts::protocol
