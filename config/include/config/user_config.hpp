#ifndef TS_CONFIG_USER_CONFIG_HPP
#define TS_CONFIG_USER_CONFIG_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace ts::config {

    struct UserConfig {
        float volumeDb = 0.0F;
        bool muted = false;
    };

    class UserConfigStore {
      public:
        explicit UserConfigStore( std::filesystem::path directory );

        [[nodiscard]] UserConfig Load( std::string_view uniqueId ) const;
        void Save( std::string_view uniqueId, const UserConfig& config ) const;

        [[nodiscard]] static bool IsDefault( const UserConfig& config );
        [[nodiscard]] static float MinVolumeDb();
        [[nodiscard]] static float MaxVolumeDb();

      private:
        [[nodiscard]] std::filesystem::path PathFor( std::string_view uniqueId ) const;
        [[nodiscard]] static std::string EncodeFileName( std::string_view uniqueId );
        static void Validate( std::string_view uniqueId, const UserConfig& config );

        std::filesystem::path m_Directory;
    };

} // namespace ts::config

#endif // TS_CONFIG_USER_CONFIG_HPP
