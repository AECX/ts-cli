#define INITGUID

#include <algorithm>
#include <array>
#include <audio/audio_backend.hpp>
#include <audio/audio_types.hpp>
#include <charconv>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <iomanip>
#include <memory>
#include <mutex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// clang-format off
#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
// clang-format on

namespace ts::audio {

    namespace {

        /* Shared-mode buffer duration requested from WASAPI, in 100ns units. */
        constexpr REFERENCE_TIME BufferDuration = 200 * 10000;

        template<typename T>
        class ComPtr {
          public:
            ComPtr() = default;
            ComPtr( const ComPtr& ) = delete;
            ComPtr& operator=( const ComPtr& ) = delete;

            ComPtr( ComPtr&& other ) noexcept: pointer( other.Release() ) {
            }

            ComPtr& operator=( ComPtr&& other ) noexcept {
                if ( this != &other ) {
                    Reset();
                    pointer = other.Release();
                }
                return *this;
            }

            ~ComPtr() {
                Reset();
            }

            void Reset() {
                if ( pointer != nullptr ) {
                    pointer->Release();
                    pointer = nullptr;
                }
            }

            [[nodiscard]] T* Release() {
                T* result = pointer;
                pointer = nullptr;
                return result;
            }

            [[nodiscard]] T** AddressOf() {
                return &pointer;
            }

            [[nodiscard]] T* operator->() const {
                return pointer;
            }

            [[nodiscard]] explicit operator bool() const {
                return pointer != nullptr;
            }

            T* pointer = nullptr;
        };

        /*
         * A small cross-thread work queue: Post() from any thread, DrainAndRun()
         * from the owning thread once its paired Win32 event is signaled. Used
         * to marshal WASAPI/COM calls onto the one thread that owns them.
         */
        class TaskQueue {
          public:
            void Post( std::function<void()> task ) {
                {
                    std::scoped_lock lock( m_Mutex );
                    m_Tasks.push_back( std::move( task ) );
                }
                SetEvent( m_Event );
            }

            void DrainAndRun() {
                std::deque<std::function<void()>> tasks;
                {
                    std::scoped_lock lock( m_Mutex );
                    std::swap( tasks, m_Tasks );
                }
                for ( auto& task : tasks ) {
                    task();
                }
            }

            HANDLE m_Event = nullptr;

          private:
            std::mutex m_Mutex;
            std::deque<std::function<void()>> m_Tasks;
        };

        /* Posts `fn` onto `queue`'s owning thread and blocks for its result. */
        template<typename Fn>
        auto RunOnQueue( TaskQueue& queue, Fn&& fn ) {
            using Result = std::invoke_result_t<Fn>;

            auto promise = std::make_shared<std::promise<Result>>();
            std::future<Result> future = promise->get_future();

            queue.Post( [promise, fn = std::forward<Fn>( fn )]() mutable {
                try {
                    if constexpr ( std::is_void_v<Result> ) {
                        fn();
                        promise->set_value();
                    } else {
                        promise->set_value( fn() );
                    }
                } catch ( ... ) {
                    promise->set_exception( std::current_exception() );
                }
            } );

            return future.get();
        }

        [[nodiscard]] std::uint32_t FnvHash( std::wstring_view value ) {
            std::uint32_t hash = 2166136261U;

            for ( const wchar_t character : value ) {
                hash ^= static_cast<std::uint32_t>( character ) & 0xFFFFU;
                hash *= 16777619U;
            }

            return hash;
        }

        [[nodiscard]] std::string WideToUtf8( const wchar_t* value ) {
            if ( value == nullptr ) {
                return {};
            }

            const int length = ::WideCharToMultiByte( CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr );
            if ( length <= 1 ) {
                return {};
            }

            std::string result( static_cast<std::size_t>( length - 1 ), '\0' );
            ::WideCharToMultiByte( CP_UTF8, 0, value, -1, result.data(), length, nullptr, nullptr );
            return result;
        }

