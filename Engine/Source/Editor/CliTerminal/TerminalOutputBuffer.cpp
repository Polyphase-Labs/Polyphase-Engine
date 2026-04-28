#if EDITOR

#include "TerminalOutputBuffer.h"

#include "Log.h"

#include <utility>

void TerminalOutputBuffer::Append(TerminalEntryKind kind, std::string text, float timestamp)
{
    if (text.empty())
    {
        return;
    }

    TerminalOutputEntry entry;
    entry.mKind = kind;
    entry.mText = std::move(text);
    entry.mTimestamp = timestamp;

    size_t pendingAfter = 0;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPending.push_back(std::move(entry));
        mDirty = true;
        pendingAfter = mPending.size();
    }

    static int sAppendCount = 0;
    if (sAppendCount < 5)
    {
        ++sAppendCount;
        LogDebug("[CLI] Buffer::Append #%d pending=%zu this=%p", sAppendCount, pendingAfter, (void*)this);
    }
}

void TerminalOutputBuffer::Drain()
{
    std::deque<TerminalOutputEntry> drained;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mPending.empty())
        {
            mDirty = false;
            return;
        }
        drained.swap(mPending);
        mDirty = false;
    }

    static int sDrainCount = 0;
    if (sDrainCount < 5)
    {
        ++sDrainCount;
        LogDebug("[CLI] Buffer::Drain #%d drained=%zu entries=%zu this=%p", sDrainCount, drained.size(), mEntries.size(), (void*)this);
    }

    for (TerminalOutputEntry& entry : drained)
    {
        mEntries.push_back(std::move(entry));
    }

    while (mEntries.size() > kMaxEntries)
    {
        mEntries.pop_front();
    }
}

void TerminalOutputBuffer::Clear()
{
    mEntries.clear();
    // Pending entries (if any) remain and will arrive on the next Drain.
}

bool TerminalOutputBuffer::IsDirty() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mDirty;
}

#endif
