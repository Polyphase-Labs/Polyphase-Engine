#if EDITOR

#include "BuildDependencyWindow.h"
#include "EditorWidgets.h"
#include "EditorUtils.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/General/GeneralModule.h"
#include "Preferences/External/ExternalModule.h"

#include "Log.h"
#include "System/System.h"

#include "imgui.h"

#include <cstdlib>

static BuildDependencyWindow sBuildDependencyWindow;

BuildDependencyWindow* GetBuildDependencyWindow()
{
    return &sBuildDependencyWindow;
}

void BuildDependencyWindow::CheckMake()
{
    BuildDependency dep;
    dep.mName = "make";
    dep.mDescription = "Required for GameCube / Wii / 3DS builds";
#if PLATFORM_WINDOWS
    dep.mInstallHint = "Install MSYS2 and run: pacman -S make";
    dep.mInstallUrl = "https://www.msys2.org/";
#else
    dep.mInstallHint = "Install via: sudo apt install build-essential";
    dep.mInstallUrl = "https://www.gnu.org/software/make/";
#endif

    std::string output;
    SYS_Exec("make --version", &output);

    if (!output.empty() && output.find("Make") != std::string::npos)
    {
        dep.mStatus = DependencyStatus::Found;
        // Extract first line as version
        size_t newline = output.find('\n');
        dep.mVersion = (newline != std::string::npos) ? output.substr(0, newline) : output;
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }

    mDependencies.push_back(dep);
}

void BuildDependencyWindow::CheckDevkitPro()
{
    BuildDependency dep;
    dep.mName = "devkitPro";
    dep.mDescription = "Required for GameCube / Wii / 3DS builds";
    dep.mInstallHint = "Download the devkitPro installer";
    dep.mInstallUrl = "https://devkitpro.org/wiki/Getting_Started";

    std::string dkpPath = GetDevkitproPath();

    if (!dkpPath.empty())
    {
        dep.mStatus = DependencyStatus::Found;
        dep.mVersion = dkpPath;
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }

    mDependencies.push_back(dep);
}