        [[nodiscard]] std::string FormatHresult( HRESULT hr ) {
            char buffer[256] {};

            const DWORD length = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                                   nullptr,
                                                   static_cast<DWORD>( hr ),
                                                   0,
                                                   buffer,
                                                   sizeof( buffer ),
                                                   nullptr );

            if ( length == 0 ) {
                std::ostringstream stream;
                stream << "HRESULT 0x" << std::hex << std::setw( 8 ) << std::setfill( '0' ) << static_cast<std::uint32_t>( hr );
                return stream.str();
            }

            std::string message( buffer, length );

            while ( !message.empty() && ( message.back() == '\n' || message.back() == '\r' ) ) {
                message.pop_back();
            }

            return message;
        }

        void ThrowIfFailed( HRESULT hr, const char* what ) {
            if ( FAILED( hr ) ) {
                throw std::runtime_error( std::string( what ) + ": " + FormatHresult( hr ) );
            }
        }

        [[nodiscard]] WAVEFORMATEXTENSIBLE BuildTargetFormat() {
            WAVEFORMATEXTENSIBLE format {};

            format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            format.Format.nChannels = static_cast<WORD>( Channels );
            format.Format.nSamplesPerSec = SampleRate;
            format.Format.wBitsPerSample = 32;
            format.Format.nBlockAlign = static_cast<WORD>( format.Format.nChannels * format.Format.wBitsPerSample / 8 );
            format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
            format.Format.cbSize = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
            format.Samples.wValidBitsPerSample = 32;
            format.dwChannelMask = SPEAKER_FRONT_CENTER;
            format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

            return format;
        }

        class WasapiBackend final: public AudioBackend {
          public:
            WasapiBackend() = default;

            ~WasapiBackend() override {
                Stop();
            }

            void Start( CaptureCallback capture, PlaybackCallback playback ) override {
                if ( m_Running ) {
                    throw std::runtime_error( "WASAPI audio backend is already running" );
                }

                m_CaptureCallback = std::move( capture );
                m_PlaybackCallback = std::move( playback );

                CreateSharedHandles();

                try {
                    m_ControlThread = std::thread( &WasapiBackend::ControlThreadMain, this );
                    m_CaptureThread = std::thread( &WasapiBackend::CaptureThreadMain, this );
                    m_RenderThread = std::thread( &WasapiBackend::RenderThreadMain, this );

                    RunOnQueue( m_ControlQueue, [this] {
                        InitializeOnControlThread();
                    } );
                } catch ( ... ) {
                    Stop();
                    throw;
                }

                m_Running = true;
            }

            void Stop() override {
                if ( !m_ControlThread.joinable() && !m_CaptureThread.joinable() && !m_RenderThread.joinable() ) {
                    return;
                }

                if ( m_StopEvent != nullptr ) {
                    SetEvent( m_StopEvent );
                }

                if ( m_ControlThread.joinable() ) {
                    m_ControlThread.join();
                }
                if ( m_CaptureThread.joinable() ) {
                    m_CaptureThread.join();
                }
                if ( m_RenderThread.joinable() ) {
                    m_RenderThread.join();
                }

                CloseSharedHandles();

                m_CaptureCallback = {};
                m_PlaybackCallback = {};
                m_Running = false;
            }

            std::vector<AudioDevice> Devices() const override {
                if ( !m_Running ) {
                    return {};
                }

                return RunOnQueue( m_ControlQueue, [this] {
                    return EnumerateDevicesOnControlThread();
                } );
            }

            void SetInputDevice( std::string_view selector ) override {
                SetDevice( DeviceKind::Input, selector );
            }

            void SetOutputDevice( std::string_view selector ) override {
                SetDevice( DeviceKind::Output, selector );
            }

