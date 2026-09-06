#if PLATFORM_MAC

// POSIX half of the macOS system layer. Mirrors System_Linux.cpp; the
// Cocoa-facing functions (window, event pump, clipboard, dialogs, executable
// path, "open in Finder") live in System_MacCocoa.mm.

#include "System/System.h"
#include "System/SystemUtils.h"
#include "Graphics/Graphics.h"

#include "Engine.h"
#include "Renderer.h"
#include "Log.h"
#include "Input/Input.h"
#include "EmbeddedFile.h"

#include <cstring>
#include <cerrno>

#include <chrono>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

#if API_VULKAN
#include "Graphics/Vulkan/VramAllocator.h"
#endif

// Files
bool SYS_DoesFileExist(const char* path, bool isAsset)
{
    struct stat info;
    bool exists = false;

    int32_t retStatus = stat(path, &info);

    if (retStatus == 0)
    {
        // If the file is actually a directory, than return false.
        exists = !(info.st_mode & S_IFDIR);
    }

    return exists;
}

void SYS_AcquireFileData(const char* path, bool isAsset, int32_t maxSize, char*& outData, uint32_t& outSize)
{
    outData = nullptr;
    outSize = 0;

    // VFS shim: check the embedded raw-asset table before opening from disk.
    // See SystemUtils.cpp::SYS_LookupEmbeddedRawAsset for details.
    {
        uint32_t embeddedSize = 0;
        const char* embeddedData = SYS_LookupEmbeddedRawAsset(path, embeddedSize);
        if (embeddedData != nullptr)
        {
            uint32_t copySize = (maxSize > 0 && uint32_t(maxSize) < embeddedSize)
                ? uint32_t(maxSize)
                : embeddedSize;
            outData = (char*)malloc(copySize);
            outSize = copySize;
            memcpy(outData, embeddedData, copySize);
            return;
        }
    }

    FILE* file = fopen(path, "rb");

    if (file != nullptr)
    {
        int32_t fileSize = 0;
        fseek(file, 0, SEEK_END);
        fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (maxSize > 0)
        {
            fileSize = glm::min(fileSize, maxSize);
        }

        outData = (char*)malloc(fileSize);
        outSize = uint32_t(fileSize);
        fread(outData, fileSize, 1, file);

        fclose(file);
        file = nullptr;
    }
    else
    {
        LogError("Failed to open file: %s", path);
    }
}

void SYS_ReleaseFileData(char* data)
{
    if (data != nullptr)
    {
        free(data);
    }
}

std::string SYS_GetPolyphasePath()
{
    // Inside an .app bundle the binary sits in Contents/MacOS/ and the engine
    // tree (Engine/, Standalone/, External/, Template/, Tools/) is staged under
    // Contents/Resources/. See Installers/build_app_mac.sh.
    std::string bundleResources = SYS_GetBundleResourcePath();
    if (bundleResources != "")
    {
        return bundleResources;
    }

    std::string polyphaseDirectory = SYS_GetCurrentDirectoryPath();
    if (SYS_DoesFileExist((polyphaseDirectory + "Polyphase/imgui.ini").c_str(), false))
    {
        polyphaseDirectory = polyphaseDirectory + "Polyphase/";
    }
    if (!SYS_DoesFileExist((polyphaseDirectory + "Standalone/Standalone.rc").c_str(), false))
    {
        std::string polyphaseEXE = SYS_GetExecutablePath();
        size_t lastSlash = polyphaseEXE.find_last_of("/");
        polyphaseDirectory = polyphaseEXE.substr(0, lastSlash + 1);
    }
    return polyphaseDirectory;
}

std::string SYS_GetBundleResourcePath()
{
    std::string exe = SYS_GetExecutablePath();
    size_t pos = exe.find("/Contents/MacOS/");
    if (pos == std::string::npos)
    {
        return "";
    }
    return exe.substr(0, pos) + "/Contents/Resources/";
}

std::string SYS_GetCurrentDirectoryPath()
{
    char path[MAX_PATH_SIZE] = {};
    getcwd(path, MAX_PATH_SIZE);
    return std::string(path) + "/";
}

