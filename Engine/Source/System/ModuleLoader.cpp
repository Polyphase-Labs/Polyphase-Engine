#if EDITOR

#include "ModuleLoader.h"
#include "Log.h"

#include <string>

#if PLATFORM_WINDOWS

#include <Windows.h>

static std::string sLastError;

void* MOD_Load(const char* path)
{
    sLastError.clear();

    // Native addons may ship third-party DLLs (e.g. FFmpeg) alongside their own DLL. For
    // LoadLibrary to find those as transitive imports we add the addon DLL's directory
    // to the dynamic search set and use the *USER_DIRS* flag so Windows searches
    // AddDllDirectory-registered paths when resolving imports.
    //
    // LOAD_WITH_ALTERED_SEARCH_PATH alone is insufficient — it covers the *first-level*
    // import resolution when loading an absolute-path DLL, but transitive imports (e.g.
    // avcodec-62.dll -> swresample-6.dll) fall back to the standard search order, which
    // does NOT include the addon's own directory.

    // Extract the directory containing the DLL.
    std::string dllPath(path);
    std::string dllDir;
    size_t lastSlash = dllPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        dllDir = dllPath.substr(0, lastSlash);
    }

    // AddDllDirectory takes a wide string. Convert from UTF-8/ANSI.
    DLL_DIRECTORY_COOKIE cookie = nullptr;
    if (!dllDir.empty())
    {
        int wlen = MultiByteToWideChar(CP_ACP, 0, dllDir.c_str(), -1, nullptr, 0);
        if (wlen > 0)
        {
            std::wstring wdir(wlen, L'\0');
            MultiByteToWideChar(CP_ACP, 0, dllDir.c_str(), -1, &wdir[0], wlen);
            // Trim the trailing null that the wide buffer includes.
            if (!wdir.empty() && wdir.back() == L'\0') wdir.pop_back();
            cookie = AddDllDirectory(wdir.c_str());
        }
    }

    DWORD flags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS;
    HMODULE module = LoadLibraryExA(path, nullptr, flags);
    if (module == nullptr)
    {
        // Fall back to the older semantics in case the newer flags aren't honored (pre-Win8).
        module = LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }

    if (module == nullptr)
    {
        DWORD error = GetLastError();
        char buffer[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer,
            sizeof(buffer),
            nullptr
        );
        sLastError = buffer;
        LogError("MOD_Load failed for '%s' (err %lu): %s", path, (unsigned long)error, sLastError.c_str());
    }

    // Keep the added directory registered for the lifetime of the module so subsequent
    // delay-loaded imports also resolve. We accept the minor leak because the cookie
    // would otherwise be lost; addons never unload in the editor workflow.
    (void)cookie;

    return static_cast<void*>(module);
}

void* MOD_Symbol(void* handle, const char* name)
{
    sLastError.clear();

    if (handle == nullptr)
    {
        sLastError = "Invalid module handle";
        return nullptr;
    }

    FARPROC proc = GetProcAddress(static_cast<HMODULE>(handle), name);
    if (proc == nullptr)
    {
        DWORD error = GetLastError();
        char buffer[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer,
            sizeof(buffer),
            nullptr
        );
        sLastError = buffer;
    }

    return reinterpret_cast<void*>(proc);
}

void MOD_Unload(void* handle)
{
    sLastError.clear();

    if (handle != nullptr)
    {
        if (!FreeLibrary(static_cast<HMODULE>(handle)))
        {
            DWORD error = GetLastError();
            char buffer[256];
            FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buffer,
                sizeof(buffer),
                nullptr
            );
            sLastError = buffer;
            LogWarning("MOD_Unload failed: %s", sLastError.c_str());
        }
    }
}

const char* MOD_GetError()
{
    return sLastError.c_str();
}

#elif PLATFORM_LINUX

#include <dlfcn.h>

static std::string sLastError;

void* MOD_Load(const char* path)
{
    sLastError.clear();

    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        const char* error = dlerror();
        sLastError = error ? error : "Unknown error";
        LogError("MOD_Load failed for '%s': %s", path, sLastError.c_str());
    }

    return handle;
}

void* MOD_Symbol(void* handle, const char* name)
{
    sLastError.clear();

    if (handle == nullptr)
    {
        sLastError = "Invalid module handle";
        return nullptr;
    }

    // Clear any existing error
    dlerror();

    void* symbol = dlsym(handle, name);

    const char* error = dlerror();
    if (error != nullptr)
    {
        sLastError = error;
        symbol = nullptr;
    }

    return symbol;
}

void MOD_Unload(void* handle)
{
    sLastError.clear();

    if (handle != nullptr)
    {
        if (dlclose(handle) != 0)
        {
            const char* error = dlerror();
            sLastError = error ? error : "Unknown error";
            LogWarning("MOD_Unload failed: %s", sLastError.c_str());
        }
    }
}

const char* MOD_GetError()
{
    return sLastError.c_str();
}

#else

// Stub implementations for unsupported platforms
static const char* sUnsupportedError = "Module loading not supported on this platform";

void* MOD_Load(const char* path)
{
    LogError("%s", sUnsupportedError);
    return nullptr;
}

void* MOD_Symbol(void* handle, const char* name)
{
    return nullptr;
}

void MOD_Unload(void* handle)
{
}

const char* MOD_GetError()
{
    return sUnsupportedError;
}

#endif // Platform

#endif // EDITOR