          private:
            void SetDevice( DeviceKind kind, std::string_view selector ) {
                if ( !m_Running ) {
                    throw std::runtime_error( "WASAPI audio backend is not running" );
                }

                RunOnQueue( m_ControlQueue, [this, kind, selector = std::string( selector )] {
                    std::string& storedSelector = kind == DeviceKind::Input ? m_InputSelector : m_OutputSelector;
                    const std::string previousSelector = storedSelector;
                    TaskQueue& targetQueue = kind == DeviceKind::Input ? m_CaptureQueue : m_RenderQueue;

                    ComPtr<IMMDevice> device = ResolveDeviceOnControlThread( kind, selector );
                    IMMDevice* handoff = device.Release();
                    storedSelector = selector;

                    try {
                        RunOnQueue( targetQueue, [this, kind, handoff] {
                            RebuildStreamOnOwningThread( kind, handoff );
                        } );
                    } catch ( ... ) {
                        const std::exception_ptr failure = std::current_exception();
                        storedSelector = previousSelector;

                        try {
                            ComPtr<IMMDevice> previous = ResolveDeviceOnControlThread( kind, previousSelector );
                            IMMDevice* previousHandoff = previous.Release();

                            RunOnQueue( targetQueue, [this, kind, previousHandoff] {
                                RebuildStreamOnOwningThread( kind, previousHandoff );
                            } );
                        } catch ( ... ) {
                            /* The original device may have disappeared too. */
                        }

                        std::rethrow_exception( failure );
                    }
                } );
            }

            void RebuildStreamOnOwningThread( DeviceKind kind, IMMDevice* device ) {
                if ( kind == DeviceKind::Input ) {
                    RebuildCaptureOnCaptureThread( device );
                } else {
                    RebuildRenderOnRenderThread( device );
                }
            }

            void InitializeOnControlThread() {
                ThrowIfFailed( ::CoCreateInstance( CLSID_MMDeviceEnumerator,
                                                   nullptr,
                                                   CLSCTX_ALL,
                                                   __uuidof( IMMDeviceEnumerator ),
                                                   reinterpret_cast<void**>( &m_Enumerator ) ),
                               "Failed to create audio device enumerator" );

                m_InputSelector = "default";
                m_OutputSelector = "default";

                ComPtr<IMMDevice> inputDevice = ResolveDeviceOnControlThread( DeviceKind::Input, m_InputSelector );
                ComPtr<IMMDevice> outputDevice = ResolveDeviceOnControlThread( DeviceKind::Output, m_OutputSelector );

                IMMDevice* inputHandoff = inputDevice.Release();
                IMMDevice* outputHandoff = outputDevice.Release();

                RunOnQueue( m_CaptureQueue, [this, inputHandoff] {
                    RebuildCaptureOnCaptureThread( inputHandoff );
                } );
                RunOnQueue( m_RenderQueue, [this, outputHandoff] {
                    RebuildRenderOnRenderThread( outputHandoff );
                } );
            }

            [[nodiscard]] std::vector<AudioDevice> EnumerateDevicesOnControlThread() const {
                std::vector<AudioDevice> devices;

                for ( const EDataFlow flow : { eRender, eCapture } ) {
                    const DeviceKind kind = flow == eRender ? DeviceKind::Output : DeviceKind::Input;

                    ComPtr<IMMDeviceCollection> collection;
                    if ( FAILED( m_Enumerator->EnumAudioEndpoints( flow, DEVICE_STATE_ACTIVE, collection.AddressOf() ) ) ) {
                        continue;
                    }

                    UINT count = 0;
                    if ( FAILED( collection->GetCount( &count ) ) ) {
                        continue;
                    }

                    for ( UINT index = 0; index < count; ++index ) {
                        ComPtr<IMMDevice> device;
                        if ( FAILED( collection->Item( index, device.AddressOf() ) ) ) {
                            continue;
                        }

                        AudioDevice entry;
                        entry.kind = kind;

                        LPWSTR endpointId = nullptr;
                        if ( SUCCEEDED( device->GetId( &endpointId ) ) && endpointId != nullptr ) {
                            entry.id = FnvHash( endpointId );
                            ::CoTaskMemFree( endpointId );
                        }

                        ComPtr<IPropertyStore> properties;
                        if ( SUCCEEDED( device->OpenPropertyStore( STGM_READ, properties.AddressOf() ) ) ) {
                            PROPVARIANT value;
                            ::PropVariantInit( &value );

                            if ( SUCCEEDED( properties->GetValue( PKEY_Device_FriendlyName, &value ) ) &&
                                 value.vt == VT_LPWSTR ) {
                                entry.description = WideToUtf8( value.pwszVal );
                            }

                            ::PropVariantClear( &value );
                        }

                        entry.name = entry.description.empty() ? std::to_string( entry.id ) : entry.description;

                        devices.push_back( std::move( entry ) );
                    }
                }

                return devices;
            }

