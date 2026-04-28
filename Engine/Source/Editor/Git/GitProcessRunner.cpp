#if EDITOR

#include "GitProcessRunner.h"
#include "Log.h"

#if PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <cstring>
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool IsCancelled(const GitCancelToken& token)
{
    return token && token->load(std::memory_order_acquire);
}

static void DrainBuffer(std::string& buffer, const std::function<void(const std::string&)>& callback)
{
    if (!callback)
    {
        buffer.clear();
        return;
    }

    size_t pos = 0;
    while (pos < buffer.size())
    {
        size_t nl = buffer.find('\n', pos);
        if (nl == std::string::npos)
        {
            break;
        }

        size_t lineEnd = nl;
        if (lineEnd > pos && buffer[lineEnd - 1] == '\r')
        {
            --lineEnd;
        }

        callback(buffer.substr(pos, lineEnd - pos));
        pos = nl + 1;
    }

    if (pos > 0)
    {
        buffer.erase(0, pos);
    }
}

static void FlushBuffer(std::string& buffer, const std::function<void(const std::string&)>& callback)
{
    if (!buffer.empty() && callback)
    {
        // Remove trailing \r if present
        if (!buffer.empty() && buffer.back() == '\r')
        {
            buffer.pop_back();
        }
        if (!buffer.empty())
        {
            callback(buffer);
        }
        buffer.clear();
    }
}

// ---------------------------------------------------------------------------
// Windows implementation
// ---------------------------------------------------------------------------

#if PLATFORM_WINDOWS

static std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty())
    {
        return {};
    }
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], sizeNeeded);
    return result;
}

static std::string BuildCommandLine(const std::vector<std::string>& args)
{
    std::string cmdLine;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            cmdLine += ' ';
        }

        const std::string& arg = args[i];
        bool needsQuoting = arg.empty() || arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos;
        if (needsQuoting)
        {
            cmdLine += '"';
            for (size_t j = 0; j < arg.size(); ++j)
            {
                if (arg[j] == '"')
                {
                    cmdLine += "\\\"";
                }
                else if (arg[j] == '\\' && j + 1 < arg.size() && arg[j + 1] == '"')
                {
                    cmdLine += "\\\\\"";
                    ++j;
                }
                else
                {
                    cmdLine += arg[j];
                }
            }
            cmdLine += '"';
        }
        else
        {
            cmdLine += arg;
        }
    }
    return cmdLine;
}