// Checks that a devkitPro compiler binary actually exists under the devkitPro
// root. A devkitPro install without the toolchain meta-package (wii-dev /
// 3ds-dev) passes CheckDevkitPro but fails partway through make.
//
// Also independently checks the DEVKITPPC / DEVKITARM environment variable
// that the Makefiles read directly (e.g. `include $(DEVKITPPC)/wii_rules` in
// Engine/Makefile_Wii) — make never goes through GetDevkitproPath(), so a
// correct devkitPro install can still fail mid-build if that variable is
// missing or points at the wrong folder (e.g. the `bin` subfolder instead of
// the devkitPPC root), which this check surfaces before packaging is even
// attempted instead of a cryptic "No such file or directory" from make.
static void CheckDevkitToolchain(
    std::vector<BuildDependency>& deps,
    const char* name,
    const char* description,
    const char* subDir,
    const char* compiler,
    const char* envVarName,
    const char* rulesFile,
    const char* pacmanPackage)
{
    BuildDependency dep;
    dep.mName = name;
    dep.mDescription = description;
    dep.mInstallHint = std::string("In the devkitPro MSYS2 shell run: pacman -S ") + pacmanPackage;
    dep.mInstallUrl = "https://devkitpro.org/wiki/Getting_Started";

    std::string dkpPath = GetDevkitproPath();

    // The Windows path comes from cygpath output, so trim trailing whitespace.
    while (!dkpPath.empty() &&
        (dkpPath.back() == '\n' || dkpPath.back() == '\r' || dkpPath.back() == ' '))
    {
        dkpPath.pop_back();
    }

    if (dkpPath.empty())
    {
        dep.mStatus = DependencyStatus::NotFound;
        dep.mInstallHint = "Install devkitPro first";
    }
    else
    {
        std::string compilerPath = dkpPath + "/" + subDir + "/bin/" + compiler;
#if PLATFORM_WINDOWS
        compilerPath += ".exe";
#endif

        if (!SYS_DoesFileExist(compilerPath.c_str(), false))
        {
            dep.mStatus = DependencyStatus::NotFound;
        }
        else
        {
            const char* envVal = getenv(envVarName);

            if (envVal == nullptr)
            {
                dep.mStatus = DependencyStatus::NotFound;
                dep.mInstallHint = std::string(envVarName) +
                    " is not set. Restart your computer so the devkitPro installer's "
                    "environment variables are picked up.";
            }
            else
            {
                // The env var may be MSYS-style (/opt/devkitpro/devkitPPC — the
                // devkitPro installer default) or Windows-style (C:\devkitPro\devkitPPC).
                // Both work for make (devkitPro's MSYS2 make resolves /opt/devkitpro
                // via its mount table), but Win32 file checks can't see MSYS paths,
                // so convert those through cygpath first.
                std::string envDir = envVal;
                bool checkable = true;

#if PLATFORM_WINDOWS
                if (!envDir.empty() && envDir[0] == '/')
                {
                    std::string converted;
                    std::string cmd = std::string("cygpath.exe -w %") + envVarName + "%";
                    SYS_Exec(cmd.c_str(), &converted);

                    while (!converted.empty() &&
                        (converted.back() == '\n' || converted.back() == '\r' || converted.back() == ' '))
                    {
                        converted.pop_back();
                    }

                    if (converted.empty())
                    {
                        // cygpath unavailable — the env var can't be verified from
                        // here, but make may still resolve it fine. Don't flag a
                        // possibly-working setup; the compiler check above passed.
                        checkable = false;
                    }
                    else
                    {
                        envDir = converted;
                    }
                }
#endif

                while (!envDir.empty() && (envDir.back() == '/' || envDir.back() == '\\'))
                {
                    envDir.pop_back();
                }
                std::string rulesPath = envDir + "/" + rulesFile;

                if (checkable && !SYS_DoesFileExist(rulesPath.c_str(), false))
                {
                    dep.mStatus = DependencyStatus::NotFound;
                    dep.mInstallHint = std::string(envVarName) + " is set to '" + envVal +
                        "', but " + rulesFile + " isn't there. It must point directly at the " +
                        subDir + " folder (e.g. C:\\devkitPro\\" + subDir + "), not a subfolder "
                        "like \\bin. Fix it in Windows Environment Variables, then restart the editor.";
                }
                else
                {
                    dep.mStatus = DependencyStatus::Found;
                    dep.mVersion = compilerPath;
                }
            }
        }
    }

    deps.push_back(dep);
}

void BuildDependencyWindow::CheckDevkitPPC()
{
    CheckDevkitToolchain(
        mDependencies,
        "devkitPPC",
        "Compiler for GameCube / Wii builds (powerpc-eabi-g++)",
        "devkitPPC",
        "powerpc-eabi-g++",
        "DEVKITPPC",
        "wii_rules",
        "wii-dev");
}

void BuildDependencyWindow::CheckDevkitARM()
{
    CheckDevkitToolchain(
        mDependencies,
        "devkitARM",
        "Compiler for 3DS builds (arm-none-eabi-g++)",
        "devkitARM",
        "arm-none-eabi-g++",
        "DEVKITARM",
        "3ds_rules",
        "3ds-dev");
}

void BuildDependencyWindow::CheckDocker()
{
    BuildDependency dep;
    dep.mName = "Docker";
    dep.mDescription = "Optional for cross-compilation (e.g. Linux builds)";
    dep.mInstallHint = "Download and install Docker Desktop";
    dep.mInstallUrl = "https://docs.docker.com/get-docker/";

    ExternalModule* ext = static_cast<ExternalModule*>(
        PreferencesManager::Get()->FindModule("External"));
    std::string dockerCmd = ext ? ext->GetDockerCommand() : "docker";
    dockerCmd += " --version";

    std::string output;
    SYS_Exec(dockerCmd.c_str(), &output);

    if (!output.empty() && output.find("Docker") != std::string::npos)
    {
        dep.mStatus = DependencyStatus::Found;
        size_t newline = output.find('\n');
        dep.mVersion = (newline != std::string::npos) ? output.substr(0, newline) : output;
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }

    mDependencies.push_back(dep);
}

