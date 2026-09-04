#if EDITOR

#include "LaunchersModule.h"
#include "../JsonSettings.h"
#include "Packaging/CiaPackager.h"

#include "System/System.h"
#include "Log.h"

#include "document.h"
#include "imgui.h"

#include <cstdlib>

DEFINE_PREFERENCES_MODULE(LaunchersModule, "Launchers", "External")

LaunchersModule::LaunchersModule()
{
}

LaunchersModule::~LaunchersModule()
{
}

void LaunchersModule::Render()
{
    bool changed = false;

    // Dolphin settings (GameCube/Wii)
    ImGui::Text("Dolphin (GameCube/Wii)");
    ImGui::Separator();

    if (DrawPathInput("Path##Dolphin", mDolphinPath, "Select Dolphin Executable"))
    {
        changed = true;
    }

    ImGui::SetNextItemWidth(-1);
    char dolphinArgsBuffer[512];
    strncpy(dolphinArgsBuffer, mDolphinArgs.c_str(), sizeof(dolphinArgsBuffer) - 1);
    dolphinArgsBuffer[sizeof(dolphinArgsBuffer) - 1] = '\0';
    if (ImGui::InputText("Args##Dolphin", dolphinArgsBuffer, sizeof(dolphinArgsBuffer)))
    {
        mDolphinArgs = dolphinArgsBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Command-line arguments. Default: {emulator} --batch -e {output}");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Azahar/Citra settings (3DS)
    ImGui::Text("Azahar/Citra (3DS Emulator)");
    ImGui::Separator();

    if (DrawPathInput("Path##Azahar", mAzaharPath, "Select Azahar/Citra Executable"))
    {
        changed = true;
    }

    ImGui::SetNextItemWidth(-1);
    char azaharArgsBuffer[512];
    strncpy(azaharArgsBuffer, mAzaharArgs.c_str(), sizeof(azaharArgsBuffer) - 1);
    azaharArgsBuffer[sizeof(azaharArgsBuffer) - 1] = '\0';
    if (ImGui::InputText("Args##Azahar", azaharArgsBuffer, sizeof(azaharArgsBuffer)))
    {
        mAzaharArgs = azaharArgsBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Command-line arguments. Default: {emulator} {output}");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // 3dslink info
    ImGui::Text("3dslink (3DS Hardware)");
    ImGui::Separator();
    ImGui::TextDisabled("3dslink is used via 'Build & Run On Device'.");
    ImGui::TextDisabled("Requires devkitPro to be installed.");

    ImGui::Text("3DS IP Address:");
    ImGui::SetNextItemWidth(-1);
    char threeDsIPBuffer[64];
    strncpy(threeDsIPBuffer, m3dsIP.c_str(), sizeof(threeDsIPBuffer) - 1);
    threeDsIPBuffer[sizeof(threeDsIPBuffer) - 1] = '\0';
    if (ImGui::InputText("##3dsIP", threeDsIPBuffer, sizeof(threeDsIPBuffer)))
    {
        m3dsIP = threeDsIPBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Optional. IP address shown by Homebrew Launcher's netloader (Y).\n"
                          "Leave blank to broadcast — required if broadcast discovery fails\n"
                          "(multiple NICs, AP client isolation, firewall).");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ── 3DS CIA tools ───────────────────────────────────────────────────────
    ImGui::Text("3DS CIA Tools");
    ImGui::Separator();
    ImGui::TextDisabled("Used by the \"Nintendo 3DS (CIA)\" build target. Leave paths blank to auto-detect");
    ImGui::TextDisabled("(editor tools folder, devkitPro/tools/bin, PATH).");

    {
        bool toolsChanged = false;
        ImGui::Text("makerom:");
        if (DrawPathInput("Path##Makerom", mMakeromPath, "Select makerom executable")) { toolsChanged = true; }
        ImGui::Text("bannertool (optional, HOME Menu banner):");
        if (DrawPathInput("Path##Bannertool", mBannertoolPath, "Select bannertool executable")) { toolsChanged = true; }
        ImGui::Text("cwavtool (optional, DSP-ADPCM banner tune):");
        if (DrawPathInput("Path##Cwavtool", mCwavtoolPath, "Select cwavtool executable")) { toolsChanged = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Only used when a profile enables the experimental DSP-ADPCM tune option.");
        }
        ImGui::Text("Python 3 (optional, 3D banners):");
        if (DrawPathInput("Path##Python", mPythonPath, "Select python executable")) { toolsChanged = true; }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Executable path or a command such as \"py -3\". Needs: pip install gltflib pillow");
        }
        ImGui::Text("pycgfx folder (optional, 3D banners):");
        if (DrawPathInput("Path##Pycgfx", mPycgfxPath, "Select pycgfx main.py")) { toolsChanged = true; }

        if (toolsChanged)
        {
            SyncCiaToolOverrides();
            changed = true;
        }

        auto status = [](const char* label, CiaPackager::Tool tool)
        {
            std::string found = CiaPackager::ResolveTool(tool);
            if (found.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "  %s: not found", label);
            }
            else
            {
                ImGui::TextDisabled("  %s: %s", label, found.c_str());
            }
        };
        status("makerom", CiaPackager::Tool::Makerom);
        status("bannertool", CiaPackager::Tool::Bannertool);
        status("cwavtool", CiaPackager::Tool::Cwavtool);
        status("python", CiaPackager::Tool::Python);
        status("pycgfx", CiaPackager::Tool::Pycgfx);

        const bool installing = CiaPackager::IsToolInstallRunning();
        if (installing) ImGui::BeginDisabled();
        if (ImGui::Button("Download makerom + bannertool + cwavtool##CiaTools"))
        {
            CiaPackager::StartToolInstall(CiaPackager::ToolInstall_Makerom | CiaPackager::ToolInstall_Bannertool |
                                          CiaPackager::ToolInstall_Cwavtool);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Fetches makerom v0.19.0 (3DSGuy/Project_CTR), bannertool 1.2.3\n"
                              "(carstene1ns/3ds-bannertool) and cwavtool 1.0.0 (PabloMK7/cwavtool)\n"
                              "from their GitHub releases into\n%s",
                              CiaPackager::GetToolsDirectory().c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Install pycgfx##CiaTools"))
        {
            CiaPackager::StartToolInstall(CiaPackager::ToolInstall_Pycgfx);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Downloads github.com/skyfloogle/pycgfx (main branch; the project has no\n"
                              "license file) and runs `python -m pip install --user gltflib pillow`.\n"
                              "Needed only for 3D HOME Menu banners.");
        }
        if (installing) ImGui::EndDisabled();

        std::string installStatus = CiaPackager::GetToolInstallStatus();
        if (!installStatus.empty())
        {
            ImGui::TextWrapped("%s%s", installing ? "Working: " : "", installStatus.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Wiiload settings (Wii Hardware)
    ImGui::Text("Wiiload (Wii Hardware)");
    ImGui::Separator();

    ImGui::Text("Wii IP Address:");
    ImGui::SetNextItemWidth(-1);
    char wiiloadIPBuffer[64];
    strncpy(wiiloadIPBuffer, mWiiloadIP.c_str(), sizeof(wiiloadIPBuffer) - 1);
    wiiloadIPBuffer[sizeof(wiiloadIPBuffer) - 1] = '\0';
    if (ImGui::InputText("##WiiloadIP", wiiloadIPBuffer, sizeof(wiiloadIPBuffer)))
    {
        mWiiloadIP = wiiloadIPBuffer;
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("IP address of your Wii (found in Homebrew Channel settings)");
    }

    ImGui::TextDisabled("Requires devkitPro and Homebrew Channel on Wii.");

    ImGui::Spacing();
    ImGui::Spacing();

    // ── ADB (Android Hardware) ──────────────────────────────────────────────
    ImGui::Text("ADB (Android Hardware)");
    ImGui::Separator();

    if (DrawPathInput("Path##Adb", mAdbPath, "Select adb executable"))
    {
        changed = true;
    }
    ImGui::TextDisabled("  Typical Windows: C:\\Android\\Sdk\\platform-tools\\adb.exe");

    {
        ImGui::SetNextItemWidth(-1);
        char buf[256];
        strncpy(buf, mAdbInstallArgs.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Install Args##Adb", buf, sizeof(buf)))
        {
            mAdbInstallArgs = buf;
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Flags passed to `adb install`. -r reinstalls keeping app data.");
        }
    }

    {
        ImGui::SetNextItemWidth(-1);
        char buf[256];
        strncpy(buf, mAndroidLaunchComponent.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Launch Component##Adb", buf, sizeof(buf)))
        {
            mAndroidLaunchComponent = buf;
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("<package>/<activity> for `adb shell am start -n` after install.\n"
                              "Defaults match Standalone/Android/app/build.gradle.\n"
                              "Empty = skip the launch step.");
        }
    }

    ImGui::Spacing();

    // Device list with Refresh button
    ImGui::Text("Device:");
    ImGui::SameLine();
    if (ImGui::Button("Refresh##AdbDevices"))
    {
        mCachedDevices = ListAdbDevices();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d detected)", (int)mCachedDevices.size());

    {
        bool autoSelected = mAndroidSerial.empty();
        if (ImGui::RadioButton("Auto (first connected)##Adb", autoSelected))
        {
            if (!autoSelected) { mAndroidSerial.clear(); changed = true; }
        }
    }

    for (const AdbDevice& dev : mCachedDevices)
    {
        std::string label = dev.mSerial;
        if (!dev.mModel.empty()) label += "  [" + dev.mModel + "]";
        label += "  (" + dev.mState + ")";
        label += "##AdbDev_" + dev.mSerial;

        bool sel = (mAndroidSerial == dev.mSerial);
        if (ImGui::RadioButton(label.c_str(), sel))
        {
            if (!sel) { mAndroidSerial = dev.mSerial; changed = true; }
        }
    }

    // Manual serial override — useful for hosts where `adb devices` is gated
    // (sudo udev rules, headless CI, scripted runs against a named emulator).
    {
        ImGui::SetNextItemWidth(-1);
        char buf[128];
        strncpy(buf, mAndroidSerial.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Selected Serial##Adb", buf, sizeof(buf)))
        {
            mAndroidSerial = buf;
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Empty = first connected device (fails if multiple).\n"
                              "Set explicitly when multiple devices/emulators are attached.");
        }
    }

    ImGui::Spacing();

    // Logcat — separate detached window so the editor doesn't freeze while
    // streaming. The "Open Logcat" button lets the user spawn one anytime,
    // independent of a build.
    if (ImGui::Checkbox("Auto-open logcat after launch", &mAutoOpenLogcat))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("After `adb shell am start`, spawn a new console window\n"
                          "running `adb logcat` so app output streams live.\n"
                          "The window stays open until you close it.");
    }

    if (ImGui::Checkbox("Clear logcat buffer first (-c)", &mLogcatAutoClear))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Run `adb logcat -c` before streaming so the window\n"
                          "shows only this run's output, not stale buffer noise.");
    }

    {
        ImGui::SetNextItemWidth(-1);
        char buf[256];
        strncpy(buf, mLogcatFilter.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Logcat Filter##Adb", buf, sizeof(buf)))
        {
            mLogcatFilter = buf;
            changed = true;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Args appended to `adb logcat`. Examples:\n"
                              "  Polyphase:V *:E      engine verbose + everyone else errors\n"
                              "  Polyphase:V *:S      engine only (silent for others)\n"
                              "  *:V                  everything verbose (very noisy)");
        }
    }

    if (ImGui::Button("Open Logcat##Adb"))
    {
        std::string cmd = BuildAdbLogcatCommand();
        if (!cmd.empty())
        {
            SYS_ExecDetached(cmd.c_str());
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Open a logcat window right now without rebuilding.");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Placeholder help
    ImGui::TextDisabled("Placeholders: {emulator}, {output}, {outputdir}");

    if (changed)
    {
        SetDirty(true);
    }
}

void LaunchersModule::LoadSettings(const rapidjson::Document& doc)
{
    mDolphinPath = JsonSettings::GetString(doc, "dolphinPath", "");
    mDolphinArgs = JsonSettings::GetString(doc, "dolphinArgs", "{emulator} --batch -e {output}");
    mAzaharPath = JsonSettings::GetString(doc, "azaharPath", "");
    mAzaharArgs = JsonSettings::GetString(doc, "azaharArgs", "{emulator} {output}");
    mWiiloadIP = JsonSettings::GetString(doc, "wiiloadIP", "");
    m3dsIP = JsonSettings::GetString(doc, "threeDsIP", "");

    mAdbPath = JsonSettings::GetString(doc, "adbPath", "");
    mAdbInstallArgs = JsonSettings::GetString(doc, "adbInstallArgs", "-r");
    mAndroidSerial = JsonSettings::GetString(doc, "androidSerial", "");
    mAndroidLaunchComponent = JsonSettings::GetString(doc, "androidLaunchComponent",
                                                     "com.you.appname/.PolyphaseActivity");
    mAutoOpenLogcat = JsonSettings::GetBool(doc, "autoOpenLogcat", true);
    mLogcatAutoClear = JsonSettings::GetBool(doc, "logcatAutoClear", true);
    mLogcatFilter = JsonSettings::GetString(doc, "logcatFilter", "Polyphase:V *:E");

    mMakeromPath = JsonSettings::GetString(doc, "makeromPath", "");
    mBannertoolPath = JsonSettings::GetString(doc, "bannertoolPath", "");
    mCwavtoolPath = JsonSettings::GetString(doc, "cwavtoolPath", "");
    mPythonPath = JsonSettings::GetString(doc, "pythonPath", "");
    mPycgfxPath = JsonSettings::GetString(doc, "pycgfxPath", "");
    SyncCiaToolOverrides();
}

void LaunchersModule::SyncCiaToolOverrides()
{
    CiaPackager::SetToolOverride(CiaPackager::Tool::Makerom, mMakeromPath);
    CiaPackager::SetToolOverride(CiaPackager::Tool::Bannertool, mBannertoolPath);
    CiaPackager::SetToolOverride(CiaPackager::Tool::Cwavtool, mCwavtoolPath);
    CiaPackager::SetToolOverride(CiaPackager::Tool::Python, mPythonPath);
    CiaPackager::SetToolOverride(CiaPackager::Tool::Pycgfx, mPycgfxPath);
}

void LaunchersModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetString(doc, "makeromPath", mMakeromPath);
    JsonSettings::SetString(doc, "bannertoolPath", mBannertoolPath);
    JsonSettings::SetString(doc, "cwavtoolPath", mCwavtoolPath);
    JsonSettings::SetString(doc, "pythonPath", mPythonPath);
    JsonSettings::SetString(doc, "pycgfxPath", mPycgfxPath);

    JsonSettings::SetString(doc, "dolphinPath", mDolphinPath);
    JsonSettings::SetString(doc, "dolphinArgs", mDolphinArgs);
    JsonSettings::SetString(doc, "azaharPath", mAzaharPath);
    JsonSettings::SetString(doc, "azaharArgs", mAzaharArgs);
    JsonSettings::SetString(doc, "wiiloadIP", mWiiloadIP);
    JsonSettings::SetString(doc, "threeDsIP", m3dsIP);

    JsonSettings::SetString(doc, "adbPath", mAdbPath);
    JsonSettings::SetString(doc, "adbInstallArgs", mAdbInstallArgs);
    JsonSettings::SetString(doc, "androidSerial", mAndroidSerial);
    JsonSettings::SetString(doc, "androidLaunchComponent", mAndroidLaunchComponent);
    JsonSettings::SetBool(doc, "autoOpenLogcat", mAutoOpenLogcat);
    JsonSettings::SetBool(doc, "logcatAutoClear", mLogcatAutoClear);
    JsonSettings::SetString(doc, "logcatFilter", mLogcatFilter);
}

bool LaunchersModule::IsEmulatorConfigured(Platform platform) const
{
    switch (platform)
    {
        case Platform::GameCube:
        case Platform::Wii:
            return !mDolphinPath.empty();

        case Platform::N3DS:
            return !mAzaharPath.empty();

        default:
            return false;
    }
}

bool LaunchersModule::Is3dsLinkConfigured() const
{
    // 3dslink is available if devkitPro is installed
#if PLATFORM_WINDOWS
    // Check if 3dslink.exe exists at the standard devkitPro location
    return SYS_DoesFileExist("C:/devkitPro/tools/bin/3dslink.exe", false);
#else
    // On Linux, just check if 3dslink command exists
    std::string output;
    SYS_Exec("which 3dslink", &output);
    return !output.empty();
#endif
}

bool LaunchersModule::IsWiiloadConfigured() const
{
    // Wiiload requires devkitPro installed and IP address set
    if (mWiiloadIP.empty())
    {
        return false;
    }

#if PLATFORM_WINDOWS
    // Check if wiiload.exe exists at the standard devkitPro location
    return SYS_DoesFileExist("C:/devkitPro/tools/bin/wiiload.exe", false);
#else
    // On Linux, just check if wiiload command exists
    std::string output;
    SYS_Exec("which wiiload", &output);
    return !output.empty();
#endif
}

std::string LaunchersModule::BuildLaunchCommand(Platform platform, const std::string& outputPath) const
{
    std::string args;
    std::string emulatorPath;

    switch (platform)
    {
        case Platform::GameCube:
        case Platform::Wii:
            emulatorPath = mDolphinPath;
            args = mDolphinArgs.empty() ? "{emulator --batch -e {output}" : mDolphinArgs;
            break;

        case Platform::N3DS:
            emulatorPath = mAzaharPath;
            args = mAzaharArgs.empty() ? "{emulator} {output}" : mAzaharArgs;
            break;

        default:
            return "";
    }

    // Get output directory
    std::string outputDir = outputPath;
    size_t lastSlash = outputDir.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        outputDir = outputDir.substr(0, lastSlash);
    }

    // Replace placeholders
    std::string cmd = args;
    ReplaceAll(cmd, "{emulator}", "\"" + emulatorPath + "\"");
    ReplaceAll(cmd, "{output}", "\"" + outputPath + "\"");
    ReplaceAll(cmd, "{outputdir}", "\"" + outputDir + "\"");

#if PLATFORM_WINDOWS
    // start "" launches the command asynchronously so the editor doesn't freeze.
    // The "" is the window title (required when the first real arg is quoted).
    // No cmd /c wrapper needed — the "start" prefix prevents cmd.exe's quote-stripping.
    cmd = "start \"\" " + cmd;
#elif PLATFORM_LINUX
    // Background the process so the editor doesn't freeze.
    cmd += " &";
#endif

    return cmd;
}

std::string LaunchersModule::Build3dsLinkCommand(const std::string& outputPath) const
{
    // -a skips UDP broadcast discovery (the default mode that needs HBL's
    // netloader to respond). Broadcast often silently fails on multi-NIC hosts
    // and isolating Wi-Fi APs; passing the explicit IP is the reliable path.
    std::string ipArg = m3dsIP.empty() ? "" : (" -a " + m3dsIP);

#if PLATFORM_WINDOWS
    // On Windows, 3dslink.exe is at C:\devkitPro\tools\bin\3dslink.exe
    std::string threeDsLink = "C:\\devkitPro\\tools\\bin\\3dslink.exe";

    if (!SYS_DoesFileExist(threeDsLink.c_str(), false))
    {
        LogError("3dslink.exe not found at %s", threeDsLink.c_str());
        return "";
    }

    // Normalize path to use backslashes on Windows
    std::string normalizedPath = outputPath;
    ReplaceAll(normalizedPath, "/", "\\");

    // Use cmd /c with special quoting for paths with spaces
    std::string cmd = "cmd /c \"\"" + threeDsLink + "\"" + ipArg + " \"" + normalizedPath + "\"\"";
    return cmd;
#else
    // On Linux, just run 3dslink directly
    return "3dslink" + ipArg + " \"" + outputPath + "\"";
#endif
}

std::string LaunchersModule::BuildWiiloadCommand(const std::string& outputPath) const
{
    if (mWiiloadIP.empty())
    {
        LogError("Wii IP address not configured");
        return "";
    }

#if PLATFORM_WINDOWS
    // On Windows, wiiload.exe is at C:\devkitPro\tools\bin\wiiload.exe
    std::string wiiloadPath = "C:\\devkitPro\\tools\\bin\\wiiload.exe";

    if (!SYS_DoesFileExist(wiiloadPath.c_str(), false))
    {
        LogError("wiiload.exe not found at %s", wiiloadPath.c_str());
        return "";
    }

    // Normalize path to use backslashes on Windows
    std::string normalizedPath = outputPath;
    ReplaceAll(normalizedPath, "/", "\\");

    // Set WIILOAD env var inline and run wiiload
    std::string cmd = "cmd /c \"set WIILOAD=tcp:" + mWiiloadIP +
                      " && \"" + wiiloadPath + "\" \"" + normalizedPath + "\"\"";
    return cmd;
#else
    // On Linux, export WIILOAD inline and run wiiload
    return "WIILOAD=tcp:" + mWiiloadIP + " wiiload \"" + outputPath + "\"";
#endif
}

bool LaunchersModule::IsAdbConfigured() const
{
    return !mAdbPath.empty() && SYS_DoesFileExist(mAdbPath.c_str(), false);
}

std::string LaunchersModule::BuildAdbInstallCommand(const std::string& apkPath) const
{
    if (!IsAdbConfigured()) { return ""; }

    std::string normAdb = mAdbPath;
    std::string normApk = apkPath;
#if PLATFORM_WINDOWS
    ReplaceAll(normAdb, "/", "\\");
    ReplaceAll(normApk, "/", "\\");
#endif

    std::string serialArg = mAndroidSerial.empty() ? "" : (" -s " + mAndroidSerial);
    std::string installArgs = mAdbInstallArgs.empty() ? "" : (" " + mAdbInstallArgs);

    return "\"" + normAdb + "\"" + serialArg + " install" + installArgs + " \"" + normApk + "\"";
}

std::string LaunchersModule::BuildAdbLaunchCommand() const
{
    if (!IsAdbConfigured()) { return ""; }
    if (mAndroidLaunchComponent.empty()) { return ""; }

    std::string normAdb = mAdbPath;
#if PLATFORM_WINDOWS
    ReplaceAll(normAdb, "/", "\\");
#endif

    std::string serialArg = mAndroidSerial.empty() ? "" : (" -s " + mAndroidSerial);
    return "\"" + normAdb + "\"" + serialArg + " shell am start -n " + mAndroidLaunchComponent;
}

std::vector<LaunchersModule::AdbDevice> LaunchersModule::ListAdbDevices() const
{
    std::vector<AdbDevice> result;
    if (!IsAdbConfigured()) { return result; }

    std::string normAdb = mAdbPath;
#if PLATFORM_WINDOWS
    ReplaceAll(normAdb, "/", "\\");
#endif

    std::string output;
    SYS_Exec(("\"" + normAdb + "\" devices -l").c_str(), &output);

    // Format of `adb devices -l` (per line after "List of devices attached"):
    //   <serial><WS>device usb:1-1.2 product:foo model:Pixel_10 device:bar transport_id:N
    // Other states ("unauthorized", "offline", "no permissions") omit the suffix.
    size_t pos = 0;
    while (pos < output.size())
    {
        size_t lineEnd = output.find('\n', pos);
        std::string line = (lineEnd == std::string::npos) ? output.substr(pos) : output.substr(pos, lineEnd - pos);
        pos = (lineEnd == std::string::npos) ? output.size() : lineEnd + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }
        if (line.empty()) { continue; }
        if (line.find("List of devices") != std::string::npos) { continue; }
        if (line.find("daemon") != std::string::npos) { continue; }
        if (line[0] == '*') { continue; }  // "* daemon not running ..." lines

        size_t firstSep = line.find_first_of(" \t");
        if (firstSep == std::string::npos) { continue; }

        AdbDevice dev;
        dev.mSerial = line.substr(0, firstSep);

        std::string rest = line.substr(firstSep);
        size_t stateStart = rest.find_first_not_of(" \t");
        if (stateStart == std::string::npos) { continue; }
        rest = rest.substr(stateStart);

        size_t stateEnd = rest.find_first_of(" \t");
        dev.mState = (stateEnd == std::string::npos) ? rest : rest.substr(0, stateEnd);

        size_t modelIdx = rest.find("model:");
        if (modelIdx != std::string::npos)
        {
            modelIdx += 6;
            size_t modelEnd = rest.find_first_of(" \t", modelIdx);
            dev.mModel = (modelEnd == std::string::npos)
                ? rest.substr(modelIdx)
                : rest.substr(modelIdx, modelEnd - modelIdx);
        }

        result.push_back(std::move(dev));
    }

    return result;
}

std::string LaunchersModule::BuildAdbLogcatCommand() const
{
    if (!IsAdbConfigured()) { return ""; }

    std::string normAdb = mAdbPath;
#if PLATFORM_WINDOWS
    ReplaceAll(normAdb, "/", "\\");
#endif

    std::string serialArg = mAndroidSerial.empty() ? "" : (" -s " + mAndroidSerial);
    std::string filter = mLogcatFilter.empty() ? "*:V" : mLogcatFilter;

#if PLATFORM_WINDOWS
    // Open a new console window via `start "title" cmd /k "<command>"`.
    //
    // Quote dance:
    //   - SystemUtils prepends `cmd.exe /c ` to whatever we hand SYS_ExecDetached.
    //     The outer cmd /c does NOT strip quotes because our string starts with
    //     `start` (not a quote), so passes through unchanged.
    //   - `start "Polyphase Logcat" cmd /k "..."` then spawns a new cmd window.
    //     The /k argument needs the ADB path quoted (it may have spaces). To
    //     survive cmd /k's "strip first and last quote" rule, we wrap the
    //     ENTIRE /k arg in an extra pair of quotes — outer pair gets stripped,
    //     inner quotes around the path survive.
    //   - Net effect inside the new window: `"<adb>" -s ... logcat -c && "<adb>" -s ... logcat <filter>`
    //
    // /k (not /c) keeps the window open after logcat exits — handy if the user
    // Ctrl+C's the stream and wants the history visible.
    std::string clearChain;
    if (mLogcatAutoClear)
    {
        clearChain = "\"" + normAdb + "\"" + serialArg + " logcat -c && ";
    }

    return "start \"Polyphase Logcat\" cmd /k \""
         + clearChain
         + "\"" + normAdb + "\"" + serialArg + " logcat " + filter
         + "\"";
#elif PLATFORM_LINUX
    // Best-effort terminal-emulator launch. x-terminal-emulator is the
    // Debian/Ubuntu meta-symlink; we try it first then xterm as fallback.
    // The `; exec bash` keeps the shell open after logcat exits so the user
    // can scroll back. Backgrounded with & so the editor never waits.
    std::string clearChain;
    if (mLogcatAutoClear)
    {
        clearChain = "'" + normAdb + "'" + serialArg + " logcat -c; ";
    }
    std::string inner = clearChain
                      + "'" + normAdb + "'" + serialArg + " logcat " + filter
                      + "; exec bash";
    return "(x-terminal-emulator -T 'Polyphase Logcat' -e bash -c \"" + inner + "\" "
           "|| xterm -T 'Polyphase Logcat' -e bash -c \"" + inner + "\") &";
#else
    return "";
#endif
}

bool LaunchersModule::DrawPathInput(const char* label, std::string& path, const char* dialogTitle)
{
    bool changed = false;

    ImGui::PushID(label);

    float buttonWidth = 70.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float inputWidth = ImGui::GetContentRegionAvail().x - buttonWidth - spacing;

    ImGui::SetNextItemWidth(inputWidth);
    char pathBuffer[512];
    strncpy(pathBuffer, path.c_str(), sizeof(pathBuffer) - 1);
    pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    if (ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer)))
    {
        path = pathBuffer;
        changed = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Browse...", ImVec2(buttonWidth, 0)))
    {
        std::vector<std::string> files = SYS_OpenFileDialog();
        if (!files.empty() && !files[0].empty())
        {
            path = files[0];
            changed = true;
        }
    }

    ImGui::PopID();

    return changed;
}

void LaunchersModule::ReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos)
    {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
}

#endif