            [[nodiscard]] ComPtr<IMMDevice> ResolveDeviceOnControlThread( DeviceKind kind, std::string_view selector ) {
                const EDataFlow flow = kind == DeviceKind::Input ? eCapture : eRender;

                if ( selector == "default" ) {
                    ComPtr<IMMDevice> device;
                    ThrowIfFailed( m_Enumerator->GetDefaultAudioEndpoint( flow, eCommunications, device.AddressOf() ),
                                   "No default audio device is available" );
                    return device;
                }

                std::uint32_t id = 0;
                const char* first = selector.data();
                const char* last = first + selector.size();
                const auto parsed = std::from_chars( first, last, id );
                const bool isNumeric = parsed.ec == std::errc {} && parsed.ptr == last;

                ComPtr<IMMDeviceCollection> collection;
                ThrowIfFailed( m_Enumerator->EnumAudioEndpoints( flow, DEVICE_STATE_ACTIVE, collection.AddressOf() ),
                               "Failed to enumerate audio devices" );

                UINT count = 0;
                ThrowIfFailed( collection->GetCount( &count ), "Failed to count audio devices" );

                for ( UINT index = 0; index < count; ++index ) {
                    ComPtr<IMMDevice> candidate;
                    if ( FAILED( collection->Item( index, candidate.AddressOf() ) ) ) {
                        continue;
                    }

                    LPWSTR endpointId = nullptr;
                    std::uint32_t candidateId = 0;
                    if ( SUCCEEDED( candidate->GetId( &endpointId ) ) && endpointId != nullptr ) {
                        candidateId = FnvHash( endpointId );
                        ::CoTaskMemFree( endpointId );
                    }

                    if ( isNumeric ) {
                        if ( candidateId == id ) {
                            return candidate;
                        }
                        continue;
                    }

                    std::string description;
                    ComPtr<IPropertyStore> properties;
                    if ( SUCCEEDED( candidate->OpenPropertyStore( STGM_READ, properties.AddressOf() ) ) ) {
                        PROPVARIANT value;
                        ::PropVariantInit( &value );
                        if ( SUCCEEDED( properties->GetValue( PKEY_Device_FriendlyName, &value ) ) && value.vt == VT_LPWSTR ) {
                            description = WideToUtf8( value.pwszVal );
                        }
                        ::PropVariantClear( &value );
                    }

                    if ( description == selector ) {
                        return candidate;
                    }
                }

                throw std::runtime_error( "Audio device is not available: " + std::string( selector ) );
            }

            void RebuildCaptureOnCaptureThread( IMMDevice* rawDevice ) {
                ComPtr<IMMDevice> device;
                device.pointer = rawDevice;

                DestroyCaptureClientOnCaptureThread();

                ComPtr<IAudioClient> client;
                ThrowIfFailed( device->Activate( __uuidof( IAudioClient ),
                                                 CLSCTX_ALL,
                                                 nullptr,
                                                 reinterpret_cast<void**>( client.AddressOf() ) ),
                               "Failed to activate capture audio client" );

                const WAVEFORMATEXTENSIBLE format = BuildTargetFormat();
                ThrowIfFailed( client->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                                   AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                                       AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                                   BufferDuration,
                                                   0,
                                                   reinterpret_cast<const WAVEFORMATEX*>( &format ),
                                                   nullptr ),
                               "Failed to initialize capture audio client" );

