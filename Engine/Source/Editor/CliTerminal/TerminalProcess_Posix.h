#pragma once

#if EDITOR
#if PLATFORM_LINUX

#include "ITerminalProcess.h"

#include <atomic>
#include <mutex>
#include <sys/types.h>
#include <thread>

/**
 * @brief POSIX backend for ITerminalProcess.
 *
 * Uses pipe(2) + fork() + execvp(). Child calls setpgid(0,0) right after
 * fork; ForceKill calls killpg(-pgid, SIGKILL) so the entire process group
 * is reaped together. A dedicated wait thread blocks on waitpid for symmetry
 * with the Windows backend.
 */
class TerminalProcess_Posix : public ITerminalProcess
{
public:
    TerminalProcess_Posix();
    ~TerminalProcess_Posix() override;

    bool Start(const TerminalLaunchConfig& cfg, std::string& outError) override;
    bool WriteStdin(const char* data, size_t len) override;
    void RequestStop() override;
    void ForceKill() override;
    bool IsRunning() const override { return mRunning.load(); }
    int  GetExitCode() const override { return mExitCode.load(); }
    void Join() override;
    bool IsTty() const override { return false; }

private:
    void ReaderLoop(int fd, TerminalEntryKind kind);
    void WaitLoop();
    void CloseAllFds();

    pid_t mPid = -1;
    pid_t mPgid = -1;

    int mStdoutPipe[2] = { -1, -1 };
    int mStderrPipe[2] = { -1, -1 };
    int mStdinPipe[2]  = { -1, -1 };

    std::thread mStdoutReader;
    std::thread mStderrReader;
    std::thread mWaitThread;

    std::atomic<bool> mRunning{ false };
    std::atomic<bool> mStopRequested{ false };
    std::atomic<int>  mExitCode{ 0 };

    std::mutex mStdinMutex;
    std::mutex mLifecycleMutex;
};

#endif // PLATFORM_LINUX
#endif // EDITOR
