#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/compression/quick_lz.hpp>
#include <span>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    class QuickLzDecoder {
      public:
        [[nodiscard]] static std::vector<std::byte> Decompress( std::span<const std::byte> data,
                                                                std::size_t maximumOutputSize ) {
            if ( data.size() < 3 ) {
                throw std::runtime_error( "QuickLZ data is too small" );
            }

            const std::uint8_t flags = Value( data[0] );

            const std::size_t headerSize = ( flags & 0x02 ) != 0 ? 9 : 3;

            if ( data.size() < headerSize ) {
                throw std::runtime_error( "QuickLZ header is truncated" );
            }

            const std::uint8_t level = static_cast<std::uint8_t>( ( flags >> 2 ) & 0x03 );

            if ( level != 1 ) {
                throw std::runtime_error( "Unsupported QuickLZ compression level" );
            }

            const std::uint8_t streamingMode = static_cast<std::uint8_t>( ( flags >> 4 ) & 0x03 );

            if ( streamingMode != 0 ) {
                throw std::runtime_error( "QuickLZ streaming mode is not supported" );
            }

            const std::size_t compressedSize = headerSize == 9 ? ReadU32Little( data, 1 ) : Value( data[1] );

            const std::size_t decompressedSize = headerSize == 9 ? ReadU32Little( data, 5 ) : Value( data[2] );

            if ( compressedSize != data.size() ) {
                throw std::runtime_error( "QuickLZ compressed size does not match input" );
            }

            if ( decompressedSize > maximumOutputSize ) {
                throw std::runtime_error( "QuickLZ output exceeds maximum command size" );
            }

            const bool compressed = ( flags & 0x01 ) != 0;

            if ( !compressed ) {
                if ( data.size() - headerSize != decompressedSize ) {
                    throw std::runtime_error( "Invalid uncompressed QuickLZ size" );
                }

                return std::vector<std::byte>( data.begin() + static_cast<std::ptrdiff_t>( headerSize ), data.end() );
            }

            if ( decompressedSize == 0 ) {
                throw std::runtime_error( "Invalid empty QuickLZ payload" );
            }

            QuickLzDecoder decoder( data, headerSize, decompressedSize );

            return decoder.Run();
        }

      private:
        QuickLzDecoder( std::span<const std::byte> data, std::size_t sourcePosition, std::size_t outputSize ):
            m_Data( data ), m_SourcePosition( sourcePosition ), m_Output( outputSize ) {
        }

        [[nodiscard]] std::vector<std::byte> Run() {
            std::uint32_t control = 1;

            while ( m_DestinationPosition < m_Output.size() ) {
                if ( control == 1 ) {
                    control = ReadControlWord();
                }

                if ( ( control & 0x01 ) != 0 ) {
                    control >>= 1;

                    DecodeReference();

                    continue;
                }

                if ( IsTail() ) {
                    DecodeTail( control );

                    break;
                }

                DecodeLiteral();

                control >>= 1;
            }

            if ( m_DestinationPosition != m_Output.size() ) {
                throw std::runtime_error( "QuickLZ output size mismatch" );
            }

            return m_Output;
        }

        [[nodiscard]] bool IsTail() const {
            if ( m_Output.size() <= 10 ) {
                return true;
            }

            return m_DestinationPosition >= m_Output.size() - 10;
        }

        void DecodeLiteral() {
            RequireSource( 1 );

            m_Output[m_DestinationPosition] = m_Data[m_SourcePosition];

            ++m_SourcePosition;
            ++m_DestinationPosition;

            if ( m_DestinationPosition >= 3 ) {
                UpdateHashUpto( m_DestinationPosition - 3 );
            }
        }

        void DecodeTail( std::uint32_t& control ) {
            while ( m_DestinationPosition < m_Output.size() ) {
                if ( control == 1 ) {
                    /*
                     * The final QuickLZ literals still have
                     * control words interspersed, but the
                     * contents no longer need to be inspected.
                     */
                    RequireSource( 4 );

                    m_SourcePosition += 4;

                    control = std::uint32_t { 1 } << 31;
                }

                RequireSource( 1 );

                m_Output[m_DestinationPosition] = m_Data[m_SourcePosition];

                ++m_SourcePosition;
                ++m_DestinationPosition;

                control >>= 1;
            }
        }

        void DecodeReference() {
            RequireSource( 2 );

            const std::uint16_t encoded = ReadU16Little( m_Data, m_SourcePosition );

            const std::size_t hash = static_cast<std::size_t>( ( encoded >> 4 ) & 0x0fff );

            std::size_t matchLength = 0;

            if ( ( encoded & 0x0f ) != 0 ) {
                matchLength = static_cast<std::size_t>( encoded & 0x0f ) + 2;

                m_SourcePosition += 2;
            } else {
                RequireSource( 3 );

                matchLength = Value( m_Data[m_SourcePosition + 2] );

                m_SourcePosition += 3;
            }

            if ( matchLength < 3 ) {
                throw std::runtime_error( "Invalid QuickLZ match length" );
            }

            if ( !m_HashValid[hash] ) {
                throw std::runtime_error( "Invalid QuickLZ hash reference" );
            }

            const std::size_t reference = m_HashTable[hash];

            if ( reference >= m_DestinationPosition ) {
                throw std::runtime_error( "Invalid QuickLZ reference offset" );
            }

            if ( matchLength > m_Output.size() - m_DestinationPosition ) {
                throw std::runtime_error( "QuickLZ reference exceeds output" );
            }

            const std::size_t matchStart = m_DestinationPosition;

            /*
             * Copy byte-by-byte intentionally. QuickLZ
             * references may overlap with the destination,
             * similar to LZ backreferences.
             */
            for ( std::size_t i = 0; i < matchLength; ++i ) {
                m_Output[m_DestinationPosition + i] = m_Output[reference + i];
            }

            m_DestinationPosition += matchLength;

            /*
             * Level 1 updates the hash entry for the match
             * start but skips the interior of the match.
             */
            UpdateHashUpto( matchStart );

            m_LastHashed = static_cast<std::ptrdiff_t>( m_DestinationPosition ) - 1;
        }

        void UpdateHashUpto( std::size_t maximumPosition ) {
            if ( m_DestinationPosition < 3 ) {
                return;
            }

            const std::size_t availableMaximum = m_DestinationPosition - 3;

            maximumPosition = std::min( maximumPosition, availableMaximum );

            std::size_t position = 0;

            if ( m_LastHashed >= 0 ) {
                position = static_cast<std::size_t>( m_LastHashed ) + 1;
            }

            if ( position > maximumPosition ) {
                return;
            }

            for ( ; position <= maximumPosition; ++position ) {
                const std::uint32_t value = static_cast<std::uint32_t>( Value( m_Output[position] ) ) |
                                            ( static_cast<std::uint32_t>( Value( m_Output[position + 1] ) ) << 8 ) |
                                            ( static_cast<std::uint32_t>( Value( m_Output[position + 2] ) ) << 16 );

                const std::size_t hash = Hash( value );

                m_HashTable[hash] = position;

                m_HashValid[hash] = true;
            }

            m_LastHashed = static_cast<std::ptrdiff_t>( maximumPosition );
        }

        [[nodiscard]] std::uint32_t ReadControlWord() {
            RequireSource( 4 );

            const std::uint32_t value = ReadU32Little( m_Data, m_SourcePosition );

            m_SourcePosition += 4;

            if ( value == 0 ) {
                throw std::runtime_error( "Invalid QuickLZ control word" );
            }

            return value;
        }

        void RequireSource( std::size_t count ) const {
            if ( m_SourcePosition > m_Data.size() || count > m_Data.size() - m_SourcePosition ) {
                throw std::runtime_error( "Unexpected end of QuickLZ data" );
            }
        }

        [[nodiscard]] static std::size_t Hash( std::uint32_t value ) {
            return static_cast<std::size_t>( ( ( value >> 12 ) ^ value ) & 0x0fff );
        }

        [[nodiscard]] static std::uint8_t Value( std::byte value ) {
            return std::to_integer<std::uint8_t>( value );
        }

        [[nodiscard]] static std::uint16_t ReadU16Little( std::span<const std::byte> data, std::size_t offset ) {
            if ( offset > data.size() || 2 > data.size() - offset ) {
                throw std::runtime_error( "Unexpected end of QuickLZ data" );
            }

            return static_cast<std::uint16_t>( Value( data[offset] ) ) |
                   ( static_cast<std::uint16_t>( Value( data[offset + 1] ) ) << 8 );
        }

        [[nodiscard]] static std::uint32_t ReadU32Little( std::span<const std::byte> data, std::size_t offset ) {
            if ( offset > data.size() || 4 > data.size() - offset ) {
                throw std::runtime_error( "Unexpected end of QuickLZ data" );
            }

            return static_cast<std::uint32_t>( Value( data[offset] ) ) |
                   ( static_cast<std::uint32_t>( Value( data[offset + 1] ) ) << 8 ) |
                   ( static_cast<std::uint32_t>( Value( data[offset + 2] ) ) << 16 ) |
                   ( static_cast<std::uint32_t>( Value( data[offset + 3] ) ) << 24 );
        }

        std::span<const std::byte> m_Data;

        std::size_t m_SourcePosition = 0;
        std::size_t m_DestinationPosition = 0;

        std::vector<std::byte> m_Output;

        std::array<std::size_t, 4096> m_HashTable {};

        std::array<bool, 4096> m_HashValid {};

        std::ptrdiff_t m_LastHashed = -1;
    };

    std::vector<std::byte> QuickLz::Decompress( std::span<const std::byte> data, std::size_t maximumOutputSize ) {
        return QuickLzDecoder::Decompress( data, maximumOutputSize );
    }

} // namespace ts::protocol
