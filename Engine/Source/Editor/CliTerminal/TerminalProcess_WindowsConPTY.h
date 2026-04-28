#pragma once

#if EDITOR
#if PLATFORM_WINDOWS

#include "ITerminalOutputParser.h"
#include "ITerminalProcess.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

/**
 * @brief Windows pseudo-console (ConPTY) backend for ITerminalProcess.
 *
 * Uses CreatePseudoConsole + STARTUPINFOEXA so that the child process sees
 * a real terminal on its stdio. Required for interactive CLIs (claude,
 * python -i, node, vim, etc.) that refuse to start when stdin is a plain
 * pipe.
 *
 * The ConPTY symbols (CreatePseudoConsole, ClosePseudoConsole,
 * ResizePseudoConsole) are resolved at runtime via GetProcAddress so this
 * file always compiles regardless of the SDK version. CreateTerminalProcessConPTY()
 * returns nullptr if the symbols can't be resolved (Windows < 10 1809),
 * letting the caller fall back to the pipe-based backend.
 *
 * Output stream is the child's combined stdout/stderr with terminal escape
 * sequences. AnsiStripper removes the escapes before delivering chunks to
 * the panel; full ANSI colour rendering is a future enhancement.
 *
 * Win32 handle types are stored as void* / unsigned long here so this
 * header does NOT pull in windows.h.
 */
class TerminalProcess_WindowsConPTY : public ITerminalProcess
{
public:
    TerminalProcess_WindowsConPTY();
    ~TerminalProcess_WindowsConPTY() override;

    bool Start(const TerminalLaunchConfig& cfg, std::string& outError) override;
    bool WriteStdin(const char* data, size_t len) override;
    void RequestStop() override;
    void ForceKill() override;
    bool IsRunning() const override { return mRunning.load(); }
    int  GetExitCode() const override { return mExitCode.load(); }
    void Join() override;
    bool IsTty() const override { return true; }

    /** Returns true if ConPTY is available on this OS (resolved lazily). */
    static bool IsConPtyAvailable();

private:
    void ReaderLoop();
    void WaitLoop();
    void CloseAllHandles();

    void* mProcess = nullptr;        // HANDLE
    unsigned long mProcessId = 0;    // DWORD
    void* mPseudoConsole = nullptr;  // HPCON

    void* mInputWrite = nullptr;     // we WRITE here -> ConPTY child stdin
    void* mOutputRead = nullptr;     // we READ here  <- ConPTY child stdout

    void* mAttributeListMem = nullptr;  // HeapAlloc'd PROC_THREAD_ATTRIBUTE_LIST*

    std::unique_ptr<ITerminalOutputParser> mParser;

    std::thread mReader;
    std::thread mWaitThread;

    std::atomic<bool> mRunning{ false };
    std::atomic<bool> mStopRequested{ false };
    std::atomic<int>  mExitCode{ 0 };

    std::mutex mStdinMutex;
    std::mutex mLifecycleMutex;
};

#endif // PLATFORM_WINDOWS
#endif // EDITOR
