#if EDITOR
#if PLATFORM_WINDOWS

#include "TerminalProcess_Windows.h"

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

// MSVC: be defensive about /W3 + /WX. The Windows headers and the std::thread
// member-pointer constructors can trip a number of warnings depending on the
// SDK version; suppress the common ones here so this file always compiles.
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

    static std::string QuoteArg(const std::string& s)
    {
        // MSVCRT command-line parsing rules: a sequence of backslashes followed
        // by a quote needs the backslashes doubled and the quote escaped. We
        // always wrap each token in quotes for simplicity.
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

    static void CloseHandleIfValid(HANDLE& h)
    {
        if (h != nullptr && h != INVALID_HANDLE_VALUE)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }
}

TerminalProcess_Windows::TerminalProcess_Windows() {}

TerminalProcess_Windows::~TerminalProcess_Windows()
{
    ForceKill();
    Join();
    CloseAllHandles();
}

std::string TerminalProcess_Windows::BuildCommandLine(const std::string& exe,
                                                      const std::vector<std::string>& args)
{
    std::string cmd = QuoteArg(exe);
    for (size_t i = 0; i < args.size(); ++i)
    {
        cmd.push_back(' ');
        cmd.append(QuoteArg(args[i]));
    }
    return cmd;
}

bool TerminalProcess_Windows::Start(const TerminalLaunchConfig& cfg, std::string& outError)
{
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);

    if (mRunning.load())
    {
        outError = "Process is already running.";
        return false;
    }

    mStopRequested.store(false);
    mExitCode.store(0);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdoutR = nullptr;
    HANDLE stdoutW = nullptr;
    HANDLE stderrR = nullptr;
    HANDLE stderrW = nullptr;
    HANDLE stdinR  = nullptr;
    HANDLE stdinW  = nullptr;

    if (!CreatePipe(&stdoutR, &stdoutW, &sa, 0))
    {
        outError = "CreatePipe(stdout) failed.";
        return false;
    }
    if (!CreatePipe(&stderrR, &stderrW, &sa, 0))
    {
        outError = "CreatePipe(stderr) failed.";
        CloseHandleIfValid(stdoutR);
        CloseHandleIfValid(stdoutW);
        return false;
    }
    if (!CreatePipe(&stdinR, &stdinW, &sa, 0))
    {
        outError = "CreatePipe(stdin) failed.";
        CloseHandleIfValid(stdoutR);
        CloseHandleIfValid(stdoutW);
        CloseHandleIfValid(stderrR);
        CloseHandleIfValid(stderrW);
        return false;
    }

    // Parent ends must NOT be inheritable so the child gets only its own ends.
    SetHandleInformation(stdoutR, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrR, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdinW,  HANDLE_FLAG_INHERIT, 0);

    // Create the Job Object before spawning so we can attach immediately.
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job == nullptr)
    {
        outError = "CreateJobObject failed.";
        CloseHandleIfValid(stdoutR);
        CloseHandleIfValid(stdoutW);
        CloseHandleIfValid(stderrR);
        CloseHandleIfValid(stderrW);
        CloseHandleIfValid(stdinR);
        CloseHandleIfValid(stdinW);
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
    ZeroMemory(&jeli, sizeof(jeli));
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
    {
        outError = "SetInformationJobObject failed.";
        CloseHandle(job);
        CloseHandleIfValid(stdoutR);
        CloseHandleIfValid(stdoutW);
        CloseHandleIfValid(stderrR);
        CloseHandleIfValid(stderrW);
        CloseHandleIfValid(stdinR);
        CloseHandleIfValid(stdinW);
        return false;
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdoutW;
    si.hStdError = stderrW;
    si.hStdInput = stdinR;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = BuildCommandLine(cfg.mExecutable, cfg.mArgs);
    std::string cmdLineMutable = cmdLine;  // CreateProcessA wants a writable buffer

    // Build environment block (additive over inherited env). Format is a
    // double-null-terminated KEY=VALUE\0KEY=VALUE\0\0 buffer.
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

    DWORD createFlags = CREATE_NO_WINDOW | CREATE_SUSPENDED;

    BOOL ok = CreateProcessA(
        nullptr,
        const_cast<char*>(cmdLineMutable.c_str()),
        nullptr, nullptr,
        TRUE,
        createFlags,
        useEnvBlock ? const_cast<char*>(envBlock.data()) : nullptr,
        cfg.mWorkingDir.empty() ? nullptr : cfg.mWorkingDir.c_str(),
        &si,
        &pi);

    if (!ok)
    {
        DWORD err = GetLastError();
        outError = "CreateProcess failed (error " + std::to_string(err) + ") for: " + cmdLine;
        CloseHandle(job);
        CloseHandleIfValid(stdoutR);
        CloseHandleIfValid(stdoutW);
        CloseHandleIfValid(stderrR);
        CloseHandleIfValid(stderrW);
        CloseHandleIfValid(stdinR);
        CloseHandleIfValid(stdinW);
        return false;
    }

    // Attach to job before resuming so descendants are also captured.
    if (!AssignProcessToJobObject(job, pi.hProcess))
    {
        // Most likely cause: the editor itself is in a job that doesn't allow
        // nested jobs. Carry on without tree cleanup — better than failing.
        DWORD assignErr = GetLastError();
        LogWarning("[CLI] AssignProcessToJobObject failed (error %lu); tree cleanup disabled.", assignErr);
        CloseHandle(job);
        job = nullptr;
    }

    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    // Close child ends in the parent.
    CloseHandle(stdoutW);
    CloseHandle(stderrW);
    CloseHandle(stdinR);

    mProcess = pi.hProcess;
    mProcessId = pi.dwProcessId;
    mJob = job;
    mStdoutRead  = stdoutR;
    mStdoutWrite = nullptr;
    mStderrRead  = stderrR;
    mStderrWrite = nullptr;
    mStdinRead   = nullptr;
    mStdinWrite  = stdinW;

    mRunning.store(true);

    mStdoutReader = std::thread(&TerminalProcess_Windows::ReaderLoop, this, mStdoutRead, TerminalEntryKind::Stdout);
    mStderrReader = std::thread(&TerminalProcess_Windows::ReaderLoop, this, mStderrRead, TerminalEntryKind::Stderr);
    mWaitThread   = std::thread(&TerminalProcess_Windows::WaitLoop, this);

    LogDebug("[CLI] Started PID %lu: %s", mProcessId, cmdLine.c_str());
    return true;
}

void TerminalProcess_Windows::ReaderLoop(void* pipeHandle, TerminalEntryKind kind)
{
    char buffer[kReadBufferSize];
    DWORD bytesRead = 0;

    for (;;)
    {
        BOOL ok = ReadFile(static_cast<HANDLE>(pipeHandle), buffer, kReadBufferSize, &bytesRead, nullptr);
        if (!ok || bytesRead == 0)
        {
            DWORD err = GetLastError();
            if (err != ERROR_BROKEN_PIPE && err != ERROR_OPERATION_ABORTED && err != 0)
            {
                LogDebug("[CLI] Reader exit (kind=%d) error=%lu", static_cast<int>(kind), err);
            }
            return;
        }

        if (mOnOutput)
        {
            mOnOutput(kind, std::string(buffer, static_cast<size_t>(bytesRead)));
        }
    }
}

void TerminalProcess_Windows::WaitLoop()
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

bool TerminalProcess_Windows::WriteStdin(const char* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mStdinWrite == nullptr)
    {
        return false;
    }

    DWORD toWrite = static_cast<DWORD>(len);
    DWORD written = 0;
    BOOL ok = WriteFile(static_cast<HANDLE>(mStdinWrite), data, toWrite, &written, nullptr);
    if (!ok || written != toWrite)
    {
        return false;
    }
    return true;
}

