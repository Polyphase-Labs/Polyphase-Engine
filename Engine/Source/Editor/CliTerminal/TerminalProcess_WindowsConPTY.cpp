#if EDITOR
#if PLATFORM_WINDOWS

#include "TerminalProcess_WindowsConPTY.h"

#include "TerminalParserRegistry.h"

#include "Log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>
#include <string>
#include <utility>

// PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE may not be defined on older SDKs.
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

// HPCON is a typedef for VOID* on Windows 10 1809+ SDKs. If not present,
// declare it ourselves so this file compiles against older headers.
#if !defined(HPCON)
typedef VOID* HPCON;
#endif

// MSVC: be defensive about /W3 + /WX. Same set as the pipe-based backend.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4189) // local variable initialized but not referenced
#pragma warning(disable: 4244) // conversion, possible loss of data
#pragma warning(disable: 4267) // conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable: 4456) // declaration hides previous local declaration
#pragma warning(disable: 4458) // declaration hides class member
#pragma warning(disable: 4459) // declaration hides global declaration
#pragma warning(disable: 4505) // unreferenced local function has been removed
#pragma warning(disable: 4800) // forcing value to bool
#endif

namespace
{
    constexpr DWORD kReadBufferSize = 4096;
    constexpr SHORT kDefaultCols = 120;
    constexpr SHORT kDefaultRows = 30;

    // ---- ConPTY symbol resolution (dynamic, for SDK/OS compatibility) ----

    typedef HRESULT (WINAPI *CreatePseudoConsoleFn)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
    typedef VOID    (WINAPI *ClosePseudoConsoleFn)(HPCON);
    typedef HRESULT (WINAPI *ResizePseudoConsoleFn)(HPCON, COORD);

    static CreatePseudoConsoleFn sCreatePseudoConsole = nullptr;
    static ClosePseudoConsoleFn  sClosePseudoConsole  = nullptr;
    static ResizePseudoConsoleFn sResizePseudoConsole = nullptr;
    static bool sConPtyResolved = false;
    static bool sConPtyAvailable = false;

    static bool ResolveConPty()
    {
        if (sConPtyResolved)
        {
            return sConPtyAvailable;
        }
        sConPtyResolved = true;

        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        if (hKernel32 == nullptr)
        {
            return false;
        }

        sCreatePseudoConsole = reinterpret_cast<CreatePseudoConsoleFn>(
            GetProcAddress(hKernel32, "CreatePseudoConsole"));
        sClosePseudoConsole = reinterpret_cast<ClosePseudoConsoleFn>(
            GetProcAddress(hKernel32, "ClosePseudoConsole"));
        sResizePseudoConsole = reinterpret_cast<ResizePseudoConsoleFn>(
            GetProcAddress(hKernel32, "ResizePseudoConsole"));

        sConPtyAvailable = (sCreatePseudoConsole != nullptr &&
                            sClosePseudoConsole != nullptr);
        if (sConPtyAvailable)
        {
            LogDebug("[CLI] ConPTY available");
        }
        else
        {
            LogWarning("[CLI] ConPTY NOT available — Windows 10 1809+ required");
        }
        return sConPtyAvailable;
    }

    // ---- Command line builder (same as pipe backend) ----

    static std::string QuoteArg(const std::string& s)
    {
        std::string out;
        out.push_back('"');
        size_t backslashes = 0;
        for (size_t i = 0; i < s.size(); ++i)
        {
            char c = s[i];
            if (c == '\\')
            {
                ++backslashes;
                continue;
            }
            if (c == '"')
            {
                out.append(backslashes * 2 + 1, '\\');
                backslashes = 0;
                out.push_back('"');
                continue;
            }
            if (backslashes > 0)
            {
                out.append(backslashes, '\\');
                backslashes = 0;
            }
            out.push_back(c);
        }
        out.append(backslashes * 2, '\\');
        out.push_back('"');
        return out;
    }

    static std::string BuildCommandLine(const std::string& exe, const std::vector<std::string>& args)
    {
        std::string cmd = QuoteArg(exe);
        for (size_t i = 0; i < args.size(); ++i)
        {
            cmd.push_back(' ');
            cmd.append(QuoteArg(args[i]));
        }
        return cmd;
    }
}

// ---------------------------------------------------------------------------

bool TerminalProcess_WindowsConPTY::IsConPtyAvailable()
{
    return ResolveConPty();
}

