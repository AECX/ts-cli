#ifndef TS_PROTOCOL_SESSION_BOOTSTRAP_HPP
#define TS_PROTOCOL_SESSION_BOOTSTRAP_HPP

#include <protocol/crypto/session_material.hpp>
#include <protocol/packet/sequence_state.hpp>

namespace ts::protocol {

    struct SessionBootstrap {
        SessionMaterial material;

        PacketSequenceState sequences;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_BOOTSTRAP_HPP
