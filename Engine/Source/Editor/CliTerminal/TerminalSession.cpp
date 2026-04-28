#if EDITOR

#include "TerminalSession.h"

#include "Preferences/External/CliModule.h"
#include "Preferences/PreferencesManager.h"

#include "Clock.h"
#include "Engine.h"
#include "Log.h"

#include <chrono>
#include <thread>
#include <utility>

namespace
{
    float NowSeconds()
    {
        const Clock* clk = GetAppClock();
        return clk != nullptr ? static_cast<float>(clk->GetTime()) : 0.0f;
    }

    CliModule* FindCliModule()
    {
        PreferencesManager* mgr = PreferencesManager::Get();
        if (mgr == nullptr)
        {
            return nullptr;
        }
        return static_cast<CliModule*>(mgr->FindModule("External/CLI"));
    }
}

const char* TerminalSessionStateName(TerminalSessionState state)
{
    switch (state)
    {
        case TerminalSessionState::Stopped:  return "Stopped";
        case TerminalSessionState::Starting: return "Starting";
        case TerminalSessionState::Running:  return "Running";
        case TerminalSessionState::Stopping: return "Stopping";
        case TerminalSessionState::Error:    return "Error";
        default:                             return "?";
    }
}

TerminalSession::TerminalSession() = default;

TerminalSession::~TerminalSession()
{
    ShutdownSync();
}

void TerminalSession::Tick()
{
    mBuffer.Drain();

    if (mPendingExit.load())
    {
        mPendingExit.store(false);

        // Drain any remaining output that arrived between exit and now.
        mBuffer.Drain();

        TerminalSessionState prev = mState.load();
        bool failed = (mLastExitCode != 0);

        // Join the stop helper thread (if any) BEFORE resetting mProcess.
        // The helper thread holds a raw pointer into mProcess via `this`, so
        // destroying the ITerminalProcess instance while it's still running
        // would be a use-after-free.
        {
            std::lock_guard<std::mutex> lock(mStopThreadMutex);
            if (mStopThread.joinable())
            {
                mStopThread.join();
            }
        }

        // Tear down process object so the next Start gets a fresh one.
        if (mProcess)
        {
            mProcess->Join();
            mProcess.reset();
        }

        EmitSystem(std::string("Process exited with code ") + std::to_string(mLastExitCode));

        if (mWantsRestartAfterStop.load())
        {
            mWantsRestartAfterStop.store(false);
            TransitionTo(TerminalSessionState::Stopped);
            Start();
            return;
        }

        if (prev == TerminalSessionState::Stopping)
        {
            TransitionTo(TerminalSessionState::Stopped);
        }
        else
        {
            TransitionTo(failed ? TerminalSessionState::Error : TerminalSessionState::Stopped);
        }
    }
}

bool TerminalSession::Start()
{
    TerminalSessionState s = mState.load();
    if (s != TerminalSessionState::Stopped && s != TerminalSessionState::Error)
    {
        return false;
    }

    // Make sure any leftover stop helper has joined.
    {
        std::lock_guard<std::mutex> lock(mStopThreadMutex);
        if (mStopThread.joinable())
        {
            mStopThread.join();
        }
    }

    CliModule* cli = FindCliModule();
    if (cli == nullptr)
    {
        mLastError = "CLI preferences module not registered.";
        EmitSystem(mLastError);
        TransitionTo(TerminalSessionState::Error);
        return false;
    }

    if (!cli->mEnabled)
    {
        mLastError = "CLI Terminal is disabled in Preferences > External > CLI.";
        EmitSystem(mLastError);
        TransitionTo(TerminalSessionState::Error);
        return false;
    }

    TerminalLaunchConfig cfg = cli->BuildLaunchConfig();
    if (cfg.mExecutable.empty())
    {
        mLastError = "No shell resolved — check Preferences > External > CLI.";
        EmitSystem(mLastError);
        TransitionTo(TerminalSessionState::Error);
        return false;
    }

    TransitionTo(TerminalSessionState::Starting);

    // Pick a backend. ConPTY is required for interactive CLIs (claude, python,
    // node, etc.) that refuse to start when stdin is a plain pipe. The pipe
    // backend is the fallback for non-interactive use or for older Windows.
    ITerminalProcess* backend = nullptr;
    if (cli->mUseConPty)
    {
        backend = CreateTerminalProcessConPTY();
        if (backend == nullptr)
        {
            EmitSystem("ConPTY unavailable; falling back to pipe backend.");
        }
    }
    if (backend == nullptr)
    {
        backend = CreateTerminalProcess();
    }

    mProcess.reset(backend);
    if (!mProcess)
    {
        mLastError = "No terminal process implementation for this platform.";
        EmitSystem(mLastError);
        TransitionTo(TerminalSessionState::Error);
        return false;
    }

    mProcess->SetOutputCallback([this](TerminalEntryKind k, std::string t) {
        OnProcessOutput(k, std::move(t));
    });
    mProcess->SetExitCallback([this](int code) {
        OnProcessExit(code);
    });

    std::string err;
    if (!mProcess->Start(cfg, err))
    {
        mLastError = err;
        EmitSystem("Failed to start: " + err);
        mProcess.reset();
        TransitionTo(TerminalSessionState::Error);
        return false;
    }

    mExecutableLabel = cfg.mExecutable;
    mLastError.clear();
    mLastExitCode = 0;

    EmitSystem("Started: " + cfg.mExecutable);
    TransitionTo(TerminalSessionState::Running);
    return true;
}

