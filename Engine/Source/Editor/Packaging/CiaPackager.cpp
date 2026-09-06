#if EDITOR

#include "CiaPackager.h"
#include "BannerGltfExporter.h"
#include "LinuxHostShell.h"

#include "EngineTypes.h"
#include "Engine.h"
#include "Utilities.h"
#include "EditorUtils.h"
#include "AssetManager.h"
#include "Assets/Texture.h"
#include "Assets/SoundWave.h"
#include "Preferences/JsonSettings.h"
#include "AutoUpdater/HttpClient.h"
#include "System/System.h"
#include "Log.h"

#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace CiaPackager
{
    const char* const kTargetId = "polyphase.n3ds.cia";

    namespace
    {
        // Shared with the canonical 3DS target (PackagingWindow's inline block
        // and the make command-line overrides in ActionManager).
        constexpr const char* kTitleKey        = "n3ds.title";

        constexpr const char* kProductCodeKey  = "cia.productCode";
        constexpr const char* kUniqueIdKey     = "cia.uniqueId";
        constexpr const char* kVersionKey      = "cia.version";
        constexpr const char* kRsfPathKey      = "cia.rsfPath";
        constexpr const char* kBannerModeKey   = "cia.bannerMode";
        constexpr const char* kBannerImageKey  = "cia.bannerImage";
        constexpr const char* kBannerSceneKey  = "cia.bannerScene";
        constexpr const char* kBannerSpinKey   = "cia.bannerSpin";
        constexpr const char* kBannerRotateKey = "cia.bannerRotate";
        constexpr const char* kBannerRotMinKey = "cia.bannerRotMin";
        constexpr const char* kBannerRotMaxKey = "cia.bannerRotMax";
        constexpr const char* kBannerAudioKey  = "cia.bannerAudio";
        constexpr const char* kBannerLoopKey   = "cia.bannerAudioLoop";
        constexpr const char* kBannerAdpcmKey  = "cia.bannerAudioAdpcm";

        constexpr const char* kMakeromUrlWin   = "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-win_x86_64.zip";
        constexpr const char* kMakeromUrlLinux = "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip";
        constexpr const char* kBannertoolUrlWin   = "https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.3/bannertool-1.2.3-windows.zip";
        constexpr const char* kBannertoolUrlLinux = "https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.3/bannertool-1.2.3-linux.tar.gz";
        // Pinned to a commit: the engine's banner_cgfx.py calls into pycgfx's
        // internals (convert_gltf / write / CFLT), so an unannounced upstream
        // change must not reach users through the download button.
        constexpr const char* kPycgfxUrl       = "https://github.com/skyfloogle/pycgfx/archive/1f78850086f3a77c41e07162e842f97a5bf3c18a.zip";
        constexpr const char* kCwavtoolUrl     = "https://github.com/PabloMK7/cwavtool/releases/download/1.0.0/cwavtool.zip";

        // HOME Menu banner limits. The tune MUST be 44.1 kHz: a 32 kHz CWAV
        // (PCM16 or DSP-ADPCM, looped or not) plays as a string of loud beeps
        // on hardware, verified on a real 3DS. Every known-working homebrew
        // tune (PKSM, 3DShell, Universal-Updater) is 16-bit stereo at
        // 44.1/48 kHz and ~2 s, i.e. ~96k sample frames / ~384 KB of PCM.
        constexpr int32_t  kBannerWidth      = 256;
        constexpr int32_t  kBannerHeight     = 128;
        constexpr uint32_t kBannerAudioRate  = 44100;
        constexpr uint32_t kBannerAudioMaxFrames = 96000;   // ~2.18 s at 44.1 kHz

        // ------------------------------------------------------------------
        // Small string / path helpers
        // ------------------------------------------------------------------

        void ReplaceAll(std::string& str, const std::string& from, const std::string& to)
        {
            if (from.empty()) return;
            size_t pos = 0;
            while ((pos = str.find(from, pos)) != std::string::npos)
            {
                str.replace(pos, from.size(), to);
                pos += to.size();
            }
        }

        std::string Trim(std::string s)
        {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
            {
                s.pop_back();
            }
            size_t start = 0;
            while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
            {
                ++start;
            }
            return s.substr(start);
        }

        std::string FirstLine(const std::string& s)
        {
            size_t nl = s.find_first_of("\r\n");
            return Trim(nl == std::string::npos ? s : s.substr(0, nl));
        }

        std::string WithSlash(std::string dir)
        {
            if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            {
                dir += '/';
            }
            return dir;
        }

        std::string ForwardSlashes(std::string path)
        {
            ReplaceAll(path, "\\", "/");
            return path;
        }

        std::string NativePath(std::string path)
        {
#if PLATFORM_WINDOWS
            ReplaceAll(path, "/", "\\");
#endif
            return path;
        }

        std::string DirName(const std::string& path)
        {
            size_t slash = path.find_last_of("/\\");
            return (slash == std::string::npos) ? std::string() : path.substr(0, slash);
        }

        bool IsAbsolutePath(const std::string& p)
        {
            return (p.size() >= 2 && p[1] == ':') || (!p.empty() && (p[0] == '/' || p[0] == '\\'));
        }

        std::string ResolveProjectPath(const std::string& p, const std::string& projectDir)
        {
            if (p.empty() || IsAbsolutePath(p)) return p;
            return projectDir + p;
        }

        bool FileExists(const std::string& path)
        {
            return !path.empty() && SYS_DoesFileExist(path.c_str(), false);
        }

        bool HasExtension(const std::string& path, const char* ext)
        {
            size_t n = std::strlen(ext);
            if (path.size() < n) return false;
            for (size_t i = 0; i < n; ++i)
            {
                if (std::tolower((unsigned char)path[path.size() - n + i]) != std::tolower((unsigned char)ext[i])) return false;
            }
            return true;
        }

        // "Assets/Music/BlueLoop.oct", "BlueLoop.oct" or "BlueLoop" -> "BlueLoop".
        // Lets users paste an .oct path where an asset name is expected.
        std::string AssetStem(const std::string& spec)
        {
            size_t slash = spec.find_last_of("/\\");
            std::string name = (slash == std::string::npos) ? spec : spec.substr(slash + 1);
            if (HasExtension(name, ".oct")) name = name.substr(0, name.size() - 4);
            return name;
        }

        // Loads RGBA8 pixels from an image file or an imported Texture asset.
        // Returns nullptr on failure. When `outOwned` is set the caller must
        // stbi_image_free the result.
        const unsigned char* LoadRgbaSource(const std::string& spec, const std::string& projectDir,
                                            int& outW, int& outH, unsigned char*& outOwned)
        {
            outOwned = nullptr;
            outW = outH = 0;
            if (spec.empty()) return nullptr;

            std::string file = ResolveProjectPath(spec, projectDir);
            if (FileExists(file) && !HasExtension(file, ".oct"))
            {
                int comps = 0;
                outOwned = stbi_load(file.c_str(), &outW, &outH, &comps, 4);
                return outOwned;
            }

            // Editor-side pixels are RGBA8 whatever the asset's console format.
            Texture* texture = LoadAsset<Texture>(AssetStem(spec));
            if (texture != nullptr &&
                texture->GetPixels().size() == (size_t)texture->GetWidth() * texture->GetHeight() * 4 &&
                texture->GetWidth() > 0 && texture->GetHeight() > 0)
            {
                outW = (int)texture->GetWidth();
                outH = (int)texture->GetHeight();
                return texture->GetPixels().data();
            }
            return nullptr;
        }

        bool ReadTextFile(const std::string& path, std::string& out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in) return false;
            out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            return true;
        }

        uint64_t FileSize(const std::string& path)
        {
            std::error_code ec;
            uint64_t size = (uint64_t)std::filesystem::file_size(std::filesystem::path(path), ec);
            return ec ? 0 : size;
        }

        std::string ExeName(const char* base)
        {
#if PLATFORM_WINDOWS
            return std::string(base) + ".exe";
#else
            return base;
#endif
        }

        std::string Opt(const PolyphaseBuildContext* ctx, const char* key, const std::string& fallback = "")
        {
            return LinuxHostShell::GetOption(ctx, key, fallback);
        }

        void Report(const PolyphaseBuildContext* ctx, const std::string& line)
        {
            LinuxHostShell::Report(ctx, line);
        }

        // Runs `program args`, capturing combined output. `program` is either a
        // path to an executable (quoted) or a bare command line such as
        // "py -3" (passed through).
        bool RunTool(const std::string& program, const std::string& args, std::string* outOutput)
        {
            std::string prog = FileExists(program) ? ("\"" + NativePath(program) + "\"") : program;
            std::string cmd = prog + " " + args;
#if PLATFORM_WINDOWS
            // SYS_ExecFull prepends `cmd.exe /c`. A line that starts with a
            // quote loses its first and last quote character, so wrap the
            // whole thing in one more pair — same trick as 3dslink launch.
            if (!cmd.empty() && cmd[0] == '"')
            {
                cmd = "\"" + cmd + "\"";
            }
#endif
            int exitCode = -1;
            std::string out;
            SYS_ExecFull(cmd.c_str(), &out, nullptr, &exitCode);
            if (outOutput != nullptr) *outOutput = out;
            return exitCode == 0;
        }

        std::string Quoted(const std::string& path)
        {
            return "\"" + NativePath(path) + "\"";
        }

        // ------------------------------------------------------------------
        // Tool discovery
        // ------------------------------------------------------------------

        std::mutex sToolMutex;
        std::string sToolOverride[(int)Tool::Count];
        bool sToolCacheValid[(int)Tool::Count] = {};
        std::string sToolCache[(int)Tool::Count];

        std::string FindOnPath(const char* name)
        {
            std::string out;
#if PLATFORM_WINDOWS
            SYS_Exec((std::string("where ") + name + " 2>nul").c_str(), &out);
#else
            SYS_Exec((std::string("which ") + name + " 2>/dev/null").c_str(), &out);
#endif
            std::string first = FirstLine(out);
            return FileExists(first) ? first : std::string();
        }

        bool PythonWorks(const std::string& program)
        {
            if (program.empty()) return false;
            std::string out;
            RunTool(program, "--version", &out);
            return out.find("Python 3") != std::string::npos;
        }

        std::string ResolveExecutable(const char* name, const std::string& override)
        {
            const std::string exe = ExeName(name);

            if (!override.empty())
            {
                if (FileExists(override)) return override;
                std::string inDir = WithSlash(override) + exe;
                if (FileExists(inDir)) return inDir;
            }

            std::string inTools = GetToolsDirectory() + "/" + exe;
            if (FileExists(inTools)) return inTools;

            std::string dkp = Trim(GetDevkitproPath());
            if (!dkp.empty())
            {
                std::string inDkp = WithSlash(ForwardSlashes(dkp)) + "tools/bin/" + exe;
                if (FileExists(inDkp)) return inDkp;
            }

            return FindOnPath(name);
        }

        std::string ResolvePython(const std::string& override)
        {
            if (!override.empty())
            {
                return PythonWorks(override) ? override : std::string();
            }

#if PLATFORM_WINDOWS
            const char* candidates[] = { "py -3", "python3", "python" };
#else
            const char* candidates[] = { "python3", "python" };
#endif
            for (const char* c : candidates)
            {
                if (PythonWorks(c)) return c;
            }
            return "";
        }

        std::string ResolvePycgfx(const std::string& override)
        {
            auto dirWithMain = [](const std::string& dir) -> std::string
            {
                std::string d = WithSlash(dir);
                return FileExists(d + "main.py") ? d : std::string();
            };

            if (!override.empty())
            {
                if (FileExists(override) && override.size() > 7 &&
                    override.compare(override.size() - 7, 7, "main.py") == 0)
                {
                    return WithSlash(DirName(override));
                }
                std::string d = dirWithMain(override);
                if (!d.empty()) return d;
            }

            std::string tools = GetToolsDirectory();
            std::string d = dirWithMain(tools + "/pycgfx");
            if (!d.empty()) return d;
            d = dirWithMain(tools + "/pycgfx/pycgfx-main");
            return d;
        }

        std::string ResolveToolUncached(Tool tool)
        {
            const std::string& override = sToolOverride[(int)tool];
            switch (tool)
            {
            case Tool::Makerom:    return ResolveExecutable("makerom", override);
            case Tool::Bannertool: return ResolveExecutable("bannertool", override);
            case Tool::Cwavtool:   return ResolveExecutable("cwavtool", override);
            case Tool::Python:     return ResolvePython(override);
            case Tool::Pycgfx:     return ResolvePycgfx(override);
            default:               return "";
            }
        }

        // ------------------------------------------------------------------
        // Tool download / install (background thread)
        // ------------------------------------------------------------------

        std::atomic<bool> sInstallRunning{false};
        std::atomic<bool> sInstallCancel{false};
        std::mutex sInstallMutex;
        std::string sInstallStatus;

        void SetInstallStatus(const std::string& status)
        {
            std::lock_guard<std::mutex> lock(sInstallMutex);
            sInstallStatus = status;
            LogDebug("3DS tools: %s", status.c_str());
        }

        bool Download(const std::string& url, const std::string& dest)
        {
            SYS_RemoveFile(dest.c_str());

            if (HttpClient::IsAvailable())
            {
                auto progress = [](size_t, size_t) {};
                HttpClient::DownloadFile(url, dest, progress, sInstallCancel);
            }

            if (!FileExists(dest))
            {
                std::string out;
                SYS_Exec(("curl -L -s -o \"" + NativePath(dest) + "\" \"" + url + "\"").c_str(), &out);
            }

            return FileExists(dest) && FileSize(dest) > 0;
        }

        bool ExtractArchive(const std::string& archive, const std::string& destDir)
        {
            RemoveDir(destDir.c_str());
            CreateDirectoryRecursive(destDir);

            std::string out;
#if PLATFORM_WINDOWS
            // bsdtar ships with Windows 10+ and reads both zip and tar.gz.
            SYS_Exec(("C:\\Windows\\System32\\tar.exe -xf \"" + NativePath(archive) + "\" -C \"" + NativePath(destDir) + "\"").c_str(), &out);
#else
            const bool targz = archive.size() > 7 && archive.compare(archive.size() - 7, 7, ".tar.gz") == 0;
            if (targz)
            {
                SYS_Exec(("tar -xzf \"" + archive + "\" -C \"" + destDir + "\"").c_str(), &out);
            }
            else
            {
                SYS_Exec(("unzip -o -q \"" + archive + "\" -d \"" + destDir + "\"").c_str(), &out);
            }
#endif
            std::error_code ec;
            return !std::filesystem::is_empty(std::filesystem::path(destDir), ec) && !ec;
        }

        // Archives that ship several builds (cwavtool: windows-i686/,
        // windows-x86_64/, linux-x86_64/, ...) get the 64-bit one.
        std::string FindFileRecursive(const std::string& root, const std::string& fileName)
        {
            std::vector<std::string> matches;
            std::error_code ec;
            std::filesystem::recursive_directory_iterator it(std::filesystem::path(root), ec);
            std::filesystem::recursive_directory_iterator end;
            for (; !ec && it != end; it.increment(ec))
            {
                if (it->is_regular_file(ec) && it->path().filename().u8string() == fileName)
                {
                    matches.push_back(ForwardSlashes(it->path().u8string()));
                }
            }
            if (matches.empty()) return "";

            for (const std::string& m : matches)
            {
                if (m.find("x86_64") != std::string::npos || m.find("x64") != std::string::npos) return m;
            }
            return matches[0];
        }

        bool InstallBinary(const std::string& url, const std::string& archiveName, const char* binaryBase)
        {
            const std::string toolsDir = GetToolsDirectory();
            const std::string dlDir = toolsDir + "/dl";
            CreateDirectoryRecursive(dlDir);

            const std::string archive = dlDir + "/" + archiveName;
            SetInstallStatus("Downloading " + archiveName + "...");
            if (!Download(url, archive))
            {
                SetInstallStatus("Download failed: " + url);
                return false;
            }

            SetInstallStatus("Extracting " + archiveName + "...");
            const std::string extractDir = dlDir + "/" + archiveName + "_extract";
            if (!ExtractArchive(archive, extractDir))
            {
                SetInstallStatus("Extraction failed: " + archive);
                return false;
            }

            const std::string binaryName = ExeName(binaryBase);
            std::string found = FindFileRecursive(extractDir, binaryName);
            if (found.empty())
            {
                SetInstallStatus(std::string("Archive did not contain ") + binaryName);
                return false;
            }

            const std::string dest = toolsDir + "/" + binaryName;
            std::error_code ec;
            std::filesystem::copy_file(std::filesystem::path(found), std::filesystem::path(dest),
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec || !FileExists(dest))
            {
                SetInstallStatus("Failed to copy " + binaryName + " into " + toolsDir);
                return false;
            }
#if !PLATFORM_WINDOWS
            SYS_Exec(("chmod +x \"" + dest + "\"").c_str());
#endif
            RemoveDir(extractDir.c_str());
            SYS_RemoveFile(archive.c_str());
            SetInstallStatus(std::string("Installed ") + binaryName);
            return true;
        }

        bool InstallPycgfx()
        {
            // pycgfx is useless without an interpreter, so say so up front
            // instead of downloading and failing at the pip step.
            if (ResolveTool(Tool::Python).empty())
            {
                SetInstallStatus("Python 3 not found. Install Python 3.12 from python.org (tick \"Add python.exe to PATH\"), "
                                 "restart the editor, then click Install pycgfx again.");
                return false;
            }

            const std::string toolsDir = GetToolsDirectory();
            const std::string dlDir = toolsDir + "/dl";
            CreateDirectoryRecursive(dlDir);

            const std::string archive = dlDir + "/pycgfx-main.zip";
            SetInstallStatus("Downloading pycgfx...");
            if (!Download(kPycgfxUrl, archive))
            {
                SetInstallStatus(std::string("Download failed: ") + kPycgfxUrl);
                return false;
            }

            SetInstallStatus("Extracting pycgfx...");
            const std::string extractDir = dlDir + "/pycgfx_extract";
            if (!ExtractArchive(archive, extractDir))
            {
                SetInstallStatus("Extraction failed: " + archive);
                return false;
            }

            std::string mainPy = FindFileRecursive(extractDir, "main.py");
            if (mainPy.empty())
            {
                SetInstallStatus("pycgfx archive did not contain main.py");
                return false;
            }

            const std::string srcDir = DirName(mainPy);
            const std::string dest = toolsDir + "/pycgfx";
            RemoveDir(dest.c_str());
            std::error_code ec;
            std::filesystem::rename(std::filesystem::path(srcDir), std::filesystem::path(dest), ec);
            if (ec)
            {
                // Cross-device or locked: fall back to a copy.
                ec.clear();
                std::filesystem::copy(std::filesystem::path(srcDir), std::filesystem::path(dest),
                                      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
            }
            if (ec || !FileExists(dest + "/main.py"))
            {
                SetInstallStatus("Failed to move pycgfx into " + dest);
                return false;
            }
            RemoveDir(extractDir.c_str());
            SYS_RemoveFile(archive.c_str());

            InvalidateToolCache();
            std::string python = ResolveTool(Tool::Python);
            if (python.empty())
            {
                SetInstallStatus("pycgfx installed, but no Python 3 was found. Install Python 3.12 and run: pip install gltflib pillow");
                return false;
            }

            SetInstallStatus("Installing pycgfx dependencies (pip install gltflib pillow)...");
            std::string out;
            if (!RunTool(python, "-m pip install --user gltflib pillow", &out))
            {
                LogWarning("pip install output:\n%s", out.c_str());
                SetInstallStatus("pycgfx installed, but pip failed. Run manually: " + python + " -m pip install gltflib pillow");
                return false;
            }

            SetInstallStatus("Installed pycgfx");
            return true;
        }

        void InstallWorker(uint32_t mask)
        {
            bool ok = true;

            if (mask & ToolInstall_Makerom)
            {
#if PLATFORM_WINDOWS
                ok = InstallBinary(kMakeromUrlWin, "makerom-v0.19.0.zip", "makerom") && ok;
#else
                ok = InstallBinary(kMakeromUrlLinux, "makerom-v0.19.0.zip", "makerom") && ok;
#endif
            }

            if (mask & ToolInstall_Bannertool)
            {
#if PLATFORM_WINDOWS
                ok = InstallBinary(kBannertoolUrlWin, "bannertool-1.2.3.zip", "bannertool") && ok;
#else
                ok = InstallBinary(kBannertoolUrlLinux, "bannertool-1.2.3.tar.gz", "bannertool") && ok;
#endif
            }

            if (mask & ToolInstall_Cwavtool)
            {
                ok = InstallBinary(kCwavtoolUrl, "cwavtool-1.0.0.zip", "cwavtool") && ok;
            }

            if (mask & ToolInstall_Pycgfx)
            {
                ok = InstallPycgfx() && ok;
            }

            InvalidateToolCache();

            if (ok)
            {
                SetInstallStatus("Done. Tools installed to " + GetToolsDirectory());
            }

            sInstallRunning.store(false);
        }

        // ------------------------------------------------------------------
        // Identity helpers
        // ------------------------------------------------------------------

        std::string SanitizeAscii(const std::string& in, size_t maxLen)
        {
            std::string out;
            for (char c : in)
            {
                unsigned char uc = (unsigned char)c;
                if (uc >= 0x20 && uc < 0x7f && c != '"' && c != '$' && c != '`' && c != '\\')
                {
                    out += c;
                }
                if (out.size() >= maxLen) break;
            }
            return out;
        }

        std::string DefaultProductCode(const std::string& projectName)
        {
            std::string code;
            for (char c : projectName)
            {
                if (std::isalnum((unsigned char)c))
                {
                    code += (char)std::toupper((unsigned char)c);
                }
                if (code.size() == 4) break;
            }
            while (code.size() < 4) code += 'X';
            return "CTR-P-" + code;
        }

        uint32_t Fnv1a(const std::string& s)
        {
            uint32_t hash = 2166136261u;
            for (unsigned char c : s)
            {
                hash ^= c;
                hash *= 16777619u;
            }
            return hash;
        }

        std::string HexUniqueId(uint32_t id)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%05X", id & 0xFFFFF);
            return buf;
        }

        std::string DefaultUniqueId(const std::string& projectName)
        {
            // 0xFF000-0xFFFFF is the homebrew block; hashing the project name
            // into the low 12 bits keeps the title id stable across rebuilds.
            return HexUniqueId(0xFF000u | (Fnv1a(projectName) & 0xFFFu));
        }

        bool ParseUniqueId(const std::string& text, uint32_t& outId)
        {
            std::string t = Trim(text);
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) t = t.substr(2);
            if (t.empty() || t.size() > 5) return false;
            uint32_t v = 0;
            for (char c : t)
            {
                if (!std::isxdigit((unsigned char)c)) return false;
                v = (v << 4) | (uint32_t)(std::isdigit((unsigned char)c) ? c - '0' : (std::tolower((unsigned char)c) - 'a' + 10));
            }
            outId = v;
            return true;
        }

        void ParseVersion(const std::string& text, int& major, int& minor, int& micro)
        {
            major = 1; minor = 0; micro = 0;
            int a = 0, b = 0, c = 0;
            int n = std::sscanf(text.c_str(), "%d.%d.%d", &a, &b, &c);
            if (n >= 1) major = a;
            if (n >= 2) minor = b;
            if (n >= 3) micro = c;
            // Title versions pack into 16 bits as 6.6.4.
            major = std::max(0, std::min(major, 63));
            minor = std::max(0, std::min(minor, 63));
            micro = std::max(0, std::min(micro, 15));
        }

        // ------------------------------------------------------------------
        // Banner image
        // ------------------------------------------------------------------

        // Scales `src` to fit inside maxW x maxH (aspect preserved) and alpha
        // composites it centred onto a 256x128 dark card.
        std::vector<uint8_t> ComposeBannerCard(const uint8_t* src, int srcW, int srcH, int maxW, int maxH)
        {
            std::vector<uint8_t> card((size_t)kBannerWidth * kBannerHeight * 4);
            for (size_t i = 0; i < card.size(); i += 4)
            {
                card[i + 0] = 30; card[i + 1] = 30; card[i + 2] = 36; card[i + 3] = 255;
            }

            if (src == nullptr || srcW <= 0 || srcH <= 0) return card;

            float scale = std::min((float)maxW / (float)srcW, (float)maxH / (float)srcH);
            int dstW = std::max(1, (int)(srcW * scale + 0.5f));
            int dstH = std::max(1, (int)(srcH * scale + 0.5f));

            std::vector<uint8_t> scaled((size_t)dstW * dstH * 4);
            stbir_resize_uint8_srgb(src, srcW, srcH, 0, scaled.data(), dstW, dstH, 0, STBIR_RGBA);

            int ox = (kBannerWidth - dstW) / 2;
            int oy = (kBannerHeight - dstH) / 2;
            for (int y = 0; y < dstH; ++y)
            {
                for (int x = 0; x < dstW; ++x)
                {
                    const uint8_t* s = &scaled[((size_t)y * dstW + x) * 4];
                    uint8_t* d = &card[((size_t)(y + oy) * kBannerWidth + (x + ox)) * 4];
                    float a = s[3] / 255.0f;
                    for (int c = 0; c < 3; ++c)
                    {
                        d[c] = (uint8_t)(s[c] * a + d[c] * (1.0f - a) + 0.5f);
                    }
                    d[3] = 255;
                }
            }
            return card;
        }

        bool WriteBannerPng(const std::string& path, const std::vector<uint8_t>& rgba)
        {
            // The 3DS texture cook toggles stb's global flip flag; make sure it
            // is off for our own writes.
            stbi_flip_vertically_on_write(0);
            return stbi_write_png(path.c_str(), kBannerWidth, kBannerHeight, 4, rgba.data(), kBannerWidth * 4) != 0;
        }

        std::string BuildBannerImage(const PolyphaseBuildContext* ctx, const std::string& workDir,
                                     const std::string& projectDir, const std::string& engineDir)
        {
            const std::string outPng = workDir + "banner.png";
            const std::string spec = Opt(ctx, kBannerImageKey);

            if (!spec.empty())
            {
                int w = 0, h = 0;
                unsigned char* owned = nullptr;
                const unsigned char* pixels = LoadRgbaSource(spec, projectDir, w, h, owned);
                if (pixels != nullptr)
                {
                    std::vector<uint8_t> card = ComposeBannerCard(pixels, w, h, kBannerWidth, kBannerHeight);
                    if (owned != nullptr) stbi_image_free(owned);
                    if (WriteBannerPng(outPng, card))
                    {
                        Report(ctx, "CIA banner: image from '" + spec + "'.");
                        return outPng;
                    }
                }
                Report(ctx, "CIA banner: '" + spec + "' is neither a readable image file nor an RGBA8 Texture asset; using the default card.");
            }

            // Default card: project icon (PNG only) or the engine logo.
            std::string iconPath;
            const std::string& projIcon = GetEngineConfig()->mIconPath;
            if (projIcon.size() > 4 && projIcon.compare(projIcon.size() - 4, 4, ".png") == 0)
            {
                iconPath = ResolveProjectPath(projIcon, projectDir);
            }
            if (!FileExists(iconPath))
            {
                iconPath = engineDir + "PolyphaseLogo_128.png";
            }

            int w = 0, h = 0, comps = 0;
            unsigned char* pixels = FileExists(iconPath) ? stbi_load(iconPath.c_str(), &w, &h, &comps, 4) : nullptr;
            std::vector<uint8_t> card = ComposeBannerCard(pixels, w, h, 112, 112);
            if (pixels != nullptr) stbi_image_free(pixels);

            return WriteBannerPng(outPng, card) ? outPng : std::string();
        }

        // ------------------------------------------------------------------
        // Banner audio
        // ------------------------------------------------------------------

        bool WriteWav(const std::string& path, const std::vector<int16_t>& stereo, uint32_t rate)
        {
            FILE* f = fopen(path.c_str(), "wb");
            if (f == nullptr) return false;

            const uint32_t dataBytes = (uint32_t)(stereo.size() * sizeof(int16_t));
            const uint16_t channels = 2;
            const uint16_t bits = 16;
            const uint32_t byteRate = rate * channels * bits / 8;
            const uint16_t blockAlign = channels * bits / 8;

            auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
            auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };

            fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); fwrite("WAVE", 1, 4, f);
            fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(channels); w32(rate); w32(byteRate); w16(blockAlign); w16(bits);
            fwrite("data", 1, 4, f); w32(dataBytes);
            if (!stereo.empty()) fwrite(stereo.data(), 1, dataBytes, f);
            fclose(f);
            return true;
        }

        void ApplyFadeOut(std::vector<int16_t>& stereo, uint32_t fadeFrames)
        {
            const uint32_t frames = (uint32_t)(stereo.size() / 2);
            if (frames == 0) return;
            fadeFrames = std::min(fadeFrames, frames);
            for (uint32_t i = 0; i < fadeFrames; ++i)
            {
                float g = (float)(fadeFrames - i) / (float)fadeFrames;
                size_t idx = (size_t)(frames - fadeFrames + i) * 2;
                stereo[idx + 0] = (int16_t)(stereo[idx + 0] * g);
                stereo[idx + 1] = (int16_t)(stereo[idx + 1] * g);
            }
        }

        // Converts a resident SoundWave to 16-bit stereo 44.1 kHz, trimmed to
        // the banner sample limit with a short fade to avoid the end click.
        bool SoundWaveToBannerWav(SoundWave* wave, const std::string& outPath)
        {
            const uint8_t* data = wave->GetWaveData();
            const uint32_t size = wave->GetWaveDataSize();
            const uint32_t bits = wave->GetBitsPerSample();
            const uint32_t channels = std::max(1u, std::min(2u, wave->GetNumChannels()));
            const uint32_t rate = std::max(1u, wave->GetSampleRate());

            if (data == nullptr || size == 0 || (bits != 8 && bits != 16)) return false;

            const uint32_t frameBytes = channels * bits / 8;
            const uint32_t srcFrames = size / frameBytes;
            if (srcFrames == 0) return false;

            auto sample = [&](uint32_t frame, uint32_t ch) -> float
            {
                frame = std::min(frame, srcFrames - 1);
                ch = std::min(ch, channels - 1);
                const uint8_t* p = data + (size_t)frame * frameBytes + ch * bits / 8;
                if (bits == 8)
                {
                    return ((int)p[0] - 128) / 128.0f;
                }
                int16_t v;
                std::memcpy(&v, p, 2);
                return v / 32768.0f;
            };

            const uint64_t wantFrames = (uint64_t)srcFrames * kBannerAudioRate / rate;
            const uint32_t dstFrames = (uint32_t)std::min<uint64_t>(wantFrames, (uint64_t)kBannerAudioMaxFrames);
            if (dstFrames == 0) return false;

            std::vector<int16_t> stereo((size_t)dstFrames * 2);
            const double step = (double)rate / (double)kBannerAudioRate;
            for (uint32_t i = 0; i < dstFrames; ++i)
            {
                double pos = i * step;
                uint32_t f0 = (uint32_t)pos;
                float t = (float)(pos - f0);
                for (uint32_t ch = 0; ch < 2; ++ch)
                {
                    float v = sample(f0, ch) * (1.0f - t) + sample(f0 + 1, ch) * t;
                    v = std::max(-1.0f, std::min(1.0f, v));
                    stereo[(size_t)i * 2 + ch] = (int16_t)(v * 32767.0f);
                }
            }

            ApplyFadeOut(stereo, kBannerAudioRate / 20);
            return WriteWav(outPath, stereo, kBannerAudioRate);
        }

        std::string BuildBannerAudio(const PolyphaseBuildContext* ctx, const std::string& bannertool,
                                     const std::string& workDir, const std::string& projectDir)
        {
            const std::string spec = Opt(ctx, kBannerAudioKey);
            const bool loop = Opt(ctx, kBannerLoopKey, "false") == "true";
            const std::string wavPath = workDir + "banner.wav";
            const std::string bcwavPath = workDir + "banner.bcwav";

            std::string input;
            if (!spec.empty())
            {
                std::string file = ResolveProjectPath(spec, projectDir);
                if (FileExists(file) && !HasExtension(file, ".oct"))
                {
                    // Decode through the engine so a WAV/OGG at any rate or
                    // length gets the same 44.1 kHz / ~2 s normalisation as an
                    // asset. Only if that fails is the file handed over raw.
                    std::string bytes;
                    SoundWave decoded;
                    const bool converted =
                        ReadTextFile(file, bytes) &&
                        SoundWave::LoadFromMemory((const uint8_t*)bytes.data(), bytes.size(),
                                                  HasExtension(file, ".ogg") ? "ogg" : "wav", decoded) &&
                        SoundWaveToBannerWav(&decoded, wavPath);
                    decoded.Destroy();   // the destructor does not free the PCM buffer
                    if (converted)
                    {
                        input = wavPath;
                        char info[200];
                        std::snprintf(info, sizeof(info), "CIA banner: audio from file '%s' (%u Hz, %u ch, %.1f s -> 44.1 kHz stereo, max %.1f s).",
                                      spec.c_str(), decoded.GetSampleRate(), decoded.GetNumChannels(), decoded.GetDuration(),
                                      (float)kBannerAudioMaxFrames / (float)kBannerAudioRate);
                        Report(ctx, info);
                    }
                    else
                    {
                        input = file;
                        Report(ctx, "CIA banner: audio from file '" + spec + "' (passed to bannertool as-is; make sure it is 16-bit 44.1 kHz stereo and about 2 s).");
                    }
                }
                else
                {
                    // Asset name, or an .oct path pasted from the browser. In the
                    // editor every SoundWave keeps its PCM resident (streaming
                    // only skips the load at runtime), so no special case here.
                    const std::string assetName = AssetStem(spec);
                    SoundWave* wave = LoadAsset<SoundWave>(assetName);
                    if (wave == nullptr)
                    {
                        Report(ctx, "CIA banner: '" + spec + "' is neither an audio file nor a SoundWave asset; using silence.");
                    }
                    else if (wave->GetWaveData() == nullptr || wave->GetWaveDataSize() == 0)
                    {
                        Report(ctx, "CIA banner: SoundWave '" + assetName + "' has no loaded PCM; using silence.");
                    }
                    else if (SoundWaveToBannerWav(wave, wavPath))
                    {
                        input = wavPath;
                        char info[200];
                        std::snprintf(info, sizeof(info), "CIA banner: audio from SoundWave '%s' (%u Hz, %u ch, %.1f s -> 44.1 kHz stereo, max %.1f s).",
                                      assetName.c_str(), wave->GetSampleRate(), wave->GetNumChannels(), wave->GetDuration(),
                                      (float)kBannerAudioMaxFrames / (float)kBannerAudioRate);
                        Report(ctx, info);
                    }
                    else
                    {
                        Report(ctx, "CIA banner: could not convert SoundWave '" + assetName + "' (unsupported bit depth?); using silence.");
                    }
                }
            }

            if (input.empty())
            {
                std::vector<int16_t> silence((size_t)kBannerAudioRate * 2 * 2, 0);   // 2 s
                if (!WriteWav(wavPath, silence, kBannerAudioRate)) return "";
                input = wavPath;
            }

            // PCM16 via bannertool is the proven path (same as PKSM / 3DShell /
            // Universal-Updater). DSP-ADPCM via cwavtool is opt-in: ~1/4 the
            // size, but untested on hardware at 44.1 kHz.
            std::string out;
            if (Opt(ctx, kBannerAdpcmKey, "false") == "true")
            {
                const std::string cwavtool = ResolveTool(Tool::Cwavtool);
                if (cwavtool.empty())
                {
                    Report(ctx, "CIA banner: DSP-ADPCM requested but cwavtool was not found; using PCM16.");
                }
                else
                {
                    std::string args = "-i " + Quoted(input) + " -o " + Quoted(bcwavPath) + " -e dspadpcm";
                    if (loop) args += " -ls 0 -le end";
                    if (RunTool(cwavtool, args, &out) && FileExists(bcwavPath))
                    {
                        return bcwavPath;
                    }
                    LogWarning("CIA banner: cwavtool failed, falling back to bannertool makecwav.\n%s", out.c_str());
                }
            }

            std::string args = "makecwav -i " + Quoted(input) + " -o " + Quoted(bcwavPath);
            if (loop) args += " -l true";

            if (!RunTool(bannertool, args, &out) || !FileExists(bcwavPath))
            {
                LogWarning("CIA banner: bannertool makecwav failed.\n%s", out.c_str());
                return "";
            }
            return bcwavPath;
        }

        // ------------------------------------------------------------------
        // Banner (.bnr)
        // ------------------------------------------------------------------

        std::string BuildBannerCgfx(const PolyphaseBuildContext* ctx, const std::string& workDir)
        {
            const std::string sceneName = Opt(ctx, kBannerSceneKey);
            if (sceneName.empty())
            {
                Report(ctx, "CIA banner: bannerMode is 'scene' but no scene is set; using the image banner.");
                return "";
            }

            const std::string python = ResolveTool(Tool::Python);
            const std::string pycgfx = ResolveTool(Tool::Pycgfx);
            if (python.empty() || pycgfx.empty())
            {
                Report(ctx, "CIA banner: 3D banners need Python 3 + pycgfx (Preferences > External > Launchers). Using the image banner.");
                return "";
            }

            BannerGltfExporter::Options opts;
            opts.mSceneName = sceneName;
            opts.mOutputDir = workDir;
            opts.mSpinDegreesPerSec = (float)std::atof(Opt(ctx, kBannerSpinKey, "30").c_str());
            opts.mRotate = Opt(ctx, kBannerRotateKey, "true") != "false";
            opts.mRotMinDeg = (float)std::atof(Opt(ctx, kBannerRotMinKey, "0").c_str());
            opts.mRotMaxDeg = (float)std::atof(Opt(ctx, kBannerRotMaxKey, "360").c_str());

            std::string error;
            std::string gltf = BannerGltfExporter::Export(opts, error);
            if (gltf.empty())
            {
                Report(ctx, "CIA banner: scene export failed (" + error + "); using the image banner.");
                return "";
            }

            const std::string cgfx = workDir + "banner.cgfx";
            SYS_RemoveFile(cgfx.c_str());
            std::string out;
            // The engine's driver script runs pycgfx's conversion and then applies
            // the scene's directional light and ambient colour (pycgfx alone
            // gives a white headlight from the camera and black ambient).
            const std::string engineDir = WithSlash(ctx->engineDir != nullptr ? ctx->engineDir : "");
            const std::string driver = engineDir + "Standalone/3DS/banner_cgfx.py";
            // No trailing slash inside the quotes: on Windows `\"` would escape
            // the closing quote and glue the next argument on.
            std::string pycgfxDir = pycgfx;
            while (!pycgfxDir.empty() && (pycgfxDir.back() == '/' || pycgfxDir.back() == '\\')) pycgfxDir.pop_back();

            bool ok = false;
            if (FileExists(driver))
            {
                ok = RunTool(python, Quoted(driver) + " " + Quoted(pycgfxDir) + " " + Quoted(gltf) + " " + Quoted(cgfx), &out);
            }
            else
            {
                LogWarning("CIA banner: %s missing; converting with pycgfx directly (default lighting).", driver.c_str());
                ok = RunTool(python, Quoted(pycgfx + "main.py") + " " + Quoted(gltf) + " " + Quoted(cgfx), &out);
            }
            if (!ok || !FileExists(cgfx))
            {
                LogWarning("pycgfx output:\n%s", out.c_str());
                Report(ctx, "CIA banner: pycgfx failed (see log); using the image banner.");
                return "";
            }
            if (out.find("512KB") != std::string::npos || FileSize(cgfx) > 512 * 1024)
            {
                Report(ctx, "CIA banner: the 3D banner exceeds the 512 KB CGFX limit. Use fewer/smaller textures or meshes. Using the image banner.");
                return "";
            }
            return cgfx;
        }

        std::string BuildBanner(const PolyphaseBuildContext* ctx, const std::string& workDir,
                                const std::string& projectDir, const std::string& engineDir)
        {
            const std::string bannertool = ResolveTool(Tool::Bannertool);
            if (bannertool.empty())
            {
                Report(ctx, "CIA: bannertool not found; the CIA will have no HOME Menu banner. "
                            "Install it from Preferences > External > Launchers.");
                return "";
            }

            const std::string bcwav = BuildBannerAudio(ctx, bannertool, workDir, projectDir);
            if (bcwav.empty())
            {
                Report(ctx, "CIA: banner audio failed; skipping the banner.");
                return "";
            }

            std::string modelArg;
            if (Opt(ctx, kBannerModeKey, "image") == "scene")
            {
                Report(ctx, "CIA: building 3D banner from scene...");
                std::string cgfx = BuildBannerCgfx(ctx, workDir);
                if (!cgfx.empty()) modelArg = "-ci " + Quoted(cgfx);
            }

            if (modelArg.empty())
            {
                std::string png = BuildBannerImage(ctx, workDir, projectDir, engineDir);
                if (png.empty())
                {
                    Report(ctx, "CIA: banner image failed; skipping the banner.");
                    return "";
                }
                modelArg = "-i " + Quoted(png);
            }

            const std::string bnr = workDir + "banner.bnr";
            std::string out;
            if (!RunTool(bannertool, "makebanner " + modelArg + " -ca " + Quoted(bcwav) + " -o " + Quoted(bnr), &out) ||
                !FileExists(bnr))
            {
                LogWarning("bannertool makebanner output:\n%s", out.c_str());
                Report(ctx, "CIA: bannertool makebanner failed (see log); skipping the banner.");
                return "";
            }
            return bnr;
        }

        // ------------------------------------------------------------------
        // Target callbacks
        // ------------------------------------------------------------------

        int32_t Cia_Validate(char* outReason, size_t reasonCap)
        {
            if (!ResolveTool(Tool::Makerom).empty())
            {
                return 1;
            }

            if (outReason != nullptr && reasonCap > 0)
            {
                std::snprintf(outReason, reasonCap, "%s",
                    "makerom not found. Use the download button in Preferences > External > Launchers "
                    "(3DS CIA Tools), or get it from github.com/3DSGuy/Project_CTR/releases and put it on PATH "
                    "or in devkitPro/tools/bin.");
            }
            return 0;
        }

        int32_t Cia_PostPackage(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr || ctx->packageOutputDir == nullptr || ctx->projectName == nullptr)
            {
                LogError("CIA: build context is incomplete.");
                return 0;
            }

            const std::string projectName = ctx->projectName;
            const std::string projectDir  = WithSlash(ctx->projectDir != nullptr ? ctx->projectDir : "");
            const std::string outDir      = WithSlash(ctx->packageOutputDir);
            const std::string engineDir   = WithSlash(ctx->engineDir != nullptr ? ctx->engineDir : "");
            const std::string binDir      = WithSlash(ctx->compiledBinaryDir != nullptr ? ctx->compiledBinaryDir : "");

            const std::string makerom = ResolveTool(Tool::Makerom);
            if (makerom.empty())
            {
                char reason[512] = {};
                Cia_Validate(reason, sizeof(reason));
                LogError("CIA: %s", reason);
                Report(ctx, std::string("CIA: ") + reason);
                return 0;
            }

            if (binDir.empty())
            {
                LogError("CIA: the engine did not report where the compiled .elf lives (compiledBinaryDir).");
                return 0;
            }

            std::string elf = binDir + projectName + ".elf";
            if (!FileExists(elf)) elf = binDir + "Polyphase.elf";
            std::string smdh = binDir + projectName + ".smdh";
            if (!FileExists(smdh)) smdh = binDir + "Polyphase.smdh";

            if (!FileExists(elf) || !FileExists(smdh))
            {
                LogError("CIA: expected %s.elf and .smdh in %s (make output). Try Force Rebuild.", projectName.c_str(), binDir.c_str());
                Report(ctx, "CIA: FAILED — .elf/.smdh not found in " + binDir);
                return 0;
            }

            const std::string workDir = projectDir + "Intermediate/3DS_CIA/";
            RemoveDir(workDir.c_str());
            if (!CreateDirectoryRecursive(workDir))
            {
                LogError("CIA: failed to create %s", workDir.c_str());
                return 0;
            }

            // RomFS: makerom builds the image from a directory, so stage the
            // packaged tree minus the binaries we are wrapping.
            Report(ctx, "CIA: staging romfs...");
            const std::string romfsDir = workDir + "romfs";
            CreateDir(romfsDir.c_str());
            if (!SYS_CopyDirectoryRecursive(outDir, romfsDir))
            {
                LogError("CIA: failed to copy %s into %s", outDir.c_str(), romfsDir.c_str());
                return 0;
            }
            // Strip every binary at the romfs root by extension, not by name:
            // the Packaged dir accumulates renamed / versioned .cia files
            // between builds and each one would otherwise ride along inside
            // the romfs (tens of MB per copy).
            {
                std::error_code ec;
                std::filesystem::directory_iterator it(std::filesystem::path(romfsDir), ec);
                std::filesystem::directory_iterator end;
                for (; !ec && it != end; it.increment(ec))
                {
                    if (!it->is_regular_file(ec)) continue;
                    const std::string name = it->path().filename().u8string();
                    if (HasExtension(name, ".3dsx") || HasExtension(name, ".cia") ||
                        HasExtension(name, ".smdh") || HasExtension(name, ".elf"))
                    {
                        SYS_RemoveFile((romfsDir + "/" + name).c_str());
                    }
                }
            }

            // Identity.
            const std::string title = SanitizeAscii(Opt(ctx, kTitleKey, projectName), 64);
            const std::string shortTitle = SanitizeAscii(title.empty() ? projectName : title, 8);
            const std::string productCode = SanitizeAscii(Opt(ctx, kProductCodeKey, DefaultProductCode(projectName)), 16);

            uint32_t uniqueId = 0;
            if (!ParseUniqueId(Opt(ctx, kUniqueIdKey, DefaultUniqueId(projectName)), uniqueId))
            {
                LogWarning("CIA: invalid unique id '%s'; using the project default.", Opt(ctx, kUniqueIdKey).c_str());
                ParseUniqueId(DefaultUniqueId(projectName), uniqueId);
            }

            int major = 1, minor = 0, micro = 0;
            ParseVersion(Opt(ctx, kVersionKey, "1.0.0"), major, minor, micro);

            // SMDH: make only rebuilds its .smdh when it actually runs, so a
            // cached build or an icon edit would ship a stale one. Regenerate it
            // here from the profile options with devkitPro's smdhtool; fall back
            // to make's copy if the tool is missing.
            {
                const std::string smdhtool = ResolveExecutable("smdhtool", "");
                if (!smdhtool.empty())
                {
                    std::string iconPng = workDir + "icon.png";
                    if (!WriteSmdhIcon(Opt(ctx, "n3ds.iconPath", GetEngineConfig()->mIconPath), projectDir, iconPng))
                    {
                        iconPng = WithSlash(ForwardSlashes(Trim(GetDevkitproPath()))) + "libctru/default_icon.png";
                    }

                    if (FileExists(iconPng))
                    {
                        const std::string description = SanitizeAscii(Opt(ctx, "n3ds.description", "Built with Polyphase Engine"), 128);
                        const std::string author = SanitizeAscii(Opt(ctx, "n3ds.author", "Unspecified Author"), 64);
                        const std::string outSmdh = workDir + "app.smdh";
                        std::string out;
                        if (RunTool(smdhtool, "--create \"" + title + "\" \"" + description + "\" \"" + author + "\" " +
                                    Quoted(iconPng) + " " + Quoted(outSmdh), &out) && FileExists(outSmdh))
                        {
                            smdh = outSmdh;
                        }
                        else
                        {
                            LogWarning("CIA: smdhtool failed, using the .smdh from make.\n%s", out.c_str());
                        }
                    }
                }
                else
                {
                    LogWarning("CIA: smdhtool not found; using the .smdh from make (may be stale on cached builds).");
                }
            }

            // RSF.
            std::string rsfTemplate = ResolveProjectPath(Opt(ctx, kRsfPathKey), projectDir);
            if (rsfTemplate.empty()) rsfTemplate = engineDir + "Standalone/3DS/template.rsf";

            std::string rsf;
            if (!ReadTextFile(rsfTemplate, rsf))
            {
                LogError("CIA: RSF template not found: %s", rsfTemplate.c_str());
                Report(ctx, "CIA: FAILED — RSF template missing: " + rsfTemplate);
                return 0;
            }
            ReplaceAll(rsf, "$(APP_TITLE)", shortTitle);
            ReplaceAll(rsf, "$(APP_PRODUCT_CODE)", productCode);
            ReplaceAll(rsf, "$(APP_UNIQUE_ID)", HexUniqueId(uniqueId));
            ReplaceAll(rsf, "$(APP_ROMFS)", ForwardSlashes(romfsDir));

            const std::string rsfPath = workDir + "app.rsf";
            if (!LinuxHostShell::WriteTextFileLF(rsfPath, rsf))
            {
                return 0;
            }

            // Banner (optional).
            const std::string banner = BuildBanner(ctx, workDir, projectDir, engineDir);

            // makerom.
            const std::string ciaPath = outDir + projectName + ".cia";
            SYS_RemoveFile(ciaPath.c_str());

            std::string args;
            args += "-f cia -o " + Quoted(ciaPath);
            args += " -rsf " + Quoted(rsfPath);
            args += " -target t -exefslogo";
            args += " -elf " + Quoted(elf);
            args += " -icon " + Quoted(smdh);
            if (!banner.empty()) args += " -banner " + Quoted(banner);
            args += " -major " + std::to_string(major) + " -minor " + std::to_string(minor) + " -micro " + std::to_string(micro);

            Report(ctx, "CIA: running makerom...");
            LogDebug("CIA: \"%s\" %s", makerom.c_str(), args.c_str());

            std::string out;
            bool ok = RunTool(makerom, args, &out);
            if (!ok || !FileExists(ciaPath) || FileSize(ciaPath) == 0)
            {
                LogError("CIA: makerom failed.\n%s", out.c_str());
                Report(ctx, "CIA: FAILED — see the log for makerom output.");
                return 0;
            }
            if (!out.empty())
            {
                LogDebug("makerom: %s", out.c_str());
            }

            char titleId[32];
            std::snprintf(titleId, sizeof(titleId), "0x000400000%05X00", uniqueId & 0xFFFFF);
            Report(ctx, "CIA: wrote " + projectName + ".cia (Title ID " + titleId + ", " + productCode +
                        (banner.empty() ? ", no banner)" : ", with banner)"));
            return 1;
        }

        // ------------------------------------------------------------------
        // Profile options UI
        // ------------------------------------------------------------------

        void DrawTextOption(const PolyphaseBuildContext* ctx, const char* label,
                            const char* key, const char* fallback, const char* tooltip)
        {
            char buf[512] = {0};
            const std::string current = Opt(ctx, key, fallback);
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

        void DrawToolStatus(const char* label, Tool tool)
        {
            std::string path = ResolveTool(tool);
            if (path.empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: not found", label);
            }
            else
            {
                ImGui::TextDisabled("%s: %s", label, path.c_str());
            }
        }

        void Cia_DrawProfileOptions(const PolyphaseBuildContext* ctx)
        {
            if (ctx == nullptr) return;

            const std::string projectName = GetEngineConfig()->mProjectName;
            const std::string defaultCode = DefaultProductCode(projectName);
            const std::string defaultId = DefaultUniqueId(projectName);

            DrawTextOption(ctx, "Product Code", kProductCodeKey, defaultCode.c_str(),
                "CTR-P-XXXX shown in system settings / title managers.\n"
                "Any 4 uppercase letters or digits after CTR-P-.");
            DrawTextOption(ctx, "Unique ID", kUniqueIdKey, defaultId.c_str(),
                "Unique part of the Title ID (hex, 0x00000-0xFFFFF).\n"
                "0xFF000-0xFFFFF is the homebrew block that never collides with\n"
                "retail titles. The default is derived from the project name and\n"
                "stays stable across rebuilds; two games must not share an id.");

            uint32_t id = 0;
            if (ParseUniqueId(Opt(ctx, kUniqueIdKey, defaultId), id))
            {
                ImGui::TextDisabled("Title ID: 0x000400000%05X00", id & 0xFFFFF);
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Unique ID must be up to 5 hex digits.");
            }

            DrawTextOption(ctx, "Version", kVersionKey, "1.0.0",
                "major.minor.micro title version (max 63.63.15). Bump it so a\n"
                "newer .cia installs over an older one.");
            DrawTextOption(ctx, "Custom RSF (optional)", kRsfPathKey, "",
                "Path to your own makerom RSF, absolute or project-relative.\n"
                "Leave empty for the engine template (Standalone/3DS/template.rsf).\n"
                "The $(APP_*) placeholders are substituted in either case.");

            ImGui::Separator();
            ImGui::Text("HOME Menu Banner");

            {
                const bool scene = Opt(ctx, kBannerModeKey, "image") == "scene";
                int mode = scene ? 1 : 0;
                const char* modes[] = { "Image (256x128)", "3D Scene" };
                const bool havePython = !ResolveTool(Tool::Python).empty();
                const bool havePycgfx = !ResolveTool(Tool::Pycgfx).empty();
                const bool sceneToolsMissing = !havePython || !havePycgfx;

                if (ImGui::Combo("Banner Mode", &mode, modes, 2) && ctx->SetProfileSetting != nullptr)
                {
                    ctx->SetProfileSetting(kBannerModeKey, mode == 1 ? "scene" : "image");
                    if (mode == 1 && sceneToolsMissing)
                    {
                        ImGui::OpenPopup("3D Banner Requirements");
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Image: a flat card built by bannertool.\n"
                                      "3D Scene: the static meshes of a Scene asset, converted with pycgfx.\n"
                                      "3D banners only render on real hardware (Azahar has no HOME Menu)\n"
                                      "and fall back to the image banner when pycgfx is missing.");
                }

                // Modal shown when 3D Scene is picked without the tools.
                if (ImGui::BeginPopupModal("3D Banner Requirements", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::TextWrapped("3D banners are converted with pycgfx, a Python tool.");
                    ImGui::Spacing();
                    if (!havePython)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Python 3 was not found.");
                        ImGui::TextWrapped("Install Python 3.12 from python.org and tick \"Add python.exe to PATH\" "
                                           "in its installer, then restart the editor. You can also point the "
                                           "Python field in Preferences > External > Launchers at an existing python.exe.");
                        if (ImGui::Button("Open python.org/downloads"))
                        {
                            // Same recipe as BuildDependencyWindow's install links.
#if PLATFORM_WINDOWS
                            SYS_ExecDetached("start https://www.python.org/downloads/");
#else
                            SYS_ExecDetached(SYS_OPEN_CMD " https://www.python.org/downloads/ &");
#endif
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "pycgfx was not found.");
                        ImGui::TextWrapped("Install pycgfx (downloads github.com/skyfloogle/pycgfx and runs "
                                           "pip install gltflib pillow). The project has no license file, so the "
                                           "editor only fetches it on your request.");
                        const bool installing = IsToolInstallRunning();
                        if (installing) ImGui::BeginDisabled();
                        if (ImGui::Button("Install pycgfx"))
                        {
                            StartToolInstall(ToolInstall_Pycgfx);
                        }
                        if (installing) ImGui::EndDisabled();
                        std::string status = GetToolInstallStatus();
                        if (!status.empty())
                        {
                            ImGui::TextWrapped("%s%s", installing ? "Working: " : "", status.c_str());
                        }
                    }
                    ImGui::Spacing();
                    ImGui::TextDisabled("Until then the build falls back to the image banner.");
                    ImGui::Spacing();
                    if (ImGui::Button("Close"))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (mode == 1)
                {
                    // Persistent warning while the tools are missing.
                    if (sceneToolsMissing)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                            !havePython ? "Python 3 not found: 3D banner will fall back to the image banner."
                                        : "pycgfx not found: 3D banner will fall back to the image banner.");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Details##3DBanner"))
                        {
                            ImGui::OpenPopup("3D Banner Requirements");
                        }
                    }

                    DrawTextOption(ctx, "Banner Scene", kBannerSceneKey, "",
                        "Scene asset whose StaticMesh3D nodes become the banner model.\n"
                        "Keep it small: the converted model must stay under 512 KB.");
                    {
                        bool rotate = Opt(ctx, kBannerRotateKey, "true") != "false";
                        if (ImGui::Checkbox("Rotate", &rotate) && ctx->SetProfileSetting != nullptr)
                        {
                            ctx->SetProfileSetting(kBannerRotateKey, rotate ? "true" : "false");
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Animate the scene about the vertical axis. Off = static model.");
                        }
                    }
                    DrawTextOption(ctx, "Speed (deg/s)", kBannerSpinKey, "30",
                        "Angular speed about the vertical axis. For a full spin a negative\n"
                        "value reverses the direction.");
                    DrawTextOption(ctx, "Rotation Min (deg)", kBannerRotMinKey, "0",
                        "Start of the rotation range.");
                    DrawTextOption(ctx, "Rotation Max (deg)", kBannerRotMaxKey, "360",
                        "End of the rotation range. A span of 360 or more is a continuous\n"
                        "turntable spin; a smaller span sways back and forth between Min\n"
                        "and Max (e.g. -30 / 30), easing at the ends. With Rotate off the\n"
                        "model is posed at Min.");
                }
                else
                {
                    DrawTextOption(ctx, "Banner Image", kBannerImageKey, "",
                        "PNG path (absolute or project-relative) or a Texture asset name.\n"
                        "Scaled to fit 256x128 on a dark card. Empty = project icon / engine logo.");
                }
            }

            DrawTextOption(ctx, "Banner Audio", kBannerAudioKey, "",
                "WAV/OGG path or a SoundWave asset name. Converted to 16-bit stereo\n"
                "44.1 kHz and trimmed to about 2.2 seconds (the HOME Menu limit;\n"
                "a 32 kHz tune plays as beeps). Empty = silence.");
            {
                bool loop = Opt(ctx, kBannerLoopKey, "false") == "true";
                if (ImGui::Checkbox("Loop Banner Audio", &loop) && ctx->SetProfileSetting != nullptr)
                {
                    ctx->SetProfileSetting(kBannerLoopKey, loop ? "true" : "false");
                }
                bool adpcm = Opt(ctx, kBannerAdpcmKey, "false") == "true";
                if (ImGui::Checkbox("DSP-ADPCM Tune (experimental)", &adpcm) && ctx->SetProfileSetting != nullptr)
                {
                    ctx->SetProfileSetting(kBannerAdpcmKey, adpcm ? "true" : "false");
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Encode the tune with cwavtool as DSP-ADPCM (about a quarter of the size).\n"
                                      "PCM16 is the proven default; leave this off unless you have tested it.");
                }
            }

            ImGui::Separator();
            DrawToolStatus("makerom", Tool::Makerom);
            DrawToolStatus("bannertool", Tool::Bannertool);
            if (Opt(ctx, kBannerAdpcmKey, "false") == "true")
            {
                DrawToolStatus("cwavtool (DSP-ADPCM tune)", Tool::Cwavtool);
            }
            if (Opt(ctx, kBannerModeKey, "image") == "scene")
            {
                DrawToolStatus("python", Tool::Python);
                DrawToolStatus("pycgfx", Tool::Pycgfx);
            }
            ImGui::TextDisabled("Install / locate tools in Preferences > External > Launchers.");
            ImGui::Spacing();
            ImGui::TextDisabled("The .cia is written beside the .3dsx. Install it on hardware with FBI");
            ImGui::TextDisabled("(copy to SD or Remote Install); in Azahar use File > Install CIA.");
        }
    }

    // ----------------------------------------------------------------------
    // Public API
    // ----------------------------------------------------------------------

    std::string ResolveTool(Tool tool)
    {
        if (tool >= Tool::Count) return "";

        std::lock_guard<std::mutex> lock(sToolMutex);
        const int idx = (int)tool;
        if (!sToolCacheValid[idx])
        {
            sToolCache[idx] = ResolveToolUncached(tool);
            sToolCacheValid[idx] = true;
        }
        return sToolCache[idx];
    }

    void InvalidateToolCache()
    {
        std::lock_guard<std::mutex> lock(sToolMutex);
        for (bool& v : sToolCacheValid) v = false;
    }

    void SetToolOverride(Tool tool, const std::string& path)
    {
        if (tool >= Tool::Count) return;
        std::lock_guard<std::mutex> lock(sToolMutex);
        if (sToolOverride[(int)tool] != path)
        {
            // Only this tool re-probes; probing shells out (`where`, `--version`)
            // and each call lands in the log.
            sToolOverride[(int)tool] = path;
            sToolCacheValid[(int)tool] = false;
        }
    }

    bool WriteSmdhIcon(const std::string& source, const std::string& projectDir, const std::string& outPng)
    {
        int w = 0, h = 0;
        unsigned char* owned = nullptr;
        const unsigned char* pixels = LoadRgbaSource(source, WithSlash(projectDir), w, h, owned);
        if (pixels == nullptr)
        {
            // A Windows .ico (the App Settings default) isn't loadable by stb,
            // and unknown asset names land here too.
            return false;
        }

        std::vector<unsigned char> dst(48 * 48 * 4);
        stbir_resize_uint8_srgb(pixels, w, h, 0, dst.data(), 48, 48, 0, STBIR_RGBA);
        if (owned != nullptr) stbi_image_free(owned);

        CreateDirectoryRecursive(DirName(outPng));
        // The 3DS texture cook flips stb writes globally; reset for ours.
        stbi_flip_vertically_on_write(0);
        return stbi_write_png(outPng.c_str(), 48, 48, 4, dst.data(), 48 * 4) != 0;
    }

    std::string GetToolsDirectory()
    {
        // <APPDATA>/PolyphaseEditor/Preferences -> <APPDATA>/PolyphaseEditor/Tools/3DS
        std::string prefs = ForwardSlashes(JsonSettings::GetPreferencesDirectory());
        while (!prefs.empty() && prefs.back() == '/') prefs.pop_back();
        std::string parent = DirName(prefs);
        if (parent.empty()) parent = prefs;
        return parent + "/Tools/3DS";
    }

    void StartToolInstall(uint32_t mask)
    {
        if (mask == 0) return;
        bool expected = false;
        if (!sInstallRunning.compare_exchange_strong(expected, true)) return;

        sInstallCancel.store(false);
        SetInstallStatus("Starting...");
        std::thread(InstallWorker, mask).detach();
    }

    bool IsToolInstallRunning()
    {
        return sInstallRunning.load();
    }

    std::string GetToolInstallStatus()
    {
        std::lock_guard<std::mutex> lock(sInstallMutex);
        return sInstallStatus;
    }

    void FillDesc(PolyphaseBuildTargetDesc& outDesc)
    {
        outDesc = PolyphaseBuildTargetDesc{};
        outDesc.apiVersion          = POLYPHASE_BUILD_TARGET_API_VERSION;
        outDesc.targetId            = kTargetId;
        outDesc.displayName         = "Nintendo 3DS (CIA)";
        outDesc.iconText            = "";
        outDesc.category            = "Handheld";
        outDesc.basePlatform        = (int32_t)Platform::N3DS;
        // The compiled artifact stays the .3dsx (so emulator / 3dslink launch
        // keeps working); the .cia is produced beside it by PostPackage.
        outDesc.binaryExtension     = ".3dsx";
        outDesc.requiresDocker      = 0;
        outDesc.supportsRunOnDevice = 1;
        outDesc.supportsEmulator    = 1;

        outDesc.Validate            = &Cia_Validate;
        outDesc.PostPackage         = &Cia_PostPackage;
        outDesc.DrawProfileOptions  = &Cia_DrawProfileOptions;
    }
}

#endif /* EDITOR */
