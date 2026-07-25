#include "SystemUtils.h"
#include "System.h"
#include "Log.h"
#include "EmbeddedFile.h"
#include "ContentObfuscation.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !PLATFORM_WINDOWS && !defined(POLYPHASE_PLATFORM_ADDON)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

#if PLATFORM_WINDOWS
#include <Windows.h>
#include <mutex>

// Process-wide Job Object: every child we spawn via the Windows exec helpers
// below gets attached to it. JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE means the OS
// terminates every member of the Job the moment our last handle to it closes
// (process exit, including SIGKILL/taskkill). That makes orphan mspdbsrv.exe /
// cl.exe / link.exe / packagers impossible — they die with the editor and
// can't outlive us to keep PDBs locked.
static std::once_flag sEditorJobOnce;
static HANDLE         sEditorJob = nullptr;

static void EnsureEditorJobObject()
{
    std::call_once(sEditorJobOnce, []() {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (job == nullptr)
        {
            LogWarning("CreateJobObject failed (gle=%lu); subprocess containment disabled.", GetLastError());
            return;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info)))
        {
            LogWarning("SetInformationJobObject failed (gle=%lu); subprocess containment disabled.", GetLastError());
            CloseHandle(job);
            return;
        }
        sEditorJob = job;
    });
}

// Spawn a child process with stdout+stderr captured into outCombined,
// attached to the editor's Job Object. Returns the process exit code,
// or -1 on failure to spawn.
static int WindowsExecInJob(const char* cmd, std::string* outCombined)
{
    EnsureEditorJobObject();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
    {
        LogError("CreatePipe failed (gle=%lu)", GetLastError());
        return -1;
    }
    // Read end stays with the parent — must not be inherited or the child
    // can't EOF the pipe when it exits.
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError  = writePipe;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    // Mutable buffer for CreateProcessA's cmdline parameter. Prepend `cmd.exe /c`
    // so shell builtins (copy/move/del/cd), pipes (|), chains (&&), redirects (>),
    // and %VAR% expansion work — these were what `_popen` implicitly provided.
    // Without this, e.g. SYS_CopyFile shells out to `copy "src" "dst" /Y` and
    // CreateProcess fails (ERROR_FILE_NOT_FOUND, no copy.exe on disk) silently —
    // which is exactly how Config.ini + <project>.octp went missing from
    // addon-target packages until 2026-06.
    std::string cmdBuf = (cmd && *cmd) ? (std::string("cmd.exe /c ") + cmd) : std::string();

    BOOL ok = CreateProcessA(
        nullptr,
        cmdBuf.empty() ? nullptr : &cmdBuf[0],
        nullptr, nullptr,
        TRUE,                                  // bInheritHandles
        CREATE_SUSPENDED | CREATE_NO_WINDOW,   // create suspended so we can Assign first
        nullptr, nullptr,
        &si, &pi);

    if (!ok)
    {
        DWORD gle = GetLastError();
        LogError("CreateProcess failed (gle=%lu) for: %s", gle, cmd ? cmd : "");
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return -1;
    }

    // Attach to Job before Resume so the child can never escape — even between
    // Resume and the first instruction it executes.
    if (sEditorJob != nullptr)
    {
        if (!AssignProcessToJobObject(sEditorJob, pi.hProcess))
        {
            // Likely cause: parent already in a restrictive Job (e.g. launched
            // under a debugger / launcher that bans nested jobs on this OS).
            // Continue without containment so the build still runs.
            LogWarning("AssignProcessToJobObject failed (gle=%lu); child runs uncontained.", GetLastError());
        }
    }

    ResumeThread(pi.hThread);

    // Parent doesn't write to the pipe — close our write end so the child's
    // EOF signal propagates when it exits.
    CloseHandle(writePipe);

    // Drain the pipe.
    if (outCombined) outCombined->clear();
    {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(readPipe, buf, sizeof(buf), &n, nullptr) && n > 0)
        {
            if (outCombined) outCombined->append(buf, buf + n);
        }
    }
    CloseHandle(readPipe);

    // Wait for the child to actually finish, then collect exit code.
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (int)exitCode;
}

