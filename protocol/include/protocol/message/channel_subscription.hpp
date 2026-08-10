#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_SUBSCRIPTION_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_SUBSCRIPTION_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <vector>

namespace ts::protocol {

    class ChannelSubscribeAll {
      public:
        [[nodiscard]] static std::vector<std::byte> Serialize();
    };

    class ChannelSubscribe {
      public:
        explicit ChannelSubscribe( std::uint64_t channelId );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        std::uint64_t m_ChannelId = 0;
    };

    struct ChannelSubscriptionEntry {
        std::uint64_t channelId = 0;
    };

    class ChannelSubscriptionState {
      public:
        [[nodiscard]] static ChannelSubscriptionState Parse( const Command& command );

        [[nodiscard]] bool Subscribed() const;
        [[nodiscard]] const std::vector<ChannelSubscriptionEntry>& Entries() const;

      private:
        bool m_Subscribed = false;
        std::vector<ChannelSubscriptionEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_SUBSCRIPTION_HPP
