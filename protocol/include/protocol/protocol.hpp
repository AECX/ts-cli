#ifndef TS_PROTOCOL_PROTOCOL_HPP
#define TS_PROTOCOL_PROTOCOL_HPP

/*
 * Umbrella header for the TeamSpeak client protocol library.
 *
 * Including this gives you everything needed to open a connection, drive a
 * session, read server state and send chat/voice. The individual headers
 * remain available for callers that prefer narrower includes.
 *
 * See docs/protocol/api.md for a worked introduction.
 */

#include <protocol/client_profile.hpp>
#include <protocol/connection.hpp>
#include <protocol/connection_statistics.hpp>
#include <protocol/error.hpp>
#include <protocol/identity.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/session/audio_state.hpp>
#include <protocol/session/disconnect.hpp>
#include <protocol/session/event.hpp>
#include <protocol/state/channel.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client.hpp>
#include <protocol/state/client_store.hpp>
#include <protocol/voice/voice.hpp>

#endif // TS_PROTOCOL_PROTOCOL_HPP