// Fire-and-forget spawn. Unlike WindowsExecInJob, this:
//  - Does NOT inherit handles (bInheritHandles=FALSE). This is the critical
//    fix for the "editor freezes when launching VS Code fresh" bug — the old
//    path leaked the stdout/stderr pipe write end through cmd.exe into the
//    spawned editor, so ReadFile() on the parent blocked until VS Code (the
//    inheritor of last resort) exited. With FALSE, no handle leak.
//  - Does NOT wait on the child or pipe stdout/stderr.
//  - Uses CREATE_BREAKAWAY_FROM_JOB so killing Polyphase does NOT also kill
//    the user's VS Code session (the editor Job has KILL_ON_JOB_CLOSE).
//  - Routes through cmd.exe /c so shell builtins like `start ""` keep working
//    (matches ExecCommon's behavior).
static void WindowsExecDetached(const char* cmd)
{
    if (cmd == nullptr || *cmd == 0) return;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};

    std::string cmdBuf = std::string("cmd.exe /c ") + cmd;

    DWORD flags = DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB;

    BOOL ok = CreateProcessA(
        nullptr,
        &cmdBuf[0],
        nullptr, nullptr,
        FALSE,        // bInheritHandles — must be FALSE; no pipe inheritance.
        flags,
        nullptr, nullptr,
        &si, &pi);

    if (!ok)
    {
        // CREATE_BREAKAWAY_FROM_JOB fails (ERROR_ACCESS_DENIED) if our Job
        // forbids breakaway. Retry without it — the child still runs detached,
        // it just dies with the editor. Better than not launching at all.
        DWORD gle = GetLastError();
        if (gle == ERROR_ACCESS_DENIED)
        {
            flags &= ~CREATE_BREAKAWAY_FROM_JOB;
            ok = CreateProcessA(
                nullptr,
                &cmdBuf[0],
                nullptr, nullptr,
                FALSE,
                flags,
                nullptr, nullptr,
                &si, &pi);
        }
    }

    if (!ok)
    {
        LogError("CreateProcess (detached) failed (gle=%lu) for: %s", GetLastError(), cmd);
        return;
    }

    // Don't wait, don't keep handles around.
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}
#endif // PLATFORM_WINDOWS

void ExecCommon(const char* cmd, std::string* output)
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    // Addon-runtime platforms (PSP etc.) have no subprocess facility — newlib
    // ships no popen/pclose and the kernel has no fork/exec. Just log + return.
    (void)cmd;
    if (output) output->clear();
    LogDebug("[Exec] (no subprocess support on this platform): %s", cmd ? cmd : "");
#elif PLATFORM_WINDOWS
    // Route through the Job-Object-attached exec so children die with the
    // editor. Same observable interface as the old _popen path (first ~1KB
    // line of output captured; trailing newline stripped) but no orphaned
    // mspdbsrv / cl.exe / link.exe on editor crash or kill.
    LogDebug("[Exec] %s", cmd);
    if (output != nullptr)
    {
        std::string combined;
        WindowsExecInJob(cmd, &combined);
        // Match legacy fgets-of-first-line behavior — many callers only want
        // a short one-liner answer (e.g. tool-version probes).
        size_t firstNewline = combined.find_first_of("\r\n");
        *output = (firstNewline == std::string::npos) ? combined : combined.substr(0, firstNewline);
        LogDebug(" >> %s", output->c_str());
    }
    else
    {
        WindowsExecInJob(cmd, nullptr);
    }
#else
    LogDebug("[Exec] %s", cmd);

    if (output != nullptr)
    {
        FILE* file = popen(cmd, "r");

        if (file != nullptr)
        {
            char buffer[1024];
            if (fgets(buffer, 1024, file))
            {
                *output = buffer;
            }

            // Strip newlines
            std::string& str = *output;
            for (int32_t i = int32_t(str.size()) - 1; i >= 0; --i)
            {
                if (str[i] == '\n' ||
                    str[i] == '\r')
                {
                    str.erase(str.begin() + i);
                }
            }

            LogDebug(" >> %s", output->c_str());

            pclose(file);
            file = nullptr;
        }
        else
        {
            LogError("Failed to run command");
        }
    }
    else
    {
        system(cmd);
    }
#endif // POLYPHASE_PLATFORM_ADDON / PLATFORM_WINDOWS
}

void ExecCommonDetached(const char* cmd)
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    (void)cmd;
    LogDebug("[ExecDetached] (no subprocess support on this platform): %s", cmd ? cmd : "");
#elif PLATFORM_WINDOWS
    LogDebug("[ExecDetached] %s", cmd);
    WindowsExecDetached(cmd);