std::string SYS_GetAbsolutePath(const std::string& relativePath)
{
    std::string absPath;
    char* resolvedPath = realpath(relativePath.c_str(), nullptr);
    if (resolvedPath != nullptr)
    {
        absPath = resolvedPath;
        free(resolvedPath);
    }
    else
    {
        // realpath fails if path doesn't exist - resolve manually
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr)
        {
            std::string fullPath = relativePath;
            if (relativePath[0] != '/')
            {
                fullPath = std::string(cwd) + "/" + relativePath;
            }

            // Normalize path by resolving . and ..
            std::vector<std::string> parts;
            std::stringstream ss(fullPath);
            std::string part;
            while (std::getline(ss, part, '/'))
            {
                if (part == ".." && !parts.empty() && parts.back() != "..")
                {
                    parts.pop_back();
                }
                else if (part != "." && part != "")
                {
                    parts.push_back(part);
                }
            }

            for (const auto& p : parts)
            {
                absPath += "/" + p;
            }
        }
    }

    if (absPath != "" && DoesDirExist(absPath.c_str()))
        absPath += "/";

    return absPath;
}

void SYS_SetWorkingDirectory(const std::string& dirPath)
{
    chdir(dirPath.c_str());
}

bool SYS_CreateDirectory(const char* dirPath)
{
    int32_t ret = mkdir(dirPath, 0777);

    // EEXIST is never actionable -- nearly every caller uses this as "ensure
    // it exists" and ignores the return. Anything else is worth surfacing,
    // but only with the path attached.
    if (ret < 0 && errno != EEXIST)
    {
        LogWarning("mkdir failed for '%s': %s", dirPath, strerror(errno));
    }

    return (ret == 0);
}

void SYS_RemoveDirectory(const char* dirPath)
{
    std::string cmdStr = std::string("rm -rf ") + "\"" + dirPath + "\"";
    SYS_Exec(cmdStr.c_str());
}

void SYS_RemoveFile(const char* path)
{
    remove(path);
}

bool SYS_Rename(const char* oldPath, const char* newPath)
{
    return (rename(oldPath, newPath) == 0);
}

void SYS_OpenDirectory(const std::string& dirPath, DirEntry& outDirEntry)
{
    strncpy(outDirEntry.mDirectoryPath, dirPath.c_str(), MAX_PATH_SIZE);

    outDirEntry.mDir = opendir(dirPath.c_str());
    if (outDirEntry.mDir == nullptr)
    {
        // Directory doesn't exist or can't be opened — not an error, caller checks mValid.
        return;
    }

    dirent* ent = readdir(outDirEntry.mDir);
    if (ent == nullptr)
    {
        outDirEntry.mValid = false;
    }
    else
    {
        memcpy(outDirEntry.mFilename, ent->d_name, MAX_PATH_SIZE);

        struct stat statbuf;
        std::string fullPath = dirPath + outDirEntry.mFilename;
        stat(fullPath.c_str(), &statbuf);

        outDirEntry.mDirectory = S_ISDIR(statbuf.st_mode);
        outDirEntry.mValid = true;
    }
}

void SYS_IterateDirectory(DirEntry& dirEntry)
{
    if (dirEntry.mDir == nullptr)
    {
        dirEntry.mValid = false;
        return;
    }

    dirent* ent = readdir(dirEntry.mDir);
    if (ent == nullptr)
    {
        dirEntry.mValid = false;
    }
    else
    {
        memcpy(dirEntry.mFilename, ent->d_name, MAX_PATH_SIZE);

        struct stat statbuf;
        std::string fullPath = std::string(dirEntry.mDirectoryPath) + dirEntry.mFilename;
        stat(fullPath.c_str(), &statbuf);

        dirEntry.mDirectory = S_ISDIR(statbuf.st_mode);
        dirEntry.mValid = true;
    }
}

void SYS_CloseDirectory(DirEntry& dirEntry)
{
    // Callers close unconditionally, even when SYS_OpenDirectory failed;
    // closedir(NULL) segfaults on macOS.
    if (dirEntry.mDir != nullptr)
    {
        closedir(dirEntry.mDir);
        dirEntry.mDir = nullptr;
    }
    dirEntry.mValid = false;
}

// Threads
ThreadObject* SYS_CreateThread(ThreadFuncFP func, void* arg)
{
    ThreadObject* retThread = new ThreadObject();

    int status = pthread_create(
        retThread,
        nullptr,
        func,
        arg
    );

    if (status != 0)
    {
        LogError("Failed to create Thread");
    }

    return retThread;
}

void SYS_JoinThread(ThreadObject* thread)
{
    pthread_join(*thread, nullptr);
}

void SYS_DestroyThread(ThreadObject* thread)
{
    delete thread;
}

