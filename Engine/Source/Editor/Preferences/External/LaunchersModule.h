#pragma once

#if EDITOR

#include "../PreferencesModule.h"
#include "EngineTypes.h"
#include <string>
#include <vector>

/**
 * @brief Preferences module for configuring external emulator and tool paths.
 *
 * This module allows users to configure paths and command-line arguments
 * for emulators (Dolphin, Azahar/Citra) and tools (3dslink) used in
 * the Build & Run workflow.
 *
 * Settings are stored in user preferences and persist across projects.
 */
class LaunchersModule : public PreferencesModule
{
public:
    DECLARE_PREFERENCES_MODULE(LaunchersModule)

    LaunchersModule();
    virtual ~LaunchersModule();

    virtual const char* GetName() const override { return GetStaticName(); }
    virtual const char* GetParentPath() const override { return GetStaticParentPath(); }
    virtual void Render() override;
    virtual void LoadSettings(const rapidjson::Document& doc) override;
    virtual void SaveSettings(rapidjson::Document& doc) override;

    /** @brief Path to Dolphin emulator executable (GameCube/Wii) */
    std::string mDolphinPath;

    /** @brief Command-line arguments for Dolphin. Supports placeholders: {emulator}, {output}, {outputdir} */
    std::string mDolphinArgs = "{emulator} --batch -e {output}";

    /** @brief Path to Azahar/Citra emulator executable (3DS) */
    std::string mAzaharPath;

    /** @brief Command-line arguments for Azahar. Supports placeholders: {emulator}, {output}, {outputdir} */
    std::string mAzaharArgs = "{emulator} {output}";

    /**
     * @brief Checks if an emulator is configured for the given platform.
     * @param platform The target platform
     * @return True if the emulator path is set for that platform
     */
    bool IsEmulatorConfigured(Platform platform) const;

    /**
     * @brief Checks if 3dslink is configured.
     * @return True if 3dslink can be used
     */
    bool Is3dsLinkConfigured() const;

    /**
     * @brief Checks if wiiload is configured.
     * @return True if wiiload can be used (devkitPro installed and IP set)
     */
    bool IsWiiloadConfigured() const;

    /** @brief IP address of Wii for wiiload */
    std::string mWiiloadIP;

    /**
     * @brief IP address of 3DS for 3dslink (optional).
     *
     * When empty, 3dslink falls back to UDP broadcast discovery, which is
     * unreliable on hosts with multiple NICs, Wi-Fi access points that
     * isolate clients, or firewalls that block outbound 17491/UDP. Setting
     * this passes `-a <ip>` to skip discovery.
     */
    std::string m3dsIP;

    /**
     * @brief Builds the full launch command with placeholder substitution.
     * @param platform The target platform
     * @param outputPath Full path to the built executable
     * @return The complete command string ready for SYS_Exec
     */
    std::string BuildLaunchCommand(Platform platform, const std::string& outputPath) const;

    /**
     * @brief Builds the 3dslink command for sending to hardware.
     * On Windows, uses devkitPro MSYS2. On Linux, runs directly.
     * @param outputPath Full path to the built .3dsx file
     * @return The complete command string ready for SYS_Exec
     */
    std::string Build3dsLinkCommand(const std::string& outputPath) const;

    /**
     * @brief Builds the wiiload command for sending to Wii hardware.
     * Requires devkitPro and a configured Wii IP address.
     * @param outputPath Full path to the built .dol/.elf file
     * @return The complete command string ready for SYS_Exec
     */
    std::string BuildWiiloadCommand(const std::string& outputPath) const;

    // ── 3DS CIA tools ────────────────────────────────────────────────────────
    // Explicit paths for the "Nintendo 3DS (CIA)" packager. Empty = auto-detect
    // (editor tools dir, devkitPro/tools/bin, PATH). Mirrored into
    // CiaPackager::SetToolOverride whenever they change.

    /** @brief makerom executable (github.com/3DSGuy/Project_CTR). */
    std::string mMakeromPath;

    /** @brief bannertool executable (github.com/carstene1ns/3ds-bannertool). */
    std::string mBannertoolPath;

    /** @brief cwavtool executable (github.com/PabloMK7/cwavtool) — DSP-ADPCM banner tune encoder. */
    std::string mCwavtoolPath;

