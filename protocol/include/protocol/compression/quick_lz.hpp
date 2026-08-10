#ifndef TS_PROTOCOL_COMPRESSION_QUICK_LZ_HPP
#define TS_PROTOCOL_COMPRESSION_QUICK_LZ_HPP

#include <cstddef>
#include <span>
#include <vector>

namespace ts::protocol {

    class QuickLz {
      public:
        [[nodiscard]] static std::vector<std::byte> Decompress( std::span<const std::byte> data,
                                                                std::size_t maximumOutputSize );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_COMPRESSION_QUICK_LZ_HPP