void TerminalSession::Stop()
{
    if (mState.load() != TerminalSessionState::Running)
    {
        return;
    }

    TransitionTo(TerminalSessionState::Stopping);
    EmitSystem("Stop requested...");

    int timeoutMs = 2000;
    CliModule* cli = FindCliModule();
    if (cli != nullptr)
    {
        timeoutMs = cli->mGracefulShutdownMs;
    }

    {
        std::lock_guard<std::mutex> lock(mStopThreadMutex);
        if (mStopThread.joinable())
        {
            mStopThread.join();
        }
        mStopThread = std::thread(&TerminalSession::RunStopSequence, this, timeoutMs);
    }
}

void TerminalSession::Restart()
{
    TerminalSessionState s = mState.load();
    if (s == TerminalSessionState::Running)
    {
        mWantsRestartAfterStop.store(true);
        Stop();
    }
    else if (s == TerminalSessionState::Stopped || s == TerminalSessionState::Error)
    {
        Start();
    }
}

void TerminalSession::Clear()
{
    mBuffer.Clear();
}

bool TerminalSession::SendCommand(const std::string& line)
{
    if (mState.load() != TerminalSessionState::Running || !mProcess)
    {
        return false;
    }

    const bool tty = mProcess->IsTty();

    // In pipe mode the child doesn't see a terminal so it won't echo input
    // back; we synthesize an echo line so the user can see what they typed.
    // In TTY mode the shell echoes input itself via the pseudo-console, so
    // a synthetic echo would produce double-printing.
    if (!tty)
    {
        mBuffer.Append(TerminalEntryKind::UserEcho, "> " + line + "\n", NowSeconds());
    }

    // Line terminator differs between modes:
    //  - Pipe mode: bytes go straight to the child's stdin; cmd / bash / etc.
    //    treat '\n' as the standard line terminator.
    //  - ConPTY mode: conhost translates raw bytes into terminal key events.
    //    "Enter" must be CR ('\r'); LF is treated as a literal char and the
    //    shell never sees a line-commit, so the command sits in the input
    //    buffer forever and never runs.
    std::string toSend = line;
    toSend.push_back(tty ? '\r' : '\n');
    return mProcess->WriteStdin(toSend.data(), toSend.size());
}

bool TerminalSession::SendRaw(const char* data, size_t len)
{
    if (mState.load() != TerminalSessionState::Running || !mProcess)
    {
        return false;
    }
    if (data == nullptr || len == 0)
    {
        return true;
    }
    return mProcess->WriteStdin(data, len);
}

void TerminalSession::ShutdownSync()
{
    mWantsRestartAfterStop.store(false);

    // 1. Trigger fast termination so the wait thread and helper unblock.
    if (mProcess)
    {
        mProcess->RequestStop();   // Close stdin (cooperative — best effort)
        mProcess->ForceKill();     // Hard kill — guaranteed to unblock the wait thread
    }

    // 2. Join the stop helper thread BEFORE touching mProcess. The helper
    //    holds raw pointers into mProcess via `this`.
    {
        std::lock_guard<std::mutex> lock(mStopThreadMutex);
        if (mStopThread.joinable())
        {
            mStopThread.join();
        }
    }

    // 3. Now safe to join reader/wait threads and destroy the process.
    if (mProcess)
    {
        mProcess->Join();
        mProcess.reset();
    }

    mPendingExit.store(false);
    TransitionTo(TerminalSessionState::Stopped);
}

void TerminalSession::EmitSystem(const std::string& msg)
{
    mBuffer.Append(TerminalEntryKind::System, "[CLI] " + msg + "\n", NowSeconds());
}

void TerminalSession::OnProcessOutput(TerminalEntryKind kind, std::string text)
{
    static int sCallCount = 0;
    if (sCallCount < 5)
    {
        ++sCallCount;
        LogDebug("[CLI] OnProcessOutput #%d kind=%d size=%zu", sCallCount, static_cast<int>(kind), text.size());
    }
    mBuffer.Append(kind, std::move(text), NowSeconds());
}

void TerminalSession::OnProcessExit(int exitCode)
{
    mLastExitCode = exitCode;
    mPendingExit.store(true);
}

void TerminalSession::RunStopSequence(int timeoutMs)
{
    LogDebug("[CLI] RunStopSequence enter timeout=%d", timeoutMs);

    if (!mProcess)
    {
        LogDebug("[CLI] RunStopSequence: no mProcess, setting pendingExit");
        mPendingExit.store(true);
        return;
    }

    // Best-effort cooperative exit.
    LogDebug("[CLI] RunStopSequence: writing 'exit\\n'");
    mProcess->WriteStdin("exit\n", 5);
    LogDebug("[CLI] RunStopSequence: calling RequestStop");
    mProcess->RequestStop();

    LogDebug("[CLI] RunStopSequence: graceful wait loop start");
    const int sliceMs = 25;
    int waited = 0;
    while (waited < timeoutMs && mProcess->IsRunning())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(sliceMs));
        waited += sliceMs;
    }
    LogDebug("[CLI] RunStopSequence: graceful wait done (waited=%d, running=%d)",
             waited, mProcess->IsRunning() ? 1 : 0);

    if (mProcess->IsRunning())
    {
        LogDebug("[CLI] Graceful timeout (%d ms) elapsed; force-killing.", timeoutMs);
        mProcess->ForceKill();
    }

    LogDebug("[CLI] RunStopSequence exit");
    // Wait thread will set mPendingExit via OnProcessExit; nothing else to do here.
}

void TerminalSession::TransitionTo(TerminalSessionState newState)
{
    TerminalSessionState prev = mState.exchange(newState);
    if (prev != newState)
    {
        LogDebug("[CLI] State %s -> %s", TerminalSessionStateName(prev), TerminalSessionStateName(newState));
    }
}

#endif
