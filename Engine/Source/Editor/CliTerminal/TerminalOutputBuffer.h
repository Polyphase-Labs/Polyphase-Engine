#pragma once

#if EDITOR

#include <deque>
#include <mutex>
#include <string>

/**
 * @brief Tag for one TerminalOutputEntry.
 */
enum class TerminalEntryKind
{
    Stdout,    // raw stdout chunk from the child process
    Stderr,    // raw stderr chunk from the child process
    System,    // session/lifecycle notice (e.g. "[CLI] started cmd.exe")
    UserEcho,  // synthetic echo of a command the user typed
};

/**
 * @brief One chunk of output destined for the panel.
 *
 * Stored as raw chunks (may contain newlines) — the renderer splits on
 * newline at draw time. Storing chunks rather than per-line entries keeps
 * the producer side cheap and avoids per-byte allocations on the reader
 * thread.
 */
struct TerminalOutputEntry
{
    TerminalEntryKind mKind;
    std::string mText;
    float mTimestamp;  // seconds since engine start, or 0 if AppClock unavailable
};

/**
 * @brief Thread-safe double-buffered output store for TerminalSession.
 *
 * Producers (reader threads) call Append from any thread; the entry lands
 * in mPending under mMutex. The UI thread calls Drain at the start of every
 * frame, which moves pending entries to the committed deque mEntries and
 * trims the deque to kMaxEntries. Mirrors the DebugLogWindow drain pattern.
 */
class TerminalOutputBuffer
{
public:
    /** Append from any thread. Sets the dirty flag for the UI to notice. */
    void Append(TerminalEntryKind kind, std::string text, float timestamp);

    /** UI thread only: move pending → committed and apply the cap. */
    void Drain();

    /** UI thread only: clear committed entries. Pending will be drained next frame. */
    void Clear();

    /** UI thread only. */
    const std::deque<TerminalOutputEntry>& GetEntries() const { return mEntries; }

    /** Returns true if Drain would have new data to commit. */
    bool IsDirty() const;

private:
    std::deque<TerminalOutputEntry> mEntries;
    std::deque<TerminalOutputEntry> mPending;
    mutable std::mutex mMutex;
    bool mDirty = false;

    static const size_t kMaxEntries = 8192;
};

#endif
