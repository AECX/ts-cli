#ifndef TS_CONFIG_CHAT_SETTINGS_HPP
#define TS_CONFIG_CHAT_SETTINGS_HPP

namespace ts::config {

    struct ChatSettings {
        bool warnOpenLinks = true;
        bool blurImages = true;
    };

} // namespace ts::config
#endif