TerminalProcess_WindowsConPTY::TerminalProcess_WindowsConPTY() {}

TerminalProcess_WindowsConPTY::~TerminalProcess_WindowsConPTY()
{
    ForceKill();
    Join();
    CloseAllHandles();
}

bool TerminalProcess_WindowsConPTY::Start(const TerminalLaunchConfig& cfg, std::string& outError)
{
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);

    if (mRunning.load())
    {
        outError = "Process is already running.";
        return false;
    }

    if (!ResolveConPty())
    {
        outError = "ConPTY is not available on this Windows version (requires 10 1809 or later).";
        return false;
    }

    mStopRequested.store(false);
    mExitCode.store(0);

    // Pick the parser via the registry. Auto-detection looks at cfg.mExecutable
    // (e.g. `claude.exe` -> ClaudeCodeParser); falls back to the default ANSI
    // stripper for everything else.
    mParser = TerminalParserRegistry::Get().CreateParserFor(cfg);
    if (mParser)
    {
        mParser->Reset();
        LogDebug("[CLI] ConPTY using parser \"%s\"", mParser->GetName());
    }

    // Create the two pipes that the pseudo-console will use:
    //   inputRead  <- ConPTY reads child stdin from here  (we write to inputWrite)
    //   outputWrite -> ConPTY writes child stdout here    (we read from outputRead)
    HANDLE inputRead   = nullptr;
    HANDLE inputWrite  = nullptr;
    HANDLE outputRead  = nullptr;
    HANDLE outputWrite = nullptr;

    if (!CreatePipe(&inputRead, &inputWrite, nullptr, 0))
    {
        outError = "CreatePipe(input) failed.";
        return false;
    }
    if (!CreatePipe(&outputRead, &outputWrite, nullptr, 0))
    {
        outError = "CreatePipe(output) failed.";
        CloseHandle(inputRead);
        CloseHandle(inputWrite);
        return false;
    }

    // Create the pseudo-console attached to the PTY-side of the pipes.
    COORD size;
    size.X = kDefaultCols;
    size.Y = kDefaultRows;

    HPCON hPC = nullptr;
    HRESULT hr = sCreatePseudoConsole(size, inputRead, outputWrite, 0, &hPC);
    if (FAILED(hr))
    {
        outError = "CreatePseudoConsole failed (HRESULT 0x" +
                   std::to_string(static_cast<unsigned long>(hr)) + ")";
        CloseHandle(inputRead);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        return false;
    }

    // Per the official ConPTY sample: after CreatePseudoConsole succeeds, the
    // PTY-side handles are dup'd into the conhost; close our copies now.
    CloseHandle(inputRead);
    inputRead = nullptr;
    CloseHandle(outputWrite);
    outputWrite = nullptr;

    // Build the PROC_THREAD_ATTRIBUTE_LIST that attaches the pseudo-console.
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    void* attrListMem = HeapAlloc(GetProcessHeap(), 0, attrListSize);
    if (attrListMem == nullptr)
    {
        outError = "HeapAlloc for proc thread attribute list failed.";
        sClosePseudoConsole(hPC);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return false;
    }

    LPPROC_THREAD_ATTRIBUTE_LIST attrList =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListMem);

    if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize))
    {
        outError = "InitializeProcThreadAttributeList failed.";
        HeapFree(GetProcessHeap(), 0, attrListMem);
        sClosePseudoConsole(hPC);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return false;
    }

    if (!UpdateProcThreadAttribute(attrList, 0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   hPC, sizeof(HPCON), nullptr, nullptr))
    {
        outError = "UpdateProcThreadAttribute failed.";
        DeleteProcThreadAttributeList(attrList);
        HeapFree(GetProcessHeap(), 0, attrListMem);
        sClosePseudoConsole(hPC);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return false;
    }

    STARTUPINFOEXA siEx;
    ZeroMemory(&siEx, sizeof(siEx));
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXA);
    siEx.lpAttributeList = attrList;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = BuildCommandLine(cfg.mExecutable, cfg.mArgs);
    std::string cmdLineMutable = cmdLine;

    // Build environment block (additive over inherited env).
    std::string envBlock;
    bool useEnvBlock = !cfg.mEnv.empty();
    if (useEnvBlock)
    {
        LPCH envStrings = GetEnvironmentStringsA();
        if (envStrings != nullptr)
        {
            const char* p = envStrings;
            while (*p != '\0')
            {
                size_t entryLen = std::strlen(p);
                envBlock.append(p, entryLen);
                envBlock.push_back('\0');
                p += entryLen + 1;
            }
            FreeEnvironmentStringsA(envStrings);
        }
        for (size_t i = 0; i < cfg.mEnv.size(); ++i)
        {
            const std::pair<std::string, std::string>& kv = cfg.mEnv[i];
            if (kv.first.empty())
            {
                continue;
            }
            envBlock.append(kv.first);
            envBlock.push_back('=');
            envBlock.append(kv.second);
            envBlock.push_back('\0');
        }
        envBlock.push_back('\0');
    }

    BOOL ok = CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLineMutable.c_str()),
        nullptr, nullptr,
        FALSE,                              // ConPTY does its own handle management
        EXTENDED_STARTUPINFO_PRESENT,
        useEnvBlock ? const_cast<char*>(envBlock.data()) : nullptr,
        cfg.mWorkingDir.empty() ? nullptr : cfg.mWorkingDir.c_str(),
        &siEx.StartupInfo,
        &pi);

    if (!ok)
    {
        DWORD err = GetLastError();
        outError = "CreateProcess failed (error " + std::to_string(err) + ") for: " + cmdLine;
        DeleteProcThreadAttributeList(attrList);
        HeapFree(GetProcessHeap(), 0, attrListMem);
        sClosePseudoConsole(hPC);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return false;
    }

    CloseHandle(pi.hThread);

    mProcess         = pi.hProcess;
    mProcessId       = pi.dwProcessId;
    mPseudoConsole   = hPC;
    mInputWrite      = inputWrite;
    mOutputRead      = outputRead;
    mAttributeListMem = attrListMem;

    mRunning.store(true);

    mReader     = std::thread(&TerminalProcess_WindowsConPTY::ReaderLoop, this);
    mWaitThread = std::thread(&TerminalProcess_WindowsConPTY::WaitLoop, this);

    LogDebug("[CLI] Started ConPTY PID %lu: %s", mProcessId, cmdLine.c_str());
    return true;
}