#else
    LogDebug("[ExecDetached] %s", cmd);
    if (cmd == nullptr || *cmd == 0) return;

    // POSIX double-fork so the grandchild reparents to init and we don't have
    // to wait or reap. Redirect all std fds to /dev/null so the grandchild
    // can't inherit anything that would keep the editor's tty open.
    pid_t pid = fork();
    if (pid < 0)
    {
        LogError("fork() failed for detached exec: %s", cmd);
        return;
    }

    if (pid == 0)
    {
        // First child: detach and spawn the grandchild, then exit.
        setsid();

        pid_t grand = fork();
        if (grand == 0)
        {
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0)
            {
                dup2(devnull, 0);
                dup2(devnull, 1);
                dup2(devnull, 2);
                if (devnull > 2) close(devnull);
            }
            execlp("/bin/sh", "sh", "-c", cmd, (char*)nullptr);
            _exit(127);
        }

        _exit(0);
    }

    // Parent: reap the first child immediately so it doesn't zombie.
    int status = 0;
    waitpid(pid, &status, 0);
#endif
}

bool SYS_ExecFull(const char* cmd, std::string* outStdout, std::string* outStderr, int* outExitCode)
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    // No subprocess support — see ExecCommon comment.
    (void)cmd;
    if (outStdout)   outStdout->clear();
    if (outStderr)   outStderr->clear();
    if (outExitCode) *outExitCode = -1;
    return false;
#elif PLATFORM_WINDOWS
    LogDebug("[ExecFull] %s", cmd);

    std::string combined;
    int exitCode = WindowsExecInJob(cmd, &combined);

    if (outStdout != nullptr) *outStdout = combined;
    // stderr is folded into stdout at the pipe layer in WindowsExecInJob.
    if (outStderr != nullptr) outStderr->clear();
    if (outExitCode != nullptr) *outExitCode = exitCode;

    LogDebug("[ExecFull] Exit code: %d, Output length: %zu", exitCode, combined.size());
    return exitCode == 0;
#else
    LogDebug("[ExecFull] %s", cmd);

    // Build command that captures both stdout and stderr
    std::string fullCmd = cmd;
    fullCmd += " 2>&1";

    FILE* file = popen(fullCmd.c_str(), "r");
    if (file == nullptr)
    {
        LogError("Failed to run command: %s", cmd);
        if (outExitCode != nullptr)
        {
            *outExitCode = -1;
        }
        return false;
    }

    // Read all output
    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), file) != nullptr)
    {
        output += buffer;
    }

    int status = pclose(file);

#if PLATFORM_3DS || PLATFORM_DOLPHIN
    int exitCode = status;
#else
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    // Set output parameters
    if (outStdout != nullptr)
    {
        *outStdout = output;
    }

    // stderr is combined with stdout due to 2>&1 redirect
    if (outStderr != nullptr)
    {
        outStderr->clear();
    }

    if (outExitCode != nullptr)
    {
        *outExitCode = exitCode;
    }

    LogDebug("[ExecFull] Exit code: %d, Output length: %zu", exitCode, output.size());

    return exitCode == 0;
#endif // POLYPHASE_PLATFORM_ADDON / PLATFORM_WINDOWS
}

// ---------------------------------------------------------------------------
// Embedded raw-asset VFS — runtime-registration model.
//
// The generated Generated/EmbeddedAssets.cpp (when linked into a shipped exe)
// runs a static initializer that calls SYS_RegisterEmbeddedRawAssets, handing
// off a pointer to its gEmbeddedRawAssets[] table. Editor builds don't link
// the generated cpp, so registration is a no-op and lookups always miss.
// ---------------------------------------------------------------------------

static EmbeddedFile* sEmbeddedRawTable = nullptr;
static uint32_t      sEmbeddedRawCount = 0;

void SYS_RegisterEmbeddedRawAssets(EmbeddedFile* table, uint32_t count)
{
    sEmbeddedRawTable = table;
    sEmbeddedRawCount = count;
}

// Canonicalise a path for VFS lookup: forward-slashes only, strip a leading
// "./" if present. Must match the format ActionManager emits as mLookupKey.
static void CanonicaliseVfsKey(const char* in, char* out, size_t outCap)
{
    if (in == nullptr || outCap == 0) { if (outCap) out[0] = 0; return; }

    if (in[0] == '.' && (in[1] == '/' || in[1] == '\\'))
    {
        in += 2;
    }

    size_t i = 0;
    while (in[i] != 0 && i + 1 < outCap)
    {
        char c = in[i];
        if (c == '\\') c = '/';
        out[i] = c;
        ++i;
    }
    out[i] = 0;
}

