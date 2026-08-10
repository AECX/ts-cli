#include <algorithm>
#include <array>
#include <audio/audio_backend.hpp>
#include <audio/audio_types.hpp>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ts::audio {

    namespace {

        class PipeWireBackend final: public AudioBackend {
          public:
            PipeWireBackend() = default;

            ~PipeWireBackend() override {
                Stop();
            }

            void Start( CaptureCallback capture, PlaybackCallback playback ) override {
                if ( m_Loop != nullptr ) {
                    throw std::runtime_error( "PipeWire audio backend is already running" );
                }

                m_CaptureCallback = std::move( capture );
                m_PlaybackCallback = std::move( playback );

                pw_init( nullptr, nullptr );

                m_Loop = pw_thread_loop_new( "ts-cli-audio", nullptr );
                if ( m_Loop == nullptr ) {
                    throw std::runtime_error( "Failed to create PipeWire thread loop" );
                }

                m_Context = pw_context_new( pw_thread_loop_get_loop( m_Loop ), nullptr, 0 );
                if ( m_Context == nullptr ) {
                    Stop();
                    throw std::runtime_error( "Failed to create PipeWire context" );
                }

                m_Core = pw_context_connect( m_Context, nullptr, 0 );
                if ( m_Core == nullptr ) {
                    Stop();
                    throw std::runtime_error( "Failed to connect to PipeWire" );
                }

                m_Registry = pw_core_get_registry( m_Core, PW_VERSION_REGISTRY, 0 );
                if ( m_Registry == nullptr ) {
                    Stop();
                    throw std::runtime_error( "Failed to get PipeWire registry" );
                }

                static const pw_registry_events RegistryEvents {
                    .version = PW_VERSION_REGISTRY_EVENTS,
                    .global = &PipeWireBackend::RegistryGlobal,
                    .global_remove = &PipeWireBackend::RegistryGlobalRemove,
                };

                pw_registry_add_listener( m_Registry, &m_RegistryListener, &RegistryEvents, this );

                if ( pw_thread_loop_start( m_Loop ) < 0 ) {
                    Stop();
                    throw std::runtime_error( "Failed to start PipeWire thread loop" );
                }

                m_Running = true;

                pw_thread_loop_lock( m_Loop );
                try {
                    RecreateCaptureStreamLocked();
                    RecreatePlaybackStreamLocked();
                } catch ( ... ) {
                    pw_thread_loop_unlock( m_Loop );
                    Stop();
                    throw;
                }
                pw_thread_loop_unlock( m_Loop );
            }

            void Stop() override {
                if ( m_Loop == nullptr ) {
                    return;
                }

                if ( m_Running ) {
                    pw_thread_loop_lock( m_Loop );
                    DestroyStreamsLocked();
                    pw_thread_loop_unlock( m_Loop );
                    pw_thread_loop_stop( m_Loop );
                    m_Running = false;
                } else {
                    DestroyStreamsLocked();
                }

                if ( m_Registry != nullptr ) {
                    spa_hook_remove( &m_RegistryListener );
                    m_Registry = nullptr;
                }

                if ( m_Core != nullptr ) {
                    pw_core_disconnect( m_Core );
                    m_Core = nullptr;
                }

                if ( m_Context != nullptr ) {
                    pw_context_destroy( m_Context );
                    m_Context = nullptr;
                }

                pw_thread_loop_destroy( m_Loop );
                m_Loop = nullptr;

                {
                    std::scoped_lock lock( m_DeviceMutex );
                    m_Devices.clear();
                }

                m_CaptureCallback = {};
                m_PlaybackCallback = {};
            }

            std::vector<AudioDevice> Devices() const override {
                std::scoped_lock lock( m_DeviceMutex );
                return m_Devices;
            }

            void SetInputDevice( std::string_view selector ) override {
                const Target target = ResolveTarget( DeviceKind::Input, selector );
                const std::string previousSelector = m_InputSelector;
                const Target previousTarget = m_InputTarget;

                m_InputSelector = std::string( selector );
                m_InputTarget = target;

                pw_thread_loop_lock( m_Loop );

                try {
                    RecreateCaptureStreamLocked();
                } catch ( ... ) {
                    const std::exception_ptr failure = std::current_exception();

                    m_InputSelector = previousSelector;
                    m_InputTarget = previousTarget;

                    try {
                        RecreateCaptureStreamLocked();
                    } catch ( ... ) {
                        /* The original device may have disappeared too. */
                    }

                    pw_thread_loop_unlock( m_Loop );
                    std::rethrow_exception( failure );
                }

                pw_thread_loop_unlock( m_Loop );
            }

            void SetOutputDevice( std::string_view selector ) override {
                const Target target = ResolveTarget( DeviceKind::Output, selector );
                const std::string previousSelector = m_OutputSelector;
                const Target previousTarget = m_OutputTarget;

                m_OutputSelector = std::string( selector );
                m_OutputTarget = target;

                pw_thread_loop_lock( m_Loop );

                try {
                    RecreatePlaybackStreamLocked();
                } catch ( ... ) {
                    const std::exception_ptr failure = std::current_exception();

                    m_OutputSelector = previousSelector;
                    m_OutputTarget = previousTarget;

                    try {
                        RecreatePlaybackStreamLocked();
                    } catch ( ... ) {
                        /* The original device may have disappeared too. */
                    }

                    pw_thread_loop_unlock( m_Loop );
                    std::rethrow_exception( failure );
                }

                pw_thread_loop_unlock( m_Loop );
            }

          private:
            struct Target {
                std::uint32_t id = PW_ID_ANY;
            };

            static void RegistryGlobal( void* data,
                                        std::uint32_t id,
                                        std::uint32_t permissions,
                                        const char* type,
                                        std::uint32_t version,
                                        const spa_dict* props ) {
                (void)permissions;
                (void)version;

                if ( data == nullptr || type == nullptr || props == nullptr ||
                     std::strcmp( type, PW_TYPE_INTERFACE_Node ) != 0 ) {
                    return;
                }

                const char* mediaClass = spa_dict_lookup( props, PW_KEY_MEDIA_CLASS );
                if ( mediaClass == nullptr ) {
                    return;
                }

                DeviceKind kind;
                if ( std::strcmp( mediaClass, "Audio/Source" ) == 0 ) {
                    kind = DeviceKind::Input;
                } else if ( std::strcmp( mediaClass, "Audio/Sink" ) == 0 ) {
                    kind = DeviceKind::Output;
                } else {
                    return;
                }

                const char* name = spa_dict_lookup( props, PW_KEY_NODE_NAME );
                const char* description = spa_dict_lookup( props, PW_KEY_NODE_DESCRIPTION );

                AudioDevice device;
                device.id = id;
                device.kind = kind;
                device.name = name == nullptr ? std::to_string( id ) : name;
                device.description = description == nullptr ? device.name : description;

                auto& self = *static_cast<PipeWireBackend*>( data );
                std::scoped_lock lock( self.m_DeviceMutex );

                const auto existing =
                    std::find_if( self.m_Devices.begin(), self.m_Devices.end(), [id]( const AudioDevice& value ) {
                        return value.id == id;
                    } );

                if ( existing == self.m_Devices.end() ) {
                    self.m_Devices.push_back( std::move( device ) );
                } else {
                    *existing = std::move( device );
                }
            }

            static void RegistryGlobalRemove( void* data, std::uint32_t id ) {
                if ( data == nullptr ) {
                    return;
                }

                auto& self = *static_cast<PipeWireBackend*>( data );
                std::scoped_lock lock( self.m_DeviceMutex );
                std::erase_if( self.m_Devices, [id]( const AudioDevice& device ) {
                    return device.id == id;
                } );
            }

            static void CaptureProcess( void* data ) {
                auto& self = *static_cast<PipeWireBackend*>( data );
                pw_buffer* buffer = pw_stream_dequeue_buffer( self.m_CaptureStream );

                if ( buffer == nullptr ) {
                    return;
                }

                spa_buffer* spaBuffer = buffer->buffer;

                if ( spaBuffer != nullptr && spaBuffer->n_datas != 0 ) {
                    spa_data& plane = spaBuffer->datas[0];

                    if ( plane.data != nullptr && plane.chunk != nullptr ) {
                        const std::uint32_t offset = std::min( plane.chunk->offset, plane.maxsize );
                        const std::uint32_t available = plane.maxsize - offset;
                        const std::uint32_t byteCount = std::min( plane.chunk->size, available );
                        const auto* bytes = static_cast<const std::uint8_t*>( plane.data ) + offset;
                        const auto* samples = reinterpret_cast<const float*>( bytes );
                        const std::size_t count = static_cast<std::size_t>( byteCount ) / sizeof( float );

                        if ( self.m_CaptureCallback && count != 0 ) {
                            self.m_CaptureCallback( std::span<const float>( samples, count ) );
                        }
                    }
                }

                (void)pw_stream_queue_buffer( self.m_CaptureStream, buffer );
            }

            static void PlaybackProcess( void* data ) {
                auto& self = *static_cast<PipeWireBackend*>( data );
                pw_buffer* buffer = pw_stream_dequeue_buffer( self.m_PlaybackStream );

                if ( buffer == nullptr ) {
                    return;
                }

                spa_buffer* spaBuffer = buffer->buffer;

                if ( spaBuffer != nullptr && spaBuffer->n_datas != 0 ) {
                    spa_data& plane = spaBuffer->datas[0];

                    if ( plane.data != nullptr && plane.chunk != nullptr ) {
                        const std::size_t maxSamples = static_cast<std::size_t>( plane.maxsize ) / sizeof( float );
                        const std::size_t requested =
                            buffer->requested == 0 ? maxSamples
                                                   : std::min( maxSamples, static_cast<std::size_t>( buffer->requested ) );
                        auto* samples = static_cast<float*>( plane.data );

                        if ( self.m_PlaybackCallback && requested != 0 ) {
                            self.m_PlaybackCallback( std::span<float>( samples, requested ) );
                        }

                        plane.chunk->offset = 0;
                        plane.chunk->stride = static_cast<std::int32_t>( sizeof( float ) );
                        plane.chunk->size = static_cast<std::uint32_t>( requested * sizeof( float ) );
                    }
                }

                (void)pw_stream_queue_buffer( self.m_PlaybackStream, buffer );
            }

            [[nodiscard]] Target ResolveTarget( DeviceKind kind, std::string_view selector ) const {
                if ( selector == "default" ) {
                    return Target {};
                }

                std::uint32_t id = 0;
                const char* first = selector.data();
                const char* last = first + selector.size();
                const auto parsed = std::from_chars( first, last, id );

                std::scoped_lock lock( m_DeviceMutex );

                if ( parsed.ec == std::errc {} && parsed.ptr == last ) {
                    const auto match =
                        std::find_if( m_Devices.begin(), m_Devices.end(), [id, kind]( const AudioDevice& device ) {
                            return device.id == id && device.kind == kind;
                        } );

                    if ( match == m_Devices.end() ) {
                        throw std::runtime_error( "Audio device id is not available" );
                    }

                    return Target { .id = id };
                }

                const auto match =
                    std::find_if( m_Devices.begin(), m_Devices.end(), [selector, kind]( const AudioDevice& device ) {
                        return device.kind == kind && ( device.name == selector || device.description == selector );
                    } );

                if ( match == m_Devices.end() ) {
                    throw std::runtime_error( "Audio device is not available: " + std::string( selector ) );
                }

                return Target { .id = match->id };
            }

            [[nodiscard]] const spa_pod* BuildFormat( spa_pod_builder& builder ) const {
                spa_audio_info_raw info {};
                info.format = SPA_AUDIO_FORMAT_F32;
                info.rate = SampleRate;
                info.channels = static_cast<std::uint32_t>( Channels );
                info.position[0] = SPA_AUDIO_CHANNEL_MONO;
                return spa_format_audio_raw_build( &builder, SPA_PARAM_EnumFormat, &info );
            }

            void RecreateCaptureStreamLocked() {
                DestroyCaptureStreamLocked();

                pw_properties* properties = pw_properties_new( PW_KEY_MEDIA_TYPE,
                                                               "Audio",
                                                               PW_KEY_MEDIA_CATEGORY,
                                                               "Capture",
                                                               PW_KEY_MEDIA_ROLE,
                                                               "Communication",
                                                               nullptr );

                m_CaptureStream = pw_stream_new( m_Core, "ts-cli-capture", properties );
                if ( m_CaptureStream == nullptr ) {
                    throw std::runtime_error( "Failed to create PipeWire capture stream" );
                }

                static const pw_stream_events Events = [] {
                    pw_stream_events events {};

                    events.version = PW_VERSION_STREAM_EVENTS;
                    events.process = &PipeWireBackend::CaptureProcess;

                    return events;
                }();
                pw_stream_add_listener( m_CaptureStream, &m_CaptureListener, &Events, this );

                std::array<std::uint8_t, 1024> storage {};
                spa_pod_builder builder = SPA_POD_BUILDER_INIT( storage.data(), static_cast<std::uint32_t>( storage.size() ) );
                const spa_pod* params[] = { BuildFormat( builder ) };

                const int result =
                    pw_stream_connect( m_CaptureStream,
                                       PW_DIRECTION_INPUT,
                                       m_InputTarget.id,
                                       static_cast<pw_stream_flags>( PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                                     PW_STREAM_FLAG_RT_PROCESS ),
                                       params,
                                       1 );

                if ( result < 0 ) {
                    DestroyCaptureStreamLocked();
                    throw std::runtime_error( "Failed to connect PipeWire capture stream" );
                }
            }

            void RecreatePlaybackStreamLocked() {
                DestroyPlaybackStreamLocked();

                pw_properties* properties = pw_properties_new( PW_KEY_MEDIA_TYPE,
                                                               "Audio",
                                                               PW_KEY_MEDIA_CATEGORY,
                                                               "Playback",
                                                               PW_KEY_MEDIA_ROLE,
                                                               "Communication",
                                                               nullptr );

                m_PlaybackStream = pw_stream_new( m_Core, "ts-cli-playback", properties );
                if ( m_PlaybackStream == nullptr ) {
                    throw std::runtime_error( "Failed to create PipeWire playback stream" );
                }

                static const pw_stream_events Events = [] {
                    pw_stream_events events {};

                    events.version = PW_VERSION_STREAM_EVENTS;
                    events.process = &PipeWireBackend::PlaybackProcess;

                    return events;
                }();
                pw_stream_add_listener( m_PlaybackStream, &m_PlaybackListener, &Events, this );

                std::array<std::uint8_t, 1024> storage {};
                spa_pod_builder builder = SPA_POD_BUILDER_INIT( storage.data(), static_cast<std::uint32_t>( storage.size() ) );
                const spa_pod* params[] = { BuildFormat( builder ) };

                const int result =
                    pw_stream_connect( m_PlaybackStream,
                                       PW_DIRECTION_OUTPUT,
                                       m_OutputTarget.id,
                                       static_cast<pw_stream_flags>( PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                                     PW_STREAM_FLAG_RT_PROCESS ),
                                       params,
                                       1 );

                if ( result < 0 ) {
                    DestroyPlaybackStreamLocked();
                    throw std::runtime_error( "Failed to connect PipeWire playback stream" );
                }
            }

            void DestroyCaptureStreamLocked() {
                if ( m_CaptureStream == nullptr ) {
                    return;
                }

                spa_hook_remove( &m_CaptureListener );
                m_CaptureListener = {};
                pw_stream_destroy( m_CaptureStream );
                m_CaptureStream = nullptr;
            }

            void DestroyPlaybackStreamLocked() {
                if ( m_PlaybackStream == nullptr ) {
                    return;
                }

                spa_hook_remove( &m_PlaybackListener );
                m_PlaybackListener = {};
                pw_stream_destroy( m_PlaybackStream );
                m_PlaybackStream = nullptr;
            }

            void DestroyStreamsLocked() {
                DestroyCaptureStreamLocked();
                DestroyPlaybackStreamLocked();
            }

            pw_thread_loop* m_Loop = nullptr;
            pw_context* m_Context = nullptr;
            pw_core* m_Core = nullptr;
            pw_registry* m_Registry = nullptr;
            pw_stream* m_CaptureStream = nullptr;
            pw_stream* m_PlaybackStream = nullptr;

            spa_hook m_RegistryListener {};
            spa_hook m_CaptureListener {};
            spa_hook m_PlaybackListener {};

            bool m_Running = false;
            CaptureCallback m_CaptureCallback;
            PlaybackCallback m_PlaybackCallback;

            mutable std::mutex m_DeviceMutex;
            std::vector<AudioDevice> m_Devices;

            std::string m_InputSelector = "default";
            std::string m_OutputSelector = "default";
            Target m_InputTarget;
            Target m_OutputTarget;
        };

    } // namespace

    std::unique_ptr<AudioBackend> CreateAudioBackend() {
        return std::make_unique<PipeWireBackend>();
    }

} // namespace ts::audio
