#pragma once

#if EDITOR

#include "System/SystemTypes.h"
#include <string>
#include <deque>
#include <vector>
#include <set>
#include <mutex>

struct DebugLogEntry
{
    LogSeverity mSeverity;
    std::string mMessage;
    float mTimestamp; // seconds since engine start
    uint64_t mSeq = 0; // monotonic sequence number; 0 = unassigned
};

class DebugLogWindow
{
public:
    void Draw();
    void DrawContent();
    void Clear();
    static void LogCallback(LogSeverity severity, const char* message);

    // Snapshot recent entries for external consumers (e.g. REST controller).
    // Returns entries with seq > sinceSeq, up to maxCount. outNextSeq receives
    // the latest sequence number produced so far (use it as the next sinceSeq).
    // Must be called from the main thread.
    void GetEntriesSnapshot(uint64_t sinceSeq,
                            uint32_t maxCount,
                            std::vector<DebugLogEntry>& outEntries,
                            uint64_t& outNextSeq);

    bool mShowDebug = true;
    bool mShowWarnings = true;
    bool mShowErrors = true;
    bool mAutoScroll = true;

private:
    std::deque<DebugLogEntry> mEntries;
    std::deque<DebugLogEntry> mPendingEntries;
    std::mutex mBufferMutex;
    uint64_t mNextSeq = 0; // last assigned seq; guarded by mBufferMutex
    static const size_t kMaxEntries = 2048;

    char mSearchBuffer[256] = {};
    bool mSearchActive = false;
    std::vector<int> mSearchMatches;
    int mCurrentMatchIndex = -1;
    bool mNeedScrollToMatch = false;

    std::set<int> mSelectedEntries;
    int mLastClickedRow = -1;

    void DrainPendingEntries();
    void CopyAllToClipboard();
    void CopySelectedToClipboard();
    void UpdateSearchMatches();
    void GoToNextMatch();
    void GoToPrevMatch();
};

DebugLogWindow* GetDebugLogWindow();

#endif
