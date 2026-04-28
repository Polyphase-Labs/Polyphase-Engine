#pragma once

#if EDITOR

#include "ITerminalProcess.h"
#include "TerminalOutputBuffer.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

/**
 * @brief Lifecycle states for a TerminalSession.
 *
 * All transitions happen on the UI thread (in Tick() or Start/Stop/Restart
 * handlers). Background reader/wait threads only post output and set the
 * mPendingExit flag.
 */
enum class TerminalSessionState
{
    Stopped,
    Starting,
    Running,
    Stopping,
    Error,
};

const char* TerminalSessionStateName(TerminalSessionState state);

/**
 * @brief Owns one external CLI process and its output buffer.
 *
 * Single instance per panel for the MVP. The Panel calls Tick() every frame
 * (drains pending output, advances the state machine), and Start/Stop/Send
 * from button handlers and the input box.
 */
class TerminalSession
{
public:
    TerminalSession();
    ~TerminalSession();

    /** UI thread: drain pending output and apply deferred state transitions. */
    void Tick();

    /** Stopped/Error → Starting → Running. Returns false on launch failure. */
    bool Start();

    /** Running → Stopping. Spawns the helper thread that runs the shutdown sequence. */
    void Stop();

    /** Stop, then Start once state hits Stopped. */
    void Restart();

    /** Clear committed output entries (does not touch the process). */
    void Clear();

    /** Append a UserEcho entry and write the line + LF to stdin. */
    bool SendCommand(const std::string& line);

    /**
     * @brief Send raw bytes to the child's stdin with no echo and no line
     * terminator. Use for TUI key sequences like arrow keys (`\x1b[A`),
     * Enter (`\r`), Ctrl+C (`\x03`), etc.
     */
    bool SendRaw(const char* data, size_t len);

    /** Synchronous teardown for editor shutdown — joins everything before returning. */
    void ShutdownSync();

    TerminalSessionState GetState() const { return mState.load(); }
    int GetLastExitCode() const { return mLastExitCode; }
    const std::string& GetExecutableLabel() const { return mExecutableLabel; }
    const std::string& GetLastError() const { return mLastError; }
    TerminalOutputBuffer& GetBuffer() { return mBuffer; }

    /** True when the running backend exposes a real TTY (e.g. ConPTY). */
    bool IsTty() const { return mProcess && mProcess->IsTty(); }

private:
    void EmitSystem(const std::string& msg);
    void OnProcessOutput(TerminalEntryKind kind, std::string text);
    void OnProcessExit(int exitCode);
    void RunStopSequence(int timeoutMs);
    void TransitionTo(TerminalSessionState newState);

    std::unique_ptr<ITerminalProcess> mProcess;
    TerminalOutputBuffer mBuffer;

    std::atomic<TerminalSessionState> mState{ TerminalSessionState::Stopped };
    std::atomic<bool> mPendingExit{ false };
    std::atomic<bool> mWantsRestartAfterStop{ false };

    std::string mLastError;
    std::string mExecutableLabel;
    int mLastExitCode = 0;

    std::thread mStopThread;
    std::mutex mStopThreadMutex;
};

#endif
