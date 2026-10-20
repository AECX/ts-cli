# Configuration and identity

`ts-cli` intentionally supports one local TeamSpeak identity. Ordinary client preferences, the local identity, and preferences about remote users are stored separately so each file has one clear responsibility.

All of this is implemented once, in the `ts-config` library (`config/`), and shared by `ts-cli` and `ts-gtk`. Neither front end reads or writes these files itself. That includes applying the saved `[audio]` settings to the audio engine at startup (`ApplyAudioSettings`): a saved device or filter that isn't available right now is logged as a warning and skipped, and the remaining settings still apply.

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

last_server=voice.example.net:9987
```

The client-version fields form one TeamSpeak wire profile. Custom values should be changed as a complete known-good tuple rather than mixed independently.

Audio input/output selectors, capture-filter selection, and microphone activation threshold are persisted immediately when their corresponding `/audio` command succeeds. Local microphone transmit/mute state is deliberately session-only and always starts disabled.

`last_server` is written by the [GTK client](gtk.md) after each successful connect and prefills its server address on the next start. It is optional and `ts-cli` ignores it.

## Local `identity`

The `identity` file is the only persistent local TeamSpeak identity used by this client. There is no identity selector and no local `identities/` directory.

It stores the private TeamSpeak identity together with its security level and validated hashcash/key offset. Treat this file as private identity material.

The [GTK client](gtk.md)'s options can show the identity's unique ID, replace it with a newly generated one or a private key PEM file, and export it as PEM (written owner-only, like `identity` itself). `IdentityStore::ImportPem`/`ExportPem` in `ts-config` implement the PEM side for both clients. The unique ID other clients see is `base64(SHA-1(base64(public key)))` (`protocol::Identity::UniqueId`).

### Raising the security level

Some servers only admit identities of a minimum security level. The level is how many trailing zero bits `SHA-1(base64(public key) + key offset)` has, so raising it means searching for a larger key offset; the key, and with it the unique ID, stays the same. Each level doubles the expected number of offsets to try (about 2^level, roughly two million per second on one core), so level 20 takes about a second and level 30 about ten minutes.

```bash
ts-cli --security-level 24
```

This raises the level of the local `identity`, saves it and exits without connecting. Ctrl+C stops the search and leaves the file unchanged. The GTK client offers the same in its [identity options](gtk.md#identity). Both use `IdentityStore::IncreaseSecurityLevel`, which continues from the current key offset (no smaller offset reaches even the current level) and checks a stop token as it goes.

If `config.conf` is missing but a valid `identity` file still exists, first-run configuration preserves that identity rather than silently replacing it.

If `identity` is missing while `config.conf` exists, startup fails instead of silently generating a new identity.

Only this layout is supported. Configuration keys from older versions (`version_profile`, `identity_security_level`, `identity_private_key`, `identity_key_offset`) are rejected as unknown keys rather than migrated.

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

When no `config.conf` exists, `ts-cli` runs an interactive setup that collects the client profile. If there is no `identity` file it also creates/imports the one local TeamSpeak identity and calculates its key offset. If an identity file already exists, setup validates and preserves it.

The [GTK client](gtk.md) reads and writes the same files. Without a terminal to prompt on, its first run writes the default client profile and, if there is no `identity` yet, generates one at the default security level; an existing identity is preserved just like in the interactive setup. The nickname and identity can then be changed in its options; run `ts-cli` once first if you want a custom client profile.

See [Audio and voice](audio.md) for runtime audio behavior and [Runtime model](runtime.md) for ownership/threading.
