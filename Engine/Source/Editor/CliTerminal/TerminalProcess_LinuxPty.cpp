#if EDITOR
#if PLATFORM_LINUX

#include "TerminalProcess_LinuxPty.h"

#include "TerminalParserRegistry.h"

#include "Log.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pty.h>          // forkpty (glibc / libutil)
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern char** environ;

namespace
{
    constexpr size_t kReadBufferSize = 4096;
    constexpr unsigned short kDefaultCols = 120;
    constexpr unsigned short kDefaultRows = 30;
}

TerminalProcess_LinuxPty::TerminalProcess_LinuxPty() = default;

TerminalProcess_LinuxPty::~TerminalProcess_LinuxPty()
{
    ForceKill();
    Join();
    CloseFds();
}

bool TerminalProcess_LinuxPty::Start(const TerminalLaunchConfig& cfg, std::string& outError)
{
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);

    if (mRunning.load())
    {
        outError = "Process is already running.";
        return false;
    }

    mStopRequested.store(false);
    mExitCode.store(0);

    // Pick the parser via the registry. Auto-detection looks at cfg.mExecutable
    // (e.g. `claude` -> ClaudeCodeParser); falls back to the default ANSI
    // stripper for everything else.
    mParser = TerminalParserRegistry::Get().CreateParserFor(cfg);
    if (mParser)
    {
        mParser->Reset();
        LogDebug("[CLI] LinuxPty using parser \"%s\"", mParser->GetName());
    }

    // Initial window size for the PTY. Apps that respect SIGWINCH will
    // adjust their layout if we ResizePseudoConsole-equivalent later.
    struct winsize ws;
    ws.ws_row = kDefaultRows;
    ws.ws_col = kDefaultCols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    int masterFd = -1;
    pid_t pid = forkpty(&masterFd, nullptr, nullptr, &ws);
    if (pid == -1)
    {
        outError = std::string("forkpty failed: ") + std::strerror(errno);
        return false;
    }

    if (pid == 0)
    {
        // ---- Child ----
        // forkpty has already called setsid() and made the slave PTY our
        // controlling terminal, with stdio wired to it.

        // Apply working directory.
        if (!cfg.mWorkingDir.empty())
        {
            if (chdir(cfg.mWorkingDir.c_str()) != 0)
            {
                // Soft failure; keep going from inherited cwd.
            }
        }

        // Apply additive env vars.
        for (const auto& kv : cfg.mEnv)
        {
            if (!kv.first.empty())
            {
                setenv(kv.first.c_str(), kv.second.c_str(), 1);
            }
        }

        // Tell the child it's running under a real terminal so colour /
        // line-editing libraries opt in. Don't overwrite if the user set
        // it themselves via the env var editor.
        setenv("TERM", "xterm-256color", 0);

        // Build argv: [exe, args..., nullptr]
        std::vector<char*> argv;
        argv.reserve(cfg.mArgs.size() + 2);
        argv.push_back(const_cast<char*>(cfg.mExecutable.c_str()));
        for (const std::string& a : cfg.mArgs)
        {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        execvp(cfg.mExecutable.c_str(), argv.data());

        // Exec failed.
        const char* msg = std::strerror(errno);
        write(STDERR_FILENO, "execvp failed: ", 15);
        write(STDERR_FILENO, msg, std::strlen(msg));
        write(STDERR_FILENO, "\n", 1);
        _exit(127);
    }

    // ---- Parent ----
    mPid = pid;
    mPtyFd = masterFd;

    // Master fd in non-blocking mode so the polled reader can sleep
    // between iterations and check the stop flag.
    int flags = fcntl(mPtyFd, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(mPtyFd, F_SETFL, flags | O_NONBLOCK);
    }

    mRunning.store(true);

    mReader     = std::thread(&TerminalProcess_LinuxPty::ReaderLoop, this);
    mWaitThread = std::thread(&TerminalProcess_LinuxPty::WaitLoop, this);

    LogDebug("[CLI] Started PTY PID %d: %s", static_cast<int>(mPid), cfg.mExecutable.c_str());
    return true;
}

