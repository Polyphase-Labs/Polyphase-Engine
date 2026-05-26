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

#if PLATFORM_WINDOWS
#include <Windows.h>
#endif

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

class FileWatcher
{
public:
    FileWatcher();
    ~FileWatcher();

    // Initialize the file watcher
    bool Initialize();

    // Shutdown the file watcher
    void Shutdown();

    // Add a directory to watch
    bool WatchDirectory(const std::string& directory, bool recursive = true);

    // Remove a directory from watching
    void UnwatchDirectory(const std::string& directory);

    // Set callback for file change events
    void SetFileChangeCallback(FileChangeCallback callback);

    // Update function to process events (called from main thread)
    void Update();

    // Enable/disable the file watcher
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return mEnabled; }

private:
    void WatcherThread();
    void ProcessEvents();

#if PLATFORM_WINDOWS
    struct WatchInfo
    {
        HANDLE directoryHandle;
        OVERLAPPED overlapped;
        char buffer[8192];
        std::string path;
        bool recursive;
    };

    std::vector<WatchInfo> mWatchInfos;
    HANDLE mCompletionPort;
#endif

    std::thread mWatcherThread;
    std::atomic<bool> mRunning;
    std::atomic<bool> mEnabled;

    FileChangeCallback mCallback;

    std::vector<FileChangeEvent> mPendingEvents;
    std::mutex mEventsMutex;

    // Track last modification times to avoid duplicate events
    std::unordered_map<std::string, uint64_t> mLastModifyTimes;
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
    bool WatchDirectory(const std::string&, bool = true)                    { return false; }
    void UnwatchDirectory(const std::string&)                               {}
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
