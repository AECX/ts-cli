#include <chrono>
#include <client/platform/console_io.hpp>
#include <condition_variable>
#include <cstdio>
#include <io.h>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <windows.h>

namespace ts::client::platform {

    bool StandardOutputIsTerminal() {
        return ::_isatty( ::_fileno( stdout ) ) != 0;
    }

    bool StandardInputIsTerminal() {
        return ::_isatty( ::_fileno( stdin ) ) != 0;
    }

    bool StandardErrorIsTerminal() {
        return ::_isatty( ::_fileno( stderr ) ) != 0;
    }

    void InitializeConsole() {
        SetConsoleOutputCP( CP_UTF8 );
        SetConsoleCP( CP_UTF8 );
    }

    namespace {

        /*
         * Runs the actual blocking std::getline on a dedicated thread and
         * hands completed lines to PollStandardInputLine, since waiting on
         * the console handle directly (WaitForSingleObject) signals on
         * every keystroke rather than only on completed lines. See the
         * rationale in console_io.hpp.
         */
        class StandardInputReader {
          public:
            static StandardInputReader& Instance() {
                static StandardInputReader instance;
                return instance;
            }

            StandardInputReader( const StandardInputReader& ) = delete;
            StandardInputReader& operator=( const StandardInputReader& ) = delete;

            StandardInputPoll Poll( std::chrono::milliseconds timeout ) {
                std::unique_lock lock( m_Mutex );

                const bool signaled = m_ConditionVariable.wait_for( lock, timeout, [this] {
                    return m_LineReady || m_Closed;
                } );

                if ( !signaled ) {
                    return {};
                }

                if ( m_LineReady ) {
                    StandardInputPoll result { .status = StandardInputStatus::Ready, .line = std::move( m_Line ) };
                    m_LineReady = false;
                    lock.unlock();
                    m_ConditionVariable.notify_one(); // let the reader thread read the next line
                    return result;
                }

                return { .status = StandardInputStatus::Closed, .line = {} };
            }

          private:
            StandardInputReader() {
                m_Thread = std::thread( [this] {
                    Run();
                } );
                m_Thread.detach();
            }

            ~StandardInputReader() = default;

            void Run() {
                std::string line;

                while ( std::getline( std::cin, line ) ) {
                    std::unique_lock lock( m_Mutex );
                    m_ConditionVariable.wait( lock, [this] {
                        return !m_LineReady;
                    } );
                    m_Line = std::move( line );
                    m_LineReady = true;
                    lock.unlock();
                    m_ConditionVariable.notify_one();
                }

                {
                    const std::lock_guard lock( m_Mutex );
                    m_Closed = true;
                }
                m_ConditionVariable.notify_one();
            }

            std::mutex m_Mutex;
            std::condition_variable m_ConditionVariable;
            std::string m_Line;
            bool m_LineReady = false;
            bool m_Closed = false;
            std::thread m_Thread;
        };

    } // namespace

    StandardInputPoll PollStandardInputLine( std::chrono::milliseconds timeout ) {
        return StandardInputReader::Instance().Poll( timeout );
    }

} // namespace ts::client::platform
