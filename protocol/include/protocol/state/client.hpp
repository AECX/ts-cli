#ifndef TS_PROTOCOL_STATE_CLIENT_HPP
#define TS_PROTOCOL_STATE_CLIENT_HPP

#include <cstdint>
#include <string>

namespace ts::protocol {

    struct Client {
        std::uint16_t id = 0;
        std::uint64_t channelId = 0;

        std::string nickname;
        std::string uniqueId;

        bool away = false;
        bool inputMuted = false;
        bool outputMuted = false;
        bool inputHardware = true;
        bool outputHardware = true;
        bool recording = false;
        bool prioritySpeaker = false;
        bool channelCommander = false;
        bool serverQuery = false;

        /*
         * A move notification can arrive before the matching
         * client-enter notification. In that case we keep a
         * partial client so channel placement is not lost.
         */
        bool detailsKnown = false;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_STATE_CLIENT_HPP