                ThrowIfFailed( client->SetEventHandle( m_CaptureAudioEvent ), "Failed to set capture audio event handle" );

                ComPtr<IAudioCaptureClient> captureClient;
                ThrowIfFailed( client->GetService( __uuidof( IAudioCaptureClient ),
                                                   reinterpret_cast<void**>( captureClient.AddressOf() ) ),
                               "Failed to get capture audio client service" );

                ThrowIfFailed( client->Start(), "Failed to start capture audio client" );

                m_CaptureAudioClient = client.Release();
                m_CaptureClient = captureClient.Release();
            }

            void RebuildRenderOnRenderThread( IMMDevice* rawDevice ) {
                ComPtr<IMMDevice> device;
                device.pointer = rawDevice;

                DestroyRenderClientOnRenderThread();

                ComPtr<IAudioClient> client;
                ThrowIfFailed( device->Activate( __uuidof( IAudioClient ),
                                                 CLSCTX_ALL,
                                                 nullptr,
                                                 reinterpret_cast<void**>( client.AddressOf() ) ),
                               "Failed to activate render audio client" );

                const WAVEFORMATEXTENSIBLE format = BuildTargetFormat();
                ThrowIfFailed( client->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                                   AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                                       AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                                   BufferDuration,
                                                   0,
                                                   reinterpret_cast<const WAVEFORMATEX*>( &format ),
                                                   nullptr ),
                               "Failed to initialize render audio client" );

                ThrowIfFailed( client->SetEventHandle( m_RenderAudioEvent ), "Failed to set render audio event handle" );

                UINT32 bufferFrames = 0;
                ThrowIfFailed( client->GetBufferSize( &bufferFrames ), "Failed to get render buffer size" );

                ComPtr<IAudioRenderClient> renderClient;
                ThrowIfFailed(
                    client->GetService( __uuidof( IAudioRenderClient ), reinterpret_cast<void**>( renderClient.AddressOf() ) ),
                    "Failed to get render audio client service" );

                ThrowIfFailed( client->Start(), "Failed to start render audio client" );