void TerminalProcess_WindowsConPTY::ReaderLoop()
{
    LogDebug("[CLI] ConPTY ReaderLoop entry");

    char buffer[kReadBufferSize];
    int reads = 0;
    unsigned long totalBytes = 0;
    int callbackInvocations = 0;

    // Polled read loop. Using PeekNamedPipe + ReadFile (rather than blocking
    // ReadFile) means we can check mStopRequested between iterations and
    // shut down cleanly without needing CancelSynchronousIo or closing the
    // read handle from another thread.
    while (!mStopRequested.load())
    {
        DWORD bytesAvailable = 0;
        BOOL peekOk = PeekNamedPipe(static_cast<HANDLE>(mOutputRead),
                                    nullptr, 0, nullptr,
                                    &bytesAvailable, nullptr);
        if (!peekOk)
        {
            DWORD err = GetLastError();
            LogDebug("[CLI] ConPTY reader exit after %d reads (%lu bytes total, %d callbacks); peek failed err=%lu",
                     reads, totalBytes, callbackInvocations, err);
            return;
        }

        if (bytesAvailable == 0)
        {
            // No data right now. Briefly sleep to avoid busy-spinning.
            Sleep(10);
            continue;
        }

        DWORD bytesRead = 0;
        BOOL ok = ReadFile(static_cast<HANDLE>(mOutputRead),
                           buffer, kReadBufferSize,
                           &bytesRead, nullptr);
        if (!ok || bytesRead == 0)
        {
            DWORD err = GetLastError();
            LogDebug("[CLI] ConPTY reader exit after %d reads (%lu bytes total, %d callbacks); read failed ok=%d err=%lu",
                     reads, totalBytes, callbackInvocations, ok ? 1 : 0, err);
            return;
        }

        ++reads;
        totalBytes += bytesRead;

        std::string clean = mParser
            ? mParser->Process(buffer, static_cast<size_t>(bytesRead))
            : std::string(buffer, static_cast<size_t>(bytesRead));
        size_t cleanedSize = clean.size();

        // Log the first few reads so we can confirm data is flowing.
        if (reads <= 3)
        {
            char preview[33];
            DWORD previewLen = bytesRead < 32 ? bytesRead : 32;
            for (DWORD i = 0; i < previewLen; ++i)
            {
                unsigned char b = static_cast<unsigned char>(buffer[i]);
                preview[i] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
            }
            preview[previewLen] = '\0';
            LogDebug("[CLI] ConPTY read #%d: raw=%lu cleaned=%zu preview=\"%s\"",
                     reads, bytesRead, cleanedSize, preview);
        }

        if (cleanedSize > 0)
        {
            if (mOnOutput)
            {
                ++callbackInvocations;
                if (callbackInvocations <= 3)
                {
                    LogDebug("[CLI] ConPTY -> mOnOutput call #%d size=%zu", callbackInvocations, cleanedSize);
                }
                mOnOutput(TerminalEntryKind::Stdout, std::move(clean));
            }
            else if (reads <= 3)
            {
                LogDebug("[CLI] ConPTY WARNING: mOnOutput is null, dropping %zu cleaned bytes", cleanedSize);
            }
        }
    }

    LogDebug("[CLI] ConPTY reader exit (stop requested) after %d reads (%lu bytes, %d callbacks)",
             reads, totalBytes, callbackInvocations);
}

