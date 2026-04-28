#pragma once

#if EDITOR
#if PLATFORM_LINUX

#include "ITerminalOutputParser.h"
#include "ITerminalProcess.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <thread>

/**
 * @brief Linux pseudo-terminal backend for ITerminalProcess.
 *
 * Uses forkpty() (from libutil, requires linking -lutil) so the child
 * process sees a real PTY on its stdio. This is the Linux counterpart to
 * the Windows ConPTY backend and is required for interactive CLIs (claude,
 * python -i, vim, htop, etc.) that refuse to start without a controlling
 * terminal.
 *
 * The master fd is set non-blocking and polled in a reader thread; output
 * is run through AnsiStripper just like the Windows ConPTY backend so the
 * existing log-line renderer can display it.
 */
class TerminalProcess_LinuxPty : public ITerminalProcess
{
public:
    TerminalProcess_LinuxPty();
    ~TerminalProcess_LinuxPty() override;

    bool Start(const TerminalLaunchConfig& cfg, std::string& outError) override;
    bool WriteStdin(const char* data, size_t len) override;
    void RequestStop() override;
    void ForceKill() override;
    bool IsRunning() const override { return mRunning.load(); }
    int  GetExitCode() const override { return mExitCode.load(); }
    void Join() override;
    bool IsTty() const override { return true; }

private:
    void ReaderLoop();
    void WaitLoop();
    void CloseFds();

    pid_t mPid = -1;
    int   mPtyFd = -1;     // master side of the PTY pair

    std::unique_ptr<ITerminalOutputParser> mParser;

    std::thread mReader;
    std::thread mWaitThread;

    std::atomic<bool> mRunning{ false };
    std::atomic<bool> mStopRequested{ false };
    std::atomic<int>  mExitCode{ 0 };

    std::mutex mStdinMutex;
    std::mutex mLifecycleMutex;
};

#endif // PLATFORM_LINUX
#endif // EDITOR
