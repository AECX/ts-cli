#ifndef TS_LOG_LEVEL_HPP
#define TS_LOG_LEVEL_HPP

#include <cstdint>

namespace ts::log {

    enum class Level : std::uint8_t { Trace, Debug, Info, Warning, Error, Off };

} // namespace ts::log

#endif // TS_LOG_LEVEL_HPP
