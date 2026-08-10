#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <protocol/crypto/bootstrap_crypto.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/server_packet.hpp>
#include <span>
#include <stdexcept>

namespace ts::test {

    void RunBootstrapCryptoTests() {
        /*
         * Real TeamSpeak bootstrap ACK vector.
         *
         * Client header metadata:
         *
         *   packet ID = 0
         *   client ID = 0
         *   type      = Ack
         *
         * Plaintext:
         *
         *   acknowledged Command ID = 0
         *
         * Complete known packet:
         *
         *   a4 7b 47 94 db a9 6a c5
         *   00 00
         *   00 00
         *   06
         *   fe 18
         *
         * This is also present in ReSpeak's test suite and
         * matches our live tcpdump.
         */
        {
            const protocol::BootstrapCrypto crypto;

            const auto meta = Hex( "00 00 "
                                   "00 00 "
                                   "06" );

            const auto plaintext = Hex( "00 00" );

            const auto encrypted =
                crypto.Encrypt( std::span<const std::byte>( meta ), std::span<const std::byte>( plaintext ) );

            const std::array<std::byte, 8> expectedMac = { std::byte { 0xa4 },
                                                           std::byte { 0x7b },
                                                           std::byte { 0x47 },
                                                           std::byte { 0x94 },
                                                           std::byte { 0xdb },
                                                           std::byte { 0xa9 },
                                                           std::byte { 0x6a },
                                                           std::byte { 0xc5 } };

            ExpectEqual( encrypted.mac, expectedMac, "Bootstrap ACK MAC does not match golden vector" );

            ExpectEqual( encrypted.data, Hex( "fe 18" ), "Bootstrap ACK ciphertext does not match golden vector" );
        }

        /*
         * Independently generated server-direction EAX vector.
         *
         * Server metadata:
         *
         *   packet ID = 0
         *   type      = Command
         *
         * Plaintext:
         *
         *   initivexpand2 test=1
         */
        {
            const auto rawPacket = Hex( "20 7c 89 b1 2a 68 cc ef "
                                        "00 00 "
                                        "02 "
                                        "97 76 8b 54 ad 79 e3 af "
                                        "87 eb aa 1a 0b fb d7 54 "
                                        "fb 03 a2 6b" );

            const protocol::Packet packet( rawPacket );

            const protocol::ServerPacket serverPacket = protocol::ServerPacket::Parse( packet );

            const protocol::BootstrapCrypto crypto;

            const auto plaintext = crypto.Decrypt( serverPacket );

            ExpectEqual( plaintext, Bytes( "initivexpand2 test=1" ), "Bootstrap server packet decrypted incorrectly" );
        }

        /*
         * Authentication failure must be detected before
         * plaintext is trusted.
         */
        {
            auto rawPacket = Hex( "20 7c 89 b1 2a 68 cc ef "
                                  "00 00 "
                                  "02 "
                                  "97 76 8b 54 ad 79 e3 af "
                                  "87 eb aa 1a 0b fb d7 54 "
                                  "fb 03 a2 6b" );

            rawPacket[0] ^= std::byte { 0x01 };

            const protocol::Packet packet( rawPacket );

            const protocol::ServerPacket serverPacket = protocol::ServerPacket::Parse( packet );

            const protocol::BootstrapCrypto crypto;

            ExpectThrows<std::runtime_error>(
                [&crypto, &serverPacket]() {
                    (void)crypto.Decrypt( serverPacket );
                },
                "Bootstrap MAC tampering was not detected" );
        }
    }

} // namespace ts::test
