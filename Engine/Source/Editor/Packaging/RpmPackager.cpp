#if EDITOR

#include "RpmPackager.h"
#include "LinuxHostShell.h"

#include "EngineTypes.h"
#include "System/System.h"
#include "Log.h"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace RpmPackager
{
    const char* const kTargetId = "polyphase.linux.rpm";

    namespace
    {
        constexpr const char* kVersionKey   = "rpm.version";
        constexpr const char* kReleaseKey   = "rpm.release";
        constexpr const char* kLicenseKey   = "rpm.license";
        constexpr const char* kSummaryKey   = "rpm.summary";
        constexpr const char* kPrefixKey    = "rpm.prefix";
        constexpr const char* kRequiresKey  = "rpm.requires";
        constexpr const char* kIconPathKey  = "rpm.iconPath";
        constexpr const char* kWslDistroKey = "rpm.wslDistro";

        // Fedora/openSUSE names. They differ from the Debian names used by
        // Installers/build_deb_linux.sh (libvulkan1, libxcb1, libasound2), which
        // is why this is user-overridable — RPM distros don't agree among
        // themselves either.
        constexpr const char* kDefaultRequires = "vulkan-loader, libxcb, alsa-lib";
        constexpr const char* kDefaultPrefix   = "/opt";

        // RPM package names must be lowercase and free of spaces.
        std::string ToPackageName(const std::string& projectName)
        {
            std::string out;
            out.reserve(projectName.size());

            for (char c : projectName)
            {
                if (std::isalnum((unsigned char)c))
                {
                    out += (char)std::tolower((unsigned char)c);
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

            return out.empty() ? std::string("polyphase-game") : out;
        }

        // '-' separates Version from Release in an RPM filename, so it can never
        // appear inside either field.
        std::string SanitizeVersionField(const std::string& value, const char* fallback)
        {
            std::string out;
            for (char c : value)
            {
                if (std::isalnum((unsigned char)c) || c == '.' || c == '_' || c == '+')
                {
                    out += c;
                }
                else if (c == '-')
                {
                    out += '_';
                }
            }
            return out.empty() ? std::string(fallback) : out;
        }

        int32_t Rpm_Validate(char* outReason, size_t reasonCap)
        {
            if (LinuxHostShell::HasTool("rpmbuild"))
            {
                return 1;
            }

            if (outReason != nullptr && reasonCap > 0)
            {
                std::snprintf(outReason, reasonCap, "%s",
                    LinuxHostShell::UsesWsl()
                        ? "rpmbuild not reachable via WSL. Install WSL2 + a distro, then inside "
                          "WSL run: sudo apt install rpm (Debian/Ubuntu) or sudo dnf install "
                          "rpm-build (Fedora)."
                        : "rpmbuild not found on PATH. Install it with: sudo dnf install "
                          "rpm-build (Fedora) or sudo apt install rpm (Debian/Ubuntu).");
            }

            return 0;
        }

        int32_t Rpm_PostPackage(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr || ctx->packageOutputDir == nullptr || ctx->projectName == nullptr)
            {
                LogError("RPM: build context is incomplete.");
                return 0;
            }

            const std::string projectName = ctx->projectName;
            const std::string projectDir  = ctx->projectDir != nullptr ? ctx->projectDir : "";
            const std::string outDir      = ctx->packageOutputDir;
            const std::string wslDistro   = LinuxHostShell::GetOption(ctx, kWslDistroKey);

            const std::string pkgName = ToPackageName(projectName);
            const std::string version = SanitizeVersionField(
                LinuxHostShell::GetOption(ctx, kVersionKey, "1.0.0"), "1.0.0");
            const std::string release = SanitizeVersionField(
                LinuxHostShell::GetOption(ctx, kReleaseKey, "1"), "1");
            const std::string license      = LinuxHostShell::GetOption(ctx, kLicenseKey, "Proprietary");
            const std::string summary      = LinuxHostShell::GetOption(ctx, kSummaryKey,
                                                 projectName + " (built with Polyphase Engine)");
            const std::string prefix       = LinuxHostShell::GetOption(ctx, kPrefixKey, kDefaultPrefix);
            const std::string requiresList = LinuxHostShell::GetOption(ctx, kRequiresKey, kDefaultRequires);
            const std::string iconOpt      = LinuxHostShell::GetOption(ctx, kIconPathKey);

            const std::string installDir = prefix + "/" + pkgName;

            // Scratch lives under Intermediate/ so Force Rebuild's wipe of that
            // directory cleans it up for free.
            const std::string topDir     = projectDir + "Intermediate/rpm";
            const std::string stageDir   = topDir + "/stage";
            const std::string specPath   = topDir + "/SPECS/" + pkgName + ".spec";
            const std::string scriptPath = topDir + "/build_rpm.sh";

            LinuxHostShell::Report(ctx, "RPM: staging payload...");

            // Start from a clean tree — a stale stage/ would be copied verbatim
            // into the package.
            SYS_RemoveDirectory(topDir.c_str());

            const bool dirsOk =
                LinuxHostShell::EnsureHostDir(topDir + "/SPECS") &&
                LinuxHostShell::EnsureHostDir(topDir + "/RPMS") &&
                LinuxHostShell::EnsureHostDir(topDir + "/BUILD") &&
                LinuxHostShell::EnsureHostDir(topDir + "/BUILDROOT") &&
                LinuxHostShell::EnsureHostDir(stageDir + installDir) &&
                LinuxHostShell::EnsureHostDir(stageDir + "/usr/bin") &&
                LinuxHostShell::EnsureHostDir(stageDir + "/usr/share/applications") &&
                LinuxHostShell::EnsureHostDir(stageDir + "/usr/share/icons/hicolor/128x128/apps");

            if (!dirsOk)
            {
                LogError("RPM: failed to create the staging tree under %s", topDir.c_str());
                return 0;
            }

            // Launcher wrapper. The engine resolves project paths relative to the
            // working directory, so this must cd before exec — the same reason
            // build_deb_linux.sh installs a wrapper rather than a symlink.
            {
                std::string wrapper;
                wrapper += "#!/bin/sh\n";
                wrapper += "cd " + LinuxHostShell::Quote(installDir) + " || exit 1\n";
                wrapper += "exec " + LinuxHostShell::Quote(installDir + "/" + projectName + ".elf") +
                           " \"$@\"\n";

                if (!LinuxHostShell::WriteTextFileLF(stageDir + "/usr/bin/" + pkgName, wrapper))
                {
                    return 0;
                }
            }

            {
                std::string desktop;
                desktop += "[Desktop Entry]\n";
                desktop += "Type=Application\n";
                desktop += "Name=" + projectName + "\n";
                desktop += "Comment=" + summary + "\n";
                desktop += "Exec=/usr/bin/" + pkgName + "\n";
                desktop += "Icon=" + pkgName + "\n";
                desktop += "Terminal=false\n";
                desktop += "Categories=Game;\n";

                if (!LinuxHostShell::WriteTextFileLF(
                        stageDir + "/usr/share/applications/" + pkgName + ".desktop", desktop))
                {
                    return 0;
                }
            }

            // Optional icon, resolved relative to the project dir when not absolute.
            bool hasIcon = false;
            if (!iconOpt.empty())
            {
                std::string iconSrc = iconOpt;
                if (!SYS_DoesFileExist(iconSrc.c_str(), false))
                {
                    iconSrc = projectDir + iconOpt;
                }

                if (SYS_DoesFileExist(iconSrc.c_str(), false))
                {
                    const std::string dest =
                        stageDir + "/usr/share/icons/hicolor/128x128/apps/" + pkgName + ".png";
                    hasIcon = SYS_CopyFile(iconSrc.c_str(), dest.c_str());
                    if (!hasIcon)
                    {
                        LogWarning("RPM: failed to copy icon '%s'; packaging without one.", iconSrc.c_str());
                    }
                }
                else
                {
                    LogWarning("RPM: icon '%s' not found; packaging without one.", iconOpt.c_str());
                }
            }

            {
                std::string spec;
                // Suppress everything rpmbuild normally does to a compiled
                // payload. The ELF is already built and stripped; letting
                // brp-strip and debuginfo extraction run against it either fails
                // outright or rewrites the binary.
                spec += "%global debug_package %{nil}\n";
                spec += "%define __os_install_post %{nil}\n";
                spec += "%define _build_id_links none\n\n";

                spec += "Name:           " + pkgName + "\n";
                spec += "Version:        " + version + "\n";
                spec += "Release:        " + release + "\n";
                spec += "Summary:        " + summary + "\n";
                spec += "License:        " + license + "\n";
                spec += "BuildArch:      x86_64\n";
                if (!requiresList.empty())
                {
                    spec += "Requires:       " + requiresList + "\n";
                }
                // Without this, rpmbuild introspects the ELF and emits automatic
                // Requires on raw soname strings (libvulkan.so.1()(64bit), ...)
                // that frequently don't resolve on the target distro. The
                // explicit Requires above are the contract instead.
                spec += "AutoReqProv:    no\n\n";

                spec += "%description\n" + summary + "\n\n";

                spec += "%install\n";
                spec += "rm -rf %{buildroot}\n";
                spec += "mkdir -p %{buildroot}\n";
                // %install is shell; quote the staging path in case the project
                // lives somewhere with spaces.
                spec += "cp -a " +
                        LinuxHostShell::Quote(LinuxHostShell::ToShellPath(stageDir) + "/.") +
                        " %{buildroot}/\n\n";

                spec += "%files\n";
                spec += "%defattr(-,root,root,-)\n";
                spec += installDir + "\n";
                spec += "/usr/bin/" + pkgName + "\n";
                spec += "/usr/share/applications/" + pkgName + ".desktop\n";
                if (hasIcon)
                {
                    spec += "/usr/share/icons/hicolor/128x128/apps/" + pkgName + ".png\n";
                }
                spec += "\n";

                spec += "%post\n";
                spec += "command -v update-desktop-database >/dev/null 2>&1 && "
                        "update-desktop-database /usr/share/applications 2>/dev/null || true\n";
                spec += "command -v gtk-update-icon-cache >/dev/null 2>&1 && "
                        "gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true\n";
                spec += "exit 0\n\n";

                spec += "%postun\n";
                spec += "command -v update-desktop-database >/dev/null 2>&1 && "
                        "update-desktop-database /usr/share/applications 2>/dev/null || true\n";
                spec += "exit 0\n\n";

                spec += "%changelog\n";

                if (!LinuxHostShell::WriteTextFileLF(specPath, spec))
                {
                    return 0;
                }
            }

            LinuxHostShell::Report(ctx, "RPM: running rpmbuild...");

            // One script does the payload copy, the mode normalisation and the
            // rpmbuild run. Everything crossing the shell boundary is a single
            // quoted script path, so no nested-quoting games.
            {
                // Quote() everything the script interpolates — a project path
                // containing an apostrophe would otherwise break the script.
                const std::string shStage = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(stageDir));
                const std::string shOut   = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(outDir));
                const std::string shTop   = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(topDir));
                const std::string shSpec  = LinuxHostShell::Quote(LinuxHostShell::ToShellPath(specPath));

                std::string script;
                script += "#!/bin/sh\n";
                script += "set -e\n";
                script += "STAGE=" + shStage + "\n";
                script += "OUTDIR=" + shOut + "\n";
                script += "TOPDIR=" + shTop + "\n";
                script += "INSTALL=\"$STAGE" + installDir + "\"\n";
                // Single-quoted so a project name with shell metacharacters
                // stays literal.
                script += "ELF=" + LinuxHostShell::Quote(projectName + ".elf") + "\n";
                script += "\n";
                script += "cp -a \"$OUTDIR/.\" \"$INSTALL/\"\n";
                // The packaged ELF never had an execute bit, and a DrvFs/NTFS
                // source hands out unreliable modes regardless.
                script += "chmod -R u+rwX,go+rX \"$STAGE\"\n";
                script += "chmod 0755 \"$INSTALL/$ELF\"\n";
                script += "chmod 0755 \"$STAGE/usr/bin/" + pkgName + "\"\n";
                script += "\n";
                script += "rpmbuild -bb --define \"_topdir $TOPDIR\" " + shSpec + "\n";
                script += "\n";
                script += "SRC=$(find \"$TOPDIR/RPMS\" -name '*.rpm' -type f | head -n 1)\n";
                script += "if [ -z \"$SRC\" ]; then echo 'no .rpm produced' >&2; exit 1; fi\n";
                script += "cp -f \"$SRC\" \"$OUTDIR/\"\n";
                script += "basename \"$SRC\"\n";

                std::string out;
                if (LinuxHostShell::RunScript(script, scriptPath, &out, wslDistro) != 0)
                {
                    LogError("RPM: packaging failed.\n%s", out.c_str());
                    LinuxHostShell::Report(ctx, "RPM: FAILED — see the log for rpmbuild output.");
                    return 0;
                }

                while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
                {
                    out.pop_back();
                }

                const size_t lastLine = out.find_last_of('\n');
                const std::string rpmName = (lastLine == std::string::npos) ? out : out.substr(lastLine + 1);

                LinuxHostShell::Report(ctx, "RPM: wrote " + rpmName);
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

        void Rpm_DrawProfileOptions(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr) return;

            DrawTextOption(ctx, "Version", kVersionKey, "1.0.0",
                "RPM Version field. Hyphens become underscores — RPM uses '-'\n"
                "to separate Version from Release.");
            DrawTextOption(ctx, "Release", kReleaseKey, "1",
                "RPM Release field. Bump for a repackage of the same game version.");
            DrawTextOption(ctx, "License", kLicenseKey, "Proprietary", nullptr);
            DrawTextOption(ctx, "Summary", kSummaryKey, "", "One-line package description.");
            DrawTextOption(ctx, "Install Prefix", kPrefixKey, kDefaultPrefix,
                "Game tree installs to <prefix>/<package-name>. A wrapper at\n"
                "/usr/bin/<package-name> cds there and launches it.");
            DrawTextOption(ctx, "Requires", kRequiresKey, kDefaultRequires,
                "Comma-separated RPM dependencies. Defaults are Fedora names;\n"
                "openSUSE and others differ, so override as needed.\n"
                "Automatic dependency scanning is disabled on purpose.");
            DrawTextOption(ctx, "Icon (PNG)", kIconPathKey, "",
                "Optional 128x128 PNG, absolute or relative to the project dir.\n"
                "Left empty, the package ships without a desktop icon.");

            if (LinuxHostShell::UsesWsl())
            {
                DrawTextOption(ctx, "WSL Distro", kWslDistroKey, "",
                    "Windows-only: passed to `wsl -d <name>`. Empty uses the default distro.");
                ImGui::TextDisabled("rpmbuild runs inside WSL2 on Windows hosts.");
                ImGui::TextDisabled("  Inside WSL: sudo apt install rpm");
            }

            ImGui::Separator();
            ImGui::TextDisabled("Saves fall back to ~/.local/share/<Project>/Saves");
            ImGui::TextDisabled("when the install prefix is not writable.");
        }
    }

    void FillDesc(PolyphaseBuildTargetDesc& outDesc)
    {
        outDesc = PolyphaseBuildTargetDesc{};
        outDesc.apiVersion          = POLYPHASE_BUILD_TARGET_API_VERSION;
        outDesc.targetId            = kTargetId;
        outDesc.displayName         = "Linux (RPM)";
        outDesc.iconText            = "";
        outDesc.category            = "Desktop";
        outDesc.basePlatform        = (int32_t)Platform::Linux;
        // The compiled artifact is still an ELF; the .rpm is produced beside it
        // by PostPackage. Declaring ".rpm" here would break the engine's
        // post-copy "<projectName><ext> exists" check in FinalizeLocalBuild.
        outDesc.binaryExtension     = ".elf";
        outDesc.requiresDocker      = 0;
        outDesc.supportsRunOnDevice = 0;
        outDesc.supportsEmulator    = 0;

        // GetCompileCommand deliberately left null: the legacy Linux make path
        // builds the ELF exactly as the plain Linux target does.
        outDesc.Validate            = &Rpm_Validate;
        outDesc.PostPackage         = &Rpm_PostPackage;
        outDesc.DrawProfileOptions  = &Rpm_DrawProfileOptions;
    }
}

#endif /* EDITOR */