void BuildDependencyWindow::CheckVisualStudio()
{
    BuildDependency dep;
    dep.mName = "Visual Studio";
    dep.mDescription = "Required for Windows builds";
    dep.mInstallUrl = "https://visualstudio.microsoft.com/";

#if PLATFORM_WINDOWS
    dep.mInstallHint = "Download and install Visual Studio with C++ workload";

    std::string devenvPath = GetDevenvPath();

    if (!devenvPath.empty())
    {
        dep.mStatus = DependencyStatus::Found;
        dep.mVersion = devenvPath;
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }
#else
    dep.mStatus = DependencyStatus::Skipped;
    dep.mInstallHint = "Windows only";
#endif

    mDependencies.push_back(dep);
}

void BuildDependencyWindow::CheckGradle()
{
    BuildDependency dep;
    dep.mName = "Gradle";
    dep.mDescription = "Required for Android builds";
    dep.mInstallHint = "Install Android Studio (includes Gradle)";
    dep.mInstallUrl = "https://developer.android.com/studio";

    ExternalModule* extModule = static_cast<ExternalModule*>(
        PreferencesManager::Get()->FindModule("External"));
    std::string gradleCmd = extModule ? extModule->GetGradleCommand() : "gradle";
    gradleCmd += " --version";

    std::string output;
    SYS_Exec(gradleCmd.c_str(), &output);

    if (!output.empty() && output.find("Gradle") != std::string::npos)
    {
        dep.mStatus = DependencyStatus::Found;
        // Find the "Gradle X.Y.Z" line
        size_t pos = output.find("Gradle ");
        if (pos != std::string::npos)
        {
            size_t newline = output.find('\n', pos);
            dep.mVersion = (newline != std::string::npos) ? output.substr(pos, newline - pos) : output.substr(pos);
        }
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }

    mDependencies.push_back(dep);
}

void BuildDependencyWindow::CheckDotnet()
{
    BuildDependency dep;
    dep.mName = ".NET SDK";
    dep.mDescription = "Required only for C# scripting (8.0 or newer)";
#if PLATFORM_WINDOWS
    dep.mInstallHint = "Install via: winget install Microsoft.DotNet.SDK.8";
#else
    dep.mInstallHint = "Install via: sudo apt install dotnet-sdk-8.0";
#endif
    dep.mInstallUrl = "https://dotnet.microsoft.com/download/dotnet/8.0";

    std::string output;
    SYS_Exec("dotnet --version", &output);

    // Expect a bare version like "9.0.316".
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
        output.pop_back();

    if (!output.empty() && isdigit((unsigned char)output[0]))
    {
        int major = atoi(output.c_str());
        dep.mStatus = (major >= 8) ? DependencyStatus::Found : DependencyStatus::NotFound;
        dep.mVersion = ".NET SDK " + output;
    }
    else
    {
        dep.mStatus = DependencyStatus::NotFound;
    }

    mDependencies.push_back(dep);
}

