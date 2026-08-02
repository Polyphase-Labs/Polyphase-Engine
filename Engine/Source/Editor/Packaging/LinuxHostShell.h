#pragma once

#if EDITOR

#include <string>
#include <cstdint>

#include "Plugins/PolyphaseBuildTargetAPI.h"

/**
 * Shared plumbing for build targets that drive Linux packaging tools
 * (rpmbuild, appimagetool, mksquashfs). On a Linux host these run directly;
 * on a Windows host they run inside WSL, which means host paths have to be
 * translated to their /mnt/<drive> equivalents first.
 *
 * The same approach is used by the out-of-tree LinuxARM64 build-target addon,
 * which drives a cross-toolchain through `wsl bash -lc`.
 */
namespace LinuxHostShell
{
    /** True when commands have to be routed through WSL (i.e. Windows host). */
    bool UsesWsl();

    /**
     * Translate a host path into one the Linux shell can see.
     * Windows: "M:\Projects\My Game" -> "/mnt/m/Projects/My Game".
     * Paths that already look POSIX (leading '/') pass through unchanged, so
     * users can type WSL-side paths into profile options.
     */
    std::string ToShellPath(const std::string& hostPath);

    /**
     * Single-quote for POSIX sh, escaping any embedded single quotes.
     * This is for text that bash itself will parse — inside a RunScript body,
     * or inside a Run body. It is NOT correct for the outer command line,
     * which follows host rules; Run and RunScript handle that themselves.
     */
    std::string Quote(const std::string& str);

    /**
     * Wrap a shell body into a command line SYS_Exec / SYS_ExecFull can run.
     * Windows: wsl [-d <distro>] bash -lc "<body>"
     * Linux:   bash -lc "<body>"
     * wslDistro is ignored on a Linux host.
     */
    std::string BuildCommand(const std::string& body, const std::string& wslDistro = "");

    /**
     * Run a short shell body and capture stdout+stderr combined. Returns the
     * exit code, or -1 if the process could not be started at all.
     *
     * The body is nested inside `bash -lc "..."`, and the layer outside that
     * differs by host — cmd.exe on Windows, /bin/sh on Linux — so the two
     * disagree about escaping. Keep bodies free of '"', '%', '$' and backticks;
     * anything needing real shell syntax belongs in RunScript, where only a
     * quoted script path crosses the boundary.
     */
    int32_t Run(const std::string& body, std::string* outOutput = nullptr,
                const std::string& wslDistro = "");

    /**
     * Write `scriptBody` to `hostScriptPath` (LF endings) and execute it.
     * Only the script path crosses the shell boundary, so the script itself can
     * use variables, command substitution and heredocs freely. Prefer this over
     * Run for anything beyond a one-liner probe.
     */
    int32_t RunScript(const std::string& scriptBody, const std::string& hostScriptPath,
                      std::string* outOutput = nullptr, const std::string& wslDistro = "");

    /** mkdir -p on the host filesystem (not through the Linux shell). */
    bool EnsureHostDir(const std::string& hostPath);

    /**
     * True if `tool` resolves on PATH inside the Linux shell. Used by Validate
     * callbacks — note those run on a background thread with a TTL cache
     * precisely because this can block on WSL startup for several seconds.
     */
    bool HasTool(const std::string& tool, const std::string& wslDistro = "");

    /** Read a profile option, returning `fallback` when unset or empty. */
    std::string GetOption(const PolyphaseBuildContext* ctx, const char* key,
                          const std::string& fallback = "");

    /** Forward a line to the build-output log, falling back to the engine log. */
    void Report(const PolyphaseBuildContext* ctx, const std::string& line);

    /**
     * Open a file for writing in BINARY mode. Shell scripts and RPM scriptlets
     * written from a Windows host must not get CRLF line endings — a wrapper
     * script with CRLF dies with "bad interpreter". Every generated file in
     * this packaging path goes through here.
     */
    bool WriteTextFileLF(const std::string& hostPath, const std::string& contents);
}

#endif /* EDITOR */
