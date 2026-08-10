#ifndef TS_PROTOCOL_COMMAND_WRITER_HPP
#define TS_PROTOCOL_COMMAND_WRITER_HPP

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    class CommandWriter {
      public:
        explicit CommandWriter( std::string_view name );

        void Write( std::string_view name );

        void Write( std::string_view name, std::string_view value );

        template<std::integral T>
        void Write( std::string_view name, T value ) {
            Write( name, std::to_string( value ) );
        }

        void NextRow();

        [[nodiscard]] std::vector<std::byte> Take() const;

      private:
        static void ValidateName( std::string_view name );

        [[nodiscard]] static std::string EncodeValue( std::string_view value );

        void WritePrefix( std::string_view name );

        std::string m_Data;

        bool m_FirstRow = true;
        bool m_RowHasParameter = false;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_COMMAND_WRITER_HPP