                m_RenderAudioClient = client.Release();
                m_RenderClient = renderClient.Release();
                m_RenderBufferFrames = bufferFrames;
            }

            void DestroyCaptureClientOnCaptureThread() {
                if ( m_CaptureAudioClient != nullptr ) {
                    m_CaptureAudioClient->Stop();
                }

                if ( m_CaptureClient != nullptr ) {
                    m_CaptureClient->Release();
                    m_CaptureClient = nullptr;
                }

                if ( m_CaptureAudioClient != nullptr ) {
                    m_CaptureAudioClient->Release();
                    m_CaptureAudioClient = nullptr;
                }
            }

            void DestroyRenderClientOnRenderThread() {
                if ( m_RenderAudioClient != nullptr ) {
                    m_RenderAudioClient->Stop();
                }

                if ( m_RenderClient != nullptr ) {
                    m_RenderClient->Release();
                    m_RenderClient = nullptr;
                }

                if ( m_RenderAudioClient != nullptr ) {
                    m_RenderAudioClient->Release();
                    m_RenderAudioClient = nullptr;
                }

                m_RenderBufferFrames = 0;
            }

            void DrainCaptureOnCaptureThread() {
                if ( m_CaptureClient == nullptr ) {
                    return;
                }

                UINT32 packetLength = 0;

                while ( SUCCEEDED( m_CaptureClient->GetNextPacketSize( &packetLength ) ) && packetLength != 0 ) {
                    BYTE* data = nullptr;
                    UINT32 framesAvailable = 0;
                    DWORD flags = 0;

                    const HRESULT hr = m_CaptureClient->GetBuffer( &data, &framesAvailable, &flags, nullptr, nullptr );

                    if ( FAILED( hr ) ) {
                        break;
                    }

                    if ( framesAvailable != 0 && m_CaptureCallback ) {
                        if ( ( flags & AUDCLNT_BUFFERFLAGS_SILENT ) != 0 ) {
                            std::size_t remaining = framesAvailable;
                            while ( remaining != 0 ) {
                                const std::size_t chunk = std::min( remaining, m_CaptureSilenceBuffer.size() );
                                m_CaptureCallback( std::span<const float>( m_CaptureSilenceBuffer.data(), chunk ) );
                                remaining -= chunk;
                            }
                        } else {
                            const auto* samples = reinterpret_cast<const float*>( data );
                            m_CaptureCallback( std::span<const float>( samples, framesAvailable ) );
                        }
                    }

                    m_CaptureClient->ReleaseBuffer( framesAvailable );
                }
            }

            void DrainRenderOnRenderThread() {
                if ( m_RenderClient == nullptr || m_RenderAudioClient == nullptr ) {
                    return;
                }

                UINT32 padding = 0;
                if ( FAILED( m_RenderAudioClient->GetCurrentPadding( &padding ) ) ) {
                    return;
                }

                const UINT32 framesAvailable = m_RenderBufferFrames > padding ? m_RenderBufferFrames - padding : 0;
                if ( framesAvailable == 0 ) {
                    return;
                }

                BYTE* data = nullptr;
                if ( FAILED( m_RenderClient->GetBuffer( framesAvailable, &data ) ) ) {
                    return;
                }

                auto* samples = reinterpret_cast<float*>( data );

                if ( m_PlaybackCallback ) {
                    m_PlaybackCallback( std::span<float>( samples, framesAvailable ) );
                } else {
                    std::fill( samples, samples + framesAvailable, 0.0F );
                }

                m_RenderClient->ReleaseBuffer( framesAvailable, 0 );
            }

            void CreateSharedHandles() {
                m_StopEvent = ::CreateEventW( nullptr, TRUE, FALSE, nullptr );
                m_CaptureAudioEvent = ::CreateEventW( nullptr, FALSE, FALSE, nullptr );
                m_RenderAudioEvent = ::CreateEventW( nullptr, FALSE, FALSE, nullptr );
                m_ControlQueue.m_Event = ::CreateEventW( nullptr, FALSE, FALSE, nullptr );
                m_CaptureQueue.m_Event = ::CreateEventW( nullptr, FALSE, FALSE, nullptr );
                m_RenderQueue.m_Event = ::CreateEventW( nullptr, FALSE, FALSE, nullptr );

                if ( m_StopEvent == nullptr || m_CaptureAudioEvent == nullptr || m_RenderAudioEvent == nullptr ||
                     m_ControlQueue.m_Event == nullptr || m_CaptureQueue.m_Event == nullptr ||
                     m_RenderQueue.m_Event == nullptr ) {
                    CloseSharedHandles();
                    throw std::runtime_error( "Failed to create WASAPI synchronization events" );
                }
            }

            void CloseSharedHandles() {
                const auto close = []( HANDLE& handle ) {
                    if ( handle != nullptr ) {
                        ::CloseHandle( handle );
                        handle = nullptr;
                    }
                };

                close( m_StopEvent );
                close( m_CaptureAudioEvent );
                close( m_RenderAudioEvent );
                close( m_ControlQueue.m_Event );
                close( m_CaptureQueue.m_Event );
                close( m_RenderQueue.m_Event );
            }

            void ControlThreadMain() {
                const bool comInitialized = SUCCEEDED( ::CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );

                HANDLE handles[] = { m_StopEvent, m_ControlQueue.m_Event };

                while ( true ) {
                    const DWORD waitResult = ::WaitForMultipleObjects( 2, handles, FALSE, INFINITE );

                    if ( waitResult == WAIT_OBJECT_0 + 1 ) {
                        m_ControlQueue.DrainAndRun();
                        continue;
                    }

                    break;
                }

                if ( m_Enumerator != nullptr ) {
                    m_Enumerator->Release();
                    m_Enumerator = nullptr;
                }

                if ( comInitialized ) {
                    ::CoUninitialize();
                }
            }

            void CaptureThreadMain() {
                const bool comInitialized = SUCCEEDED( ::CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );

                DWORD mmcssTaskIndex = 0;
                HANDLE mmcssHandle = ::AvSetMmThreadCharacteristicsW( L"Pro Audio", &mmcssTaskIndex );

                HANDLE handles[] = { m_StopEvent, m_CaptureAudioEvent, m_CaptureQueue.m_Event };

                while ( true ) {
                    const DWORD waitResult = ::WaitForMultipleObjects( 3, handles, FALSE, INFINITE );

                    if ( waitResult == WAIT_OBJECT_0 + 1 ) {
                        DrainCaptureOnCaptureThread();
                        continue;
                    }

                    if ( waitResult == WAIT_OBJECT_0 + 2 ) {
                        m_CaptureQueue.DrainAndRun();
                        continue;
                    }

                    break;
                }

                DestroyCaptureClientOnCaptureThread();

                if ( mmcssHandle != nullptr ) {
                    ::AvRevertMmThreadCharacteristics( mmcssHandle );
                }

                if ( comInitialized ) {
                    ::CoUninitialize();
                }
            }

            void RenderThreadMain() {
                const bool comInitialized = SUCCEEDED( ::CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );

                DWORD mmcssTaskIndex = 0;
                HANDLE mmcssHandle = ::AvSetMmThreadCharacteristicsW( L"Pro Audio", &mmcssTaskIndex );

                HANDLE handles[] = { m_StopEvent, m_RenderAudioEvent, m_RenderQueue.m_Event };

                while ( true ) {
                    const DWORD waitResult = ::WaitForMultipleObjects( 3, handles, FALSE, INFINITE );

                    if ( waitResult == WAIT_OBJECT_0 + 1 ) {
                        DrainRenderOnRenderThread();
                        continue;
                    }

                    if ( waitResult == WAIT_OBJECT_0 + 2 ) {
                        m_RenderQueue.DrainAndRun();
                        continue;
                    }

                    break;
                }

                DestroyRenderClientOnRenderThread();

                if ( mmcssHandle != nullptr ) {
                    ::AvRevertMmThreadCharacteristics( mmcssHandle );
                }

                if ( comInitialized ) {
                    ::CoUninitialize();
                }
            }

            std::thread m_ControlThread;
            std::thread m_CaptureThread;
            std::thread m_RenderThread;

            HANDLE m_StopEvent = nullptr;
            HANDLE m_CaptureAudioEvent = nullptr;
            HANDLE m_RenderAudioEvent = nullptr;

            mutable TaskQueue m_ControlQueue;
            mutable TaskQueue m_CaptureQueue;
            mutable TaskQueue m_RenderQueue;

            bool m_Running = false;

            CaptureCallback m_CaptureCallback;
            PlaybackCallback m_PlaybackCallback;

            IMMDeviceEnumerator* m_Enumerator = nullptr;

            std::string m_InputSelector = "default";
            std::string m_OutputSelector = "default";

            IAudioClient* m_CaptureAudioClient = nullptr;
            IAudioCaptureClient* m_CaptureClient = nullptr;
            std::array<float, 1920> m_CaptureSilenceBuffer {};

            IAudioClient* m_RenderAudioClient = nullptr;
            IAudioRenderClient* m_RenderClient = nullptr;
            UINT32 m_RenderBufferFrames = 0;
        };

    } // namespace

    std::unique_ptr<AudioBackend> CreateAudioBackend() {
        return std::make_unique<WasapiBackend>();
    }

} // namespace ts::audio
