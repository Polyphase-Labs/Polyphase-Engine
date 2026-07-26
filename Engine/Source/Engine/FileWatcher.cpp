#include "FileWatcher.h"
#include "Log.h"
#include "System/System.h"
#include "EngineTypes.h"
#include "Engine.h"

// Variant-2 addon platforms (e.g. PSP) get a tiny stub at the END of this
// file that returns null from GetFileWatcher() — every call site already
// null-checks, so this is enough to keep the engine ticking on platforms
// that have no threading. The rest of the file is the desktop implementation,
// which is fully portable: std::thread + stat() + the SYS_* directory API.
#if !defined(POLYPHASE_PLATFORM_ADDON)

#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

// How long between directory scans, and how finely we chop that sleep so
// Shutdown() joins promptly instead of blocking for a whole interval.
static const uint32_t kPollIntervalMs = 500;
static const uint32_t kPollSliceMs = 50;

// Returns 0 when the file can't be stat'd (missing, or momentarily locked by
// the editor that's writing it). Callers treat 0 as "no reading this scan"
// rather than as a timestamp. std::filesystem is deliberately avoided — the
// Win32 x86 project configs and the Linux makefiles don't guarantee C++17.
static int64_t GetFileModTime(const std::string& path)
{
#if PLATFORM_WINDOWS
    struct _stat64 st;
    if (_stat64(path.c_str(), &st) != 0)
        return 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return 0;
#endif
    return (int64_t)st.st_mtime;
}

static bool HasLuaExtension(const std::string& path)
{
    return path.size() >= 4 && path.compare(path.size() - 4, 4, ".lua") == 0;
}

static FileWatcher* sFileWatcher = nullptr;

FileWatcher* GetFileWatcher()
{
    return sFileWatcher;
}

void CreateFileWatcher()
{
    if (sFileWatcher == nullptr)
    {
        sFileWatcher = new FileWatcher();
    }
}

void DestroyFileWatcher()
{
    if (sFileWatcher != nullptr)
    {
        delete sFileWatcher;
        sFileWatcher = nullptr;
    }
}

FileWatcher::FileWatcher()
    : mRunning(false)
    , mEnabled(true)
    , mNeedsRebaseline(true)
{
}

FileWatcher::~FileWatcher()
{
    Shutdown();
}

bool FileWatcher::Initialize()
{
    if (mRunning)
    {
        return true;
    }

    mRunning = true;
    mNeedsRebaseline = true;
    mWatcherThread = std::thread(&FileWatcher::WatcherThread, this);

    return true;
}

void FileWatcher::Shutdown()
{
    if (mRunning)
    {
        mRunning = false;

        if (mWatcherThread.joinable())
        {
            mWatcherThread.join();
        }

        {
            std::lock_guard<std::mutex> lock(mWatchMutex);
            mWatchDirs.clear();
        }

        {
            std::lock_guard<std::mutex> lock(mEventsMutex);
            mPendingEvents.clear();
        }

        // Only ever touched by the (now joined) worker thread.
        mSnapshots.clear();
    }
}

// Watched roots are stored with a trailing '/' so the recursive walk can just
// concatenate. Callers don't necessarily supply one — Engine.cpp passes
// "<projectDir>Scripts" unterminated.
static std::string NormalizeWatchPath(const std::string& directory)
{
    std::string path = directory;
    for (size_t i = 0; i < path.length(); ++i)
    {
        if (path[i] == '\\')
            path[i] = '/';
    }
    if (!path.empty() && path[path.length() - 1] != '/')
    {
        path += '/';
    }
    return path;
}

bool FileWatcher::WatchDirectory(const std::string& directory, bool recursive)
{
    if (!mRunning)
    {
        LogError("FileWatcher not initialized");
        return false;
    }

    std::string path = NormalizeWatchPath(directory);

    LogDebug("Attempting to watch directory: %s (recursive: %s)", path.c_str(), recursive ? "true" : "false");

    {
        std::lock_guard<std::mutex> lock(mWatchMutex);

        // Re-watching the same root is a no-op rather than a duplicate entry —
        // LoadProject can run more than once per session for the same project.
        for (uint32_t i = 0; i < mWatchDirs.size(); ++i)
        {
            if (mWatchDirs[i].path == path)
            {
                mWatchDirs[i].recursive = recursive;
                return true;
            }
        }

        WatchDir watchDir;
        watchDir.path = path;
        watchDir.recursive = recursive;
        mWatchDirs.push_back(watchDir);
    }

    // The first scan over a newly added root only records mtimes; otherwise
    // opening a project would fire a reload for every script it contains.
    mNeedsRebaseline = true;

    return true;
}

