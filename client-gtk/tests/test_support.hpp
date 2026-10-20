#ifndef TS_CLIENT_GTK_TEST_SUPPORT_HPP
#define TS_CLIENT_GTK_TEST_SUPPORT_HPP

#include <stdexcept>
#include <string>
#include <string_view>

namespace ts::client::gtk::test {

    inline void Expect( bool condition, std::string_view message ) {
        if ( !condition ) {
            throw std::runtime_error( std::string( message ) );
        }
    }

    template<typename Left, typename Right>
    void ExpectEqual( const Left& left, const Right& right, std::string_view message ) {
        if ( !( left == right ) ) {
            throw std::runtime_error( std::string( message ) );
        }
    }

} // namespace ts::client::gtk::test

#endif // TS_CLIENT_GTK_TEST_SUPPORT_HPP
