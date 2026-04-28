#pragma once

#if EDITOR

#include "document.h"

#include <deque>
#include <string>

/**
 * @brief Bounded command history with Up/Down cursor for the input box.
 *
 * Newest entries are at the back. Cursor == -1 means "below the newest"
 * (the user is editing a fresh line). Pressing Up walks toward index 0
 * (oldest); pressing Down walks back toward the fresh line.
 */
class CommandHistory
{
public:
    /** Append a command. Skips no-op duplicates of the most recent entry. */
    void Add(const std::string& cmd);

    /** Move toward older entries. Returns nullptr at the top. */
    const std::string* Prev();

    /** Move toward newer entries. Returns nullptr when back at the fresh line. */
    const std::string* Next();

    /** Reset to the "below newest" state (call after submitting a command). */
    void ResetCursor() { mCursor = -1; }

    /** Persist as a JSON array. */
    void SaveTo(rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc) const;

    /** Restore from a JSON array (silently ignores non-array). */
    void LoadFrom(const rapidjson::Value& arr);

private:
    std::deque<std::string> mItems;
    int mCursor = -1;
    static const size_t kMaxItems = 256;
};

#endif