MutexObject* SYS_CreateMutex()
{
    MutexObject* retMutex = new MutexObject();

    pthread_mutexattr_t mutexAttrib;
    pthread_mutexattr_init(&mutexAttrib);
    pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE);

    int status = pthread_mutex_init(retMutex, &mutexAttrib);

    pthread_mutexattr_destroy(&mutexAttrib);

    if (status != 0)
    {
        LogError("Failed to create Mutex");
    }

    return retMutex;
}

void SYS_LockMutex(MutexObject* mutex)
{
    int status = pthread_mutex_lock(mutex);

    if (status != 0)
    {
        LogError("Failed to lock mutex");
    }
}

void SYS_UnlockMutex(MutexObject* mutex)
{
    int status = pthread_mutex_unlock(mutex);

    if (status != 0)
    {
        LogError("Failed to unlock mutex");
    }
}

void SYS_DestroyMutex(MutexObject* mutex)
{
    pthread_mutex_destroy(mutex);
    delete mutex;
}

void SYS_Sleep(uint32_t milliseconds)
{
    usleep(milliseconds * 1000);
}

// Time
uint64_t SYS_GetTimeMicroseconds()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto epoch = now_us.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::microseconds>(epoch);
    return value.count();
}

std::string SYS_GetFileName(const std::string& relativePath)
{
    // Strip directory
    size_t slash = relativePath.find_last_of("/\\");
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;

    // Strip extension (last '.' after the last slash)
    size_t dot = relativePath.find_last_of('.');
    if (dot == std::string::npos || dot < start) {
        dot = relativePath.size(); // no extension
    }

    return relativePath.substr(start, dot - start);
}

bool SYS_CopyDirectoryRecursive(const std::string& sourceDir,
                                const std::string& destDir)
{
    std::string cmd =
    std::string("cp -R \"") + sourceDir + "/.\" \"" + destDir + "\"";

    SYS_Exec(cmd.c_str());
    return true;
}

void SYS_CopyDirectory(const char* sourceDir, const char* destDir)
{
    std::string cmd = std::string("cp -R \"") + sourceDir + "\" \"" + destDir + "\"";
    SYS_Exec(cmd.c_str());
}

bool SYS_CopyFile(const char* sourcePath, const char* destPath)
{
    std::string cmd = std::string("cp \"") + sourcePath + "\" \"" + destPath + "\"";
    SYS_Exec(cmd.c_str());

    // Confirm the copy landed; a failed cp (e.g. unwritable destination dir)
    // must be reported so packaging can abort instead of shipping a broken build.
    struct stat info;
    return (stat(destPath, &info) == 0) && !S_ISDIR(info.st_mode);
}

bool SYS_MoveDirectory(const char* sourceDir, const char* destDir)
{
    std::string cmd = std::string("mv \"") + sourceDir + "\" \"" + destDir + "\"";
    return SYS_ExecFull(cmd.c_str(), nullptr, nullptr, nullptr);
}

void SYS_MoveFile(const char* sourcePath, const char* destPath)
{
    std::string cmd = std::string("mv \"") + sourcePath + "\" \"" + destPath + "\"";
    SYS_Exec(cmd.c_str());
}

// Process
void SYS_Exec(const char* cmd, std::string* output)
{
    ExecCommon(cmd, output);
}

void SYS_ExecDetached(const char* cmd)
{
    ExecCommonDetached(cmd);
}

bool SYS_KillProcessByName(const char* processName)
{
    if (processName == nullptr || *processName == 0) return false;

    std::string cmd = std::string("pkill -9 ") + processName + " >/dev/null 2>&1";

    std::string out;
    int exitCode = -1;
    SYS_ExecFull(cmd.c_str(), &out, nullptr, &exitCode);

    if (exitCode == 0)
    {
        LogDebug("SYS_KillProcessByName: terminated %s", processName);
        return true;
    }
    LogDebug("SYS_KillProcessByName: no live %s (exit=%d)", processName, exitCode);
    return false;
}

bool SYS_SpawnDetachedExecutable(const char* exePath, const char* args)
{
    if (exePath == nullptr || *exePath == 0) return false;

    // Double-fork so the grandchild reparents to launchd and survives the parent.
    pid_t pid = fork();
    if (pid < 0)
    {
        LogError("SYS_SpawnDetachedExecutable: fork failed for %s", exePath);
        return false;
    }
    if (pid == 0)
    {
        setsid();
        pid_t grand = fork();
        if (grand == 0)
        {
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0)
            {
                dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2);
                if (devnull > 2) close(devnull);
            }
            // Use the shell to handle quoted args without us needing argv splitting.
            std::string full = std::string("exec \"") + exePath + "\"";
            if (args && *args) { full += ' '; full += args; }
            execlp("/bin/sh", "sh", "-c", full.c_str(), (char*)nullptr);
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

// Memory
void* SYS_AlignedMalloc(uint32_t size, uint32_t alignment)
{
    // posix_memalign requires a power-of-two multiple of sizeof(void*);
    // callers pass 4/16/32 so clamp the small ones up.
    if (alignment < sizeof(void*))
    {
        alignment = (uint32_t)sizeof(void*);
    }

    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, size) != 0)
    {
        return nullptr;
    }
    return ptr;
}