void TerminalProcess_Windows::RequestStop()
{
    mStopRequested.store(true);

    // Closing the parent's stdin write end sends EOF to the child. Most
    // shells exit on EOF, which is the cooperative shutdown path.
    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mStdinWrite != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mStdinWrite));
        mStdinWrite = nullptr;
    }
}

void TerminalProcess_Windows::ForceKill()
{
    if (!mRunning.load() && mProcess == nullptr)
    {
        return;
    }

    if (mJob != nullptr)
    {
        TerminateJobObject(static_cast<HANDLE>(mJob), 1);
    }
    else if (mProcess != nullptr)
    {
        TerminateProcess(static_cast<HANDLE>(mProcess), 1);
    }
}

void TerminalProcess_Windows::Join()
{
    if (mStdoutReader.joinable()) mStdoutReader.join();
    if (mStderrReader.joinable()) mStderrReader.join();
    if (mWaitThread.joinable())   mWaitThread.join();
}

void TerminalProcess_Windows::CloseAllHandles()
{
    if (mStdoutRead != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mStdoutRead));
        mStdoutRead = nullptr;
    }
    if (mStderrRead != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mStderrRead));
        mStderrRead = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(mStdinMutex);
        if (mStdinWrite != nullptr)
        {
            CloseHandle(static_cast<HANDLE>(mStdinWrite));
            mStdinWrite = nullptr;
        }
    }
    if (mProcess != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mProcess));
        mProcess = nullptr;
    }
    if (mJob != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(mJob));
        mJob = nullptr;
    }
    mProcessId = 0;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // PLATFORM_WINDOWS
#endif // EDITOR
