#ifndef TS_PROTOCOL_SESSION_EVENT_HPP
#define TS_PROTOCOL_SESSION_EVENT_HPP

#include <cstdint>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/voice/voice.hpp>
#include <string>
#include <utility>
#include <variant>

namespace ts::protocol {

    struct TextMessageEvent {
        TextMessageEntry message;
        std::optional<TextMessageTarget> replyTarget;
        std::string channelName;
        std::string privatePeerName;
        bool outgoing = false;
    };

    enum class ClientPresenceKind { Joined, Left };

    struct ClientPresenceEvent {
        ClientPresenceKind kind = ClientPresenceKind::Joined;
        std::uint16_t clientId = 0;
        std::uint64_t channelId = 0;
        std::string clientName;
        std::string channelName;
    };

    struct CommandErrorEvent {
        std::uint32_t id = 0;
        std::string message;
    };

    struct VoiceEvent {
        VoiceFrame frame;
    };

    struct PokeEvent {
        std::uint16_t invokerId = 0;
        std::string invokerName;
        std::string invokerUniqueId;
        std::string message;
    };

    using SessionEvent = std::variant<TextMessageEvent, ClientPresenceEvent, CommandErrorEvent, VoiceEvent, PokeEvent>;

    /*
     * Overload set built from a pack of lambdas, so a caller can dispatch on
     * a SessionEvent without hand-rolling an if constexpr chain.
     */
    template<typename... Handlers>
    struct EventVisitor: Handlers... {
        using Handlers::operator()...;
    };

    template<typename... Handlers>
    EventVisitor( Handlers... ) -> EventVisitor<Handlers...>;

    /*
     * Dispatch event to whichever handler accepts its alternative. Provide a
     * generic [](const auto&){} handler last to ignore the rest; without one,
     * leaving an alternative unhandled is a compile error, which is what keeps
     * callers honest when a new event type is added.
     */
    template<typename... Handlers>
    void Visit( const SessionEvent& event, Handlers&&... handlers ) {
        std::visit( EventVisitor { std::forward<Handlers>( handlers )... }, event );
    }

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_EVENT_HPP
