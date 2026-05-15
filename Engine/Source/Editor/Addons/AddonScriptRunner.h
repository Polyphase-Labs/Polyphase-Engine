#pragma once

#if EDITOR

#include <string>

/**
 * @brief Executes the optional onInstall shell script declared by an addon.
 *
 * Trust model:
 *   - Addons whose ID starts with "com.polyphase." run their install scripts silently.
 *   - All others trigger a synchronous trust prompt (native OS dialog on Windows;
 *     logged-and-skipped on Linux unless the InstalledAddon record already
 *     carries a sticky trust flag).
 *
 * Bash discovery (Windows): Git for Windows -> 32-bit Git -> PATH -> none.
 * On Linux/Mac the interpreter is simply "bash" via PATH.
 */
namespace AddonScriptRunner
{
    enum class TrustResult
    {
        TrustOnce,      // Run script this time but don't remember
        TrustAlways,    // Run script and persist trust in installed_addons.json
        Skip,           // Don't run the script; keep the addon installed
        CancelInstall,  // Don't run the script and revert the install
    };

    /// True for first-party addon IDs (prefix "com.polyphase.").
    bool IsTrustedAddonId(const std::string& id);

    /// Absolute path to a bash interpreter, or empty if none was found.
    std::string FindBashInterpreter();

    /// Show the trust prompt for an addon. Synchronous.
    TrustResult ShowTrustModal(const std::string& addonId,
                               const std::string& source,
                               const std::string& scriptRel);

    /// Run an addon's onInstall script. Returns true if the script ran AND exited 0.
    /// outError is populated on any failure path. scriptRel is the path stored in
    /// package.json (relative to addonDir).
    bool RunOnInstall(const std::string& addonId,
                      const std::string& addonDir,
                      const std::string& scriptRel,
                      std::string& outError);
}

#endif
