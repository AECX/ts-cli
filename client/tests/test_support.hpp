#ifndef TS_CLIENT_TEST_SUPPORT_HPP
#define TS_CLIENT_TEST_SUPPORT_HPP

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace ts::client::test {

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

    /*
     * A portable substitute for getpid()-based temp-directory naming: just
     * needs to keep concurrent test-binary runs from colliding on the same
     * path, not to be a cryptographic nonce.
     */
    [[nodiscard]] inline std::string UniqueTestDirectorySuffix() {
        std::ostringstream stream;
        stream << std::this_thread::get_id();
        return stream.str();
    }

} // namespace ts::client::test

#endif // TS_CLIENT_TEST_SUPPORT_HPP