void FileWatcher::UnwatchDirectory(const std::string& directory)
{
    std::string path = NormalizeWatchPath(directory);

    {
        std::lock_guard<std::mutex> lock(mWatchMutex);
        for (auto it = mWatchDirs.begin(); it != mWatchDirs.end(); ++it)
        {
            if (it->path == path)
            {
                mWatchDirs.erase(it);
                break;
            }
        }
    }

    // Files under the removed root fall out of mSnapshots on the next scan
    // (they simply stop being visited). Suppress that scan's events so the
    // drop doesn't surface as a burst of deletions.
    mNeedsRebaseline = true;
}

void FileWatcher::UnwatchAll()
{
    {
        std::lock_guard<std::mutex> lock(mWatchMutex);
        mWatchDirs.clear();
    }

    // Anything already queued belongs to the project being torn down. Dropping
    // it here keeps stale paths from reaching the callback after the switch.
    {
        std::lock_guard<std::mutex> lock(mEventsMutex);
        mPendingEvents.clear();
    }

    mNeedsRebaseline = true;
}

void FileWatcher::SetFileChangeCallback(FileChangeCallback callback)
{
    mCallback = callback;
}

void FileWatcher::Update()
{
    if (!mEnabled)
        return;
        
    ProcessEvents();
}

void FileWatcher::SetEnabled(bool enabled)
{
    // Rebaseline either way: while disabled we stop scanning, so the snapshot
    // goes stale. Re-enabling must not dump a reload for every file the user
    // saved in the meantime.
    mEnabled = enabled;
    mNeedsRebaseline = true;
}

