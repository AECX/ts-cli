#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/client_poke.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace ts::protocol {

    namespace {

        [[nodiscard]] std::uint64_t ParseUnsigned( std::string_view value, std::string_view name ) {
            std::uint64_t result = 0;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars( first, last, result );
            if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
                throw std::runtime_error( "Invalid unsigned command parameter: " + std::string( name ) );
            }
            return result;
        }

    } // namespace

    ClientPoke ClientPoke::Parse( const Command& command ) {
        if ( command.Name() != "notifyclientpoke" ) {
            throw std::runtime_error( "Expected notifyclientpoke command" );
        }

        if ( command.Rows().size() != 1 ) {
            throw std::runtime_error( "notifyclientpoke must contain exactly one poke" );
        }

        const CommandRow& row = command.Rows().front();
        ClientPoke result;

        const std::uint64_t invokerId = ParseUnsigned( row.Require( "invokerid" ), "invokerid" );

        if ( invokerId > std::numeric_limits<std::uint16_t>::max() ) {
            throw std::runtime_error( "Invalid notifyclientpoke invokerid" );
        }

        result.m_Entry.invokerId = static_cast<std::uint16_t>( invokerId );
        result.m_Entry.invokerName = row.Require( "invokername" );
        result.m_Entry.invokerUniqueId = row.Require( "invokeruid" );
        result.m_Entry.message = row.Require( "msg" );

        return result;
    }

    const ClientPokeEntry& ClientPoke::Entry() const {
        return m_Entry;
    }

} // namespace ts::protocol
