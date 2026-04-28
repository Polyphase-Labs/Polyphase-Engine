#pragma once

#if EDITOR

#include "TerminalOutputBuffer.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Configuration passed to ITerminalProcess::Start.
 *
 * Resolved by CliModule::BuildLaunchConfig from user preferences.
 */
struct TerminalLaunchConfig
{
    std::string mExecutable;                                       // Absolute or PATH-resolvable path
    std::vector<std::string> mArgs;                                // Args (no quoting required)
    std::string mWorkingDir;                                       // Optional; empty = inherit parent
    std::vector<std::pair<std::string, std::string>> mEnv;         // Additive env vars
    int mGracefulShutdownMs = 2000;                                // Stop helper graceful timeout
};

/**
 * @brief Cross-platform external process contract for the CLI Terminal panel.
 *
 * Implementations spawn an external CLI shell, stream stdout/stderr through
 * mOnOutput callbacks, accept stdin writes, and support cooperative + forced
 * shutdown. ConPTY is intentionally out of scope for the MVP; a future
 * pseudo-console backend can implement this same interface without changing
 * any session/UI code.
 */
class ITerminalProcess
{
public:
    using OutputCallback = std::function<void(TerminalEntryKind kind, std::string text)>;
    using ExitCallback = std::function<void(int exitCode)>;

    virtual ~ITerminalProcess() = default;

    /** Spawn the process. Returns false on launch failure (outError populated). */
    virtual bool Start(const TerminalLaunchConfig& cfg, std::string& outError) = 0;

    /** Write bytes to the child's stdin. Returns false if the pipe is closed. */
    virtual bool WriteStdin(const char* data, size_t len) = 0;

    /** Cooperative stop: closes stdin (sends EOF) and marks cancellation. */
    virtual void RequestStop() = 0;

    /** Hard kill including process tree (Job Object on Windows, killpg on POSIX). */
    virtual void ForceKill() = 0;

    /** True between successful Start() and the wait thread observing exit. */
    virtual bool IsRunning() const = 0;

    /** Valid once IsRunning() returns false. */
    virtual int GetExitCode() const = 0;

    /** Joins reader and wait threads. Idempotent; safe to call repeatedly. */
    virtual void Join() = 0;

    /**
     * @brief True if the child sees a real terminal (TTY/PTY) on its stdio.
     *
     * The pipe-based backend returns false; the ConPTY backend returns true.
     * TerminalSession uses this to decide whether to synthesize a UserEcho
     * entry for typed commands (pipe mode: yes, because the child won't echo;
     * TTY mode: no, because the shell echoes input back via stdout itself).
     */
    virtual bool IsTty() const = 0;

    void SetOutputCallback(OutputCallback cb) { mOnOutput = std::move(cb); }
    void SetExitCallback(ExitCallback cb) { mOnExit = std::move(cb); }

protected:
    OutputCallback mOnOutput;
    ExitCallback mOnExit;
};

/** Factory: returns the pipe-based backend (no TTY) for the current platform. */
ITerminalProcess* CreateTerminalProcess();

/**
 * @brief Factory: returns the ConPTY backend on Windows.
 *
 * Returns nullptr on platforms without a pseudo-console implementation, or
 * if the running OS doesn't expose CreatePseudoConsole. Callers should fall
 * back to CreateTerminalProcess() in that case.
 */
ITerminalProcess* CreateTerminalProcessConPTY();

#endif
