#ifndef TS_CLIENT_CONFIG_CONFIG_HPP
#define TS_CLIENT_CONFIG_CONFIG_HPP

#include <audio/audio_types.hpp>
#include <client/config/identity_store.hpp>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <protocol/client_profile.hpp>
#include <string>
#include <string_view>

namespace ts::client {

    class Config {
      public:
        [[nodiscard]] static Config Load( const std::filesystem::path& path );
        [[nodiscard]] static Config Create( protocol::ClientProfile profile );
        [[nodiscard]] static protocol::ClientProfile DefaultProfile();

        void Save( const std::filesystem::path& path );
        void Save();

        [[nodiscard]] protocol::ClientProfile Profile() const;
        [[nodiscard]] audio::AudioSettings AudioSettings() const;
        void SetAudioSettings( const audio::AudioSettings& settings );
        void SetNickname( std::string nickname );

        [[nodiscard]] bool NeedsSave() const;
        [[nodiscard]] bool HasLegacyIdentity() const;
        [[nodiscard]] LegacyIdentityConfig LegacyIdentity() const;
        void ClearLegacyIdentity();

      private:
        [[nodiscard]] static Config Parse( std::string_view data, bool& migrated );
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view name );
        [[nodiscard]] static float ParseFloat( std::string_view value, std::string_view name );
        [[nodiscard]] static std::string_view Trim( std::string_view value );

        std::string m_Nickname;
        std::uint32_t m_ClientInitVersion = 0;
        std::string m_ClientVersion;
        std::string m_ClientPlatform;
        std::string m_ClientVersionSignature;
        audio::AudioSettings m_AudioSettings;

        bool m_NeedsSave = false;
        bool m_HasLegacyIdentity = false;
        LegacyIdentityConfig m_LegacyIdentity;
        std::optional<std::filesystem::path> m_Path;
    };

    using ConfigMutator = std::function<void( Config& )>;

} // namespace ts::client

#endif // TS_CLIENT_CONFIG_CONFIG_HPP
