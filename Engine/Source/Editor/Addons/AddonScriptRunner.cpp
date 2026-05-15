#if EDITOR

#include "AddonScriptRunner.h"

#include "Log.h"
#include "System/System.h"

#if PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <cstdlib>
#include <string>

namespace
{
    const char* PlatformName()
    {
#if PLATFORM_WINDOWS
        return "Windows";
#elif PLATFORM_LINUX
        return "Linux";
#elif PLATFORM_MAC
        return "Mac";
#else
        return "Unknown";
#endif
    }

    std::string Quote(const std::string& s)
    {
        return std::string("\"") + s + "\"";
    }
}

namespace AddonScriptRunner
{
    bool IsTrustedAddonId(const std::string& id)
    {
        const std::string prefix = "com.polyphase.";
        return id.size() >= prefix.size() && id.compare(0, prefix.size(), prefix) == 0;
    }

    std::string FindBashInterpreter()
    {
#if PLATFORM_WINDOWS
        const char* candidates[] = {
            "C:\\Program Files\\Git\\bin\\bash.exe",
            "C:\\Program Files (x86)\\Git\\bin\\bash.exe",
        };
        for (const char* c : candidates)
        {
            if (SYS_DoesFileExist(c, false))
            {
                return c;
            }
        }

        // Fall back to PATH lookup via `where`.
        std::string out;
        SYS_Exec("where bash", &out);
        if (!out.empty())
        {
            // Trim to first line, strip CR/LF.
            size_t nl = out.find_first_of("\r\n");
            if (nl != std::string::npos) out.resize(nl);
            if (!out.empty() && SYS_DoesFileExist(out.c_str(), false))
            {
                return out;
            }
        }
        return std::string();
#else
        // bash is virtually always in PATH on Linux/Mac
        return "bash";
#endif
    }

    TrustResult ShowTrustModal(const std::string& addonId,
                               const std::string& source,
                               const std::string& scriptRel)
    {
#if PLATFORM_WINDOWS
        std::string msg;
        msg += "Addon: " + addonId + "\n";
        if (!source.empty()) msg += "Source: " + source + "\n";
        msg += "Script: " + scriptRel + "\n\n";
        msg += "This addon wants to run an install script. Install scripts "
               "execute arbitrary code on your machine and can read or modify "
               "any file your user account can.\n\n"
               "Only proceed if you trust the addon's author.\n\n"
               "  Yes     = Always trust this addon (remember and run script)\n"
               "  No      = Run script once (don't remember)\n"
               "  Cancel  = Skip script (keep addon installed but don't run script)";

        int rc = MessageBoxA(NULL, msg.c_str(),
                             "Trust install script?",
                             MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3 | MB_TOPMOST);
        switch (rc)
        {
            case IDYES:    return TrustResult::TrustAlways;
            case IDNO:     return TrustResult::TrustOnce;
            case IDCANCEL: return TrustResult::Skip;
            default:       return TrustResult::Skip;
        }
#else
        LogWarning("Addon '%s' declares an onInstall script ('%s'). Skipping (no trust UI on this platform). "
                   "Set the trusted-scripts flag on this addon manually to enable it.",
                   addonId.c_str(), scriptRel.c_str());
        return TrustResult::Skip;
#endif
    }

    bool RunOnInstall(const std::string& addonId,
                      const std::string& addonDir,
                      const std::string& scriptRel,
                      std::string& outError)
    {
        outError.clear();

        if (scriptRel.empty())
        {
            return true;
        }

        std::string scriptAbs = addonDir;
        if (!scriptAbs.empty() && scriptAbs.back() != '/' && scriptAbs.back() != '\\')
        {
            scriptAbs += "/";
        }
        scriptAbs += scriptRel;

        if (!SYS_DoesFileExist(scriptAbs.c_str(), false))
        {
            outError = "onInstall script not found: " + scriptAbs;
            LogWarning("Addon '%s': %s", addonId.c_str(), outError.c_str());
            return false;
        }

        std::string bash = FindBashInterpreter();
        if (bash.empty())
        {
            outError = "Bash interpreter not found. Install Git for Windows or add bash to PATH.";
            LogWarning("Addon '%s': %s", addonId.c_str(), outError.c_str());
#if PLATFORM_WINDOWS
            MessageBoxA(NULL,
                        "This addon's install script requires bash, but none was found on this machine. "
                        "Install Git for Windows (https://git-scm.com/download/win) and reinstall the addon "
                        "to enable its install script.",
                        "Bash not found",
                        MB_OK | MB_ICONWARNING | MB_TOPMOST);
#endif
            return false;
        }

        std::string cmd = Quote(bash) + " " + Quote(scriptAbs) +
                          " " + PlatformName() + " " + Quote(addonDir);

        LogDebug("Addon '%s': running onInstall: %s", addonId.c_str(), cmd.c_str());

        std::string out;
        std::string err;
        int exitCode = -1;
        SYS_ExecFull(cmd.c_str(), &out, &err, &exitCode);

        if (!out.empty()) LogDebug("[%s onInstall stdout]\n%s", addonId.c_str(), out.c_str());
        if (!err.empty()) LogWarning("[%s onInstall stderr]\n%s", addonId.c_str(), err.c_str());

        if (exitCode != 0)
        {
            outError = "onInstall script exited " + std::to_string(exitCode);
            LogWarning("Addon '%s': %s", addonId.c_str(), outError.c_str());
            return false;
        }

        return true;
    }
}

#endif