void FileWatcher::WatcherThread()
{
    while (mRunning)
    {
        if (mEnabled)
        {
            // Consume the flag up front so a rebaseline requested mid-scan
            // (e.g. a project load registering another root) still gets its
            // own quiet pass rather than being swallowed by this one.
            const bool rebaseline = mNeedsRebaseline.exchange(false);

            std::vector<WatchDir> watchDirs;
            {
                std::lock_guard<std::mutex> lock(mWatchMutex);
                watchDirs = mWatchDirs;
            }

            // Deletions are found by elimination: clear the marks, walk every
            // root, then sweep whatever the walk didn't touch.
            for (auto it = mSnapshots.begin(); it != mSnapshots.end(); ++it)
            {
                it->second.seenThisScan = false;
            }

            std::vector<FileChangeEvent> events;

            for (uint32_t i = 0; i < watchDirs.size() && mRunning; ++i)
            {
                ScanDirRecursive(watchDirs[i].path, watchDirs[i].recursive, events, rebaseline);
            }

            if (mRunning)
            {
                for (auto it = mSnapshots.begin(); it != mSnapshots.end(); )
                {
                    if (!it->second.seenThisScan)
                    {
                        if (!rebaseline)
                        {
                            FileChangeEvent event;
                            event.filePath = it->first;
                            event.action = FileAction::Removed;
                            events.push_back(event);
                        }
                        it = mSnapshots.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                QueueEvents(events);
            }
        }

        // Chopped so Shutdown() doesn't wait out a whole interval.
        for (uint32_t slept = 0; slept < kPollIntervalMs && mRunning; slept += kPollSliceMs)
        {
            SYS_Sleep(kPollSliceMs);
        }
    }
}

void FileWatcher::ScanDirRecursive(const std::string& dir, bool recursive, std::vector<FileChangeEvent>& outEvents, bool rebaseline, uint32_t depth)
{
    // A symlinked directory pointing back at an ancestor would otherwise spin
    // this worker forever while mSnapshots grows without bound. No real Scripts
    // tree comes close to this depth.
    if (depth > 32)
    {
        return;
    }

    DirEntry dirEntry;
    SYS_OpenDirectory(dir, dirEntry);

    // A missing or unopenable directory leaves mValid false with no handle to
    // release — closing it here would hand closedir()/FindClose() garbage.
    if (!dirEntry.mValid)
    {
        return;
    }

    // SYS_OpenDirectory already yields the first entry, so consume before iterating.
    while (dirEntry.mValid && mRunning)
    {
        if (strcmp(dirEntry.mFilename, ".") != 0 &&
            strcmp(dirEntry.mFilename, "..") != 0)
        {
            std::string path = dir + dirEntry.mFilename;

            if (dirEntry.mDirectory)
            {
                if (recursive)
                {
                    ScanDirRecursive(path + "/", true, outEvents, rebaseline, depth + 1);
                }
            }
            else if (HasLuaExtension(path))
            {
                const int64_t modTime = GetFileModTime(path);
                auto it = mSnapshots.find(path);

                if (modTime == 0)
                {
                    // Unreadable right now — most likely mid-write. Keep the
                    // entry alive so the sweep doesn't call it a deletion.
                    if (it != mSnapshots.end())
                    {
                        it->second.seenThisScan = true;
                    }
                }
                else if (it == mSnapshots.end())
                {
                    FileSnapshot snapshot;
                    snapshot.lastSeenTime = modTime;
                    // On a rebaseline pass the file counts as already known.
                    // Otherwise leave lastEmittedTime at 0 so the next scan
                    // emits Added — once the mtime has stopped moving.
                    snapshot.lastEmittedTime = rebaseline ? modTime : 0;
                    snapshot.seenThisScan = true;
                    mSnapshots[path] = snapshot;
                }
                else
                {
                    FileSnapshot& snapshot = it->second;
                    snapshot.seenThisScan = true;

                    if (rebaseline)
                    {
                        snapshot.lastSeenTime = modTime;
                        snapshot.lastEmittedTime = modTime;
                    }
                    else
                    {
                        // Settle rule: only report once the mtime has held
                        // steady for a full interval. Reloading a half-written
                        // file would fail to parse and never retry, since its
                        // mtime wouldn't change again.
                        if (modTime != snapshot.lastEmittedTime &&
                            modTime == snapshot.lastSeenTime)
                        {
                            FileChangeEvent event;
                            event.filePath = path;
                            event.action = (snapshot.lastEmittedTime == 0) ? FileAction::Added : FileAction::Modified;
                            outEvents.push_back(event);

                            snapshot.lastEmittedTime = modTime;
                        }

                        snapshot.lastSeenTime = modTime;
                    }
                }
            }
        }

        SYS_IterateDirectory(dirEntry);
    }

    SYS_CloseDirectory(dirEntry);
}

void FileWatcher::QueueEvents(const std::vector<FileChangeEvent>& events)
{
    if (events.empty())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mEventsMutex);
    mPendingEvents.insert(mPendingEvents.end(), events.begin(), events.end());
}

void FileWatcher::ProcessEvents()
{
    std::vector<FileChangeEvent> eventsToProcess;

    {
        std::lock_guard<std::mutex> lock(mEventsMutex);
        eventsToProcess = std::move(mPendingEvents);
        mPendingEvents.clear();
    }

    // The scan already filters to .lua and coalesces repeats, so this is just
    // the main-thread dispatch point — the callback reloads scripts and pokes
    // live Script nodes, neither of which is safe off the main thread.
    for (uint32_t i = 0; i < eventsToProcess.size(); ++i)
    {
        if (mCallback)
        {
            mCallback(eventsToProcess[i]);
        }
    }
}

#else  // POLYPHASE_PLATFORM_ADDON — no-op file watcher for addon platforms.

// PSP and similar consoles have no usable file-watching API and no scripts
// being hot-reloaded at runtime. Every engine call site uses
// `if (GetFileWatcher())` before invoking it, so a null return cleanly
// disables hot-reload without conditionals in the call sites.

FileWatcher* GetFileWatcher() { return nullptr; }
void CreateFileWatcher() {}
void DestroyFileWatcher() {}

#endif // !POLYPHASE_PLATFORM_ADDON
