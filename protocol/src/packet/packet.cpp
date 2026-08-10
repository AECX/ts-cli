#include <protocol/packet/packet.hpp>
#include <utility>

namespace ts::protocol {

    Packet::Packet( std::vector<std::byte> data ): m_Data( std::move( data ) ) {
    }

    std::span<const std::byte> Packet::Data() const {
        return m_Data;
    }

    std::size_t Packet::Size() const {
        return m_Data.size();
    }

} // namespace ts::protocol
