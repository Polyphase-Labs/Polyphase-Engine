#if EDITOR

#include "MacBundlePackager.h"
#include "LinuxHostShell.h"

#include "Engine.h"
#include "EngineTypes.h"
#include "System/System.h"
#include "Log.h"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace MacBundlePackager
{
    const char* const kTargetId = "polyphase.mac";

    namespace
    {
        constexpr const char* kBundleIdKey      = "mac.bundleId";
        constexpr const char* kVersionKey       = "mac.version";
        constexpr const char* kMinOsKey         = "mac.minOsVersion";
        constexpr const char* kIconPathKey      = "mac.iconPath";
        constexpr const char* kSignIdentityKey  = "mac.signingIdentity";
        constexpr const char* kNotarizeKey      = "mac.notarize";
        constexpr const char* kNotaryProfileKey = "mac.notaryProfile";
        constexpr const char* kCreateDmgKey     = "mac.createDmg";

        constexpr const char* kDefaultMinOs = "12.0";

        // Bundle identifiers are reverse-DNS: alphanumerics, '.' and '-'.
        std::string ToAppName(const std::string& projectName)
        {
            std::string out;
            out.reserve(projectName.size());

            for (char c : projectName)
            {
                if (std::isalnum((unsigned char)c))
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

        std::string XmlEscape(const std::string& in)
        {
            std::string out;
            out.reserve(in.size());
            for (char c : in)
            {
                switch (c)
                {
                case '&':  out += "&amp;"; break;
                case '<':  out += "&lt;"; break;
                case '>':  out += "&gt;"; break;
                case '"':  out += "&quot;"; break;
                default:   out += c; break;
                }
            }
            return out;
        }

        bool HasSuffix(const std::string& str, const char* suffix)
        {
            size_t n = strlen(suffix);
            return str.size() >= n && str.compare(str.size() - n, n, suffix) == 0;
        }

        std::string BuildInfoPlist(const std::string& projectName, const std::string& bundleId,
                                   const std::string& version, const std::string& minOs,
                                   const std::string& iconName)
        {
            std::string p;
            p += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            p += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
            p += "<plist version=\"1.0\">\n<dict>\n";
            p += "\t<key>CFBundleDevelopmentRegion</key>\n\t<string>en</string>\n";
            p += "\t<key>CFBundleExecutable</key>\n\t<string>" + XmlEscape(projectName) + "</string>\n";
            p += "\t<key>CFBundleIdentifier</key>\n\t<string>" + XmlEscape(bundleId) + "</string>\n";
            p += "\t<key>CFBundleName</key>\n\t<string>" + XmlEscape(projectName) + "</string>\n";
            p += "\t<key>CFBundleDisplayName</key>\n\t<string>" + XmlEscape(projectName) + "</string>\n";
            p += "\t<key>CFBundlePackageType</key>\n\t<string>APPL</string>\n";
            p += "\t<key>CFBundleInfoDictionaryVersion</key>\n\t<string>6.0</string>\n";
            p += "\t<key>CFBundleShortVersionString</key>\n\t<string>" + XmlEscape(version) + "</string>\n";
            p += "\t<key>CFBundleVersion</key>\n\t<string>" + XmlEscape(version) + "</string>\n";
            if (!iconName.empty())
            {
                p += "\t<key>CFBundleIconFile</key>\n\t<string>" + XmlEscape(iconName) + "</string>\n";
            }
            p += "\t<key>LSMinimumSystemVersion</key>\n\t<string>" + XmlEscape(minOs) + "</string>\n";
            p += "\t<key>LSApplicationCategoryType</key>\n\t<string>public.app-category.games</string>\n";
            p += "\t<key>NSHighResolutionCapable</key>\n\t<true/>\n";
            p += "\t<key>NSSupportsAutomaticGraphicsSwitching</key>\n\t<true/>\n";
            p += "</dict>\n</plist>\n";
            return p;
        }

        // Read the api_version MoltenVK advertises from the SDK's own manifest
        // so the bundled ICD json stays in step with the bundled dylib.
        std::string ReadSdkIcdApiVersion(const std::string& sdkRoot)
        {
            std::string result = "1.2.0";
            std::string path = sdkRoot + "/share/vulkan/icd.d/MoltenVK_icd.json";
            char* data = nullptr;
            uint32_t size = 0;
            if (SYS_DoesFileExist(path.c_str(), false))
            {
                SYS_AcquireFileData(path.c_str(), false, 0, data, size);
            }
            if (data != nullptr)
            {
                std::string text(data, size);
                SYS_ReleaseFileData(data);
                size_t key = text.find("\"api_version\"");
                if (key != std::string::npos)
                {
                    size_t q1 = text.find('"', key + 13);
                    size_t q2 = (q1 == std::string::npos) ? q1 : text.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1)
                    {
                        result = text.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
            return result;
        }

        int32_t Mac_Validate(char* outReason, size_t reasonCap)
        {
#if !PLATFORM_MAC
            if (outReason != nullptr && reasonCap > 0)
            {
                std::snprintf(outReason, reasonCap, "%s",
                    "macOS targets can only be built on a macOS host (Apple toolchain, codesign).");
            }
            return 0;
#else
            const char* missing = nullptr;
            if (!LinuxHostShell::HasTool("codesign"))       missing = "codesign (install the Xcode Command Line Tools: xcode-select --install)";
            else if (!LinuxHostShell::HasTool("iconutil"))  missing = "iconutil (install the Xcode Command Line Tools)";
            else if (!LinuxHostShell::HasTool("xcrun"))     missing = "xcrun (install the Xcode Command Line Tools)";
            else if (!LinuxHostShell::HasTool("make"))      missing = "make (install the Xcode Command Line Tools)";

            if (missing == nullptr)
            {
                std::string sdk = ResolveVulkanSdkRoot();
                if (sdk.empty() || !SYS_DoesFileExist((sdk + "/lib/libMoltenVK.dylib").c_str(), false))
                {
                    missing = "Vulkan SDK (LunarG) with lib/libMoltenVK.dylib — set VULKAN_SDK or install to ~/VulkanSDK";
                }
            }

            if (missing != nullptr)
            {
                if (outReason != nullptr && reasonCap > 0)
                {
                    std::snprintf(outReason, reasonCap, "Missing: %s", missing);
                }
                return 0;
            }
            return 1;
#endif
        }

        int32_t Mac_PostPackage(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr || ctx->packageOutputDir == nullptr || ctx->projectName == nullptr)
            {
                LogError("MacBundle: build context is incomplete.");
                return 0;
            }

            const std::string projectName = ctx->projectName;
            const std::string projectDir  = ctx->projectDir != nullptr ? ctx->projectDir : "";
            const std::string engineDir   = ctx->engineDir != nullptr ? ctx->engineDir : "";
            const std::string outDir      = ctx->packageOutputDir;

            const std::string appName   = ToAppName(projectName);
            const std::string bundleId  = LinuxHostShell::GetOption(ctx, kBundleIdKey, "com.polyphase." + appName);
            const std::string version   = LinuxHostShell::GetOption(ctx, kVersionKey, "1.0.0");
            const std::string minOs     = LinuxHostShell::GetOption(ctx, kMinOsKey, kDefaultMinOs);
            const std::string iconOpt   = LinuxHostShell::GetOption(ctx, kIconPathKey);
            const std::string identity  = LinuxHostShell::GetOption(ctx, kSignIdentityKey);
            const bool notarize         = LinuxHostShell::GetOption(ctx, kNotarizeKey, "0") == "1";
            const std::string notaryPro = LinuxHostShell::GetOption(ctx, kNotaryProfileKey);
            const bool createDmg        = LinuxHostShell::GetOption(ctx, kCreateDmgKey, "0") == "1";

            const std::string sdkRoot = ResolveVulkanSdkRoot();
            if (sdkRoot.empty())
            {
                LogError("MacBundle: Vulkan SDK not found (set VULKAN_SDK or install to ~/VulkanSDK).");
                return 0;
            }

            const std::string workDir    = projectDir + "Intermediate/mac";
            const std::string scriptPath = workDir + "/build_app.sh";
            const std::string appPath    = outDir + projectName + ".app";

            LinuxHostShell::Report(ctx, "MacBundle: assembling " + projectName + ".app ...");

            SYS_RemoveDirectory(workDir.c_str());
            if (!LinuxHostShell::EnsureHostDir(workDir + "/iconset.iconset"))
            {
                LogError("MacBundle: failed to create %s", workDir.c_str());
                return 0;
            }

            // Icon source: profile option -> project icon (PNG only; the
            // Windows .ico default can't feed iconutil) -> engine logo.
            std::string iconSrc;
            if (!iconOpt.empty())
            {
                iconSrc = SYS_DoesFileExist(iconOpt.c_str(), false) ? iconOpt : projectDir + iconOpt;
                if (!SYS_DoesFileExist(iconSrc.c_str(), false))
                {
                    LogWarning("MacBundle: icon '%s' not found; falling back.", iconOpt.c_str());
                    iconSrc.clear();
                }
            }
            if (iconSrc.empty())
            {
                const std::string& cfgIcon = GetEngineConfig()->mIconPath;
                if (!cfgIcon.empty() && HasSuffix(cfgIcon, ".png"))
                {
                    std::string candidate = SYS_DoesFileExist(cfgIcon.c_str(), false) ? cfgIcon : projectDir + cfgIcon;
                    if (SYS_DoesFileExist(candidate.c_str(), false))
                    {
                        iconSrc = candidate;
                    }
                }
            }
            if (iconSrc.empty())
            {
                std::string logo = engineDir + "PolyphaseLogo_256.png";
                if (SYS_DoesFileExist(logo.c_str(), false))
                {
                    iconSrc = logo;
                    LogWarning("MacBundle: no icon set; using the engine logo. "
                               "Set the Icon option in the build profile to ship your own.");
                }
            }
            const std::string iconName = iconSrc.empty() ? "" : appName + ".icns";

            // Files generated from C++ (no shell quoting worries).
            if (!LinuxHostShell::WriteTextFileLF(workDir + "/Info.plist",
                    BuildInfoPlist(projectName, bundleId, version, minOs, iconName)))
            {
                return 0;
            }

            {
                std::string icd;
                icd += "{\n";
                icd += "    \"file_format_version\": \"1.0.0\",\n";
                icd += "    \"ICD\": {\n";
                icd += "        \"library_path\": \"../../../Frameworks/libMoltenVK.dylib\",\n";
                icd += "        \"api_version\": \"" + ReadSdkIcdApiVersion(sdkRoot) + "\",\n";
                icd += "        \"is_portability_driver\": true\n";
                icd += "    }\n";
                icd += "}\n";
                if (!LinuxHostShell::WriteTextFileLF(workDir + "/MoltenVK_icd.json", icd))
                {
                    return 0;
                }
            }

            if (!identity.empty())
            {
                // Hardened runtime + third-party dylibs (addons, MoltenVK built
                // elsewhere) need library validation disabled.
                std::string ent;
                ent += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                ent += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
                ent += "<plist version=\"1.0\">\n<dict>\n";
                ent += "\t<key>com.apple.security.cs.disable-library-validation</key>\n\t<true/>\n";
                ent += "\t<key>com.apple.security.cs.allow-unsigned-executable-memory</key>\n\t<true/>\n";
                ent += "</dict>\n</plist>\n";
                if (!LinuxHostShell::WriteTextFileLF(workDir + "/entitlements.plist", ent))
                {
                    return 0;
                }
            }

            LinuxHostShell::Report(ctx, "MacBundle: copying payload, Vulkan runtime and signing...");

            const std::string shOut     = LinuxHostShell::Quote(outDir);
            const std::string shApp     = LinuxHostShell::Quote(appPath);
            const std::string shWork    = LinuxHostShell::Quote(workDir);
            const std::string shExe     = LinuxHostShell::Quote(projectName);
            const std::string shSdk     = LinuxHostShell::Quote(sdkRoot);
            const std::string shIcon    = LinuxHostShell::Quote(iconSrc);
            const std::string shIconNm  = LinuxHostShell::Quote(iconName);
            const std::string shIdent   = LinuxHostShell::Quote(identity.empty() ? "-" : identity);
            const std::string shNotary  = LinuxHostShell::Quote(notaryPro);
            const std::string shDmg     = LinuxHostShell::Quote(outDir + appName + ".dmg");
            const std::string shVolName = LinuxHostShell::Quote(projectName);

            std::string s;
            s += "#!/bin/bash\n";
            s += "set -e\n";
            s += "OUTDIR=" + shOut + "\n";
            s += "APP=" + shApp + "\n";
            s += "WORK=" + shWork + "\n";
            s += "EXE=" + shExe + "\n";
            s += "SDK=" + shSdk + "\n";
            s += "ICONSRC=" + shIcon + "\n";
            s += "ICONNAME=" + shIconNm + "\n";
            s += "IDENTITY=" + shIdent + "\n";
            s += "\n";
            s += "rm -rf \"$APP\"\n";
            s += "mkdir -p \"$APP/Contents/MacOS\" \"$APP/Contents/Frameworks\" \"$APP/Contents/Resources/vulkan/icd.d\"\n";
            s += "\n";
            s += "# Executable + game addons.\n";
            s += "if [ ! -f \"$OUTDIR/$EXE.macho\" ]; then echo \"packaged executable not found: $OUTDIR/$EXE.macho\" >&2; exit 1; fi\n";
            s += "cp \"$OUTDIR/$EXE.macho\" \"$APP/Contents/MacOS/$EXE\"\n";
            s += "chmod 0755 \"$APP/Contents/MacOS/$EXE\"\n";
            s += "if [ -d \"$OUTDIR/Addons\" ]; then mkdir -p \"$APP/Contents/MacOS/Addons\"; cp -R \"$OUTDIR/Addons/.\" \"$APP/Contents/MacOS/Addons/\"; fi\n";
            s += "\n";
            s += "# Payload -> Resources (the engine pivots its working directory here).\n";
            s += "rsync -a --exclude '*.app' --exclude '*.dmg' --exclude '*.macho' --exclude '/Addons' --exclude '/*.dylib' \"$OUTDIR/.\" \"$APP/Contents/Resources/\"\n";
            s += "\n";
            s += "# Vulkan runtime: loader + MoltenVK + any addon dylibs dropped next to the exe.\n";
            s += "cp \"$SDK/lib/libvulkan.1.dylib\" \"$SDK/lib/libMoltenVK.dylib\" \"$APP/Contents/Frameworks/\"\n";
            s += "for f in \"$OUTDIR\"/*.dylib; do [ -f \"$f\" ] && cp \"$f\" \"$APP/Contents/Frameworks/\"; done\n";
            s += "cp \"$WORK/MoltenVK_icd.json\" \"$APP/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json\"\n";
            s += "cp \"$WORK/Info.plist\" \"$APP/Contents/Info.plist\"\n";
            // "?\?" keeps GCC from reading "??'" as a trigraph.
            s += "printf 'APPL?\?\?\?' > \"$APP/Contents/PkgInfo\"\n";
            s += "\n";
            s += "# Make the exe find the bundled loader through @rpath (before signing:\n";
            s += "# editing a Mach-O invalidates its signature).\n";
            s += "install_name_tool -id @rpath/libvulkan.1.dylib \"$APP/Contents/Frameworks/libvulkan.1.dylib\" || true\n";
            s += "VKREF=$(otool -L \"$APP/Contents/MacOS/$EXE\" | awk '/libvulkan/{print $1; exit}')\n";
            s += "if [ -n \"$VKREF\" ] && [ \"$VKREF\" != \"@rpath/libvulkan.1.dylib\" ]; then\n";
            s += "    install_name_tool -change \"$VKREF\" @rpath/libvulkan.1.dylib \"$APP/Contents/MacOS/$EXE\"\n";
            s += "fi\n";
            s += "if ! otool -l \"$APP/Contents/MacOS/$EXE\" | grep -q '@executable_path/../Frameworks'; then\n";
            s += "    install_name_tool -add_rpath @executable_path/../Frameworks \"$APP/Contents/MacOS/$EXE\"\n";
            s += "fi\n";
            s += "\n";
            s += "# Icon.\n";
            s += "if [ -n \"$ICONSRC\" ] && [ -f \"$ICONSRC\" ]; then\n";
            s += "    SET=\"$WORK/iconset.iconset\"\n";
            s += "    for sz in 16 32 128 256 512; do\n";
            s += "        sips -z $sz $sz \"$ICONSRC\" --out \"$SET/icon_${sz}x${sz}.png\" >/dev/null\n";
            s += "        dbl=$((sz * 2))\n";
            s += "        sips -z $dbl $dbl \"$ICONSRC\" --out \"$SET/icon_${sz}x${sz}@2x.png\" >/dev/null\n";
            s += "    done\n";
            s += "    iconutil -c icns \"$SET\" -o \"$APP/Contents/Resources/$ICONNAME\"\n";
            s += "fi\n";
            s += "\n";
            s += "# Sign inside-out. Ad-hoc ('-') unless an identity was given.\n";
            s += "SIGNFLAGS=()\n";
            s += "if [ \"$IDENTITY\" != \"-\" ]; then SIGNFLAGS=(--options runtime --timestamp --entitlements \"$WORK/entitlements.plist\"); fi\n";
            s += "for f in \"$APP\"/Contents/Frameworks/*.dylib \"$APP\"/Contents/MacOS/Addons/*.dylib; do\n";
            s += "    [ -f \"$f\" ] && codesign --force --sign \"$IDENTITY\" \"${SIGNFLAGS[@]}\" \"$f\"\n";
            s += "done\n";
            s += "codesign --force --sign \"$IDENTITY\" \"${SIGNFLAGS[@]}\" \"$APP\"\n";
            s += "codesign --verify --deep --strict \"$APP\"\n";

            if (notarize && !identity.empty())
            {
                s += "\n# Notarize + staple.\n";
                s += "NOTARYPROFILE=" + shNotary + "\n";
                s += "ditto -c -k --keepParent \"$APP\" \"$WORK/notary.zip\"\n";
                s += "xcrun notarytool submit \"$WORK/notary.zip\" --keychain-profile \"$NOTARYPROFILE\" --wait\n";
                s += "xcrun stapler staple \"$APP\"\n";
            }

            if (createDmg)
            {
                s += "\n# Disk image with an Applications shortcut.\n";
                s += "DMG=" + shDmg + "\n";
                s += "VOLNAME=" + shVolName + "\n";
                s += "rm -rf \"$WORK/dmgroot\" \"$DMG\"\n";
                s += "mkdir -p \"$WORK/dmgroot\"\n";
                s += "cp -R \"$APP\" \"$WORK/dmgroot/\"\n";
                s += "ln -s /Applications \"$WORK/dmgroot/Applications\"\n";
                s += "for attempt in 1 2 3; do\n";
                s += "    if hdiutil create -volname \"$VOLNAME\" -srcfolder \"$WORK/dmgroot\" -ov -format UDZO \"$DMG\"; then break; fi\n";
                s += "    [ $attempt -eq 3 ] && exit 1\n";
                s += "    sleep 2\n";
                s += "done\n";
            }

            s += "echo \"" + appName + ".app\"\n";

            std::string out;
            if (LinuxHostShell::RunScript(s, scriptPath, &out) != 0)
            {
                LogError("MacBundle: packaging failed.\n%s", out.c_str());
                LinuxHostShell::Report(ctx, "MacBundle: FAILED — see the log for tool output.");
                return 0;
            }

            LinuxHostShell::Report(ctx, "MacBundle: wrote " + projectName + ".app");
            if (createDmg)
            {
                LinuxHostShell::Report(ctx, "MacBundle: wrote " + appName + ".dmg");
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

        void DrawBoolOption(const PolyphaseBuildContext* ctx, const char* label,
                            const char* key, const char* tooltip)
        {
            bool value = LinuxHostShell::GetOption(ctx, key, "0") == "1";
            if (ImGui::Checkbox(label, &value) && ctx->SetProfileSetting != nullptr)
            {
                ctx->SetProfileSetting(key, value ? "1" : "0");
            }
            if (tooltip != nullptr && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", tooltip);
            }
        }

        void Mac_DrawProfileOptions(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr) return;

            DrawTextOption(ctx, "Bundle Identifier", kBundleIdKey, "",
                "Reverse-DNS id written to Info.plist (CFBundleIdentifier).\n"
                "Empty uses com.polyphase.<ProjectName>.");
            DrawTextOption(ctx, "Version", kVersionKey, "1.0.0",
                "CFBundleShortVersionString / CFBundleVersion.");
            DrawTextOption(ctx, "Minimum macOS", kMinOsKey, kDefaultMinOs,
                "LSMinimumSystemVersion. MoltenVK needs 12.0 or newer.");
            DrawTextOption(ctx, "Icon (PNG)", kIconPathKey, "",
                "Square PNG (512x512 or larger), absolute or relative to the project dir.\n"
                "Falls back to the project's PNG icon, then the engine logo.");
            DrawTextOption(ctx, "Signing Identity", kSignIdentityKey, "",
                "codesign identity, e.g. \"Developer ID Application: Name (TEAMID)\".\n"
                "Empty = ad-hoc signature (runs locally; Gatekeeper warns on download).");
            DrawBoolOption(ctx, "Notarize", kNotarizeKey,
                "Submit to Apple with `xcrun notarytool` and staple the ticket.\n"
                "Requires a signing identity and a keychain profile.");
            DrawTextOption(ctx, "Notary Keychain Profile", kNotaryProfileKey, "",
                "Profile name created with `xcrun notarytool store-credentials`.");
            DrawBoolOption(ctx, "Create .dmg", kCreateDmgKey,
                "Also write a compressed disk image with an Applications shortcut.");

            ImGui::Separator();
            ImGui::TextDisabled("Apple Silicon (arm64) only. Vulkan runs through MoltenVK,");
            ImGui::TextDisabled("bundled from the Vulkan SDK into Contents/Frameworks.");
            ImGui::TextDisabled("Saves go to ~/Library/Application Support/<Project>/Saves");
            ImGui::TextDisabled("when the bundle is read-only.");
        }
    }

    std::string ResolveVulkanSdkRoot()
    {
        auto looksLikeSdk = [](const std::string& dir) -> bool
        {
            return !dir.empty() &&
                   SYS_DoesFileExist((dir + "/include/vulkan/vulkan.h").c_str(), false) &&
                   SYS_DoesFileExist((dir + "/lib/libMoltenVK.dylib").c_str(), false);
        };

        if (const char* sdk = std::getenv("VULKAN_SDK"))
        {
            std::string base = sdk;
            while (!base.empty() && (base.back() == '/' || base.back() == '\\')) base.pop_back();
            if (looksLikeSdk(base)) return base;
            if (looksLikeSdk(base + "/macOS")) return base + "/macOS";
        }

        const char* home = std::getenv("HOME");
        if (home == nullptr || home[0] == '\0') return "";

        std::string root = std::string(home) + "/VulkanSDK/";
        std::string best;
        DirEntry entry = {};
        SYS_OpenDirectory(root, entry);
        while (entry.mValid)
        {
            if (entry.mDirectory && entry.mFilename[0] != '.')
            {
                std::string candidate = root + entry.mFilename + "/macOS";
                // Version directories sort lexically well enough (1.4.xxx.y).
                if (looksLikeSdk(candidate) && (best.empty() || std::string(entry.mFilename) > best.substr(root.size(), best.size() - root.size() - 6)))
                {
                    best = candidate;
                }
            }
            SYS_IterateDirectory(entry);
        }
        // Close unconditionally: DirEntry's handle member is platform-specific
        // (mDir on POSIX, mFindHandle on Windows) and this file compiles on
        // every editor host. SYS_CloseDirectory tolerates a failed open.
        SYS_CloseDirectory(entry);
        return best;
    }

    void FillDesc(PolyphaseBuildTargetDesc& outDesc)
    {
        outDesc = PolyphaseBuildTargetDesc{};
        outDesc.apiVersion          = POLYPHASE_BUILD_TARGET_API_VERSION;
        outDesc.targetId            = kTargetId;
        outDesc.displayName         = "macOS (App Bundle)";
        outDesc.iconText            = "";
        outDesc.category            = "Desktop";
        outDesc.basePlatform        = (int32_t)Platform::Mac;
        // The compiled artifact is a loose Mach-O (<Project>.macho); the .app is
        // produced beside it by PostPackage.
        outDesc.binaryExtension     = ".macho";
        outDesc.requiresDocker      = 0;
        outDesc.supportsRunOnDevice = 0;
        outDesc.supportsEmulator    = 0;

        outDesc.Validate            = &Mac_Validate;
        outDesc.PostPackage         = &Mac_PostPackage;
        outDesc.DrawProfileOptions  = &Mac_DrawProfileOptions;
    }
}

#endif /* EDITOR */
