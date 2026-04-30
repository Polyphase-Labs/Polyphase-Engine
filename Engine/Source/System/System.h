#pragma once

#include "PolyphaseAPI.h"
#include "System/SystemTypes.h"

#include <string>
#include <vector>
#include <stdarg.h>

class Stream;

void SYS_Initialize();
void SYS_Shutdown();
void SYS_Update();

// Files
bool SYS_DoesFileExist(const char* path, bool isAsset);
void SYS_AcquireFileData(const char* path, bool isAsset, int32_t maxSize, char*& outData, uint32_t& outSize);
void SYS_ReleaseFileData(char* data);
std::string SYS_GetExecutablePath();
std::string SYS_GetPolyphasePath();
std::string SYS_GetCurrentDirectoryPath();
std::string SYS_GetAbsolutePath(const std::string& relativePath);
void SYS_ExplorerOpenDirectory(const std::string& dirPath);
void SYS_OpenFileWithDefaultApp(const std::string& filePath);
void SYS_SetWorkingDirectory(const std::string& dirPath);
bool SYS_CreateDirectory(const char* dirPath);
void SYS_RemoveDirectory(const char* dirPath);
void SYS_OpenDirectory(const std::string& dirPath, DirEntry& outDirEntry);
void SYS_IterateDirectory(DirEntry& dirEntry);
void SYS_CopyDirectory(const char* sourceDir, const char* destDir);
void SYS_CopyFile(const char* sourcePath, const char* destPath);
bool SYS_CopyDirectoryRecursive(const std::string& sourceDir,
                                const std::string& destDir);
void SYS_MoveDirectory(const char* sourceDir, const char* destDir);
void SYS_MoveFile(const char* sourcePath, const char* destPath);
void SYS_CloseDirectory(DirEntry& dirEntry);
void SYS_RemoveFile(const char* path);
bool SYS_Rename(const char* oldPath, const char* newPath);
std::vector<std::string> SYS_OpenFileDialog();
std::string SYS_SaveFileDialog();
std::string SYS_SelectFolderDialog();
std::string SYS_GetFileName(const std::string& path);

// Threading — exported so native addons can use them without relinking the engine's
// implementation. See AsyncMediaPump in the VideoPlayer addon for a reference consumer.
POLYPHASE_API ThreadObject* SYS_CreateThread(ThreadFuncFP func, void* arg);
POLYPHASE_API void SYS_JoinThread(ThreadObject* thread);
POLYPHASE_API void SYS_DestroyThread(ThreadObject* thread);
POLYPHASE_API MutexObject* SYS_CreateMutex();
POLYPHASE_API void SYS_LockMutex(MutexObject* mutex);
POLYPHASE_API void SYS_UnlockMutex(MutexObject* mutex);
POLYPHASE_API void SYS_DestroyMutex(MutexObject* mutex);
POLYPHASE_API void SYS_Sleep(uint32_t milliseconds);

// Time
POLYPHASE_API uint64_t SYS_GetTimeMicroseconds();

// Process
void SYS_Exec(const char* cmd, std::string* output = nullptr);

/**
 * @brief Execute a command with full output capture.
 *
 * Unlike SYS_Exec which only captures the first 1024 bytes,
 * this function captures complete stdout/stderr output.
 *
 * @param cmd Command to execute
 * @param outStdout Pointer to receive stdout (can be nullptr)
 * @param outStderr Pointer to receive stderr (can be nullptr)
 * @param outExitCode Pointer to receive exit code (can be nullptr)
 * @return true if command executed successfully
 */
bool SYS_ExecFull(const char* cmd, std::string* outStdout, std::string* outStderr, int* outExitCode);

// Memory
void* SYS_AlignedMalloc(uint32_t size, uint32_t alignment);
void SYS_AlignedFree(void* pointer);
std::vector<MemoryStat> SYS_GetMemoryStats();
float SYS_GetRAMUsage();
float SYS_GetVRAMUsage();
float SYS_GetRAM1Usage();
float SYS_GetRAM2Usage();
float SYS_GetCPUUsage();
float SYS_GetTotalRAM();
float SYS_GetTotalVRAM();
float SYS_GetTotalRAM1();
float SYS_GetTotalRAM2();

// Save / Memcard
bool SYS_ReadSave(const char* saveName, Stream& outStream);
bool SYS_WriteSave(const char* saveName, Stream& stream);
bool SYS_DoesSaveExist(const char* saveName);
bool SYS_DeleteSave(const char* saveName);
void SYS_UnmountMemoryCard();

// Clipboard
void SYS_SetClipboardText(const std::string& str);
std::string SYS_GetClipboardText();

// Misc
void SYS_Log(LogSeverity severity, const char* format, va_list arg);
POLYPHASE_API void SYS_Assert(const char* exprString, const char* fileString, uint32_t lineNumber);
void SYS_Alert(const char* message);
void SYS_UpdateConsole();
int32_t SYS_GetPlatformTier();
void SYS_SetWindowTitle(const char* title);
void SYS_SetWindowIcon(const char* iconPath);
bool SYS_DoesWindowHaveFocus();
void SYS_SetScreenOrientation(ScreenOrientation orientation);
ScreenOrientation SYS_GetScreenOrientation();
void SYS_SetFullscreen(bool fullscreen);
bool SYS_IsFullscreen();
void SYS_SetWindowRect(int32_t x, int32_t y, int32_t width, int32_t height);
void SYS_GetWindowRect(int32_t& outX, int32_t& outY, int32_t& outWidth, int32_t& outHeight);
bool SYS_IsWindowMaximized();
void SYS_MaximizeWindow();

struct ScopedLock
{
    ScopedLock(MutexObject* mutex)
    {
        mMutex = mutex;
        SYS_LockMutex(mMutex);
    }

    ~ScopedLock()
    {
        SYS_UnlockMutex(mMutex);
    }

    MutexObject* mMutex = nullptr;
};

#define SCOPED_LOCK(mutex) ScopedLock scopedLock(mutex)

