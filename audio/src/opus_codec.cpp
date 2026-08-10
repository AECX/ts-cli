#include <algorithm>
#include <array>
#include <audio/codec.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <opus/opus.h>
#include <span>
#include <stdexcept>
#include <string>

namespace ts::audio {

    namespace {

        void CheckOpus( int result, const char* operation ) {
            if ( result >= 0 ) {
                return;
            }

            throw std::runtime_error( std::string( operation ) + ": " + opus_strerror( result ) );
        }

        class OpusEncoder final: public Encoder {
          public:
            OpusEncoder() {
                int error = OPUS_OK;
                m_Encoder = opus_encoder_create( static_cast<opus_int32>( SampleRate ),
                                                 static_cast<int>( Channels ),
                                                 OPUS_APPLICATION_VOIP,
                                                 &error );
                CheckOpus( error, "Failed to create Opus encoder" );

                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_SET_BITRATE( 48000 ) ), "Failed to set Opus bitrate" );
                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_SET_COMPLEXITY( 6 ) ), "Failed to set Opus complexity" );
                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_SET_INBAND_FEC( 1 ) ), "Failed to enable Opus FEC" );
                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_SET_PACKET_LOSS_PERC( 5 ) ),
                           "Failed to set Opus packet-loss hint" );
                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_SET_DTX( 0 ) ), "Failed to configure Opus DTX" );
            }

            ~OpusEncoder() override {
                if ( m_Encoder != nullptr ) {
                    opus_encoder_destroy( m_Encoder );
                }
            }

            std::size_t Encode( const PcmFrame& frame, std::span<std::byte> output ) override {
                const int encoded = opus_encode_float( m_Encoder,
                                                       frame.samples.data(),
                                                       static_cast<int>( SamplesPerFrame ),
                                                       reinterpret_cast<unsigned char*>( output.data() ),
                                                       static_cast<opus_int32>( output.size() ) );
                CheckOpus( encoded, "Failed to encode Opus audio" );
                return static_cast<std::size_t>( encoded );
            }

            void Reset() override {
                CheckOpus( opus_encoder_ctl( m_Encoder, OPUS_RESET_STATE ), "Failed to reset Opus encoder" );
            }

          private:
            ::OpusEncoder* m_Encoder = nullptr;
        };

        class OpusDecoder final: public Decoder {
          public:
            OpusDecoder() {
                int error = OPUS_OK;
                m_Decoder = opus_decoder_create( static_cast<opus_int32>( SampleRate ), static_cast<int>( Channels ), &error );
                CheckOpus( error, "Failed to create Opus decoder" );
            }

            ~OpusDecoder() override {
                if ( m_Decoder != nullptr ) {
                    opus_decoder_destroy( m_Decoder );
                }
            }

            std::size_t Decode( std::span<const std::byte> input,
                                DecodedAudio& output,
                                DecodeMode mode,
                                std::size_t recoverySamples ) override {
                const bool recovering = mode != DecodeMode::Normal;
                if ( recovering && ( recoverySamples == 0 || recoverySamples > output.samples.size() ) ) {
                    throw std::runtime_error( "Invalid Opus recovery duration" );
                }
                if ( mode == DecodeMode::ForwardErrorCorrection && input.empty() ) {
                    throw std::runtime_error( "Opus FEC requires the following packet" );
                }

                const bool packetLost = mode == DecodeMode::PacketLoss;
                const unsigned char* data =
                    packetLost || input.empty() ? nullptr : reinterpret_cast<const unsigned char*>( input.data() );
                const opus_int32 size = packetLost || input.empty() ? 0 : static_cast<opus_int32>( input.size() );
                const std::size_t capacity = recovering ? recoverySamples : output.samples.size();
                const int decodeFec = mode == DecodeMode::ForwardErrorCorrection ? 1 : 0;

                const int samples =
                    opus_decode_float( m_Decoder, data, size, output.samples.data(), static_cast<int>( capacity ), decodeFec );

                CheckOpus( samples, "Failed to decode Opus audio" );

                if ( samples <= 0 ) {
                    output.sampleCount = 0;
                    return 0;
                }

                output.sampleCount = static_cast<std::size_t>( samples );

                if ( recovering && output.sampleCount != recoverySamples ) {
                    throw std::runtime_error( "Opus recovery returned an unexpected duration" );
                }

                return output.sampleCount;
            }

            void Reset() override {
                CheckOpus( opus_decoder_ctl( m_Decoder, OPUS_RESET_STATE ), "Failed to reset Opus decoder" );
            }

          private:
            ::OpusDecoder* m_Decoder = nullptr;
        };

    } // namespace

    std::unique_ptr<Encoder> CreateEncoder() {
        return std::make_unique<OpusEncoder>();
    }

    std::unique_ptr<Decoder> CreateDecoder() {
        return std::make_unique<OpusDecoder>();
    }

} // namespace ts::audio