void BuildDependencyWindow::RunChecks()
{
    mDependencies.clear();

    CheckMake();
    CheckDevkitPro();
    CheckDevkitPPC();
    CheckDevkitARM();
    CheckDocker();
    CheckVisualStudio();
    CheckGradle();
    CheckDotnet();

    // Log results
    for (const BuildDependency& dep : mDependencies)
    {
        switch (dep.mStatus)
        {
        case DependencyStatus::Found:
            LogDebug("[BuildDeps] %s: Found (%s)", dep.mName.c_str(), dep.mVersion.c_str());
            break;
        case DependencyStatus::NotFound:
            LogWarning("[BuildDeps] %s: Not found - install from %s", dep.mName.c_str(), dep.mInstallUrl.c_str());
            break;
        case DependencyStatus::Skipped:
            LogDebug("[BuildDeps] %s: Skipped (not applicable on this platform)", dep.mName.c_str());
            break;
        }
    }
}

bool BuildDependencyWindow::HasMissing() const
{
    for (const BuildDependency& dep : mDependencies)
    {
        if (dep.mStatus == DependencyStatus::NotFound)
        {
            return true;
        }
    }
    return false;
}

void BuildDependencyWindow::Open()
{
    RunChecks();
    mIsOpen = true;
}

bool BuildDependencyWindow::IsOpen() const
{
    return mIsOpen;
}

void BuildDependencyWindow::Draw()
{
    if (!mIsOpen)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(600.0f, 400.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Build Dependencies", &mIsOpen, windowFlags))
    {
        ImGui::TextWrapped("Status of external tools required for building to various platforms.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("DepsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 24.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < mDependencies.size(); ++i)
            {
                const BuildDependency& dep = mDependencies[i];
                ImGui::TableNextRow();
                ImGui::PushID((int)i);

                // Status indicator
                ImGui::TableNextColumn();
                ImVec4 statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                const char* statusIcon = "-";
                switch (dep.mStatus)
                {
                case DependencyStatus::Found:
                    statusColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                    statusIcon = "O";
                    break;
                case DependencyStatus::NotFound:
                    statusColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
                    statusIcon = "X";
                    break;
                case DependencyStatus::Skipped:
                    statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    statusIcon = "-";
                    break;
                }
                ImGui::TextColored(statusColor, "%s", statusIcon);

                // Name
                ImGui::TableNextColumn();
                ImGui::Text("%s", dep.mName.c_str());

                // Details
                ImGui::TableNextColumn();
                if (dep.mStatus == DependencyStatus::Found)
                {
                    ImGui::TextWrapped("%s", dep.mVersion.c_str());
                }
                else if (dep.mStatus == DependencyStatus::NotFound)
                {
                    ImGui::TextWrapped("%s", dep.mInstallHint.c_str());
                }
                else
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", dep.mDescription.c_str());
                }

                // Action
                ImGui::TableNextColumn();
                if (dep.mStatus == DependencyStatus::NotFound && !dep.mInstallUrl.empty())
                {
                    if (ImGui::SmallButton("Install..."))
                    {
#if PLATFORM_WINDOWS
                        std::string cmd = "start " + dep.mInstallUrl;
#else
                        std::string cmd = "xdg-open " + dep.mInstallUrl + " &";
#endif
                        SYS_Exec(cmd.c_str());
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        // Re-check button at the bottom
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Re-check"))
        {
            RunChecks();
        }

        ImGui::SameLine();

        GeneralModule* generalModule = static_cast<GeneralModule*>(
            PreferencesManager::Get()->FindModule("General"));
        if (generalModule != nullptr)
        {
            bool dontShow = !generalModule->GetCheckBuildDepsOnStartup();
            if (Polyphase::Checkbox("Don't show on startup", &dontShow))
            {
                generalModule->SetCheckBuildDepsOnStartup(!dontShow);
                PreferencesManager::Get()->SaveModule(generalModule);
            }
        }

        int foundCount = 0;
        int checkedCount = 0;
        for (const BuildDependency& dep : mDependencies)
        {
            if (dep.mStatus != DependencyStatus::Skipped)
            {
                checkedCount++;
                if (dep.mStatus == DependencyStatus::Found)
                    foundCount++;
            }
        }
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%d of %d dependencies found",
            foundCount, checkedCount);
    }
    ImGui::End();
}

#endif
