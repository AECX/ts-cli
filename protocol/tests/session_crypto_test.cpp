#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/crypto/session_crypto.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/server_packet.hpp>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ts::test {

    void RunSessionCryptoTests() {
        /*
         * Real clientinit encryption vector.
         *
         * The SharedIV is the negotiated IV corresponding to
         * ReSpeak's test_new_decrypt fixture.
         *
         * The original packet has:
         *
         *   packet ID = 2
         *   client ID = 0
         *   flags/type = 0x32
         *
         * and is exactly 500 bytes including its client header.
         */
        {
            const auto sharedIvBytes = Hex( "4d 3f da b7 d8 b0 2c 82 "
                                            "70 6a 1a b4 b0 78 2d c5 "
                                            "32 03 51 5d 81 03 38 79 "
                                            "d9 14 1b 0d e2 81 ef 47 "
                                            "5f d9 7b 74 17 44 70 70 "
                                            "7b 59 28 65 92 99 19 77 "
                                            "31 d4 61 b3 e3 d1 73 59 "
                                            "62 e0 b1 81 00 4a 88 15" );

            std::array<std::byte, 64> sharedIv {};

            std::copy( sharedIvBytes.begin(), sharedIvBytes.end(), sharedIv.begin() );

            const std::array<std::byte, 8> sharedMac = { std::byte { 0x00 },
                                                         std::byte { 0x11 },
                                                         std::byte { 0x22 },
                                                         std::byte { 0x33 },
                                                         std::byte { 0x44 },
                                                         std::byte { 0x55 },
                                                         std::byte { 0x66 },
                                                         std::byte { 0x77 } };

            const protocol::SessionCrypto crypto( sharedIv, sharedMac );

            const auto meta = Hex( "00 02 "
                                   "00 00 "
                                   "32" );

            constexpr std::string_view Plaintext =
                R"(clientinit client_nickname=SplamyTest client_version=3.1.8\s[Build:\s1516614607] client_platform=Windows client_input_hardware=1 client_output_hardware=1 client_default_channel client_default_channel_password client_server_password client_meta_data client_version_sign=gDEgQf\/BiOQZdAheKccM1XWcMUj2OUQqt75oFuvF2c0MQMXyv88cZQdUuckKbcBRp7RpmLInto4PIgd7mPO7BQ== client_key_offset=455 client_nickname_phonetic client_default_token client_badges=Overwolf=0 hwid=87056c6e1268aaf5055abf8256415e)";

            const auto plaintext = Bytes( Plaintext );

            const auto encrypted = crypto.EncryptClient( protocol::PacketType::Command,
                                                         2,
                                                         0,
                                                         std::span<const std::byte>( meta ),
                                                         std::span<const std::byte>( plaintext ) );

            const std::array<std::byte, 8> expectedMac = { std::byte { 0x2b },
                                                           std::byte { 0x98 },
                                                           std::byte { 0x24 },
                                                           std::byte { 0x43 },
                                                           std::byte { 0xab },
                                                           std::byte { 0x38 },
                                                           std::byte { 0xbe },
                                                           std::byte { 0x6b } };

            ExpectEqual( encrypted.mac, expectedMac, "Session client MAC does not match golden vector" );

            const auto expectedCiphertext =
                Hex( "9abf64d4572e1349897b5e1e96fbc4a763a4c4ce1f64f0c1e3febd0a5f04a82ab1f2bc2344bb374fd16181beb8233b5b"
                     "06944280470e9b6893290a1da0776ffcd89f3beec2ce23b9694930c09efaaea0d88a6895a08ede4d5cbfea61291fc553"
                     "ac651f1e2bc1d2bd277a8bd9ab5386415579a9e56fac46d8b6b119f454bebd99179cd317dec60af205341d11f274d02b"
                     "bacdd7e9773f72a426358ca1d39016dd95bde2409cd81bf99b340887e997ea982370c6790cf4d2315046082022476683"
                     "8ea4ec4d71dd102ede701ea0001f392623aa410dd9ab0e45874da82e29e6e370515ec30a37dd73f5a364c233ff014384"
                     "beab5f1708c9f48dfba33a520f8fcdcef055789c54693c3fe72c5bfaca7cb4ca1fed77b8624660b8abc882f4b95b1284"
                     "cb6dc55019c6082dd6dd146fa50383662d7298bef04ababaf1af80e15cd4c1f81326f085788e2918e00324147dce39b2"
                     "3db71326abc3de4b94df10f1531e9cce202bba71fa3ebeefd77b21fa3260a62e92eeee2183421d384a8c48777e2f9efb"
                     "c58d4f442c5f0529c7c0e27e81b2b6b1b05eb8fa19256886248d553582dfd24c7cfab3c3f7317a5cebc6504b53fa0e86"
                     "fc8c1100fc1d506fcf96caa76a7c0b6a27e577f2efdecd4070e847a559bf37d75bfdbe9e814c702426ce696d8645bc30"
                     "0b5f28f9e7f1ce" );

            ExpectEqual( encrypted.data, expectedCiphertext, "Session client ciphertext does not match golden vector" );

            ExpectEqual( crypto.SharedMac(), sharedMac, "SessionCrypto did not preserve SharedMac" );
        }

        /*
         * Server-direction vector.
         *
         * This separately protects the 0x30 server key
         * derivation path and three-byte server metadata.
         */
        {
            const auto sharedIvBytes = Hex( "7e 34 c4 df 0a 5d bb ac "
                                            "c9 2f d1 a7 d2 48 6c 2e "
                                            "a2 f4 17 97 85 25 45 cf "
                                            "c8 92 19 01 2b 2d 52 84 "
                                            "2b 2b dd 98 ff c9 72 95 "
                                            "21 23 f3 f6 6a da 55 d9 "
                                            "d8 4a 37 e3 3b 2d 23 fe "
                                            "38 fd 14 ae 06 67 09 16" );

            std::array<std::byte, 64> sharedIv {};

            std::copy( sharedIvBytes.begin(), sharedIvBytes.end(), sharedIv.begin() );

            const std::array<std::byte, 8> sharedMac {};

            const protocol::SessionCrypto crypto( sharedIv, sharedMac );

            const auto rawPacket = Hex( "09 e2 6f 34 92 c0 ce 03 "
                                        "00 01 "
                                        "42 "
                                        "3a 14 2d 93 8c 97 c1 ab "
                                        "aa bd bf c6 c2 7e d5 6b "
                                        "b5 34 b0 2b 33 2d 58 50 "
                                        "de 9d 21 bd cd 5b e9 65 "
                                        "3f 4d c5 d6 98 93 3a 3f "
                                        "98 6a ff a9 33 4d b9 69 "
                                        "8a b7" );

            const protocol::Packet packet( rawPacket );

            const protocol::ServerPacket serverPacket = protocol::ServerPacket::Parse( packet );

            const auto plaintext = crypto.DecryptServer( serverPacket, 0 );

            ExpectEqual( plaintext,
                         Bytes( R"(initserver aclid=8 virtualserver_name=Test\sServer)" ),
                         "Session server packet decrypted incorrectly" );
        }

        /*
         * Server session packets must also reject modified
         * authentication tags.
         */
        {
            const auto sharedIvBytes = Hex( "7e 34 c4 df 0a 5d bb ac "
                                            "c9 2f d1 a7 d2 48 6c 2e "
                                            "a2 f4 17 97 85 25 45 cf "
                                            "c8 92 19 01 2b 2d 52 84 "
                                            "2b 2b dd 98 ff c9 72 95 "
                                            "21 23 f3 f6 6a da 55 d9 "
                                            "d8 4a 37 e3 3b 2d 23 fe "
                                            "38 fd 14 ae 06 67 09 16" );

            std::array<std::byte, 64> sharedIv {};

            std::copy( sharedIvBytes.begin(), sharedIvBytes.end(), sharedIv.begin() );

            const std::array<std::byte, 8> sharedMac {};

            const protocol::SessionCrypto crypto( sharedIv, sharedMac );

            auto rawPacket = Hex( "09 e2 6f 34 92 c0 ce 03 "
                                  "00 01 "
                                  "42 "
                                  "3a 14 2d 93 8c 97 c1 ab "
                                  "aa bd bf c6 c2 7e d5 6b "
                                  "b5 34 b0 2b 33 2d 58 50 "
                                  "de 9d 21 bd cd 5b e9 65 "
                                  "3f 4d c5 d6 98 93 3a 3f "
                                  "98 6a ff a9 33 4d b9 69 "
                                  "8a b7" );

            rawPacket[0] ^= std::byte { 0x80 };

            const protocol::Packet packet( rawPacket );

            const protocol::ServerPacket serverPacket = protocol::ServerPacket::Parse( packet );

            ExpectThrows<std::runtime_error>(
                [&crypto, &serverPacket]() {
                    (void)crypto.DecryptServer( serverPacket, 0 );
                },
                "Session MAC tampering was not detected" );
        }
    }

} // namespace ts::test
