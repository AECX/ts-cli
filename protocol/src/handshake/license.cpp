#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <protocol/handshake/license.hpp>
#include <stdexcept>

namespace ts::protocol {

    class LicenseCursor {
      public:
        explicit LicenseCursor( std::span<const std::byte> data ): m_Data( data ) {
        }

        [[nodiscard]] std::uint8_t ReadU8() {
            Require( 1 );

            return std::to_integer<std::uint8_t>( m_Data[m_Offset++] );
        }

        [[nodiscard]] std::uint32_t ReadU32() {
            const std::uint32_t a = ReadU8();

            const std::uint32_t b = ReadU8();

            const std::uint32_t c = ReadU8();

            const std::uint32_t d = ReadU8();

            return ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | d;
        }

        [[nodiscard]] std::span<const std::byte> ReadBytes( std::size_t count ) {
            Require( count );

            const auto result = m_Data.subspan( m_Offset, count );

            m_Offset += count;

            return result;
        }

        void SkipNullTerminated() {
            while ( true ) {
                if ( ReadU8() == 0 ) {
                    return;
                }
            }
        }

        [[nodiscard]] std::size_t Position() const {
            return m_Offset;
        }

        [[nodiscard]] std::size_t Remaining() const {
            return m_Data.size() - m_Offset;
        }

      private:
        void Require( std::size_t count ) const {
            if ( count > Remaining() ) {
                throw std::runtime_error( "Unexpected end of license data" );
            }
        }

        std::span<const std::byte> m_Data;
        std::size_t m_Offset = 0;
    };

    LicenseBlock LicenseBlock::Parse( std::span<const std::byte> data, std::size_t& offset ) {
        constexpr std::size_t MinimumBlockSize = 42;
        constexpr std::uint64_t TimestampOffset = 0x50e22700ULL;

        if ( offset > data.size() || data.size() - offset < MinimumBlockSize ) {
            throw std::runtime_error( "License block is too short" );
        }

        const auto blockData = data.subspan( offset );

        LicenseCursor reader( blockData );

        const std::uint8_t keyType = reader.ReadU8();

        if ( keyType != 0x00 ) {
            throw std::runtime_error( "Unsupported license key type" );
        }

        LicenseBlock result;

        const auto publicKey = reader.ReadBytes( result.m_PublicKey.size() );

        std::copy( publicKey.begin(), publicKey.end(), result.m_PublicKey.begin() );

        const std::uint8_t blockType = reader.ReadU8();

        switch ( blockType ) {
            case 0x00:
                result.m_Type = LicenseBlockType::Intermediate;
                break;

            case 0x01:
                result.m_Type = LicenseBlockType::Website;
                break;

            case 0x02:
                result.m_Type = LicenseBlockType::Ts3Server;
                break;

            case 0x03:
                result.m_Type = LicenseBlockType::Code;
                break;

            case 0x08:
                result.m_Type = LicenseBlockType::Ts5Server;
                break;

            case 0x20:
                result.m_Type = LicenseBlockType::Ephemeral;
                break;

            default:
                throw std::runtime_error( "Unsupported license block type" );
        }

        result.m_NotValidBefore = static_cast<std::uint64_t>( reader.ReadU32() ) + TimestampOffset;

        result.m_NotValidAfter = static_cast<std::uint64_t>( reader.ReadU32() ) + TimestampOffset;

        if ( result.m_NotValidBefore > result.m_NotValidAfter ) {
            throw std::runtime_error( "Invalid license validity range" );
        }

        switch ( result.m_Type ) {
            case LicenseBlockType::Intermediate:
                // Unknown 32-bit value.
                (void)reader.ReadU32();

                // Issuer.
                reader.SkipNullTerminated();
                break;

            case LicenseBlockType::Website:
            case LicenseBlockType::Code:
                // Issuer.
                reader.SkipNullTerminated();
                break;

            case LicenseBlockType::Ts3Server:
                // Server license type.
                (void)reader.ReadU8();

                // Maximum clients.
                (void)reader.ReadU32();

                // Issuer.
                reader.SkipNullTerminated();
                break;

            case LicenseBlockType::Ts5Server: {
                // Server license type.
                (void)reader.ReadU8();

                const std::uint8_t propertyCount = reader.ReadU8();

                for ( std::uint16_t i = 0; i < propertyCount; ++i ) {
                    const std::uint8_t propertyLength = reader.ReadU8();

                    // The length covers:
                    //
                    //   property id
                    //   data type
                    //   property contents
                    //
                    // so it must contain at least id + type.
                    if ( propertyLength < 2 ) {
                        throw std::runtime_error( "Invalid license property length" );
                    }

                    const auto property = reader.ReadBytes( propertyLength );

                    const std::uint8_t dataType = std::to_integer<std::uint8_t>( property[1] );

                    const auto content = property.subspan( 2 );

                    switch ( dataType ) {
                        case 0x00:
                            // String properties are null terminated.
                            if ( content.empty() || content.back() != std::byte { 0x00 } ) {
                                throw std::runtime_error( "Invalid license string property" );
                            }

                            break;

                        case 0x01:
                        case 0x03:
                            if ( content.size() != 4 ) {
                                throw std::runtime_error( "Invalid 32-bit license property" );
                            }

                            break;

                        case 0x02:
                        case 0x04:
                            if ( content.size() != 8 ) {
                                throw std::runtime_error( "Invalid 64-bit license property" );
                            }

                            break;

                        default:
                            // Unknown property types are still length
                            // delimited, so they can safely be skipped.
                            break;
                    }
                }

                break;
            }

            case LicenseBlockType::Ephemeral:
                break;
        }

        const std::size_t blockSize = reader.Position();

        const auto raw = blockData.first( blockSize );

        result.m_Raw.assign( raw.begin(), raw.end() );

        offset += blockSize;

        return result;
    }

