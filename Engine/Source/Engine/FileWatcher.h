#pragma once

#include "EngineTypes.h"

#if !defined(POLYPHASE_PLATFORM_ADDON)

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

enum class FileAction
{
    Added,
    Modified,
    Removed,
    Renamed
};

struct FileChangeEvent
{
    std::string filePath;
    FileAction action;
    std::string oldPath; // For rename events
};

using FileChangeCallback = std::function<void(const FileChangeEvent&)>;

// ---------------------------------------------------------------------------
// Portable polling file watcher.
//
// A worker thread walks the watched directories every kPollIntervalMs and
// diffs each file's mtime against a snapshot; differences become events that
// the main thread drains in Update(). This deliberately uses only stat() and
// the SYS_* directory API, so it behaves identically on every editor-capable
// platform (Windows, Linux, and anything added later) with no per-platform
// code. The previous implementation was Windows-only — ReadDirectoryChangesW
// over an IOCP — which meant script hot-reload silently did nothing on the
// Linux editor build, and which stored the in-flight OVERLAPPED/buffer inside
// a std::vector whose reallocation handed the kernel dangling pointers.
//
// The cost is that a change is picked up within a poll interval rather than
// instantly. For a Scripts/ tree of a few hundred .lua files that walk is
// negligible, and sub-second latency is imperceptible in an edit/save loop.
// ---------------------------------------------------------------------------
class FileWatcher
{
public:
    FileWatcher();
    ~FileWatcher();

    // Initialize the file watcher
    bool Initialize();

    // Shutdown the file watcher
    void Shutdown();

    // Add a directory to watch. `extensions` filters which files emit events
    // (case-sensitive, with leading dot); the default preserves the historical
    // .lua-only behavior for every existing call site. Empty list = all files.
    bool WatchDirectory(const std::string& directory, bool recursive = true,
                        const std::vector<std::string>& extensions = { ".lua" });

    // Remove a directory from watching
    void UnwatchDirectory(const std::string& directory);

    // Drop every watch at once. Called on project close so the next project's
    // watches don't stack on top of the previous one's.
    void UnwatchAll();

    // Set callback for file change events
    void SetFileChangeCallback(FileChangeCallback callback);

    // Update function to process events (called from main thread)
    void Update();

    // Enable/disable the file watcher
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }

private:
    struct WatchDir
    {
        std::string path;       // always ends in '/'
        bool recursive = true;
        std::vector<std::string> extensions; // empty = every file
    };

    struct FileSnapshot
    {
        int64_t lastSeenTime = 0;      // mtime observed by the most recent scan
        int64_t lastEmittedTime = 0;   // mtime of the last change we dispatched
        bool seenThisScan = false;     // scan mark used to detect deletions
    };

    void WatcherThread();
    void ProcessEvents();

    // Worker-thread only. Walks a watched root, marking and diffing snapshots.
    void ScanDirRecursive(const std::string& dir, bool recursive, const std::vector<std::string>& extensions,
                          std::vector<FileChangeEvent>& outEvents, bool rebaseline, uint32_t depth = 0);
    void QueueEvents(const std::vector<FileChangeEvent>& events);

    std::thread mWatcherThread;
    std::atomic<bool> mRunning;
    std::atomic<bool> mEnabled;

    // Set when the watch set changes or the watcher is re-enabled: the next
    // scan records mtimes without emitting, so re-enabling hot-reload doesn't
    // dispatch a reload for every file edited while it was switched off.
    std::atomic<bool> mNeedsRebaseline;

    FileChangeCallback mCallback;

    std::vector<WatchDir> mWatchDirs;
    std::mutex mWatchMutex;

    std::vector<FileChangeEvent> mPendingEvents;
    std::mutex mEventsMutex;

    // Worker-thread private: absolute path -> observed mtimes.
    std::unordered_map<std::string, FileSnapshot> mSnapshots;
};

// Global file watcher instance
FileWatcher* GetFileWatcher();
void CreateFileWatcher();
void DestroyFileWatcher();

#else  // POLYPHASE_PLATFORM_ADDON

// ---------------------------------------------------------------------------
// Addon-platform stub. The full FileWatcher uses std::mutex / std::thread /
// std::atomic — none of which are available in embedded libstdc++ builds
// like MARSDEV's m68k-elf. Hot-reload makes no sense on a ROM cart anyway,
// so we provide a header-only no-op stub. Engine.cpp's `if (GetFileWatcher())`
// checks already gate every real call site, and they all become inert when
// GetFileWatcher() returns null.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

enum class FileAction { Added, Modified, Removed, Renamed };

struct FileChangeEvent
{
    std::string filePath;
    FileAction action;
    std::string oldPath;
};

// Trivial callback type — no <functional>/std::function so we don't drag
// any extra STL into the addon build. Engine code passes a free function
// pointer at the only call site (OnScriptFileChanged in Engine.cpp).
using FileChangeCallback = void(*)(const FileChangeEvent&);

class FileWatcher
{
public:
    bool Initialize()                                                       { return false; }
    void Shutdown()                                                         {}
    bool WatchDirectory(const std::string&, bool = true,
                        const std::vector<std::string>& = {})               { return false; }
    void UnwatchDirectory(const std::string&)                               {}
    void UnwatchAll()                                                       {}
    void SetFileChangeCallback(FileChangeCallback)                          {}
    void Update()                                                           {}
    void SetEnabled(bool)                                                   {}
    bool IsEnabled() const                                                  { return false; }
};

// Declared here, defined in the matching #else block in FileWatcher.cpp so
// header inline + .cpp body don't clash at link time.
FileWatcher* GetFileWatcher();
void CreateFileWatcher();
void DestroyFileWatcher();

#endif  // POLYPHASE_PLATFORM_ADDON
