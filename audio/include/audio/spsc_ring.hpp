#ifndef TS_AUDIO_SPSC_RING_HPP
#define TS_AUDIO_SPSC_RING_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

namespace ts::audio {

    template<typename T, std::size_t Capacity>
    class SpscRing {
      public:
        static_assert( Capacity >= 2 );

        bool TryPush( const T& value ) {
            return TryEmplace( value );
        }

        bool TryPush( T&& value ) {
            return TryEmplace( std::move( value ) );
        }

        bool TryPop( T& value ) {
            const std::size_t read = m_Read.load( std::memory_order_relaxed );
            const std::size_t write = m_Write.load( std::memory_order_acquire );

            if ( read == write ) {
                return false;
            }

            value = std::move( m_Data[read] );
            m_Read.store( Next( read ), std::memory_order_release );
            return true;
        }

        void Clear() {
            T ignored;
            while ( TryPop( ignored ) ) {
            }
        }

      private:
        template<typename U>
        bool TryEmplace( U&& value ) {
            const std::size_t write = m_Write.load( std::memory_order_relaxed );
            const std::size_t next = Next( write );

            if ( next == m_Read.load( std::memory_order_acquire ) ) {
                return false;
            }

            m_Data[write] = std::forward<U>( value );
            m_Write.store( next, std::memory_order_release );
            return true;
        }

        [[nodiscard]] static constexpr std::size_t Next( std::size_t value ) {
            return ( value + 1 ) % Capacity;
        }

        std::array<T, Capacity> m_Data {};
        alignas( 64 ) std::atomic<std::size_t> m_Write { 0 };
        alignas( 64 ) std::atomic<std::size_t> m_Read { 0 };
    };

} // namespace ts::audio

#endif // TS_AUDIO_SPSC_RING_HPP
