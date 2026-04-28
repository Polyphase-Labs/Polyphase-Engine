#if EDITOR

#include "ShellPresets.h"

#include <cstdlib>

#if PLATFORM_WINDOWS
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    bool FileExistsLocal(const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }

    std::string GetEnvOr(const char* name, const char* fallback)
    {
        const char* v = std::getenv(name);
        if (v != nullptr && v[0] != '\0')
        {
            return std::string(v);
        }
        return std::string(fallback);
    }

    /** Search a list of candidate paths and return the first that exists. */
    std::string PickFirstExisting(std::initializer_list<const char*> candidates)
    {
        for (const char* c : candidates)
        {
            if (c != nullptr && FileExistsLocal(c))
            {
                return std::string(c);
            }
        }
        return std::string();
    }

#if PLATFORM_WINDOWS
    std::string ResolveGitBash()
    {
        std::string r = PickFirstExisting({
            "C:\\Program Files\\Git\\bin\\bash.exe",
            "C:\\Program Files (x86)\\Git\\bin\\bash.exe",
        });
        if (!r.empty())
        {
            return r;
        }
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData != nullptr)
        {
            std::string p = std::string(localAppData) + "\\Programs\\Git\\bin\\bash.exe";
            if (FileExistsLocal(p))
            {
                return p;
            }
        }
        return std::string();
    }

    std::string ResolveMsys2Bash()
    {
        return PickFirstExisting({
            "C:\\msys64\\usr\\bin\\bash.exe",
            "C:\\msys32\\usr\\bin\\bash.exe",
        });
    }
#endif
}

const char* ShellPresetDisplayName(ShellPreset preset)
{
    switch (preset)
    {
        case ShellPreset::Auto:       return "Auto";
        case ShellPreset::Custom:     return "Custom";
        case ShellPreset::Cmd:        return "cmd";
        case ShellPreset::PowerShell: return "PowerShell";
        case ShellPreset::Pwsh:       return "pwsh";
        case ShellPreset::GitBash:    return "Git Bash";
        case ShellPreset::Msys2Bash:  return "MSYS2 bash";
        case ShellPreset::Bash:       return "bash";
        case ShellPreset::Zsh:        return "zsh";
        case ShellPreset::Sh:         return "sh";
        default:                      return "?";
    }
}

ResolvedShell ResolveShellPreset(ShellPreset preset,
                                 const std::string& customExe,
                                 const std::vector<std::string>& customArgs)
{
    ResolvedShell out;

    auto fail = [&out](const std::string& msg) {
        out.mResolved = false;
        out.mError = msg;
        return out;
    };

    if (preset == ShellPreset::Custom)
    {
        if (customExe.empty())
        {
            return fail("Custom preset selected but no executable path is set.");
        }
        out.mExecutable = customExe;
        out.mArgs = customArgs;
        out.mResolved = true;
        return out;
    }

#if PLATFORM_WINDOWS
    switch (preset)
    {
        case ShellPreset::Auto:
        case ShellPreset::Cmd:
        {
            if (preset == ShellPreset::Auto)
            {
                out.mExecutable = GetEnvOr("COMSPEC", "C:\\Windows\\System32\\cmd.exe");
            }
            else
            {
                out.mExecutable = "C:\\Windows\\System32\\cmd.exe";
            }
            out.mArgs = { "/Q" };
            break;
        }
        case ShellPreset::PowerShell:
        {
            out.mExecutable = "powershell.exe";
            out.mArgs = { "-NoLogo", "-NoExit" };
            break;
        }
        case ShellPreset::Pwsh:
        {
            out.mExecutable = "pwsh.exe";
            out.mArgs = { "-NoLogo", "-NoExit" };
            break;
        }
        case ShellPreset::GitBash:
        {
            out.mExecutable = ResolveGitBash();
            if (out.mExecutable.empty())
            {
                return fail("Git Bash not found in standard install locations.");
            }
            out.mArgs = { "--login", "-i" };
            break;
        }
        case ShellPreset::Msys2Bash:
        {
            out.mExecutable = ResolveMsys2Bash();
            if (out.mExecutable.empty())
            {
                return fail("MSYS2 bash not found in standard install locations.");
            }
            out.mArgs = { "--login", "-i" };
            out.mExtraEnv.emplace_back("MSYSTEM", "MSYS");
            break;
        }
        case ShellPreset::Bash:
        case ShellPreset::Zsh:
        case ShellPreset::Sh:
            return fail("POSIX shell preset selected on Windows.");
        default:
            return fail("Unknown shell preset.");
    }
#else
    switch (preset)
    {
        case ShellPreset::Auto:
        {
            out.mExecutable = GetEnvOr("SHELL", "/bin/bash");
            out.mArgs = { "-i" };
            break;
        }
        case ShellPreset::Bash:
        {
            out.mExecutable = FileExistsLocal("/bin/bash") ? "/bin/bash" : "/usr/bin/bash";
            out.mArgs = { "-i" };
            break;
        }
        case ShellPreset::Zsh:
        {
            out.mExecutable = FileExistsLocal("/bin/zsh") ? "/bin/zsh" : "/usr/bin/zsh";
            out.mArgs = { "-i" };
            break;
        }
        case ShellPreset::Sh:
        {
            out.mExecutable = "/bin/sh";
            out.mArgs = { "-i" };
            break;
        }
        case ShellPreset::Cmd:
        case ShellPreset::PowerShell:
        case ShellPreset::Pwsh:
        case ShellPreset::GitBash:
        case ShellPreset::Msys2Bash:
            return fail("Windows-only shell preset selected on POSIX.");
        default:
            return fail("Unknown shell preset.");
    }
#endif

    // Append any user-supplied args after the preset defaults.
    for (const std::string& a : customArgs)
    {
        if (!a.empty())
        {
            out.mArgs.push_back(a);
        }
    }

    out.mResolved = true;
    return out;
}

std::vector<std::string> SplitCommandLine(const std::string& line)
{
    // Minimal splitter: spaces separate args, double quotes group, backslash
    // before a quote escapes it. Sufficient for the Custom Args field; users
    // who need exotic quoting should use Custom preset with explicit args.
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    bool any = false;

    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];

        if (c == '\\' && i + 1 < line.size() && line[i + 1] == '"')
        {
            cur.push_back('"');
            ++i;
            any = true;
            continue;
        }

        if (c == '"')
        {
            inQuotes = !inQuotes;
            any = true;
            continue;
        }

        if (!inQuotes && (c == ' ' || c == '\t'))
        {
            if (any)
            {
                out.push_back(std::move(cur));
                cur.clear();
                any = false;
            }
            continue;
        }

        cur.push_back(c);
        any = true;
    }

    if (any)
    {
        out.push_back(std::move(cur));
    }

    return out;
}

#endif
