#ifndef TS_PROTOCOL_RELIABILITY_COMMAND_RECEIVE_WINDOW_HPP
#define TS_PROTOCOL_RELIABILITY_COMMAND_RECEIVE_WINDOW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/packet/sequence.hpp>
#include <span>
#include <vector>

namespace ts::protocol {

    /*
     * Receive-side counterpart to ReliableCommandQueue: reorders, deduplicates
     * and reassembles one independent reliable/ordered/fragmenting command
     * stream. TeamSpeak has two such streams (Command and CommandLow), each
     * with its own in-flight fragment and out-of-order queue, so each gets
     * its own CommandReceiveWindow instance.
     *
     * Sequence/generation tracking itself stays centralized in the caller's
     * PacketSequenceState (the same place every other packet type is
     * tracked) rather than being duplicated here -- this class only owns the
     * out-of-order buffering and fragment reassembly built on top of it.
     */
    class CommandReceiveWindow {
      public:
        // Feeds one decrypted incoming packet already known to belong to
        // this stream, given the stream's PacketSequence (advanced in place
        // as packets are consumed in order) and this packet's resolved
        // generation. Returns zero or more newly-completed, decompressed
        // command payloads unblocked by this packet, in order: zero for a
        // duplicate or an out-of-order packet that gets buffered, one or
        // more if this packet fills a gap and drains a run of already-queued
        // packets behind it.
        [[nodiscard]] std::vector<std::vector<std::byte>> Feed( PacketSequence& expected,
                                                                std::uint16_t packetId,
                                                                std::uint32_t generationId,
                                                                PacketFlags flags,
                                                                std::vector<std::byte> plaintext );

      private:
        struct QueuedPacket {
            PacketFlags flags = PacketFlags::None;
            std::vector<std::byte> data;
        };

        static constexpr std::size_t MaxQueuedPackets = 64;

        void ProcessOrdered( PacketFlags flags, std::span<const std::byte> data, std::vector<std::vector<std::byte>>& ready );

        std::map<std::uint64_t, QueuedPacket> m_Queued;

        bool m_Assembling = false;
        bool m_Compressed = false;
        std::vector<std::byte> m_Fragments;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_RELIABILITY_COMMAND_RECEIVE_WINDOW_HPP
