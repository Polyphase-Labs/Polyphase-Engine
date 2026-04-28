#pragma once

#if EDITOR
#if PLATFORM_WINDOWS

#include "ITerminalProcess.h"

#include <atomic>
#include <mutex>
#include <thread>

/**
 * @brief Windows backend for ITerminalProcess.
 *
 * Uses CreateProcessA + 3 anonymous pipes (stdout, stderr, stdin) and a
 * Job Object with KILL_ON_JOB_CLOSE so the entire child process tree is
 * cleaned up on ForceKill or editor crash.
 *
 * Win32 handle types are deliberately stored as void pointers and unsigned
 * long here so this header does NOT pull in windows.h; the implementation
 * file casts them as needed.
 */
class TerminalProcess_Windows : public ITerminalProcess
{
public:
    TerminalProcess_Windows();
    ~TerminalProcess_Windows() override;

    bool Start(const TerminalLaunchConfig& cfg, std::string& outError) override;
    bool WriteStdin(const char* data, size_t len) override;
    void RequestStop() override;
    void ForceKill() override;
    bool IsRunning() const override { return mRunning.load(); }
    int  GetExitCode() const override { return mExitCode.load(); }
    void Join() override;
    bool IsTty() const override { return false; }

private:
    void ReaderLoop(void* pipeHandle, TerminalEntryKind kind);
    void WaitLoop();
    void CloseAllHandles();

    /** Quote per MSVCRT command-line parsing rules so paths with spaces work. */
    static std::string BuildCommandLine(const std::string& exe,
                                        const std::vector<std::string>& args);

    void* mProcess = nullptr;       // HANDLE
    unsigned long mProcessId = 0;   // DWORD
    void* mJob = nullptr;           // HANDLE (Job Object)

    void* mStdoutRead = nullptr;
    void* mStdoutWrite = nullptr;
    void* mStderrRead = nullptr;
    void* mStderrWrite = nullptr;
    void* mStdinRead = nullptr;
    void* mStdinWrite = nullptr;

    std::thread mStdoutReader;
    std::thread mStderrReader;
    std::thread mWaitThread;

    std::atomic<bool> mRunning{ false };
    std::atomic<bool> mStopRequested{ false };
    std::atomic<int>  mExitCode{ 0 };

    std::mutex mStdinMutex;
    std::mutex mLifecycleMutex;  // protects Start/Join/CloseAllHandles
};

#endif // PLATFORM_WINDOWS
#endif // EDITOR
