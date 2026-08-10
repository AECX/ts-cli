#ifndef TS_PROTOCOL_CRYPTO_PUZZLE_HPP
#define TS_PROTOCOL_CRYPTO_PUZZLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace ts::protocol {

    [[nodiscard]] std::array<std::byte, 64>
        SolvePuzzle( const std::array<std::byte, 64>& x, const std::array<std::byte, 64>& n, std::uint32_t level );

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_PUZZLE_HPP
