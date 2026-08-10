#ifndef TS_PROTOCOL_CLIENT_PROFILE_HPP
#define TS_PROTOCOL_CLIENT_PROFILE_HPP

#include <cstdint>
#include <string>

namespace ts::protocol {

    struct ClientVersionProfile {
        std::uint32_t initVersion = 0;

        std::string version;
        std::string platform;
        std::string signature;
    };

    struct ClientProfile {
        std::string nickname;

        ClientVersionProfile version;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CLIENT_PROFILE_HPP