const char* SYS_LookupEmbeddedRawAsset(const char* path, uint32_t& outSize)
{
    outSize = 0;
    if (path == nullptr || path[0] == 0) return nullptr;
    if (sEmbeddedRawTable == nullptr || sEmbeddedRawCount == 0) return nullptr;

    char canonical[512];
    CanonicaliseVfsKey(path, canonical, sizeof(canonical));

    for (uint32_t i = 0; i < sEmbeddedRawCount; ++i)
    {
        const EmbeddedFile& f = sEmbeddedRawTable[i];
        if (f.mName == nullptr) continue;
        if (strcmp(f.mName, canonical) == 0)
        {
            outSize = f.mSize;
            return f.mData;
        }
    }
    return nullptr;
}

// ============================================================================
//  Seekable, handle-based file reading (see System.h). Implemented once here so
//  every platform links it (shared System source, compiled everywhere). Used to
//  stream large assets (e.g. background music) from disk in chunks instead of
//  holding the whole file resident.
//
//  Path resolution: try the raw path first (works where the CWD is already the
//  asset root — Windows/Linux), then fall back to SYS_GetAbsolutePath, which each
//  platform implements to add its device prefix (e.g. "host:" on PS2). Embedded
//  (in-exe/romfs) assets that fopen can't open just return nullptr → the caller
//  degrades gracefully (streaming silently no-ops there).
//
//  Static builds wrap shipped .oct files in a ContentObfuscation container, and
//  SoundWave records its PCM offset in *decoded* space (SoundWave::LoadStream
//  stores stream.GetPos()). So the handle carries the payload base and salt, maps
//  logical offsets onto physical ones on seek, and decodes each chunk as it is
//  read. Doing it here rather than per-platform is what keeps streaming audio
//  working on GameCube, 3DS and the out-of-tree addon targets with no extra work.
// ============================================================================
struct SysFile
{
    FILE* mFile = nullptr;
    uint32_t mPayloadBase = 0;   // physical offset of decoded byte 0
    uint32_t mDecodedPos = 0;    // logical read cursor, in decoded space
    uint32_t mSalt = 0;
    bool mObfuscated = false;
};

SysFile* SYS_FileOpenRead(const char* path, bool /*isAsset*/)
{
    if (path == nullptr) return nullptr;

    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        const std::string resolved = SYS_GetAbsolutePath(path);
        file = fopen(resolved.c_str(), "rb");
    }

    if (file == nullptr) return nullptr;

    SysFile* handle = new SysFile();
    handle->mFile = file;

    char header[ContentObfuscation::kHeaderSize];
    if (fread(header, 1, sizeof(header), file) == sizeof(header) &&
        ContentObfuscation::IsContainer(header, (uint32_t)sizeof(header)))
    {
        handle->mObfuscated = true;
        handle->mPayloadBase = ContentObfuscation::kHeaderSize;
        handle->mSalt = ContentObfuscation::GetSalt(header);
    }

    fseek(file, (long)handle->mPayloadBase, SEEK_SET);
    return handle;
}

uint32_t SYS_FileRead(SysFile* file, void* dst, uint32_t bytes)
{
    if (file == nullptr || file->mFile == nullptr || dst == nullptr || bytes == 0) return 0;

    const uint32_t read = (uint32_t)fread(dst, 1, bytes, file->mFile);

    if (file->mObfuscated && read > 0)
    {
        ContentObfuscation::DecodeRange(dst, read, file->mDecodedPos, file->mSalt);
    }

    file->mDecodedPos += read;
    return read;
}

bool SYS_FileSeek(SysFile* file, uint64_t absoluteOffset)
{
    if (file == nullptr || file->mFile == nullptr) return false;

    file->mDecodedPos = (uint32_t)absoluteOffset;

    // Music/asset files are well under 2 GB, so a `long` offset is sufficient and
    // portable across the 32-bit console newlibs (avoids _fseeki64/fseeko split).
    return fseek(file->mFile, (long)(file->mPayloadBase + absoluteOffset), SEEK_SET) == 0;
}

void SYS_FileClose(SysFile* file)
{
    if (file == nullptr) return;

    if (file->mFile != nullptr) fclose(file->mFile);
    delete file;
}
