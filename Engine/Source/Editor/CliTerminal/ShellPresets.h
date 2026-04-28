#pragma once

#if EDITOR

#include <string>
#include <vector>

/**
 * @brief Built-in shell presets exposed in Preferences.
 *
 * Numeric values are persisted in the JSON settings file, so DO NOT reorder
 * existing entries — append new ones at the end and bump the JSON version
 * if reordering becomes unavoidable.
 */
enum class ShellPreset
{
    Auto = 0,       // Pick a sensible default per platform
    Custom = 1,     // Use mCustomExe + parsed mCustomArgs
    Cmd = 2,        // Windows: cmd.exe
    PowerShell = 3, // Windows: powershell.exe (Windows-bundled v5.1)
    Pwsh = 4,       // Cross-platform PowerShell 7+
    GitBash = 5,    // Windows: probes Git for Windows install paths
    Msys2Bash = 6,  // Windows: probes msys64/msys32 install paths
    Bash = 7,       // POSIX: /bin/bash
    Zsh = 8,        // POSIX: /bin/zsh
    Sh = 9,         // POSIX: /bin/sh
    Count
};

/** Resolved executable + default args ready to merge with user-supplied args. */
struct ResolvedShell
{
    std::string mExecutable;
    std::vector<std::string> mArgs;
    std::vector<std::pair<std::string, std::string>> mExtraEnv; // e.g. MSYSTEM=MSYS for Msys2Bash
    bool mResolved = false;
    std::string mError;  // populated when mResolved == false
};

/** Look up a preset; for Custom, uses customExe/customArgs verbatim. */
ResolvedShell ResolveShellPreset(ShellPreset preset,
                                 const std::string& customExe,
                                 const std::vector<std::string>& customArgs);

/** Display name for the View menu / dropdown. */
const char* ShellPresetDisplayName(ShellPreset preset);

/** Parse a flat "arg1 arg2 \"quoted arg\"" command line into a vector. */
std::vector<std::string> SplitCommandLine(const std::string& line);

#endif