void TerminalProcess_LinuxPty::ReaderLoop()
{
    LogDebug("[CLI] LinuxPty ReaderLoop entry");

    char buffer[kReadBufferSize];
    int reads = 0;
    unsigned long totalBytes = 0;

    while (!mStopRequested.load())
    {
        ssize_t n = read(mPtyFd, buffer, sizeof(buffer));
        if (n > 0)
        {
            ++reads;
            totalBytes += static_cast<unsigned long>(n);

            std::string clean = mParser
                ? mParser->Process(buffer, static_cast<size_t>(n))
                : std::string(buffer, static_cast<size_t>(n));

            if (reads <= 3)
            {
                char preview[33];
                size_t previewLen = static_cast<size_t>(n) < 32 ? static_cast<size_t>(n) : 32;
                for (size_t i = 0; i < previewLen; ++i)
                {
                    unsigned char b = static_cast<unsigned char>(buffer[i]);
                    preview[i] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
                }
                preview[previewLen] = '\0';
                LogDebug("[CLI] LinuxPty read #%d: raw=%zd cleaned=%zu preview=\"%s\"",
                         reads, n, clean.size(), preview);
            }

            if (!clean.empty() && mOnOutput)
            {
                mOnOutput(TerminalEntryKind::Stdout, std::move(clean));
            }
            continue;
        }

        if (n == 0)
        {
            // EOF: child closed its end of the PTY (typically via exit).
            LogDebug("[CLI] LinuxPty reader EOF after %d reads (%lu bytes)", reads, totalBytes);
            return;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // No data right now; sleep briefly so we don't busy-spin.
            usleep(10 * 1000);  // 10 ms
            continue;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EIO)
        {
            // Linux returns EIO on the master fd when the slave side is
            // closed (i.e. the child has exited). Treat as EOF.
            LogDebug("[CLI] LinuxPty reader EIO after %d reads (%lu bytes) - child exited", reads, totalBytes);
            return;
        }

        // Unexpected error.
        LogDebug("[CLI] LinuxPty reader error after %d reads (%lu bytes); errno=%d (%s)",
                 reads, totalBytes, errno, std::strerror(errno));
        return;
    }

    LogDebug("[CLI] LinuxPty reader exit (stop requested) after %d reads (%lu bytes)",
             reads, totalBytes);
}

void TerminalProcess_LinuxPty::WaitLoop()
{
    if (mPid > 0)
    {
        int status = 0;
        pid_t r;
        do
        {
            r = waitpid(mPid, &status, 0);
        } while (r == -1 && errno == EINTR);

        if (r == mPid)
        {
            if (WIFEXITED(status))
            {
                mExitCode.store(WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                mExitCode.store(128 + WTERMSIG(status));
            }
        }
    }

    mRunning.store(false);
    if (mOnExit)
    {
        mOnExit(mExitCode.load());
    }
}

bool TerminalProcess_LinuxPty::WriteStdin(const char* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mPtyFd < 0)
    {
        return false;
    }

    size_t total = 0;
    while (total < len)
    {
        ssize_t n = write(mPtyFd, data + total, len - total);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // PTY input buffer is full; wait briefly and retry.
                usleep(1000);
                continue;
            }
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

void TerminalProcess_LinuxPty::RequestStop()
{
    mStopRequested.store(true);
    // Don't close mPtyFd here — the reader thread is using it. The reader
    // checks mStopRequested between iterations and exits cleanly.
    // For cooperative shutdown, the session layer also writes "exit\n" /
    // "exit\r" before calling RequestStop, which most shells process and
    // then exit naturally.
}

void TerminalProcess_LinuxPty::ForceKill()
{
    if (!mRunning.load() && mPid <= 0)
    {
        return;
    }

    LogDebug("[CLI] LinuxPty ForceKill");

    mStopRequested.store(true);

    if (mPid > 0)
    {
        // forkpty calls setsid() in the child, so the child is its own
        // process group leader. killpg reaps the entire group (any
        // grandchildren the user spawned interactively).
        killpg(mPid, SIGTERM);
        usleep(100 * 1000);  // 100 ms grace period
        killpg(mPid, SIGKILL);
    }
}

void TerminalProcess_LinuxPty::Join()
{
    if (mReader.joinable())     mReader.join();
    if (mWaitThread.joinable()) mWaitThread.join();
}

void TerminalProcess_LinuxPty::CloseFds()
{
    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mPtyFd >= 0)
    {
        close(mPtyFd);
        mPtyFd = -1;
    }
    mPid = -1;
}

#endif // PLATFORM_LINUX
#endif // EDITOR
