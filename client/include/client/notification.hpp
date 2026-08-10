#ifndef TS_CLIENT_NOTIFICATION_HPP
#define TS_CLIENT_NOTIFICATION_HPP

#include <audio/audio_types.hpp>
#include <functional>

namespace ts::client {
    using NotificationCallback = std::function<void( ts::audio::NotificationType )>;
}

#endif