void SYS_AlignedFree(void* pointer)
{
    OCT_ASSERT(pointer != nullptr);
    free(pointer);
}

std::vector<MemoryStat> SYS_GetMemoryStats()
{
    return {};
}

float SYS_GetRAMUsage()
{
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
    {
        return (float)((double)info.resident_size / (1024.0 * 1024.0));
    }
    return 0.0f;
}

float SYS_GetVRAMUsage()
{
#if API_VULKAN
    return (float)(VramAllocator::GetNumAllocatedBytes() / (1024.0 * 1024.0));
#else
    return 0.0f;
#endif
}

float SYS_GetRAM1Usage()
{
    return 0.0f;
}

float SYS_GetRAM2Usage()
{
    return 0.0f;
}

float SYS_GetCPUUsage()
{
    static double sPrevCpuSeconds = 0.0;
    static uint64_t sPrevWallUs = 0;
    static float sCpuUsage = 0.0f;

    double cpuSeconds = 0.0;

    // Terminated threads are accounted in the task's basic info; live threads
    // in the thread-times info. Sum both, like /proc/self/stat's utime+stime.
    mach_task_basic_info_data_t basic;
    mach_msg_type_number_t basicCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&basic, &basicCount) == KERN_SUCCESS)
    {
        cpuSeconds += basic.user_time.seconds + basic.user_time.microseconds / 1e6;
        cpuSeconds += basic.system_time.seconds + basic.system_time.microseconds / 1e6;
    }

    task_thread_times_info_data_t times;
    mach_msg_type_number_t timesCount = TASK_THREAD_TIMES_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&times, &timesCount) == KERN_SUCCESS)
    {
        cpuSeconds += times.user_time.seconds + times.user_time.microseconds / 1e6;
        cpuSeconds += times.system_time.seconds + times.system_time.microseconds / 1e6;
    }

    uint64_t wallUs = SYS_GetTimeMicroseconds();

    if (sPrevWallUs != 0)
    {
        double cpuSecondsDelta = cpuSeconds - sPrevCpuSeconds;
        double wallSecondsDelta = (double)(wallUs - sPrevWallUs) / 1000000.0;

        if (wallSecondsDelta > 0.0)
        {
            sCpuUsage = (float)(100.0 * cpuSecondsDelta / wallSecondsDelta);
        }
    }

    sPrevCpuSeconds = cpuSeconds;
    sPrevWallUs = wallUs;

    return sCpuUsage;
}

float SYS_GetTotalRAM()
{
    uint64_t memSize = 0;
    size_t len = sizeof(memSize);
    if (sysctlbyname("hw.memsize", &memSize, &len, nullptr, 0) == 0 && memSize > 0)
    {
        return (float)((double)memSize / (1024.0 * 1024.0));
    }
    return 0.0f;
}

float SYS_GetTotalVRAM()
{
#if API_VULKAN
    return (float)(VramAllocator::GetNumAllocatedBytes() / (1024.0 * 1024.0));
#else
    return 0.0f;
#endif
}

float SYS_GetTotalRAM1()
{
    return 0.0f;
}

float SYS_GetTotalRAM2()
{
    return 0.0f;
}

// Save Game

// mkdir -p. SYS_CreateDirectory is a single-level mkdir, and the fallback
// below needs to create up to three missing components.
static bool EnsureDirPath(const std::string& path)
{
    if (path.empty() || path == "/")
        return false;

    if (DoesDirExist(path.c_str()))
        return true;

    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0)
    {
        EnsureDirPath(path.substr(0, slash));
    }

    // Re-test after mkdir: a concurrent process may have won the race.
    return SYS_CreateDirectory(path.c_str()) || DoesDirExist(path.c_str());
}