int32_t GitProcessRunner::Run(
    const std::string& workDir,
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> onStdoutLine,
    std::function<void(const std::string&)> onStderrLine,
    GitCancelToken cancelToken)
{
    // Allow credential helpers (GCM, SSH agent, etc.) to prompt the user.
    // Do NOT set GIT_TERMINAL_PROMPT=0 — that blocks GUI credential popups too.

    // Create pipes for stdout
    HANDLE hStdoutRead = nullptr;
    HANDLE hStdoutWrite = nullptr;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0))
    {
        LogError("GitProcessRunner: Failed to create stdout pipe (error %lu)", GetLastError());
        return -1;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // Create pipes for stderr
    HANDLE hStderrRead = nullptr;
    HANDLE hStderrWrite = nullptr;
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0))
    {
        LogError("GitProcessRunner: Failed to create stderr pipe (error %lu)", GetLastError());
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return -1;
    }
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    // Set up the startup info
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    std::string cmdLineUtf8 = BuildCommandLine(args);
    std::wstring cmdLineW = Utf8ToWide(cmdLineUtf8);
    std::wstring workDirW = Utf8ToWide(workDir);

    LogDebug("GitProcessRunner: %s", cmdLineUtf8.c_str());

    BOOL created = CreateProcessW(
        nullptr,
        &cmdLineW[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDirW.empty() ? nullptr : workDirW.c_str(),
        &si,
        &pi);

    // Close write ends of the pipes in the parent so reads will EOF
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    if (!created)
    {
        LogError("GitProcessRunner: CreateProcessW failed (error %lu)", GetLastError());
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);
        return -1;
    }

    // Read stdout and stderr via non-blocking PeekNamedPipe + ReadFile
    std::string stdoutBuffer;
    std::string stderrBuffer;
    char readBuf[4096];
    bool stdoutOpen = true;
    bool stderrOpen = true;

    while (stdoutOpen || stderrOpen)
    {
        if (IsCancelled(cancelToken))
        {
            LogDebug("GitProcessRunner: Cancelled, terminating process");
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);
            return -1;
        }

        bool didRead = false;

        // Peek / read stdout
        if (stdoutOpen)
        {
            DWORD avail = 0;
            if (PeekNamedPipe(hStdoutRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
            {
                DWORD bytesRead = 0;
                DWORD toRead = (avail < sizeof(readBuf)) ? avail : (DWORD)sizeof(readBuf);
                if (ReadFile(hStdoutRead, readBuf, toRead, &bytesRead, nullptr) && bytesRead > 0)
                {
                    stdoutBuffer.append(readBuf, bytesRead);
                    DrainBuffer(stdoutBuffer, onStdoutLine);
                    didRead = true;
                }
            }
            else
            {
                // PeekNamedPipe returns false when the pipe is broken (writer closed)
                DWORD peekErr = GetLastError();
                if (peekErr == ERROR_BROKEN_PIPE || avail == 0)
                {
                    // Try a blocking read to detect closure
                    DWORD bytesRead = 0;
                    if (avail == 0 && PeekNamedPipe(hStdoutRead, nullptr, 0, nullptr, &avail, nullptr))
                    {
                        // Pipe still open but nothing available
                    }
                    else
                    {
                        stdoutOpen = false;
                    }
                }
            }
        }

        // Peek / read stderr
        if (stderrOpen)
        {
            DWORD avail = 0;
            if (PeekNamedPipe(hStderrRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
            {
                DWORD bytesRead = 0;
                DWORD toRead = (avail < sizeof(readBuf)) ? avail : (DWORD)sizeof(readBuf);
                if (ReadFile(hStderrRead, readBuf, toRead, &bytesRead, nullptr) && bytesRead > 0)
                {
                    stderrBuffer.append(readBuf, bytesRead);
                    DrainBuffer(stderrBuffer, onStderrLine);
                    didRead = true;
                }
            }
            else
            {
                DWORD peekErr = GetLastError();
                if (peekErr == ERROR_BROKEN_PIPE || avail == 0)
                {
                    if (avail == 0 && PeekNamedPipe(hStderrRead, nullptr, 0, nullptr, &avail, nullptr))
                    {
                        // Pipe still open but nothing available
                    }
                    else
                    {
                        stderrOpen = false;
                    }
                }
            }
        }

        if (!didRead)
        {
            // Check if the process is still alive; if not, do a final drain and break
            DWORD waitResult = WaitForSingleObject(pi.hProcess, 10);
            if (waitResult == WAIT_OBJECT_0)
            {
                // Process exited. Do a final drain of both pipes.
                while (true)
                {
                    DWORD bytesRead = 0;
                    if (!ReadFile(hStdoutRead, readBuf, sizeof(readBuf), &bytesRead, nullptr) || bytesRead == 0)
                    {
                        break;
                    }
                    stdoutBuffer.append(readBuf, bytesRead);
                    DrainBuffer(stdoutBuffer, onStdoutLine);
                }

                while (true)
                {
                    DWORD bytesRead = 0;
                    if (!ReadFile(hStderrRead, readBuf, sizeof(readBuf), &bytesRead, nullptr) || bytesRead == 0)
                    {
                        break;
                    }
                    stderrBuffer.append(readBuf, bytesRead);
                    DrainBuffer(stderrBuffer, onStderrLine);
                }
                break;
            }
        }
    }

    // Flush any remaining partial lines
    FlushBuffer(stdoutBuffer, onStdoutLine);
    FlushBuffer(stderrBuffer, onStderrLine);

    // Wait for process to finish (should already be done)
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);

    return static_cast<int32_t>(exitCode);
}

// ---------------------------------------------------------------------------
// POSIX implementation
// ---------------------------------------------------------------------------

#else

int32_t GitProcessRunner::Run(
    const std::string& workDir,
    const std::vector<std::string>& args,
    std::function<void(const std::string&)> onStdoutLine,
    std::function<void(const std::string&)> onStderrLine,
    GitCancelToken cancelToken)
{
    if (args.empty())
    {
        LogError("GitProcessRunner: No arguments provided");
        return -1;
    }

    // Create pipes for stdout and stderr
    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};

    if (pipe(stdoutPipe) != 0)
    {
        LogError("GitProcessRunner: Failed to create stdout pipe");
        return -1;
    }
    if (pipe(stderrPipe) != 0)
    {
        LogError("GitProcessRunner: Failed to create stderr pipe");
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return -1;
    }

    // Build argv for execvp
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    std::string cmdLine;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            cmdLine += ' ';
        }
        cmdLine += args[i];
    }
    LogDebug("GitProcessRunner: %s", cmdLine.c_str());

    pid_t pid = fork();
    if (pid < 0)
    {
        LogError("GitProcessRunner: fork() failed");
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return -1;
    }

    if (pid == 0)
    {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        if (!workDir.empty())
        {
            if (chdir(workDir.c_str()) != 0)
            {
                _exit(127);
            }
        }

        // Allow credential helpers to prompt — do not suppress GIT_TERMINAL_PROMPT.

        execvp(argv[0], argv.data());
        _exit(127);
    }

    // Parent process -- close write ends
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    std::string stdoutBuffer;
    std::string stderrBuffer;
    char readBuf[4096];

    struct pollfd fds[2];
    fds[0].fd = stdoutPipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderrPipe[0];
    fds[1].events = POLLIN;

    int openCount = 2;
    while (openCount > 0)
    {
        if (IsCancelled(cancelToken))
        {
            LogDebug("GitProcessRunner: Cancelled, terminating process %d", (int)pid);
            kill(pid, SIGTERM);
            usleep(100000);

            int status = 0;
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == 0)
            {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
            }

            close(stdoutPipe[0]);
            close(stderrPipe[0]);
            return -1;
        }

        int ret = poll(fds, 2, 50);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }

        if (ret == 0)
        {
            continue;
        }

        // Read stdout
        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            ssize_t n = read(stdoutPipe[0], readBuf, sizeof(readBuf));
            if (n > 0)
            {
                stdoutBuffer.append(readBuf, n);
                DrainBuffer(stdoutBuffer, onStdoutLine);
            }
            else if (n == 0)
            {
                fds[0].fd = -1;
                --openCount;
            }
        }

        // Read stderr
        if (fds[1].revents & (POLLIN | POLLHUP))
        {
            ssize_t n = read(stderrPipe[0], readBuf, sizeof(readBuf));
            if (n > 0)
            {
                stderrBuffer.append(readBuf, n);
                DrainBuffer(stderrBuffer, onStderrLine);
            }
            else if (n == 0)
            {
                fds[1].fd = -1;
                --openCount;
            }
        }
    }

    // Flush remaining partial lines
    FlushBuffer(stdoutBuffer, onStdoutLine);
    FlushBuffer(stderrBuffer, onStderrLine);

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    // Reap child
    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
    {
        return static_cast<int32_t>(WEXITSTATUS(status));
    }

    return -1;
}

#endif // PLATFORM_WINDOWS

#endif // EDITOR
