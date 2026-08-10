# Configuration and identity

`ts-cli` intentionally supports one local TeamSpeak identity. Ordinary client preferences, the local identity, and preferences about remote users are stored separately so each file has one clear responsibility.

## Layout

The configuration root is:

```text
$XDG_CONFIG_HOME/ts-cli/
```

or, when `XDG_CONFIG_HOME` is unset:

```text
~/.config/ts-cli/
```

The current layout is:

```text
ts-cli/
├── config.conf
├── identity
└── users/
    └── <remote-identity>.conf
```

The configuration directory and `users/` directory are restricted to the current user. Configuration, identity, and per-user files are written with owner-only permissions.

## `config.conf`

`config.conf` contains settings for the local client, not identity key material and not preferences about a particular remote user.

A typical file contains:

```ini
nickname=ts-cli

client_init_version=0x142898dd
client_version=3.6.2 [Build: 1695203293]
client_platform=Linux
client_version_sign=...

audio_input=default
audio_output=default
audio_filter=none
audio_activation_threshold_db=-45
```

The client-version fields form one TeamSpeak wire profile. Custom values should be changed as a complete known-good tuple rather than mixed independently.

Audio input/output selectors, capture-filter selection, and microphone activation threshold are persisted immediately when their corresponding `/audio` command succeeds. Local microphone transmit/mute state is deliberately session-only and always starts disabled.

## Local `identity`

The `identity` file is the only persistent local TeamSpeak identity used by this client. There is no identity selector and no local `identities/` directory.

It stores the private TeamSpeak identity together with its security level and validated hashcash/key offset. Treat this file as private identity material.

If `config.conf` is missing but a valid `identity` file still exists, first-run configuration preserves that identity rather than silently replacing it.

### Migration from older configurations

Older versions stored these fields in `config.conf`:

```ini
identity_security_level=...
identity_private_key=...
identity_key_offset=...
```

When no separate `identity` file exists, startup can migrate a complete legacy identity. Migration is ordered to avoid identity loss:

1. decode and validate the legacy identity;
2. write the separate `identity` file atomically;
3. remove the legacy identity fields from the in-memory config;
4. rewrite `config.conf` without private identity material.

If `config.conf` already uses the new format but `identity` is missing, startup fails instead of silently generating a new identity.

The cryptographic role of the identity and key offset is described in [Handshake and session bootstrap](../protocol/handshake.md).

## Remote-user settings

Persistent local treatment of another TeamSpeak user is stored under:

```text
users/<remote-identity>.conf
```

The filename is derived safely from the remote TeamSpeak unique identity. Nicknames and transient client IDs are never persistence keys.

A non-default file currently contains:

```ini
identity_uid=<remote TeamSpeak unique identity>
volume_db=-8.00
muted=false
```

`volume_db` is local playback gain. `0 dB` is unchanged volume, negative values attenuate, and positive values amplify. The accepted range is `-60 dB` through `+12 dB`.

`muted=true` locally removes that talker from the mix; it does not change server state and does not tell the remote client they are muted.

Default settings (`0 dB`, not muted) do not need a file. Resetting a user to defaults removes the unnecessary file.

Commands are:

```text
/user <client>
/user <client> volume -8
/user <client> volume 75%
/user <client> volume reset
/user <client> mute
/user <client> unmute
```

A nickname is resolved against current live client state and then converted to the permanent remote unique identity before loading/saving. If a nickname is ambiguous, use `#<client-id>` for selection; persistence still uses the unique identity.

## First-run setup

When no `config.conf` exists, interactive setup collects the client profile. If there is no `identity` file it also creates/imports the one local TeamSpeak identity and calculates its key offset. If an identity file already exists, setup validates and preserves it.

See [Audio and voice](audio.md) for runtime audio behavior and [Runtime model](runtime.md) for ownership/threading.
