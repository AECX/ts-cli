#include <algorithm>
#include <charconv>
#include <cstdint>
#include <protocol/encoding/base64.hpp>
#include <protocol/handshake/init_iv_expand_2.hpp>
#include <stdexcept>
#include <string>

namespace ts::protocol {

    std::vector<std::byte> DecodeField( const CommandRow& row, std::string_view name ) {
        const std::string_view encoded = row.Require( name );

        try {
            return Base64Decode( encoded );
        } catch ( const std::exception& exception ) {
            throw std::runtime_error( "Failed to decode initivexpand2 " + std::string( name ) + " (length " +
                                      std::to_string( encoded.size() ) + "): " + exception.what() );
        }
    }

    InitIvExpand2 InitIvExpand2::Parse( const Command& command ) {
        if ( command.Name() != "initivexpand2" ) {
            throw std::runtime_error( "Expected initivexpand2 command" );
        }

        if ( command.Rows().size() != 1 ) {
            throw std::runtime_error( "initivexpand2 must contain exactly one row" );
        }

        const CommandRow& row = command.Rows().front();

        InitIvExpand2 result;

        result.m_EncodedLicense = std::string( row.Require( "l" ) );

        // result.m_License = Base64Decode( result.m_EncodedLicense );
        result.m_License = DecodeField( row, "l" );

        if ( result.m_License.empty() ) {
            throw std::runtime_error( "initivexpand2 license is empty" );
        }

        // const auto beta = Base64Decode( row.Require( "beta" ) );
        const auto beta = DecodeField( row, "beta" );

        if ( beta.size() != result.m_Beta.size() ) {
            throw std::runtime_error( "Invalid initivexpand2 beta length" );
        }

        std::copy( beta.begin(), beta.end(), result.m_Beta.begin() );

        // result.m_Omega = Base64Decode( row.Require( "omega" ) );
        result.m_Omega = DecodeField( row, "omega" );

        if ( result.m_Omega.empty() ) {
            throw std::runtime_error( "initivexpand2 omega is empty" );
        }

        // result.m_Proof = Base64Decode( row.Require( "proof" ) );
        result.m_Proof = DecodeField( row, "proof" );

        if ( result.m_Proof.empty() ) {
            throw std::runtime_error( "initivexpand2 proof is empty" );
        }

        const std::string_view ot = row.Require( "ot" );

        const char* begin = ot.data();

        const char* end = begin + ot.size();

        const auto parsed = std::from_chars( begin, end, result.m_Ot );

        if ( parsed.ec != std::errc {} || parsed.ptr != end ) {
            throw std::runtime_error( "Invalid initivexpand2 ot value" );
        }

        if ( result.m_Ot != 1 ) {
            throw std::runtime_error( "Unsupported initivexpand2 ot value" );
        }

        if ( const auto root = row.Find( "root" ) ) {
            result.m_Root = std::string( *root );
        }

        if ( const auto tvd = row.Find( "tvd" ) ) {
            result.m_Tvd = std::string( *tvd );
        }

        if ( const auto time = row.Find( "time" ) ) {
            result.m_Time = std::string( *time );
        }

        return result;
    }

    const std::string& InitIvExpand2::EncodedLicense() const {
        return m_EncodedLicense;
    }

    const std::vector<std::byte>& InitIvExpand2::License() const {
        return m_License;
    }

    const std::array<std::byte, 54>& InitIvExpand2::Beta() const {
        return m_Beta;
    }

    const std::vector<std::byte>& InitIvExpand2::Omega() const {
        return m_Omega;
    }

    const std::vector<std::byte>& InitIvExpand2::Proof() const {
        return m_Proof;
    }

    std::uint32_t InitIvExpand2::Ot() const {
        return m_Ot;
    }

    const std::optional<std::string>& InitIvExpand2::Root() const {
        return m_Root;
    }

    const std::optional<std::string>& InitIvExpand2::Tvd() const {
        return m_Tvd;
    }

    const std::optional<std::string>& InitIvExpand2::Time() const {
        return m_Time;
    }

} // namespace ts::protocol
