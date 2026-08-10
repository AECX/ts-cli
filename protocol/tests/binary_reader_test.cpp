#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/binary_reader.hpp>
#include <stdexcept>

namespace ts::test {

    void RunBinaryReaderTests() {
        {
            const std::array<std::byte, 7> data = { std::byte { 0x12 },
                                                    std::byte { 0x34 },
                                                    std::byte { 0x56 },
                                                    std::byte { 0x78 },
                                                    std::byte { 0x9a },
                                                    std::byte { 0xbc },
                                                    std::byte { 0xde } };

            protocol::BinaryReader reader( data );

            ExpectEqual( reader.ReadU8(), std::uint8_t { 0x12 }, "ReadU8 returned the wrong value" );

            ExpectEqual( reader.ReadU16(), std::uint16_t { 0x3456 }, "ReadU16 returned the wrong value" );

            ExpectEqual( reader.ReadU32(), std::uint32_t { 0x789abcde }, "ReadU32 returned the wrong value" );

            ExpectEqual( reader.Remaining(), std::size_t { 0 }, "Exact-end read did not consume the buffer" );
        }

        {
            const std::array<std::byte, 5> data = { std::byte { 0x10 },
                                                    std::byte { 0x20 },
                                                    std::byte { 0x30 },
                                                    std::byte { 0x40 },
                                                    std::byte { 0x50 } };

            protocol::BinaryReader reader( data );

            const auto bytes = reader.ReadBytes( 2 );

            ExpectEqual( bytes.size(), std::size_t { 2 }, "ReadBytes returned the wrong size" );

            ExpectEqual( bytes[0], std::byte { 0x10 }, "ReadBytes returned the wrong first byte" );

            ExpectEqual( bytes[1], std::byte { 0x20 }, "ReadBytes returned the wrong second byte" );

            reader.Skip( 2 );

            ExpectEqual( reader.ReadU8(), std::uint8_t { 0x50 }, "Skip advanced to the wrong position" );

            ExpectEqual( reader.Remaining(), std::size_t { 0 }, "Reader should be at the end after Skip" );
        }

        /*
         * This is the regression test for the bug we hit with
         * the real TS3 SET_COOKIE packet.
         *
         * Reading exactly to the end must succeed.
         */
        {
            const std::array<std::byte, 4> data = { std::byte { 0xde },
                                                    std::byte { 0xad },
                                                    std::byte { 0xbe },
                                                    std::byte { 0xef } };

            protocol::BinaryReader reader( data );

            ExpectEqual( reader.ReadU32(), std::uint32_t { 0xdeadbeef }, "Exact-end ReadU32 failed" );

            ExpectEqual( reader.Remaining(), std::size_t { 0 }, "Exact-end ReadU32 left unread data" );
        }

        {
            const std::array<std::byte, 2> data = { std::byte { 0xaa }, std::byte { 0xbb } };

            protocol::BinaryReader reader( data );

            ExpectThrows<std::runtime_error>(
                [&reader]() {
                    (void)reader.ReadU32();
                },
                "Reading beyond the buffer did not fail" );
        }

        {
            const std::array<std::byte, 2> data = { std::byte { 0xaa }, std::byte { 0xbb } };

            protocol::BinaryReader reader( data );

            ExpectThrows<std::runtime_error>(
                [&reader]() {
                    reader.Skip( 3 );
                },
                "Skipping beyond the buffer did not fail" );
        }

        {
            const std::array<std::byte, 0> data {};

            protocol::BinaryReader reader( data );

            ExpectEqual( reader.Remaining(), std::size_t { 0 }, "Empty reader should have no remaining bytes" );

            ExpectThrows<std::runtime_error>(
                [&reader]() {
                    (void)reader.ReadU8();
                },
                "Reading from an empty buffer did not fail" );
        }
    }

} // namespace ts::test