// Resolve the directory game saves live in.
//
//   1. $POLYPHASE_SAVE_DIR                               — explicit override.
//   2. <projectDir>/Saves                                — when the project dir is writable.
//   3. ~/Library/Application Support/<Project>/Saves     — else (read-only .app bundle).
//
// Keeping (2) ahead of (3) is what makes this non-breaking: every existing
// install and every editor/PIE session keeps saving next to the project. The
// Application Support fallback only engages when the install root is
// read-only, which is exactly the signed .app / /Applications case.
static std::string GetSaveDir()
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;

    if (projectDir == "")
        return "";

    const char* envOverride = getenv("POLYPHASE_SAVE_DIR");
    if (envOverride != nullptr && envOverride[0] != '\0')
        return std::string(envOverride);

    // Probe the project dir rather than Saves/ — Saves/ legitimately may not
    // exist yet on a first run, and we still want the legacy location then.
    if (access(projectDir.c_str(), W_OK) == 0)
        return projectDir + "Saves";

    const char* home = getenv("HOME");

    if (home == nullptr || home[0] == '\0')
    {
        // No writable project dir and no HOME. Fall back to the legacy
        // path so the failure surfaces in the existing error logs.
        return projectDir + "Saves";
    }

    std::string projectName = GetEngineState()->mProjectName;
    if (projectName == "")
    {
        projectName = "Polyphase";
    }

    return std::string(home) + "/Library/Application Support/" + projectName + "/Saves";
}

bool SYS_ReadSave(const char* saveName, Stream& outStream)
{
    bool success = false;
    std::string saveDir = GetSaveDir();

    if (saveDir != "")
    {
        if (SYS_DoesSaveExist(saveName))
        {
            std::string savePath = saveDir + "/" + saveName;
            outStream.ReadFile(savePath.c_str(), false);
            success = true;
        }
        else
        {
            LogError("Failed to read save.");
        }
    }
    else
    {
        LogError("Failed to read save. Project directory is unset.");
    }

    return success;
}

bool SYS_WriteSave(const char* saveName, Stream& stream)
{
    bool success = false;
    std::string saveDir = GetSaveDir();

    if (saveDir != "")
    {
        if (EnsureDirPath(saveDir))
        {
            std::string savePath = saveDir + "/" + saveName;
            stream.WriteFile(savePath.c_str());
            success = true;
            LogDebug("Save written: %s (%d bytes)", saveName, stream.GetSize());
        }
        else
        {
            LogError("Failed to open Saves directory: %s", saveDir.c_str());
        }
    }
    else
    {
        LogError("Failed to write save");
    }

    return success;
}

bool SYS_DoesSaveExist(const char* saveName)
{
    bool exists = false;
    std::string saveDir = GetSaveDir();

    if (saveDir != "")
    {
        std::string savePath = saveDir + "/" + saveName;

        FILE* file = fopen(savePath.c_str(), "rb");

        if (file != nullptr)
        {
            exists = true;
            fclose(file);
            file = nullptr;
        }
    }

    return exists;
}

bool SYS_DeleteSave(const char* saveName)
{
    bool success = false;
    std::string saveDir = GetSaveDir();

    if (saveDir != "")
    {
        std::string savePath = saveDir + "/" + saveName;
        SYS_RemoveFile(savePath.c_str());
        success = true;
    }

    return success;
}

void SYS_UnmountMemoryCard()
{

}

// Misc
void SYS_Log(LogSeverity severity, const char* format, va_list arg)
{
    vprintf(format, arg);
    printf("\n");
}

POLYPHASE_API void SYS_Assert(const char* exprString, const char* fileString, uint32_t lineNumber)
{
    const char* fileName = strrchr(fileString, '/') ? strrchr(fileString, '/') + 1 : fileString;
    LogError("[Assert] %s, %s, line %d", exprString, fileName, lineNumber);
    raise(SIGTRAP);
}

void SYS_Alert(const char* message)
{
    LogError("%s", message);
    raise(SIGTRAP);
}

void SYS_UpdateConsole()
{

}

int32_t SYS_GetPlatformTier()
{
    return 2;
}

void SYS_SetWindowIcon(const char* iconPath)
{
    // The bundle's .icns supplies the Dock icon; nothing to do at runtime.
}

bool SYS_DoesWindowHaveFocus()
{
    return GetEngineState()->mSystem.mWindowHasFocus;
}

void SYS_SetScreenOrientation(ScreenOrientation orientation)
{

}

ScreenOrientation SYS_GetScreenOrientation()
{
    return ScreenOrientation::Landscape;
}

bool SYS_IsFullscreen()
{
    return GetEngineState()->mSystem.mFullscreen;
}

#endif
