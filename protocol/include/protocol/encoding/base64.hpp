#ifndef TS_PROTOCOL_ENCODING_BASE64_HPP
#define TS_PROTOCOL_ENCODING_BASE64_HPP

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    [[nodiscard]] std::string Base64Encode( std::span<const std::byte> data );

    [[nodiscard]] std::vector<std::byte> Base64Decode( std::string_view data );

} // namespace ts::protocol

#endif // TS_PROTOCOL_ENCODING_BASE64_HPP