void TerminalProcess_WindowsConPTY::WaitLoop()
{
    if (mProcess != nullptr)
    {
        WaitForSingleObject(static_cast<HANDLE>(mProcess), INFINITE);
        DWORD code = 0;
        if (GetExitCodeProcess(static_cast<HANDLE>(mProcess), &code))
        {
            mExitCode.store(static_cast<int>(code));
        }
    }
    mRunning.store(false);
    if (mOnExit)
    {
        mOnExit(mExitCode.load());
    }
}

bool TerminalProcess_WindowsConPTY::WriteStdin(const char* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mInputWrite == nullptr)
    {
        return false;
    }

    DWORD toWrite = static_cast<DWORD>(len);
    DWORD written = 0;
    BOOL ok = WriteFile(static_cast<HANDLE>(mInputWrite), data, toWrite, &written, nullptr);
    if (!ok || written != toWrite)
    {
        return false;
    }
    return true;
}

void TerminalProcess_WindowsConPTY::RequestStop()
{
    mStopRequested.store(true);

    // Closing the parent's stdin write end signals EOF to the child.
    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mInputWrite != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mInputWrite));
        mInputWrite = nullptr;
    }
}

void TerminalProcess_WindowsConPTY::ForceKill()
{
    if (!mRunning.load() && mProcess == nullptr)
    {
        return;
    }

    LogDebug("[CLI] ConPTY ForceKill");

    // 1. Tell the polled reader loop to stop. It will exit on its next
    //    iteration (within ~10ms).
    mStopRequested.store(true);

    // 2. Close the pseudo-console. This terminates the attached child process
    //    and closes conhost's reference to the output write end.
    if (mPseudoConsole != nullptr && sClosePseudoConsole != nullptr)
    {
        sClosePseudoConsole(static_cast<HPCON>(mPseudoConsole));
        mPseudoConsole = nullptr;
    }

    // 3. Hard kill the child process if ConPTY didn't already.
    if (mProcess != nullptr)
    {
        TerminateProcess(static_cast<HANDLE>(mProcess), 1);
    }
}

void TerminalProcess_WindowsConPTY::Join()
{
    if (mReader.joinable())     mReader.join();
    if (mWaitThread.joinable()) mWaitThread.join();
}

void TerminalProcess_WindowsConPTY::CloseAllHandles()
{
    if (mPseudoConsole != nullptr && sClosePseudoConsole != nullptr)
    {
        sClosePseudoConsole(static_cast<HPCON>(mPseudoConsole));
        mPseudoConsole = nullptr;
    }
    if (mOutputRead != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mOutputRead));
        mOutputRead = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(mStdinMutex);
        if (mInputWrite != nullptr)
        {
            CloseHandle(static_cast<HANDLE>(mInputWrite));
            mInputWrite = nullptr;
        }
    }
    if (mProcess != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mProcess));
        mProcess = nullptr;
    }
    if (mAttributeListMem != nullptr)
    {
        DeleteProcThreadAttributeList(
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(mAttributeListMem));
        HeapFree(GetProcessHeap(), 0, mAttributeListMem);
        mAttributeListMem = nullptr;
    }
    mProcessId = 0;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // PLATFORM_WINDOWS
#endif // EDITOR
