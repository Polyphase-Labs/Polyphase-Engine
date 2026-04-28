#if EDITOR
#if PLATFORM_LINUX

#include "TerminalProcess_Posix.h"

#include "Log.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace
{
    constexpr size_t kReadBufferSize = 4096;

    void CloseFd(int& fd)
    {
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }
}

TerminalProcess_Posix::TerminalProcess_Posix() = default;

TerminalProcess_Posix::~TerminalProcess_Posix()
{
    ForceKill();
    Join();
    CloseAllFds();
}

bool TerminalProcess_Posix::Start(const TerminalLaunchConfig& cfg, std::string& outError)
{
    std::lock_guard<std::mutex> lifecycleLock(mLifecycleMutex);

    if (mRunning.load())
    {
        outError = "Process is already running.";
        return false;
    }

    mStopRequested.store(false);
    mExitCode.store(0);

    if (pipe(mStdoutPipe) == -1)
    {
        outError = std::string("pipe(stdout) failed: ") + std::strerror(errno);
        return false;
    }
    if (pipe(mStderrPipe) == -1)
    {
        outError = std::string("pipe(stderr) failed: ") + std::strerror(errno);
        CloseAllFds();
        return false;
    }
    if (pipe(mStdinPipe) == -1)
    {
        outError = std::string("pipe(stdin) failed: ") + std::strerror(errno);
        CloseAllFds();
        return false;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        outError = std::string("fork failed: ") + std::strerror(errno);
        CloseAllFds();
        return false;
    }

    if (pid == 0)
    {
        // ---- Child ----
        // Detach into our own process group so killpg can reap the whole tree.
        setpgid(0, 0);

        // Wire stdio.
        dup2(mStdinPipe[0], STDIN_FILENO);
        dup2(mStdoutPipe[1], STDOUT_FILENO);
        dup2(mStderrPipe[1], STDERR_FILENO);

        // Close every pipe end we no longer need.
        close(mStdinPipe[0]); close(mStdinPipe[1]);
        close(mStdoutPipe[0]); close(mStdoutPipe[1]);
        close(mStderrPipe[0]); close(mStderrPipe[1]);

        // Apply working directory.
        if (!cfg.mWorkingDir.empty())
        {
            if (chdir(cfg.mWorkingDir.c_str()) != 0)
            {
                // Soft failure: keep going from inherited cwd.
            }
        }

        // Apply additive env vars (parent env is already inherited).
        for (const auto& kv : cfg.mEnv)
        {
            if (!kv.first.empty())
            {
                setenv(kv.first.c_str(), kv.second.c_str(), 1);
            }
        }

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
    // Set child's pgid defensively to avoid the race where the parent runs
    // setpgid before the child does. EACCES is fine (child already exec'd).
    setpgid(pid, pid);
    mPgid = pid;

    // Close child ends of the pipes in the parent.
    CloseFd(mStdinPipe[0]);
    CloseFd(mStdoutPipe[1]);
    CloseFd(mStderrPipe[1]);

    mRunning.store(true);

    mStdoutReader = std::thread(&TerminalProcess_Posix::ReaderLoop, this, mStdoutPipe[0], TerminalEntryKind::Stdout);
    mStderrReader = std::thread(&TerminalProcess_Posix::ReaderLoop, this, mStderrPipe[0], TerminalEntryKind::Stderr);
    mWaitThread   = std::thread(&TerminalProcess_Posix::WaitLoop, this);

    LogDebug("[CLI] Started PID %d: %s", static_cast<int>(mPid), cfg.mExecutable.c_str());
    return true;
}

void TerminalProcess_Posix::ReaderLoop(int fd, TerminalEntryKind kind)
{
    char buffer[kReadBufferSize];
    while (true)
    {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0)
        {
            if (mOnOutput)
            {
                mOnOutput(kind, std::string(buffer, static_cast<size_t>(n)));
            }
            continue;
        }
        if (n == 0)
        {
            // EOF — child closed its end.
            return;
        }
        if (errno == EINTR)
        {
            continue;
        }
        // Read error: bail.
        return;
    }
}

void TerminalProcess_Posix::WaitLoop()
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

bool TerminalProcess_Posix::WriteStdin(const char* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return true;
    }

    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mStdinPipe[1] < 0)
    {
        return false;
    }

    size_t total = 0;
    while (total < len)
    {
        ssize_t n = write(mStdinPipe[1], data + total, len - total);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

void TerminalProcess_Posix::RequestStop()
{
    mStopRequested.store(true);

    // Closing the parent's stdin write end sends EOF to the child.
    std::lock_guard<std::mutex> lock(mStdinMutex);
    if (mStdinPipe[1] >= 0)
    {
        close(mStdinPipe[1]);
        mStdinPipe[1] = -1;
    }
}

void TerminalProcess_Posix::ForceKill()
{
    if (!mRunning.load() && mPid <= 0)
    {
        return;
    }

    if (mPgid > 0)
    {
        // SIGTERM the group, brief grace period, then SIGKILL the group.
        killpg(mPgid, SIGTERM);
        usleep(100000);
        killpg(mPgid, SIGKILL);
    }
    else if (mPid > 0)
    {
        kill(mPid, SIGKILL);
    }
}

void TerminalProcess_Posix::Join()
{
    if (mStdoutReader.joinable()) mStdoutReader.join();
    if (mStderrReader.joinable()) mStderrReader.join();
    if (mWaitThread.joinable())   mWaitThread.join();
}

void TerminalProcess_Posix::CloseAllFds()
{
    CloseFd(mStdoutPipe[0]); CloseFd(mStdoutPipe[1]);
    CloseFd(mStderrPipe[0]); CloseFd(mStderrPipe[1]);
    {
        std::lock_guard<std::mutex> lock(mStdinMutex);
        CloseFd(mStdinPipe[0]); CloseFd(mStdinPipe[1]);
    }
    mPid = -1;
    mPgid = -1;
}

#endif // PLATFORM_LINUX
#endif // EDITOR
