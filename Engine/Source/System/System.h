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

// Pulls any files the OS has dropped on the editor window since the last call
// and moves them into outPaths. Empty on non-editor builds (DragAcceptFiles is
// not registered) and on platforms whose windowing layer doesn't have drop
// support wired yet (Linux XCB, Android, consoles).
void SYS_DrainDroppedFiles(std::vector<std::string>& outPaths);

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

/**
 * @brief Fire-and-forget process spawn that NEVER blocks the caller.
 *
 * Unlike SYS_Exec / SYS_ExecFull, this:
 *  - Does not pipe stdin/stdout/stderr (avoids the inherited-pipe-handle trap
 *    where a long-lived child like Code.exe keeps the parent blocked on EOF).
 *  - Does not wait on the child to exit.
 *  - On Windows, breaks the child away from the editor's Job Object so killing
 *    the editor does not kill the spawned external program (a user opening a
 *    script in VS Code shouldn't lose their VS Code session if Polyphase
 *    crashes).
 *
 * Use this for "open in external app" flows: code editors, file explorers,
 * web browsers, anything where we don't care about exit code or output.
 *
 * @param cmd Command to execute (shell command; same syntax as SYS_Exec).
 */
void SYS_ExecDetached(const char* cmd);

/**
 * @brief Terminate every running process matching an image name.
 *
 * Windows: taskkill /F /IM <name>  (synchronous; returns 0 on success, non-zero
 *   when nothing matched or termination failed). The image name is the .exe
 *   filename, e.g. "mspdbsrv.exe".
 * Linux/macOS: pkill -9 <name>.
 * Other platforms (consoles, headless): no-op, returns false.
 *
 * Used by native-addon recovery to clear orphaned mspdbsrv / link / cl
 * processes that pin the addon .pdb after a crashed prior build. Safe to call
 * even when no process matches — the failing exit code is treated as success
 * for the caller's "kill what you can" semantics.
 *
 * @param processName Image name (e.g. "mspdbsrv.exe"). NULL/empty = no-op.
 * @return true if a process likely existed and was terminated; false if
 *         nothing matched, terminate failed, or the platform has no kill
 *         facility. Most callers should ignore the return and call
 *         SYS_Sleep afterwards to let the OS release file handles.
 */
bool SYS_KillProcessByName(const char* processName);

/**
 * @brief Spawn a detached, fully-independent copy of an executable.
 *
 * Unlike SYS_ExecDetached (which routes through cmd.exe /c and is for "open
 * external app" flows), this spawns a binary directly with NO inherited
 * handles, NO Job-Object membership, and NO shell wrapper. The new process
 * survives the parent's death — which is exactly what the addon-recovery
 * Restart Editor flow needs: the new editor must outlive the dying current
 * editor so the Job Object's KILL_ON_JOB_CLOSE kills mspdbsrv without also
 * killing the replacement.
 *
 * Windows: CreateProcessA(exePath, exePath + " " + args,
 *           DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB,
 *           bInheritHandles=FALSE).
 * Linux/macOS: posix double-fork with execv.
 * Other platforms: no-op, returns false.
 *
 * @param exePath Absolute path to the executable. NULL/empty = no-op.
 * @param args    Command-line arguments (space-separated; quoted as needed).
 *                NULL or empty means launch with no args.
 * @return true if spawn succeeded, false otherwise.
 */
bool SYS_SpawnDetachedExecutable(const char* exePath, const char* args);

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

