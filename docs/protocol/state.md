# Server state

> Status: implemented channel/client tree with full-channel subscription

`ts-cli` keeps an in-memory view of visible TeamSpeak channels and clients. The protocol/network thread is the sole owner of these stores; the terminal client reads them through `Connection` and does not mutate them directly.

## Initial channel snapshot

After `initserver`, the server sends one or more `channellist` commands followed by `channellistfinished`. A large server may split the snapshot across several commands, so `ChannelStore` accumulates entries until the finished marker arrives.

`channel_order` is a predecessor channel ID, not a numeric array index. The store reconstructs each sibling chain from that predecessor relation and validates parent references, duplicate positions, disconnected chains, and cycles before exposing the tree.

The current channel is taken from the local client's known placement when available, otherwise from the advertised default channel.

## Channel subscriptions and client visibility

TeamSpeak channel subscription controls client enter/leave visibility. Merely knowing that a channel exists is not enough to maintain a complete client tree for it.

After the channel list is complete, `ts-cli` sends `channelsubscribeall` without blocking login on its generic command result. Subscription notifications and the resulting client-visibility snapshot are normal live-session traffic and update the maintained channel/client stores as they arrive. This keeps the CLI responsive even on servers that delay or omit the expected command-result ordering; `/list` reflects the progressively synchronized state.

Entering a channel implicitly subscribes it. Newly created channels are not implicitly subscribed, so after initial login `notifychannelcreated` causes an explicit `channelsubscribe` for that channel.

The store tracks `notifychannelsubscribed` and `notifychannelunsubscribed`. If a channel becomes unsubscribed, remote clients whose only known placement is that channel are removed from the visible client store. The local client is preserved.

## Live channel updates

The channel store handles:

- `notifychannelcreated` — insert the new channel into the predecessor chain;
- `notifychanneledited` — update the fields supplied by the server;
- `notifychannelmoved` — update parent and predecessor placement;
- `notifychanneldeleted` — remove the channel and its directly associated visible clients;
- subscription notifications — update visibility state.

The server remains the source of truth; local commands do not preemptively mutate channel state before the corresponding server notification arrives.

## Client state and out-of-order notifications

Client visibility is updated from enter, move, leave, and update notifications. A full enter row includes nickname and unique identity, while some other notifications may arrive before that detail is known.

`ClientStore` therefore permits a partial client record. For example, if a move for client 42 arrives first, the store retains the new channel placement even though the nickname/UID are not known yet. When the later enter/view row completes that client, it fills in details without overwriting the newer move with an older snapshot channel.

This is important during subscription snapshots where network delivery/command ordering can expose movement and visibility updates close together.

For a leave-view notification whose target channel is still subscribed, the client can remain visible and is moved to the target channel instead of being blindly erased. A true disconnect or a move outside subscribed visibility removes it.

## Tree presentation

`/list` renders the maintained channel tree and inserts the currently known clients under each channel. Partial clients can temporarily appear as `client <id> [loading]` until their full enter/view details arrive.

`/list <channel>` resolves a channel name or ID and renders that subtree.

Related chapters: [Command protocol](commands.md), [Connection lifecycle](connection_lifecycle.md), [Text chat](text_chat.md).
