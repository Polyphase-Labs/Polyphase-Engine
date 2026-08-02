#if EDITOR

#include "AppImagePackager.h"
#include "LinuxHostShell.h"

#include "EngineTypes.h"
#include "System/System.h"
#include "Log.h"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace AppImagePackager
{
    const char* const kTargetId = "polyphase.linux.appimage";

    namespace
    {
        constexpr const char* kVersionKey     = "appimage.version";
        constexpr const char* kCategoriesKey  = "appimage.categories";
        constexpr const char* kIconPathKey    = "appimage.iconPath";
        constexpr const char* kUpdateInfoKey  = "appimage.updateInformation";
        constexpr const char* kRuntimePathKey = "appimage.runtimePath";
        constexpr const char* kWslDistroKey   = "appimage.wslDistro";

        constexpr const char* kDefaultCategories = "Game;";

        // Desktop-file basenames must be free of spaces and path separators.
        std::string ToAppName(const std::string& projectName)
        {
            std::string out;
            out.reserve(projectName.size());

            for (char c : projectName)
            {
                if (std::isalnum((unsigned char)c) || c == '_')
                {
                    out += c;
                }
                else if (!out.empty() && out.back() != '-')
                {
                    out += '-';
                }
            }

            while (!out.empty() && out.back() == '-')
            {
                out.pop_back();
            }

            return out.empty() ? std::string("PolyphaseGame") : out;
        }

        int32_t AppImage_Validate(char* outReason, size_t reasonCap)
        {
            if (LinuxHostShell::HasTool("appimagetool") || LinuxHostShell::HasTool("mksquashfs"))
            {
                return 1;
            }

            if (outReason != nullptr && reasonCap > 0)
            {
                std::snprintf(outReason, reasonCap, "%s",
                    LinuxHostShell::UsesWsl()
                        ? "appimagetool not reachable via WSL. Install WSL2 + a distro, then "
                          "download appimagetool from github.com/AppImage/AppImageKit/releases "
                          "and place it on PATH (chmod +x). squashfs-tools also works if you "
                          "supply a runtime file in the target options."
                        : "appimagetool not found on PATH. Download it from "
                          "github.com/AppImage/AppImageKit/releases and chmod +x it, or install "
                          "squashfs-tools and supply a runtime file in the target options.");
            }

            return 0;
        }

        int32_t AppImage_PostPackage(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr || ctx->packageOutputDir == nullptr || ctx->projectName == nullptr)
            {
                LogError("AppImage: build context is incomplete.");
                return 0;
            }

            const std::string projectName = ctx->projectName;
            const std::string projectDir  = ctx->projectDir != nullptr ? ctx->projectDir : "";
            const std::string outDir      = ctx->packageOutputDir;
            const std::string wslDistro   = LinuxHostShell::GetOption(ctx, kWslDistroKey);

            const std::string appName    = ToAppName(projectName);
            const std::string version    = LinuxHostShell::GetOption(ctx, kVersionKey, "1.0.0");
            const std::string categories = LinuxHostShell::GetOption(ctx, kCategoriesKey, kDefaultCategories);
            const std::string iconOpt    = LinuxHostShell::GetOption(ctx, kIconPathKey);
            const std::string updateInfo = LinuxHostShell::GetOption(ctx, kUpdateInfoKey);
            const std::string runtimeOpt = LinuxHostShell::GetOption(ctx, kRuntimePathKey);

            const std::string workDir    = projectDir + "Intermediate/appimage";
            const std::string appDir     = workDir + "/AppDir";
            const std::string scriptPath = workDir + "/build_appimage.sh";
            const std::string outputName = appName + "-x86_64.AppImage";

            LinuxHostShell::Report(ctx, "AppImage: assembling AppDir...");

            SYS_RemoveDirectory(workDir.c_str());

            const bool dirsOk =
                LinuxHostShell::EnsureHostDir(appDir + "/usr/bin") &&
                LinuxHostShell::EnsureHostDir(appDir + "/usr/share/applications") &&
                LinuxHostShell::EnsureHostDir(appDir + "/usr/share/icons/hicolor/128x128/apps");

            if (!dirsOk)
            {
                LogError("AppImage: failed to create the AppDir under %s", workDir.c_str());
                return 0;
            }

            // AppRun. readlink -f resolves the mount point the AppImage was
            // mounted at; cd into the payload so the engine's working-directory
            // -relative project resolution finds Config.ini and the asset tree.
            //
            // Saves are not redirected here on purpose: the AppImage mount is
            // read-only, so SYS_WriteSave's writability probe fails and the
            // engine falls back to $XDG_DATA_HOME/<Project>/Saves by itself.
            {
                std::string appRun;
                appRun += "#!/bin/sh\n";
                appRun += "HERE=\"$(dirname \"$(readlink -f \"$0\")\")\"\n";
                // $HERE has to expand, so the literal half is carried in its own
                // single-quoted variable rather than interpolated inline.
                appRun += "ELF=" + LinuxHostShell::Quote(projectName + ".elf") + "\n";
                appRun += "cd \"$HERE/usr/bin\" || exit 1\n";
                appRun += "exec \"$HERE/usr/bin/$ELF\" \"$@\"\n";

                if (!LinuxHostShell::WriteTextFileLF(appDir + "/AppRun", appRun))
                {
                    return 0;
                }
            }

            // appimagetool wants the .desktop at the AppDir root; the copy under
            // usr/share/applications is what desktop integration tools pick up.
            {
                std::string desktop;
                desktop += "[Desktop Entry]\n";
                desktop += "Type=Application\n";
                desktop += "Name=" + projectName + "\n";
                desktop += "Exec=" + appName + "\n";
                desktop += "Icon=" + appName + "\n";
                desktop += "Terminal=false\n";
                desktop += "Categories=" + categories + "\n";
                desktop += "X-AppImage-Version=" + version + "\n";

                if (!LinuxHostShell::WriteTextFileLF(appDir + "/" + appName + ".desktop", desktop) ||
                    !LinuxHostShell::WriteTextFileLF(
                        appDir + "/usr/share/applications/" + appName + ".desktop", desktop))
                {
                    return 0;
                }
            }

            // Icon. appimagetool refuses to build without one, so fall back to a
            // 1x1 PNG rather than failing the build over a missing asset.
            bool haveRealIcon = false;
            if (!iconOpt.empty())
            {
                std::string iconSrc = iconOpt;
                if (!SYS_DoesFileExist(iconSrc.c_str(), false))
                {
                    iconSrc = projectDir + iconOpt;
                }

                if (SYS_DoesFileExist(iconSrc.c_str(), false))
                {
                    haveRealIcon =
                        SYS_CopyFile(iconSrc.c_str(), (appDir + "/" + appName + ".png").c_str()) &&
                        SYS_CopyFile(iconSrc.c_str(),
                            (appDir + "/usr/share/icons/hicolor/128x128/apps/" + appName + ".png").c_str());

                    if (!haveRealIcon)
                    {
                        LogWarning("AppImage: failed to copy icon '%s'; using a placeholder.", iconSrc.c_str());
                    }
                }
                else
                {
                    LogWarning("AppImage: icon '%s' not found; using a placeholder.", iconOpt.c_str());
                }
            }

            if (!haveRealIcon)
            {
                // Fall back to the engine logo rather than failing the build.
                // appimagetool refuses to run without an icon, and a missing
                // game icon shouldn't be a hard stop on a test build.
                const std::string engineDir = ctx->engineDir != nullptr ? ctx->engineDir : "";
                const std::string logo = engineDir + "PolyphaseLogo_128.png";

                if (SYS_DoesFileExist(logo.c_str(), false))
                {
                    SYS_CopyFile(logo.c_str(), (appDir + "/" + appName + ".png").c_str());
                    SYS_CopyFile(logo.c_str(),
                        (appDir + "/usr/share/icons/hicolor/128x128/apps/" + appName + ".png").c_str());
                    LogWarning("AppImage: no icon set; using the engine logo. "
                               "Set the Icon option in the build profile to ship your own.");
                }
                else
                {
                    LogWarning("AppImage: no icon set and the engine logo was not found at %s. "
                               "appimagetool will likely refuse to build.", logo.c_str());
                }
            }

            LinuxHostShell::Report(ctx, "AppImage: building image...");

            {
                // Quote() everything the script interpolates — a project path
                // containing an apostrophe would otherwise break the script.
                const std::string shAppDir = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(appDir));
                const std::string shOut    = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(outDir));
                const std::string shWork   = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(workDir));

                std::string script;
                script += "#!/bin/sh\n";
                script += "set -e\n";
                script += "APPDIR=" + shAppDir + "\n";
                script += "OUTDIR=" + shOut + "\n";
                script += "WORK=" + shWork + "\n";
                script += "TARGET=\"$OUTDIR/" + outputName + "\"\n";
                // Single-quoted so a project name with shell metacharacters
                // stays literal.
                script += "ELF=" + LinuxHostShell::Quote(projectName + ".elf") + "\n";
                script += "\n";
                script += "cp -a \"$OUTDIR/.\" \"$APPDIR/usr/bin/\"\n";
                // Don't let a previous run's image end up inside the payload.
                script += "rm -f \"$APPDIR/usr/bin/" + outputName + "\"\n";
                script += "\n";
                // The execute bits are the whole ballgame: the engine never sets
                // one on packaged output, and squashfs preserves modes verbatim.
                script += "chmod -R u+rwX,go+rX \"$APPDIR\"\n";
                script += "chmod 0755 \"$APPDIR/AppRun\"\n";
                script += "chmod 0755 \"$APPDIR/usr/bin/$ELF\"\n";
                script += "\n";

                if (!runtimeOpt.empty())
                {
                    // Manual path: useful offline / in CI where appimagetool
                    // isn't available but squashfs-tools is. An AppImage is just
                    // the runtime ELF with a squashfs image appended.
                    script += "RUNTIME=" +
                              LinuxHostShell::Quote(LinuxHostShell::ToShellPath(runtimeOpt)) + "\n";
                    script += "if [ ! -f \"$RUNTIME\" ]; then echo \"runtime not found: $RUNTIME\" >&2; exit 1; fi\n";
                    script += "mksquashfs \"$APPDIR\" \"$WORK/payload.squashfs\" "
                              "-root-owned -noappend -no-progress\n";
                    script += "cat \"$RUNTIME\" \"$WORK/payload.squashfs\" > \"$TARGET\"\n";
                    script += "chmod 0755 \"$TARGET\"\n";
                }
                else
                {
                    // APPIMAGE_EXTRACT_AND_RUN lets appimagetool work where FUSE
                    // is unavailable — the norm under WSL and in containers. It's
                    // an env var rather than a flag so distro-packaged builds of
                    // appimagetool ignore it instead of erroring on it.
                    script += "export ARCH=x86_64\n";
                    script += "export APPIMAGE_EXTRACT_AND_RUN=1\n";
                    if (!updateInfo.empty())
                    {
                        script += "appimagetool -u " + LinuxHostShell::Quote(updateInfo) +
                                  " \"$APPDIR\" \"$TARGET\"\n";
                    }
                    else
                    {
                        script += "appimagetool \"$APPDIR\" \"$TARGET\"\n";
                    }
                    script += "chmod 0755 \"$TARGET\"\n";
                }

                script += "echo \"" + outputName + "\"\n";

                std::string out;
                if (LinuxHostShell::RunScript(script, scriptPath, &out, wslDistro) != 0)
                {
                    LogError("AppImage: packaging failed.\n%s", out.c_str());
                    LinuxHostShell::Report(ctx, "AppImage: FAILED — see the log for tool output.");
                    return 0;
                }

                LinuxHostShell::Report(ctx, "AppImage: wrote " + outputName);
            }

            return 1;
        }

        void DrawTextOption(const PolyphaseBuildContext* ctx, const char* label,
                            const char* key, const char* fallback, const char* tooltip)
        {
            char buf[512] = {0};
            const std::string current = LinuxHostShell::GetOption(ctx, key, fallback);
            std::snprintf(buf, sizeof(buf), "%s", current.c_str());

            if (ImGui::InputText(label, buf, sizeof(buf)) && ctx->SetProfileSetting != nullptr)
            {
                ctx->SetProfileSetting(key, buf);
            }

            if (tooltip != nullptr && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tooltip);
            }
        }

        void AppImage_DrawProfileOptions(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr) return;

            DrawTextOption(ctx, "Version", kVersionKey, "1.0.0",
                "Written to the desktop entry as X-AppImage-Version.");
            DrawTextOption(ctx, "Categories", kCategoriesKey, kDefaultCategories,
                "Freedesktop categories, semicolon-terminated (e.g. \"Game;ActionGame;\").");
            DrawTextOption(ctx, "Icon (PNG)", kIconPathKey, "",
                "128x128 PNG, absolute or relative to the project dir.\n"
                "AppImages require an icon; a blank placeholder is used if this is empty.");
            DrawTextOption(ctx, "Update Information", kUpdateInfoKey, "",
                "Optional appimagetool -u string, e.g.\n"
                "  gh-releases-zsync|owner|repo|latest|MyGame-*-x86_64.AppImage.zsync\n"
                "Enables delta updates via AppImageUpdate.");
            DrawTextOption(ctx, "Runtime File (optional)", kRuntimePathKey, "",
                "Path to an AppImage runtime binary. When set, the image is built\n"
                "with mksquashfs + cat instead of appimagetool — useful offline\n"
                "or in CI. Leave empty to use appimagetool.");

            if (LinuxHostShell::UsesWsl())
            {
                DrawTextOption(ctx, "WSL Distro", kWslDistroKey, "",
                    "Windows-only: passed to `wsl -d <name>`. Empty uses the default distro.");
                ImGui::TextDisabled("appimagetool runs inside WSL2 on Windows hosts.");
            }

            ImGui::Separator();
            ImGui::TextDisabled("The image inherits the glibc of the host that built");
            ImGui::TextDisabled("the ELF and won't start on older distros. Build in the");
            ImGui::TextDisabled("oldest supported image (Docker/build.sh) for portability.");
            ImGui::Spacing();
            ImGui::TextDisabled("The mount is read-only, so saves go to");
            ImGui::TextDisabled("~/.local/share/<Project>/Saves.");
        }
    }

    void FillDesc(PolyphaseBuildTargetDesc& outDesc)
    {
        outDesc = PolyphaseBuildTargetDesc{};
        outDesc.apiVersion          = POLYPHASE_BUILD_TARGET_API_VERSION;
        outDesc.targetId            = kTargetId;
        outDesc.displayName         = "Linux (AppImage)";
        outDesc.iconText            = "";
        outDesc.category            = "Desktop";
        outDesc.basePlatform        = (int32_t)Platform::Linux;
        // Same reasoning as the RPM target: the compiled artifact is the ELF,
        // and the .AppImage is produced beside it by PostPackage.
        outDesc.binaryExtension     = ".elf";
        outDesc.requiresDocker      = 0;
        outDesc.supportsRunOnDevice = 0;
        outDesc.supportsEmulator    = 0;

        outDesc.Validate            = &AppImage_Validate;
        outDesc.PostPackage         = &AppImage_PostPackage;
        outDesc.DrawProfileOptions  = &AppImage_DrawProfileOptions;
    }
}

#endif /* EDITOR */
