#if EDITOR

#include "LinuxHostShell.h"

#include "System/System.h"
#include "Utilities.h"
#include "Log.h"

#include <cctype>
#include <fstream>

namespace LinuxHostShell
{
    bool UsesWsl()
    {
#if PLATFORM_WINDOWS
        return true;
#else
        return false;
#endif
    }

    std::string ToShellPath(const std::string& hostPath)
    {
        if (hostPath.empty())
            return hostPath;

        // Already POSIX — the user typed a WSL-side path, or we're on Linux.
        if (hostPath[0] == '/')
            return hostPath;

        if (!UsesWsl())
            return hostPath;

        std::string result = hostPath;
        for (char& c : result)
        {
            if (c == '\\') c = '/';
        }

        // "M:/foo" -> "/mnt/m/foo". Anything else (UNC paths, relative paths)
        // is passed through with separators normalised only — WSL can't reach
        // a UNC share by drive letter and we shouldn't pretend otherwise.
        if (result.size() >= 2 && result[1] == ':' && std::isalpha((unsigned char)result[0]))
        {
            const char drive = (char)std::tolower((unsigned char)result[0]);
            result = std::string("/mnt/") + drive + result.substr(2);
        }

        return result;
    }

    std::string Quote(const std::string& str)
    {
        std::string out = "'";
        for (char c : str)
        {
            if (c == '\'')
            {
                // Close the quote, emit an escaped quote, reopen.
                out += "'\\''";
            }
            else
            {
                out += c;
            }
        }
        out += "'";
        return out;
    }

    namespace
    {
        // Quote a single argument for whatever sits between us and bash.
        //
        // Windows: SYS_ExecFull prepends `cmd.exe /c`, and wsl.exe then applies
        // Windows argv rules — double quotes group and are stripped, single
        // quotes are literal. Passing a POSIX-single-quoted path here hands bash
        // an argument with the quotes still attached.
        //
        // Linux: SYS_ExecFull uses popen, so the outer layer is `/bin/sh -c`
        // and POSIX single quoting is what's wanted.
        std::string QuoteOuterArg(const std::string& arg)
        {
#if PLATFORM_WINDOWS
            return "\"" + arg + "\"";
#else
            return Quote(arg);
#endif
        }

        std::string WslPrefix(const std::string& wslDistro)
        {
            if (!UsesWsl())
                return "";

            std::string prefix = "wsl ";
            if (!wslDistro.empty())
            {
                prefix += "-d " + wslDistro + " ";
            }
            return prefix;
        }
    }

    std::string BuildCommand(const std::string& body, const std::string& wslDistro)
    {
        std::string escaped;
        escaped.reserve(body.size() + 16);

#if PLATFORM_WINDOWS
        // cmd.exe is the outer shell here. It does not treat '\' as an escape
        // character, and '$' / backtick are literal to it — escaping those the
        // way a POSIX shell needs would leak stray backslashes through to bash.
        // '>' and '&' inside double quotes are literal to cmd.exe, so simple
        // redirecting probes pass through intact. Callers must keep '"' and
        // '%' out of the body (see the header note); RunScript exists for
        // anything more involved.
        escaped = body;
#else
        // popen gives us `/bin/sh -c "..."`, and that outer sh does expand
        // these inside the double quotes.
        for (char c : body)
        {
            if (c == '"' || c == '\\' || c == '$' || c == '`')
            {
                escaped += '\\';
            }
            escaped += c;
        }
#endif

        return WslPrefix(wslDistro) + "bash -lc \"" + escaped + "\"";
    }

    int32_t Run(const std::string& body, std::string* outOutput, const std::string& wslDistro)
    {
        const std::string cmd = BuildCommand(body, wslDistro);

        std::string stdOut;
        std::string stdErr;
        int exitCode = -1;

        if (!SYS_ExecFull(cmd.c_str(), &stdOut, &stdErr, &exitCode))
        {
            if (outOutput != nullptr)
            {
                *outOutput = "Failed to start: " + cmd;
            }
            return -1;
        }

        if (outOutput != nullptr)
        {
            *outOutput = stdOut;
            if (!stdErr.empty())
            {
                if (!outOutput->empty()) *outOutput += "\n";
                *outOutput += stdErr;
            }
        }

        return (int32_t)exitCode;
    }

    int32_t RunScript(const std::string& scriptBody, const std::string& hostScriptPath,
                      std::string* outOutput, const std::string& wslDistro)
    {
        if (!WriteTextFileLF(hostScriptPath, scriptBody))
        {
            if (outOutput != nullptr)
            {
                *outOutput = "Failed to write script: " + hostScriptPath;
            }
            return -1;
        }

        const std::string cmd = WslPrefix(wslDistro) +
                                "bash " + QuoteOuterArg(ToShellPath(hostScriptPath));

        std::string stdOut;
        std::string stdErr;
        int exitCode = -1;

        if (!SYS_ExecFull(cmd.c_str(), &stdOut, &stdErr, &exitCode))
        {
            if (outOutput != nullptr)
            {
                *outOutput = "Failed to start: " + cmd;
            }
            return -1;
        }

        if (outOutput != nullptr)
        {
            *outOutput = stdOut;
            if (!stdErr.empty())
            {
                if (!outOutput->empty()) *outOutput += "\n";
                *outOutput += stdErr;
            }
        }

        return (int32_t)exitCode;
    }

    bool EnsureHostDir(const std::string& hostPath)
    {
        if (hostPath.empty())
            return false;

        if (DoesDirExist(hostPath.c_str()))
            return true;

        size_t sep = hostPath.find_last_of("/\\");
        if (sep != std::string::npos && sep > 0)
        {
            EnsureHostDir(hostPath.substr(0, sep));
        }

        return SYS_CreateDirectory(hostPath.c_str()) || DoesDirExist(hostPath.c_str());
    }

    bool HasTool(const std::string& tool, const std::string& wslDistro)
    {
        if (tool.empty())
            return false;

        return Run("command -v " + Quote(tool) + " >/dev/null 2>&1", nullptr, wslDistro) == 0;
    }

    std::string GetOption(const PolyphaseBuildContext* ctx, const char* key, const std::string& fallback)
    {
        if (ctx == nullptr || ctx->GetProfileSetting == nullptr || key == nullptr)
            return fallback;

        char buf[1024] = {0};
        if (ctx->GetProfileSetting(key, buf, sizeof(buf)) == 0 || buf[0] == '\0')
            return fallback;

        return std::string(buf);
    }

    void Report(const PolyphaseBuildContext* ctx, const std::string& line)
    {
        if (ctx != nullptr && ctx->WriteOutputLine != nullptr)
        {
            ctx->WriteOutputLine(line.c_str());
        }

        LogDebug("%s", line.c_str());
    }

    bool WriteTextFileLF(const std::string& hostPath, const std::string& contents)
    {
        std::ofstream file(hostPath.c_str(), std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            LogError("Failed to open for write: %s", hostPath.c_str());
            return false;
        }

        file.write(contents.data(), (std::streamsize)contents.size());
        return file.good();
    }
}

#endif /* EDITOR */
