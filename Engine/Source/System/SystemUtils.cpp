#include "SystemUtils.h"
#include "System.h"
#include "Log.h"
#include "EmbeddedFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void ExecCommon(const char* cmd, std::string* output)
{
#if defined(POLYPHASE_PLATFORM_ADDON)
    // Addon-runtime platforms (PSP etc.) have no subprocess facility — newlib
    // ships no popen/pclose and the kernel has no fork/exec. Just log + return.
    (void)cmd;
    if (output) output->clear();
    LogDebug("[Exec] (no subprocess support on this platform): %s", cmd ? cmd : "");
#else
#if PLATFORM_WINDOWS
#define popen _popen
#define pclose _pclose
#endif

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

#if PLATFORM_WINDOWS
#undef popen
#undef pclose
#endif
#endif // POLYPHASE_PLATFORM_ADDON
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
#else
#if PLATFORM_WINDOWS
#define popen _popen
#define pclose _pclose
#endif

    LogDebug("[ExecFull] %s", cmd);

    // Build command that captures both stdout and stderr
    // On Windows, we redirect stderr to stdout with 2>&1
    // On Linux, same approach works
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

#if PLATFORM_WINDOWS || PLATFORM_3DS || PLATFORM_DOLPHIN
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

#if PLATFORM_WINDOWS
#undef popen
#undef pclose
#endif
#endif // POLYPHASE_PLATFORM_ADDON
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
