#include "test_support.hpp"

#include <protocol/compression/quick_lz.hpp>
#include <stdexcept>

namespace ts::test {

    void RunQuickLzTests() {
        /*
         * QuickLZ short header:
         *
         *   0x04 = level 1, not compressed
         *   0x08 = total encoded size
         *   0x05 = decoded size
         *
         * followed by "hello".
         */
        {
            const auto encoded = Bytes( { 0x04, 0x08, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f } );

            const auto decoded = protocol::QuickLz::Decompress( encoded, 64 );

            const auto expected = Bytes( "hello" );

            ExpectEqual( decoded, expected, "Uncompressed QuickLZ block decoded incorrectly" );
        }

        /*
         * Level-1 compressed fixture.
         *
         * Decodes to:
         *
         *   abcabcabcX0123456789
         *
         * The first "abc" is literal.
         * The following six bytes are produced through a
         * level-1 hash-table backreference to "abc".
         *
         * This makes sure we're testing actual reference
         * decoding, not only the QuickLZ wrapper/header.
         */
        {
            const auto encoded = Bytes( { 0x05, 0x17, 0x14,

                                          0x08, 0x00, 0x00, 0x80,

                                          0x61, 0x62, 0x63,

                                          0x74, 0x45,

                                          0x58,

                                          0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 } );

            const auto decoded = protocol::QuickLz::Decompress( encoded, 64 );

            const auto expected = Bytes( "abcabcabcX0123456789" );

            ExpectEqual( decoded, expected, "Compressed QuickLZ block decoded incorrectly" );
        }

        {
            const auto encoded = Bytes( { 0x04, 0x08, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f } );

            ExpectThrows<std::runtime_error>(
                [&encoded]() {
                    (void)protocol::QuickLz::Decompress( encoded, 4 );
                },
                "QuickLZ maximum output size was not enforced" );
        }

        {
            /*
             * Level bits = 2 instead of supported level 1.
             */
            const auto encoded = Bytes( { 0x08, 0x03, 0x00 } );

            ExpectThrows<std::runtime_error>(
                [&encoded]() {
                    (void)protocol::QuickLz::Decompress( encoded, 64 );
                },
                "Unsupported QuickLZ level did not fail" );
        }

        {
            const auto encoded = Bytes( { 0x04, 0x08 } );

            ExpectThrows<std::runtime_error>(
                [&encoded]() {
                    (void)protocol::QuickLz::Decompress( encoded, 64 );
                },
                "Truncated QuickLZ header did not fail" );
        }
    }

} // namespace ts::test
