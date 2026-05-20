#pragma once

#include "Constants.h"
#include "Maths.h"
#include <string>

// Variant 2 platform-extension arm. When an addon-provided build target sets
// POLYPHASE_PLATFORM_ADDON, ActionManager has written a bridge header at
// Generated/PolyphasePlatform_SystemTypes.h that forwards to the addon's own
// SystemTypes_Platform.h. The addon's Makefile is expected to put Generated/
// on the include path. This arm takes precedence over the PLATFORM_* arms
// below so a build that sets BOTH (e.g. basePlatform=Linux + addon override)
// resolves to the addon's typedefs rather than Linux's.
#if defined(POLYPHASE_PLATFORM_ADDON)
#include "PolyphasePlatform_SystemTypes.h"
#elif PLATFORM_WINDOWS
#include <Windows.h>
#elif PLATFORM_LINUX
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <pthread.h>
#elif PLATFORM_ANDROID
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <android/native_window.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#elif PLATFORM_DOLPHIN
#include <gccore.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif PLATFORM_3DS
#include <3ds.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

// Threading typedefs — the addon-provided header above is expected to
// declare ThreadObject / MutexObject / ThreadFuncRet itself, so the arm
// short-circuits this block too.
#if defined(POLYPHASE_PLATFORM_ADDON)
// Provided by Generated/PolyphasePlatform_SystemTypes.h above.
#elif PLATFORM_WINDOWS
typedef HANDLE ThreadObject;
typedef HANDLE MutexObject;
typedef DWORD ThreadFuncRet;
#elif (PLATFORM_LINUX || PLATFORM_ANDROID)
typedef pthread_t ThreadObject;
typedef pthread_mutex_t MutexObject;
typedef void* ThreadFuncRet;
#elif PLATFORM_DOLPHIN
typedef lwp_t ThreadObject;
typedef mutex_t MutexObject;
typedef void* ThreadFuncRet;
#elif PLATFORM_3DS
typedef Thread ThreadObject;
typedef uint32_t MutexObject;
typedef void ThreadFuncRet;
#endif

typedef ThreadFuncRet(*ThreadFuncFP)(void*);

// THREAD_RETURN is `return;` if the platform's ThreadFuncRet is void, else `return 0;`.
// Addon-provided platforms whose ThreadFuncRet is void should #define
// POLYPHASE_PLATFORM_ADDON_VOID_THREAD_RETURN in their SystemTypes_Platform.h.
#if PLATFORM_3DS || defined(POLYPHASE_PLATFORM_ADDON_VOID_THREAD_RETURN)
#define THREAD_RETURN() return;
#else
#define THREAD_RETURN() return 0;
#endif

enum class ScreenOrientation : uint8_t
{
    Landscape,
    Portrait,
    Auto,

    Count
};

struct DirEntry
{
    char mDirectoryPath[MAX_PATH_SIZE + 1] = { };
    char mFilename[MAX_PATH_SIZE + 1] = { };
    bool mDirectory = false;
    bool mValid = false;

#if defined(POLYPHASE_PLATFORM_ADDON)
    // Addon may inject members here via POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS
    // — e.g. `#define POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS SceUID mDir;`
    // in SystemTypes_Platform.h. Stays empty if the addon doesn't need any.
#ifdef POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS
    POLYPHASE_PLATFORM_ADDON_DIRENTRY_MEMBERS
#endif
#elif PLATFORM_WINDOWS
    WIN32_FIND_DATA mFindData = { };
    HANDLE mFindHandle = nullptr;
#elif (PLATFORM_LINUX || PLATFORM_ANDROID)
    DIR* mDir = nullptr;
#elif PLATFORM_DOLPHIN
    DIR* mDir = nullptr;
#elif PLATFORM_3DS
    DIR* mDir = nullptr;
#endif
};

struct SystemState
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    // Addon may inject members here via POLYPHASE_PLATFORM_ADDON_SYSTEMSTATE_MEMBERS
    // — a single macro that expands to one or more `Type mName;` declarations.
#ifdef POLYPHASE_PLATFORM_ADDON_SYSTEMSTATE_MEMBERS
    POLYPHASE_PLATFORM_ADDON_SYSTEMSTATE_MEMBERS
#endif
#elif PLATFORM_WINDOWS
    HINSTANCE mConnection = nullptr;
    HWND mWindow = nullptr;
    POINT mMinSize = {};
    bool mWindowHasFocus = true;
    bool mFullscreen = false;
#elif PLATFORM_LINUX
    xcb_connection_t* mXcbConnection = nullptr;
    xcb_screen_t* mXcbScreen = nullptr;
    xcb_window_t mXcbWindow = 0;
    xcb_intern_atom_reply_t* mAtomDeleteWindow = nullptr;
    xcb_cursor_t mNullCursor = XCB_NONE;
    bool mWindowHasFocus = false;
    bool mFullscreen = false;
#elif PLATFORM_ANDROID
    android_app* mState = nullptr;
    ANativeWindow* mWindow = nullptr;
    ANativeActivity* mActivity = nullptr;
    //EGLDisplay mDisplay = ??? is this a pointer?;
    //EGLSurface mSurface = ???;
    //EGLContext mContext = ???;
    std::string mInternalDataPath;
    int32_t mWidth= 100;
    int32_t mHeight = 100;
    bool mWindowInitialized = false;
    bool mWindowHasFocus = false;
    bool mOrientationChanged = false;
    ScreenOrientation mOrientationMode = ScreenOrientation::Landscape;
    ScreenOrientation mActiveOrientation = ScreenOrientation::Landscape;
#elif PLATFORM_DOLPHIN
    void* mFrameBuffers[2] = { };
    void* mConsoleBuffer = nullptr;
    GXRModeObj mGxRmode = { };
    uint32_t mFrameIndex = 0;
    void* mMemoryCardMountArea = nullptr;
    bool mMemoryCardMounted = false;
#elif PLATFORM_3DS
    PrintConsole mPrintConsole = {};
    float mSlider = 0.0f;
    bool mNew3DS = false;
#endif
};

enum class LogSeverity : uint32_t
{
    Debug,
    Warning,
    Error,

    Count
};

struct MemoryStat
{
    std::string mName;
    uint64_t mBytesFree = 0;
    uint64_t mBytesAllocated = 0;
};
