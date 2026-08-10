#ifndef TS_PROTOCOL_MESSAGE_SET_CONNECTION_INFO_HPP
#define TS_PROTOCOL_MESSAGE_SET_CONNECTION_INFO_HPP

#include <cstddef>
#include <protocol/connection_statistics.hpp>
#include <vector>

namespace ts::protocol {

    class SetConnectionInfo {
      public:
        explicit SetConnectionInfo( ConnectionStatistics::Snapshot statistics );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        ConnectionStatistics::Snapshot m_Statistics;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_SET_CONNECTION_INFO_HPP