    /** @brief Python 3 command or executable used to run pycgfx (e.g. "py -3"). */
    std::string mPythonPath;

    /** @brief Directory containing pycgfx's main.py (github.com/skyfloogle/pycgfx). */
    std::string mPycgfxPath;

    // ── Android (adb) ────────────────────────────────────────────────────────

    /** @brief Path to Android Debug Bridge executable. Typical Windows location: C:\Android\Sdk\platform-tools\adb.exe */
    std::string mAdbPath;

    /** @brief Flags passed to `adb install`. `-r` reinstalls keeping app data. */
    std::string mAdbInstallArgs = "-r";

    /**
     * @brief Default device serial (matches `adb devices` first column).
     * Empty = let adb pick the single connected device (errors if multiple).
     * Set this explicitly when multiple devices/emulators are connected.
     */
    std::string mAndroidSerial;

    /**
     * @brief `<package>/<activity>` for `adb shell am start -n` after install.
     * Defaults match Standalone/Android/app/build.gradle's stock applicationId
     * + Java FQN. When a project overrides applicationId via Target Options
     * (e.g. to `com.acme.bomber`), update this to either:
     *   `com.acme.bomber/com.you.appname.PolyphaseActivity`   (FQN, robust)
     *   or `com.acme.bomber/.PolyphaseActivity`               (requires the
     *                                                          Java tree
     *                                                          to also be
     *                                                          moved to
     *                                                          com.acme.bomber)
     */
    std::string mAndroidLaunchComponent = "com.you.appname/.PolyphaseActivity";

    /** @brief One row from `adb devices -l`. */
    struct AdbDevice
    {
        std::string mSerial;
        std::string mState;   // "device" | "unauthorized" | "offline"
        std::string mModel;   // optional, parsed from `-l` long form
    };

    /** @brief True if mAdbPath points at an existing file. */
    bool IsAdbConfigured() const;

    /** @brief `"adb" [-s serial] install <args> "<apk>"`. Empty string if not configured. */
    std::string BuildAdbInstallCommand(const std::string& apkPath) const;

    /** @brief `"adb" [-s serial] shell am start -n <component>`. Empty if not configured or component blank. */
    std::string BuildAdbLaunchCommand() const;

    /** @brief Run `adb devices -l` and parse the output. Returns empty on failure. */
    std::vector<AdbDevice> ListAdbDevices() const;

    /** @brief Open a logcat window after launching on device. */
    bool mAutoOpenLogcat = true;

    /** @brief Run `adb logcat -c` before streaming so the user sees only this session's output. */
    bool mLogcatAutoClear = true;

    /**
     * @brief Filter expression passed to `adb logcat`.
     * Default `Polyphase:V *:E` shows verbose for the engine tag and errors for
     * everything else — the sweet spot for game-side debugging on a Pixel.
     */
    std::string mLogcatFilter = "Polyphase:V *:E";

    /**
     * @brief Build the command that spawns a NEW, DETACHED console window
     * streaming `adb logcat`. The editor doesn't wait on it; the window
     * persists until the user closes it. Returns empty if not configured
     * or the platform has no terminal-emulator strategy wired.
     */
    std::string BuildAdbLogcatCommand() const;

private:
    // Last result of the user clicking "Refresh Devices" in the prefs UI.
    // Not persisted; refreshed on demand.
    std::vector<AdbDevice> mCachedDevices;
    /**
     * @brief Helper to draw a path input with browse button.
     * @param label The input label
     * @param path Reference to the path string
     * @param dialogTitle Title for the file dialog
     * @return True if the value changed
     */
    bool DrawPathInput(const char* label, std::string& path, const char* dialogTitle);

    /** @brief Push the CIA tool paths into CiaPackager's resolver. */
    void SyncCiaToolOverrides();

    // Set while a CIA tool path field is being edited; the resolver is only
    // re-synced once no text field is active so each keystroke doesn't
    // trigger a `where` / `--version` probe (and a log line).
    bool mCiaToolsDirty = false;

    /**
     * @brief Replaces all occurrences of a substring.
     * @param str The string to modify
     * @param from The substring to find
     * @param to The replacement string
     */
    static void ReplaceAll(std::string& str, const std::string& from, const std::string& to);
};

#endif