    LicenseBlockType LicenseBlock::Type() const {
        return m_Type;
    }

    const std::array<std::byte, 32>& LicenseBlock::PublicKey() const {
        return m_PublicKey;
    }

    std::uint64_t LicenseBlock::NotValidBefore() const {
        return m_NotValidBefore;
    }

    std::uint64_t LicenseBlock::NotValidAfter() const {
        return m_NotValidAfter;
    }

    std::span<const std::byte> LicenseBlock::Raw() const {
        return m_Raw;
    }

    std::span<const std::byte> LicenseBlock::HashData() const {
        if ( m_Raw.size() < 2 ) {
            throw std::runtime_error( "Invalid raw license block" );
        }

        return std::span<const std::byte>( m_Raw ).subspan( 1 );
    }

    bool LicenseBlock::IsServer() const {
        return m_Type == LicenseBlockType::Ts3Server || m_Type == LicenseBlockType::Ts5Server;
    }

    License License::Parse( std::span<const std::byte> data ) {
        if ( data.empty() ) {
            throw std::runtime_error( "License is empty" );
        }

        License result;

        result.m_Version = std::to_integer<std::uint8_t>( data.front() );

        // The declarations specify version 1. ReSpeak's current
        // parser intentionally accepts both 0 and 1.
        if ( result.m_Version != 0 && result.m_Version != 1 ) {
            throw std::runtime_error( "Unsupported license version" );
        }

        result.m_Raw.assign( data.begin(), data.end() );

        std::size_t offset = 1;

        bool hasParentBounds = false;
        std::uint64_t parentBefore = 0;
        std::uint64_t parentAfter = 0;

        while ( offset < data.size() ) {
            if ( result.m_Blocks.size() >= 8 ) {
                throw std::runtime_error( "License contains too many blocks" );
            }

            LicenseBlock block = LicenseBlock::Parse( data, offset );

            if ( hasParentBounds ) {
                if ( block.NotValidBefore() < parentBefore || block.NotValidAfter() > parentAfter ) {
                    throw std::runtime_error( "License block exceeds parent validity range" );
                }
            }

            parentBefore = block.NotValidBefore();

            parentAfter = block.NotValidAfter();

            hasParentBounds = true;

            result.m_Blocks.push_back( std::move( block ) );
        }

        if ( result.m_Blocks.size() < 2 ) {
            throw std::runtime_error( "License contains too few blocks" );
        }

        const LicenseBlock& serverBlock = result.m_Blocks[result.m_Blocks.size() - 2];

        if ( !serverBlock.IsServer() ) {
            throw std::runtime_error( "Penultimate license block is not a server block" );
        }

        if ( result.m_Blocks.back().Type() != LicenseBlockType::Ephemeral ) {
            throw std::runtime_error( "Final license block is not ephemeral" );
        }

        return result;
    }

    std::uint8_t License::Version() const {
        return m_Version;
    }

    std::span<const std::byte> License::Raw() const {
        return m_Raw;
    }

    const std::vector<LicenseBlock>& License::Blocks() const {
        return m_Blocks;
    }

} // namespace ts::protocol
