#ifndef TS_LOG_RECORD_HPP
#define TS_LOG_RECORD_HPP

#include "level.hpp"

#include <string_view>

namespace ts::log {

    struct Record {
        Level level;
        std::string_view component;
        std::string_view message;
    };

} // namespace ts::log

#endif // TS_LOG_RECORD_HPP
