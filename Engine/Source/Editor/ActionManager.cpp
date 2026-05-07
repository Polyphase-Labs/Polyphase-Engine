#if EDITOR

#include "ActionManager.h"
#include "EditorWidgets.h"
#include "BuildCache.h"

#if PLATFORM_WINDOWS
#include <Windows.h>
#include <ShlObj.h>
#endif

#if PLATFORM_LINUX
#include <sys/wait.h>
#include <signal.h>
#endif

#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

#include <vector>
#include <set>
#include <unordered_map>
#include <string>
#include <functional>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "Log.h"
#include "EditorConstants.h"
#include "Constants.h"
#include "World.h"
#include "Engine.h"
#include "AssetManager.h"
#include "EditorState.h"
#include "EditorUtils.h"
#include "Input/PlayerInputSystem.h"
#include "Assets/Scene.h"
#include "Assets/Texture.h"
#include "Assets/Timeline.h"
#include "Assets/StaticMesh.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/SoundWave.h"
#include "Assets/MaterialLite.h"
#include "Assets/MaterialBase.h"
#include "Assets/MaterialInstance.h"
#include "Assets/Font.h"
#include "Assets/TinyLLMAsset.h"
#include "Assets/TinyLLMTokenizerAsset.h"
#include "AssetDir.h"
#include "EmbeddedFile.h"
#include "Utilities.h"
#include "EditorUtils.h"
#include "EditorImgui.h"
#include "Timeline/TimelineActions.h"
#include "Log.h"

#include "Nodes/3D/Voxel3d.h"
#include "Nodes/3D/Terrain3d.h"
#include "Nodes/3D/TileMap2d.h"
#include "Assets/TileMap.h"
#include "TilePaint/TilePaintManager.h"
#include "TerrainSculpt/TerrainSculptManager.h"
#include "Nodes/3D/Mesh3d.h"
#include "VoxelSculpt/VoxelSculptManager.h"
#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/PointLight3d.h"
#include "Nodes/3D/DirectionalLight3d.h"
#include "Nodes/3D/Node3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Nodes/3D/Box3d.h"
#include "Nodes/3D/NavMesh3d.h"
#include "Nodes/3D/Spline3d.h"
#include "Nodes/3D/Sphere3d.h"
#include "Nodes/3D/Capsule3d.h"
#include "Nodes/3D/Particle3d.h"
#include "Nodes/3D/Audio3d.h"
#include "Nodes/3D/ShadowMesh3d.h"
#include "Nodes/3D/InstancedMesh3d.h"
#include "Nodes/3D/TextMesh3d.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/Widgets/Widget.h"
#include "Nodes/Widgets/Canvas.h"
#include "Nodes/Widgets/Quad.h"
#include "Script.h"
#include "Datum.h"
#include "GamePreview/GamePreview.h"

#include "System/System.h"
#include "Packaging/PackagingSettings.h"
#include "Addons/NativeAddonManager.h"
#include "Addons/AddonManager.h"
#include "EditorUIHookManager.h"
#include "Packaging/PackagingWindow.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/External/LaunchersModule.h"

#include "Stream.h"
#include "document.h"
#include "imgui.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define SUB_SCENE_HIER_WARN_TEXT "Cannot modify sub-scene hierarchy. Must unlink scene first."

ActionManager* ActionManager::sInstance = nullptr;

static void GatherMeshImportTypes(const char* path, std::vector<TypeId>& outImportTypes, std::vector<int32_t>& meshIndices)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_FlipUVs);
    
    bool hasCollisionMeshes = false;

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        if (IsAiCollisionMesh(scene->mMeshes[m]))
        {
            hasCollisionMeshes = true;
        }
        else
        {
            TypeId typeId = scene->mMeshes[m]->HasBones() ? SkeletalMesh::GetStaticType() : StaticMesh::GetStaticType();
            outImportTypes.push_back(typeId);
            meshIndices.push_back(int32_t(m));
        }
    }

    if (outImportTypes.size() > 1 && hasCollisionMeshes)
    {
        LogWarning("File has collision meshes but more than one non-collision mesh. Simple collision importing will be ignored.");
    }
}

void ActionManager::Create()
{
    Destroy();
    sInstance = new ActionManager();
}

void ActionManager::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

ActionManager* ActionManager::Get()
{
    return sInstance;
}

ActionManager::ActionManager()
{

}

ActionManager::~ActionManager()
{

}

void ActionManager::Update()
{
    if (mBuildPending)
    {
        mBuildPending = false;
        BuildPhase1();
    }
}

void ReplaceAllStrings(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;

    size_t startPos = 0;
    while ((startPos = str.find(from, startPos)) != std::string::npos)
    {
        str.replace(startPos, from.length(), to);
        startPos += to.length();
    }
}

// Sanitize an addon id into a valid C identifier (for the generated per-addon entry-point
// symbol name). Replaces anything not alphanumeric/underscore with underscore.
static std::string SanitizeAddonIdForSymbol(const std::string& id)
{
    std::string out;
    out.reserve(id.size());
    for (char c : id)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
        {
            out.push_back(c);
        }
        else
        {
            out.push_back('_');
        }
    }
    return out;
}

// Writes a source file that registers every EngineAndEditor-targeted addon with the
// RuntimePluginManager via POLYPHASE_REGISTER_PLUGIN. The file is compiled into the
// shipped game binary; its static initializers queue each plugin's descriptor before
// RuntimePluginManager::Initialize processes them.
static void GenerateAddonPluginsRegistrar(
    const std::string& outCppPath,
    const std::vector<NativeAddonState>& addons)
{
    std::string content;
    content += "// AUTO-GENERATED by ActionManager.cpp::GenerateAddonPluginsRegistrar.\n";
    content += "// Do not edit; regenerated on every shipped build.\n\n";
    // The shipped game compiles each addon's sources directly, so the per-addon\n
    // PolyphasePlugin_GetDesc_<id> symbol exists. The editor binary does NOT compile\n
    // those sources (it dlopens the addon .so/.dll instead), so the symbol is missing.\n
    // Both targets share Standalone/Generated/, so guard the body with #if !EDITOR to\n
    // keep this file a no-op when picked up by the editor build.\n
    content += "#if !EDITOR\n";
    content += "#include \"Plugins/PolyphasePluginAPI.h\"\n";
    content += "#include \"Plugins/RuntimePluginManager.h\"\n\n";
    for (const NativeAddonState& addon : addons)
    {
        const std::string sym = "PolyphasePlugin_GetDesc_" + SanitizeAddonIdForSymbol(addon.mAddonId);
        content += "extern \"C\" int " + sym + "(PolyphasePluginDesc*);\n";
        content += "POLYPHASE_REGISTER_PLUGIN(" + SanitizeAddonIdForSymbol(addon.mAddonId) + ", " + sym + ")\n\n";
    }
    content += "#endif // !EDITOR\n";
    Stream s(content.c_str(), (uint32_t)content.size());
    s.WriteFile(outCppPath.c_str());
}

// Recursively gather every .cpp / .c file under `dir`.
static void GatherAddonSourceFiles(const std::string& dir, std::vector<std::string>& outFiles)
{
    DirEntry entry;
    SYS_OpenDirectory(dir, entry);
    while (entry.mValid)
    {
        if (strcmp(entry.mFilename, ".") != 0 && strcmp(entry.mFilename, "..") != 0)
        {
            std::string path = dir + entry.mFilename;
            if (entry.mDirectory)
            {
                GatherAddonSourceFiles(path + "/", outFiles);
            }
            else
            {
                std::string fn = entry.mFilename;
                size_t dot = fn.find_last_of('.');
                if (dot != std::string::npos)
                {
                    std::string ext = fn.substr(dot);
                    if (ext == ".cpp" || ext == ".c")
                    {
                        outFiles.push_back(path);
                    }
                }
            }
        }
        SYS_IterateDirectory(entry);
    }
    SYS_CloseDirectory(entry);
}

// Inject addon sources / extras into Standalone.vcxproj for the Windows shipped build.
// Uses simple string-based insertion rather than full XML parsing; relies on well-known
// markers in the Standalone.vcxproj template.
static void InjectNativeAddonsIntoVcxproj(
    const std::string& vcxprojPath,
    const std::vector<NativeAddonState>& addons,
    const std::string& generatedRegistrarRelPath /* relative to vcxproj */)
{
    // Idempotent injection: restore the .vcxproj from a .orig backup on every call so we
    // don't accumulate duplicate ClCompile / AdditionalIncludeDirectories / etc. entries
    // on repeated builds. First build creates the backup; subsequent builds restore then
    // re-inject the current addon set. Restore happens BEFORE the empty-addons early-out
    // so switching from a project-with-addons to a project-without-addons doesn't leave
    // stale FFmpeg/avformat references that break the link with missing-DLL errors.
    const std::string backupPath = vcxprojPath + ".orig";
    if (SYS_DoesFileExist(backupPath.c_str(), false))
    {
        Stream restore;
        if (restore.ReadFile(backupPath.c_str(), false))
        {
            Stream writeBack(restore.GetData(), restore.GetSize());
            writeBack.WriteFile(vcxprojPath.c_str());
        }
    }
    else
    {
        SYS_CopyFile(vcxprojPath.c_str(), backupPath.c_str());
    }

    // No addons → vcxproj is now pristine, nothing to inject.
    if (addons.empty()) return;

    Stream inStream;
    if (!inStream.ReadFile(vcxprojPath.c_str(), false))
    {
        LogWarning("InjectNativeAddonsIntoVcxproj: failed to read %s", vcxprojPath.c_str());
        return;
    }
    std::string content(inStream.GetData(), inStream.GetSize());

    // Find the Main.cpp item group entry; inject new <ClCompile> items right before it.
    const std::string kAnchor = "<ClCompile Include=\"Source\\Main.cpp\" />";
    size_t anchorPos = content.find(kAnchor);
    if (anchorPos == std::string::npos)
    {
        LogWarning("InjectNativeAddonsIntoVcxproj: could not find Main.cpp anchor in %s", vcxprojPath.c_str());
        return;
    }

    // Collect all addon cpp files + extras.
    std::vector<std::string> sourceFiles;
    std::vector<std::string> extraIncludeDirs;   // absolute or relative; vcxproj consumes literally
    std::vector<std::string> extraLibDirs;
    std::vector<std::string> extraLibs;
    std::vector<std::string> extraDefines;
    // Per-addon package roots we need as additional include dirs (so `#include "Nodes/Foo.h"`
    // resolves with an addon's own `Source` dir as the root — same as in the editor build).
    std::vector<std::string> addonSourceRoots;

    for (const NativeAddonState& addon : addons)
    {
        std::string sourceDir = addon.mSourcePath + addon.mNativeMetadata.mSourceDir + "/";
        if (!DoesDirExist(sourceDir.c_str())) continue;

        addonSourceRoots.push_back(sourceDir);
        GatherAddonSourceFiles(sourceDir, sourceFiles);

        // This injection is for the Windows shipped (.vcxproj) build, so resolve
        // per-platform extras with platform="Windows". An addon putting FFmpeg
        // libs under nativePerPlatform.Windows lands them in this build; one
        // putting them under .GameCube does not.
        const auto extras = addon.mNativeMetadata.ResolveExtras("Windows");
        for (const std::string& d : extras.mExtraDefines)     extraDefines.push_back(d);
        for (const std::string& inc : extras.mExtraIncludeDirs) extraIncludeDirs.push_back(addon.mSourcePath + inc);
        for (const std::string& ld : extras.mExtraLibDirs)     extraLibDirs.push_back(addon.mSourcePath + ld);
        for (const std::string& lb : extras.mExtraLibs)        extraLibs.push_back(lb);
    }

    // Inject ClCompile entries (generated registrar + every addon .cpp).
    //
    // Addon sources are compiled *into* the game exe only for shipped configs (EDITOR=0).
    // The editor configs (DebugEditor / ReleaseEditor, EDITOR=1) load the addon dynamically
    // as a DLL via NativeAddonManager, so compiling the sources into the editor exe would
    //   (a) double-link the same symbols (DLL + static), and
    //   (b) fail with C2491 on PolyphasePlugin_GetDesc because OCTAVE_PLUGIN_EXPORT isn't
    //       defined for the editor's Standalone.vcxproj.
    // Hence the ExcludedFromBuild guards per editor config.
    {
        const char* kExcludedEditorConfigs[] = {
            "DebugEditor|Win32",
            "DebugEditor|x64",
            "ReleaseEditor|Win32",
            "ReleaseEditor|x64",
        };
        auto makeExcludedItem = [&](const std::string& include) {
            std::string s = "<ClCompile Include=\"" + include + "\">\n";
            for (const char* cfg : kExcludedEditorConfigs)
            {
                s += "      <ExcludedFromBuild Condition=\"'$(Configuration)|$(Platform)'=='";
                s += cfg;
                s += "'\">true</ExcludedFromBuild>\n";
            }
            s += "    </ClCompile>\n    ";
            return s;
        };

        std::string inject;
        inject += makeExcludedItem(generatedRegistrarRelPath);
        for (const std::string& src : sourceFiles)
        {
            std::string path = src;
            // vcxproj prefers backslashes.
            std::replace(path.begin(), path.end(), '/', '\\');
            inject += makeExcludedItem(path);
        }
        content.insert(anchorPos, inject);
        anchorPos += inject.size();
    }

    // Build the prefix strings for include dirs / lib dirs / libs / defines.
    auto buildSemiList = [](const std::vector<std::string>& items, const std::string& separator) {
        std::string s;
        for (const std::string& it : items)
        {
            std::string e = it;
            std::replace(e.begin(), e.end(), '/', '\\');
            s += e + separator;
        }
        return s;
    };

    // addonSourceRoots are added as include dirs too.
    std::vector<std::string> allIncludeDirs = addonSourceRoots;
    for (const std::string& d : extraIncludeDirs) allIncludeDirs.push_back(d);

    const std::string includeDirsPrefix = buildSemiList(allIncludeDirs, ";");
    const std::string libDirsPrefix     = buildSemiList(extraLibDirs, ";");
    const std::string libsPrefix        = [&]() {
        std::string s;
        for (const std::string& lb : extraLibs) s += lb + ";";
        return s;
    }();
    const std::string definesPrefix     = [&]() {
        std::string s;
        for (const std::string& d : extraDefines) s += d + ";";
        return s;
    }();

    // Inject into every <AdditionalIncludeDirectories>, <AdditionalDependencies>,
    // <AdditionalLibraryDirectories>, <PreprocessorDefinitions> occurrence so that Release
    // and ReleaseSteam configs all get them.
    auto prependToElement = [&](const char* openTag, const std::string& prepend) {
        if (prepend.empty()) return;
        const std::string open = std::string("<") + openTag + ">";
        size_t pos = 0;
        while ((pos = content.find(open, pos)) != std::string::npos)
        {
            const size_t insertAt = pos + open.size();
            content.insert(insertAt, prepend);
            pos = insertAt + prepend.size();
        }
    };
    prependToElement("AdditionalIncludeDirectories", includeDirsPrefix);
    prependToElement("AdditionalLibraryDirectories", libDirsPrefix);
    prependToElement("AdditionalDependencies",       libsPrefix);
    prependToElement("PreprocessorDefinitions",      definesPrefix);

    Stream outStream(content.c_str(), (uint32_t)content.size());
    outStream.WriteFile(vcxprojPath.c_str());
    LogDebug("InjectNativeAddonsIntoVcxproj: injected %zu addon(s), %zu source file(s), %zu extra libs into %s",
             addons.size(), sourceFiles.size(), extraLibs.size(), vcxprojPath.c_str());
}

// After the shipped game's exe has been copied to the Packaged directory, copy each addon's
// `copyBinaries` directories (e.g. FFmpeg DLLs) next to the exe so runtime DLL loading works.
// platform selects which nativePerPlatform.<P>.copyBinaries entries also apply on top of the
// common native.copyBinaries — e.g. an addon may bundle Windows-only FFmpeg DLLs under
// nativePerPlatform.Windows.copyBinaries that should NOT be copied on a GameCube package.
static void CopyAddonBinariesToPackaged(const std::string& packagedDir, Platform platform)
{
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr) return;
    const std::vector<NativeAddonState> engineAddons = nam->GetEngineAddons();
    const std::string platformName = GetPlatformString(platform);
    for (const NativeAddonState& addon : engineAddons)
    {
        const auto extras = addon.mNativeMetadata.ResolveExtras(platformName);
        for (const std::string& binDir : extras.mCopyBinaries)
        {
            std::string src = addon.mSourcePath + binDir;
            if (DoesDirExist(src.c_str()))
            {
                LogDebug("CopyAddonBinariesToPackaged[%s]: %s -> %s", platformName.c_str(), src.c_str(), packagedDir.c_str());
                SYS_CopyDirectory(src.c_str(), packagedDir.c_str());
            }
        }
    }
}

static void InjectNativeAddonSources(const std::string& makefilePath, const std::string& buildProjDir, Platform platform)
{
    LogWarning("[INJ] Start inject");
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr)
    {
        LogWarning("[INJ] NAM is null!");
        return;
    }

    // Discover native addons if not already done
    nam->DiscoverNativeAddons();

    // Get addons with engine target (not editor-only)
    std::vector<NativeAddonState> engineAddons = nam->GetEngineAddons();
    LogWarning("[INJ] Engine addons: %zu", engineAddons.size());

    if (engineAddons.empty())
    {
        LogWarning("[INJ] No addons to inject");

        // Even with no addons, we MUST overwrite Generated/AddonPlugins.cpp.
        // The Standalone Makefile pulls in everything under Generated/ via its
        // SOURCES list, so a stale AddonPlugins.cpp left behind by a previous
        // build (one that DID have addons) will be compiled in and trigger
        // `undefined reference to PolyphasePlugin_GetDesc_<addon>` at link
        // time on every subsequent build of any addon-less project.
        std::string generatedDir = buildProjDir + "Generated/";
        SYS_CreateDirectory(generatedDir.c_str());
        GenerateAddonPluginsRegistrar(generatedDir + "AddonPlugins.cpp", engineAddons);
        return;
    }

    LogWarning("[INJ] Injecting %zu addon(s)", engineAddons.size());

    // Read existing makefile
    Stream stream;
    if (!stream.ReadFile(makefilePath.c_str(), false))
    {
        LogError("Failed to read makefile for native addon injection: %s", makefilePath.c_str());
        return;
    }

    std::string makefileContent(stream.GetData(), stream.GetSize());

    // Build source directories list for Makefile-based builds
    std::string additionalSourceDirs;
    std::vector<std::string> sourceDirs;
    // Basenames of every .cpp/.c file the addons contribute. Injected directly
    // into the Makefile's CPPFILES/CFILES variables so we don't depend on
    // GNU make's $(wildcard ...) expanding against absolute Windows-drive paths
    // — under MSYS make (devkitPro for 3DS/Wii/GameCube), $(wildcard M:/...)
    // returns nothing and the addon objects silently drop out of the build.
    std::vector<std::string> addonCppBasenames;
    std::vector<std::string> addonCBasenames;

    // Convert Windows-drive paths (e.g. "M:/foo") to MSYS form ("/m/foo") so
    // GNU make under MSYS can resolve VPATH entries cleanly. No-op for paths
    // that don't start with a drive letter.
    auto toMsysPath = [](const std::string& in) -> std::string
    {
        if (in.size() >= 3 &&
            ((in[0] >= 'A' && in[0] <= 'Z') || (in[0] >= 'a' && in[0] <= 'z')) &&
            in[1] == ':' &&
            (in[2] == '/' || in[2] == '\\'))
        {
            std::string out = "/";
            out += char(in[0] | 0x20); // lowercase drive letter
            out += in.substr(2);
            std::replace(out.begin(), out.end(), '\\', '/');
            return out;
        }
        std::string out = in;
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    };

    for (const NativeAddonState& addon : engineAddons)
    {
        std::string sourceDir = addon.mSourcePath + addon.mNativeMetadata.mSourceDir + "/";
        LogWarning("[INJ] Addon: %s", addon.mAddonId.c_str());
        LogWarning("[INJ] SrcDir: %s", sourceDir.c_str());

        if (!DoesDirExist(sourceDir.c_str()))
        {
            LogWarning("[INJ] Dir NOT found!");
            continue;
        }
        LogWarning("[INJ] Dir exists");

        // Recursively walk the addon source tree, collecting both directories
        // (for VPATH) and explicit .cpp/.c basenames (for direct CPPFILES injection).
        std::function<void(const std::string&)> gatherSourceDirs;
        gatherSourceDirs = [&](const std::string& dir)
        {
            bool hasSourceFiles = false;
            DirEntry dirEntry;
            SYS_OpenDirectory(dir, dirEntry);

            while (dirEntry.mValid)
            {
                if (strcmp(dirEntry.mFilename, ".") != 0 && strcmp(dirEntry.mFilename, "..") != 0)
                {
                    std::string path = dir + dirEntry.mFilename;

                    if (dirEntry.mDirectory)
                    {
                        gatherSourceDirs(path + "/");
                    }
                    else
                    {
                        std::string filename = dirEntry.mFilename;
                        size_t dotPos = filename.find_last_of('.');
                        if (dotPos != std::string::npos)
                        {
                            std::string ext = filename.substr(dotPos);
                            if (ext == ".cpp")
                            {
                                hasSourceFiles = true;
                                addonCppBasenames.push_back(filename);
                                LogDebug("  Found source: %s", path.c_str());
                            }
                            else if (ext == ".c")
                            {
                                hasSourceFiles = true;
                                addonCBasenames.push_back(filename);
                                LogDebug("  Found source: %s", path.c_str());
                            }
                        }
                    }
                }

                SYS_IterateDirectory(dirEntry);
            }
            SYS_CloseDirectory(dirEntry);

            if (hasSourceFiles)
            {
                sourceDirs.push_back(dir);
            }
        };

        gatherSourceDirs(sourceDir);
        LogDebug("Injected native addon: %s", addon.mAddonId.c_str());
    }

    // Build the SOURCES (VPATH) extension. Convert each dir to MSYS form so
    // make under devkitPro (3DS/Wii/GameCube cross-compiles on Windows host)
    // sees a path it can stat. Under non-MSYS make (Linux host) the conversion
    // is a no-op since paths don't have drive letters.
    LogWarning("[INJ] Total dirs: %zu", sourceDirs.size());
    for (const std::string& dir : sourceDirs)
    {
        LogWarning("[INJ] +Dir: %s", dir.c_str());

        std::string normalizedDir = toMsysPath(dir);
        additionalSourceDirs += " \\\n\t\t\t\t" + normalizedDir;
    }

    // Inject into makefile
    // Look for common patterns in makefiles to inject our additions
    // This is a simple approach - we append to INCLUDES and SOURCES variables

    if (!sourceDirs.empty())
    {
        // Build include directories string (just directories, no -I flags - Makefile adds those)
        std::string additionalIncludeDirs;
        for (const std::string& dir : sourceDirs)
        {
            additionalIncludeDirs += " " + toMsysPath(dir);
        }

        // Try to find INCLUDES := (the actual variable, not comments)
        size_t includesPos = makefileContent.find("INCLUDES\t:=");
        if (includesPos == std::string::npos)
        {
            includesPos = makefileContent.find("INCLUDES :=");
        }
        if (includesPos == std::string::npos)
        {
            includesPos = makefileContent.find("INCLUDES:=");
        }

        if (includesPos != std::string::npos)
        {
            // Find the end of this line and append our includes
            size_t lineEnd = makefileContent.find('\n', includesPos);
            if (lineEnd != std::string::npos)
            {
                makefileContent.insert(lineEnd, additionalIncludeDirs);
            }
        }
        else
        {
            // Try CXXFLAGS as fallback (needs -I flags)
            size_t cxxflagsPos = makefileContent.find("CXXFLAGS");
            if (cxxflagsPos != std::string::npos)
            {
                std::string includesWithFlags;
                for (const std::string& dir : sourceDirs)
                {
                    includesWithFlags += " -I" + toMsysPath(dir);
                }
                size_t lineEnd = makefileContent.find('\n', cxxflagsPos);
                if (lineEnd != std::string::npos)
                {
                    makefileContent.insert(lineEnd, includesWithFlags);
                }
            }
        }
    }

    if (!additionalSourceDirs.empty())
    {
        // Try to find SOURCES := (the actual variable, not comments)
        size_t sourcesPos = makefileContent.find("SOURCES\t\t:=");
        if (sourcesPos == std::string::npos)
        {
            sourcesPos = makefileContent.find("SOURCES :=");
        }
        if (sourcesPos == std::string::npos)
        {
            sourcesPos = makefileContent.find("SOURCES:=");
        }

        if (sourcesPos != std::string::npos)
        {
            // Find the end of the SOURCES definition (may span multiple lines with backslash)
            size_t pos = sourcesPos;
            size_t lineEnd = pos;
            while (pos < makefileContent.size())
            {
                lineEnd = makefileContent.find('\n', pos);
                if (lineEnd == std::string::npos)
                {
                    lineEnd = makefileContent.size();
                    break;
                }
                // Check if line continues (ends with backslash)
                if (lineEnd > 0 && makefileContent[lineEnd - 1] == '\\')
                {
                    pos = lineEnd + 1;
                }
                else
                {
                    break;
                }
            }
            // Insert after the SOURCES definition
            makefileContent.insert(lineEnd, additionalSourceDirs);
        }
        else
        {
            // Add a new ADDON_SOURCES line at the beginning
            makefileContent = "ADDON_SOURCES :=" + additionalSourceDirs + "\n" + makefileContent;
        }
    }

    // --- Explicit CPPFILES / CFILES injection ---
    // The devkitPro Makefiles compute CPPFILES via $(wildcard $(dir)/*.cpp) over
    // each SOURCES entry. Under MSYS make on a Windows host, $(wildcard) often
    // fails to expand against absolute drive-letter paths even after they're
    // converted to /m/... form, so addon objects silently drop out of the build.
    //
    // Belt-and-braces fix: enumerate the .cpp/.c basenames here at injection
    // time and append them to CPPFILES/CFILES via a `+=` line placed AFTER the
    // existing wildcard-driven assignment. VPATH (driven by SOURCES) still
    // resolves where each basename lives. Engine source files are unaffected
    // because they live at relative paths that wildcard handles fine.
    if (!addonCppBasenames.empty() || !addonCBasenames.empty())
    {
        auto findCppFilesAssignment = [&](const char* anchor) -> size_t {
            size_t pos = makefileContent.find(anchor);
            if (pos == std::string::npos) return std::string::npos;
            // Walk to end of multi-line assignment (continues on lines ending with `\`).
            size_t lineEnd = pos;
            while (lineEnd < makefileContent.size())
            {
                size_t nl = makefileContent.find('\n', lineEnd);
                if (nl == std::string::npos) return makefileContent.size();
                if (nl > 0 && makefileContent[nl - 1] == '\\')
                    lineEnd = nl + 1;
                else
                    return nl;
            }
            return lineEnd;
        };

        if (!addonCppBasenames.empty())
        {
            size_t cppEnd = findCppFilesAssignment("CPPFILES\t:=");
            if (cppEnd == std::string::npos) cppEnd = findCppFilesAssignment("CPPFILES :=");
            if (cppEnd == std::string::npos) cppEnd = findCppFilesAssignment("CPPFILES:=");

            if (cppEnd != std::string::npos)
            {
                // Append `+=` for the explicit basenames AND a `:= $(sort)` line
                // to dedupe. The wildcard expansion above SOURCES may have
                // already picked up these basenames; without dedupe each addon
                // .cpp gets compiled twice and the linker chokes on duplicate
                // symbols ("multiple definition of X" with same .o on both
                // sides). $(sort) also alphabetizes, but build order doesn't
                // matter and dedupe is the goal.
                std::string cppAdd = "\nCPPFILES +=";
                for (const std::string& f : addonCppBasenames) cppAdd += " " + f;
                cppAdd += "\nCPPFILES := $(sort $(CPPFILES))";
                makefileContent.insert(cppEnd, cppAdd);
            }
        }

        if (!addonCBasenames.empty())
        {
            size_t cEnd = findCppFilesAssignment("CFILES\t\t:=");
            if (cEnd == std::string::npos) cEnd = findCppFilesAssignment("CFILES :=");
            if (cEnd == std::string::npos) cEnd = findCppFilesAssignment("CFILES:=");

            if (cEnd != std::string::npos)
            {
                // Same dedupe pattern as CPPFILES above.
                std::string cAdd = "\nCFILES +=";
                for (const std::string& f : addonCBasenames) cAdd += " " + f;
                cAdd += "\nCFILES := $(sort $(CFILES))";
                makefileContent.insert(cEnd, cAdd);
            }
        }
    }

    // --- Extras from each addon's package.json ---
    // Append extraDefines as -DFOO=1 flags to CFLAGS; extraIncludeDirs as additional paths
    // on INCLUDES (or raw -I if INCLUDES not found); extraLibDirs and extraLibs as -L/-l flags
    // on LIBS. Resolved per target platform so an addon's nativePerPlatform.GameCube
    // (or .Wii / .3DS / .Linux / .Android) overrides land in the right Makefile and
    // PC-only deps under nativePerPlatform.Windows don't bleed into a console build.
    const std::string platformName = GetPlatformString(platform);
    std::vector<std::string> allExtraDefines;
    std::vector<std::string> allExtraIncludeDirs;
    std::vector<std::string> allExtraLibDirs;
    std::vector<std::string> allExtraLibs;
    for (const NativeAddonState& addon : engineAddons)
    {
        const auto extras = addon.mNativeMetadata.ResolveExtras(platformName);
        for (const std::string& d : extras.mExtraDefines)     allExtraDefines.push_back(d);
        for (const std::string& inc : extras.mExtraIncludeDirs) allExtraIncludeDirs.push_back(addon.mSourcePath + inc);
        for (const std::string& ld : extras.mExtraLibDirs)     allExtraLibDirs.push_back(addon.mSourcePath + ld);
        for (const std::string& lb : extras.mExtraLibs)        allExtraLibs.push_back(lb);
    }

    auto appendAfterLine = [&](const char* anchor, const std::string& append) {
        if (append.empty()) return;
        size_t pos = makefileContent.find(anchor);
        if (pos == std::string::npos) return;
        size_t lineEnd = makefileContent.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = makefileContent.size();
        makefileContent.insert(lineEnd, append);
    };

    if (!allExtraDefines.empty())
    {
        std::string defs;
        for (const std::string& d : allExtraDefines) defs += " -D" + d;
        // CFLAGS line is typically: CFLAGS = ... -DPLATFORM_LINUX=1 -DAPI_VULKAN=1 $(INCLUDE)
        size_t before = makefileContent.size();
        appendAfterLine("CFLAGS\t=", defs);
        // Fallback — some Makefiles may use different spacing.
        if (makefileContent.size() == before) appendAfterLine("CFLAGS =", defs);
    }

    if (!allExtraIncludeDirs.empty())
    {
        std::string incs;
        for (const std::string& i : allExtraIncludeDirs)
        {
            incs += " " + toMsysPath(i);
        }
        appendAfterLine("INCLUDES\t:=", incs);
        if (makefileContent.find(incs) == std::string::npos) appendAfterLine("INCLUDES :=", incs);
    }

    if (!allExtraLibDirs.empty() || !allExtraLibs.empty())
    {
        std::string libs;
        for (const std::string& ld : allExtraLibDirs)
        {
            libs += " -L" + toMsysPath(ld);
        }
        for (const std::string& lb : allExtraLibs)
        {
            // Strip .lib / lib prefix to get the bare name for -l.
            std::string n = lb;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".lib") n = n.substr(0, n.size() - 4);
            if (n.size() > 3 && n.substr(0, 3) == "lib") n = n.substr(3);
            libs += " -l" + n;
        }
        appendAfterLine("LIBS\t:=", libs);
        if (makefileContent.find(libs) == std::string::npos) appendAfterLine("LIBS :=", libs);
    }

    // --- Generated plugin registrar ---
    // Write `Generated/AddonPlugins.cpp` so its static POLYPHASE_REGISTER_PLUGIN calls are
    // compiled into the shipped game binary. The Generated/ dir is already in SOURCES.
    {
        std::string generatedDir = buildProjDir + "Generated/";
        SYS_CreateDirectory(generatedDir.c_str());
        GenerateAddonPluginsRegistrar(generatedDir + "AddonPlugins.cpp", engineAddons);
    }

    // Write modified makefile back
    Stream outStream(makefileContent.c_str(), (uint32_t)makefileContent.size());
    outStream.WriteFile(makefilePath.c_str());

    LogDebug("Native addon sources injected into makefile.");
}

void ReplaceStringInFile(const std::string& file, const std::string& srcString, const std::string& dstString)
{
    Stream fileStream;
    fileStream.ReadFile(file.c_str(), false);
    std::string fileString = std::string(fileStream.GetData(), fileStream.GetSize());
    ReplaceAllStrings(fileString, srcString, dstString);

    Stream outStream(fileString.c_str(), (uint32_t)fileString.size());
    outStream.WriteFile(file.c_str());
}

namespace
{
    struct ExeIntegrity
    {
        bool exists = false;
        uint64_t size = 0;
        int64_t mtime = 0;
        bool validPE = false;  // starts with "MZ" — only meaningful for Windows .exe
    };

    ExeIntegrity CheckExeIntegrity(const std::string& path)
    {
        ExeIntegrity info;
#if PLATFORM_WINDOWS
        struct _stat64 st;
        if (_stat64(path.c_str(), &st) != 0) return info;
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) return info;
#endif
        info.exists = true;
        info.size = (uint64_t)st.st_size;
        info.mtime = (int64_t)st.st_mtime;

        FILE* f = fopen(path.c_str(), "rb");
        if (f != nullptr)
        {
            unsigned char sig[2] = { 0, 0 };
            if (fread(sig, 1, 2, f) == 2 && sig[0] == 'M' && sig[1] == 'Z')
                info.validPE = true;
            fclose(f);
        }
        return info;
    }
}

void ActionManager::BuildData(Platform platform, bool embedded)
{
    if (IsBuildRunning())
    {
        LogError("A build is already in progress.");
        return;
    }

    // Check build cache (unless force rebuild is set)
    BuildCache* cache = BuildCache::Get();
    if (cache != nullptr && !mBuildState.mForceRebuild)
    {
        BuildCacheResult cacheResult = cache->CheckRebuildNeeded(platform, embedded);

        if (cacheResult == BuildCacheResult::UpToDate)
        {
            LogDebug("Build cache: up to date, skipping rebuild.");

            const EngineState* engineState = GetEngineState();
            std::string projectDir = engineState->mProjectDirectory;
            std::string projectName = engineState->mProjectName;
            std::string packagedDir = projectDir + "Packaged/" + GetPlatformString(platform) + "/";

            // Get platform extension
            std::string extension;
            switch (platform)
            {
            case Platform::Windows: extension = ".exe"; break;
            case Platform::Linux: extension = ""; break;
            case Platform::Android: extension = ".apk"; break;
            case Platform::GameCube: extension = ".dol"; break;
            case Platform::Wii: extension = ".dol"; break;
            case Platform::N3DS: extension = ".3dsx"; break;
            default: extension = ""; break;
            }

            std::string outputPath = packagedDir + projectName + extension;

            if (!IsHeadless())
            {
                // Save flags before Reset clears them
                bool runAfterBuild = mBuildState.mRunAfterBuild;
                bool runOnDevice = mBuildState.mRunOnDevice;

                // Handle run-after-build by launching emulator directly (no modal)
                if (runAfterBuild)
                {
                    LogDebug("Build cache: up to date, launching %s", outputPath.c_str());

                    if (runOnDevice && platform == Platform::N3DS)
                    {
                        // 3dslink needs special handling
                        LaunchersModule* launchers = static_cast<LaunchersModule*>(
                            PreferencesManager::Get()->FindModule("External/Launchers"));
                        if (launchers != nullptr && launchers->Is3dsLinkConfigured())
                        {
                            std::string cmd = launchers->Build3dsLinkCommand(outputPath);
                            if (!cmd.empty())
                            {
                                LogDebug("Launching 3dslink: %s", cmd.c_str());
                                SYS_Exec(cmd.c_str());
                            }
                        }
                    }
                    else if (runOnDevice && platform == Platform::Wii)
                    {
                        LaunchersModule* launchers = static_cast<LaunchersModule*>(
                            PreferencesManager::Get()->FindModule("External/Launchers"));
                        if (launchers != nullptr && launchers->IsWiiloadConfigured())
                        {
                            std::string cmd = launchers->BuildWiiloadCommand(outputPath);
                            if (!cmd.empty())
                            {
                                LogDebug("Launching wiiload: %s", cmd.c_str());
                                SYS_Exec(cmd.c_str());
                            }
                        }
                    }
                    else
                    {
                        LaunchersModule* launchers = static_cast<LaunchersModule*>(
                            PreferencesManager::Get()->FindModule("External/Launchers"));
                        if (launchers != nullptr && launchers->IsEmulatorConfigured(platform))
                        {
                            std::string cmd = launchers->BuildLaunchCommand(platform, outputPath);
                            LogDebug("Launching emulator: %s", cmd.c_str());
                            SYS_Exec(cmd.c_str());
                        }
                    }
                }
                else
                {
                    // Just "Build" without run - show brief skip message
                    mBuildState.Reset();
                    mBuildState.mPlatform = platform;
                    mBuildState.mEmbedded = embedded;
                    mBuildDisplayOutput.clear();
                    mBuildAutoScroll = true;
                    mShowBuildModal = true;
                    AppendBuildOutput("Build cache: all files unchanged, skipping rebuild.\n");
                    AppendBuildOutput("Output: " + outputPath + "\n");
                    mBuildState.mComplete.store(true);
                    mBuildState.mSuccess.store(false); // Prevent auto-finalize
                }
            }
            return;
        }
        else
        {
            LogDebug("Build cache: %s", cache->GetRebuildReason().c_str());
        }
    }

    // Snapshot the user's force-rebuild request to a local BEFORE any state-mutating
    // step (we clear mForceRebuild here, and the non-headless path also calls Reset()
    // which wipes mForceCompile). After all resets/inits are done, we write the
    // snapshot back to mBuildState.mForceCompile so BuildPhase1's needCompile decision
    // still sees the user's intent when it runs (even a frame later).
    const bool forceRebuildRequested = mBuildState.mForceRebuild;
    mBuildState.mForceRebuild = false;

    // In headless mode, run the entire build synchronously (original behavior)
    if (IsHeadless())
    {
        // Reset state for synchronous use
        mBuildState.mPlatform = platform;
        mBuildState.mEmbedded = embedded;
        mBuildState.mRunAfterBuild = false;
        mBuildState.mRunOnDevice = false;
        mBuildState.mForceCompile = forceRebuildRequested;
        BuildPhase1();
        if (mBuildState.mNeedCompile)
        {
            BuildCompileThreadFunc();
        }
        FinalizeLocalBuild();
        return;
    }

    // Non-headless: open modal, defer Phase 1 to next frame
    mBuildState.Reset();
    mBuildState.mPlatform = platform;
    mBuildState.mEmbedded = embedded;
    mBuildState.mForceCompile = forceRebuildRequested; // write AFTER Reset so it survives
    mBuildDisplayOutput.clear();
    mBuildAutoScroll = true;
    mShowBuildModal = true;
    mBuildPending = true;
    AppendBuildOutput("Preparing build...\n");
}

bool ActionManager::IsBuildRunning() const
{
    return mBuildPending || mBuildState.mRunning.load() || (mShowBuildModal && !mBuildState.mComplete.load());
}

void ActionManager::AppendBuildOutput(const std::string& text)
{
    std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
    mBuildState.mOutput += text;
    mBuildState.mOutputDirty = true;
}

void ActionManager::CancelBuild()
{
    mBuildState.mCancelRequested.store(true);

#if PLATFORM_LINUX
    if (mBuildState.mProcessId > 0)
    {
        kill(mBuildState.mProcessId, SIGTERM);
    }
#elif PLATFORM_WINDOWS
    if (mBuildState.mProcessHandle != nullptr)
    {
        TerminateProcess(static_cast<HANDLE>(mBuildState.mProcessHandle), 1);
    }
#endif
}

void ActionManager::BuildPhase1()
{
    Platform platform = mBuildState.mPlatform;
    bool embedded = mBuildState.mEmbedded;

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();

    // Fire OnPreBuild hook (can cancel the build)
    if (hookMgr != nullptr)
    {
        if (!hookMgr->FireOnPreBuild((int32_t)platform))
        {
            LogWarning("Build cancelled by addon hook.");
            AppendBuildOutput("Build cancelled by addon hook.\n");
            mBuildState.mComplete.store(true);
            mBuildState.mSuccess.store(false);
            return;
        }
    }

    // Fire OnPackageStarted hook
    if (hookMgr != nullptr)
    {
        hookMgr->FireOnPackageStarted((int32_t)platform);
    }

    AppendBuildOutput("Firing pre-build hooks...\n");
    LogDebug("Begin packaging...");
    std::string polyphaseDirectory = SYS_GetPolyphasePath();

    const EngineState* engineState = GetEngineState();
    bool standalone = engineState->mStandalone;
    const std::string& projectDir = engineState->mProjectDirectory;
    const std::string& projectName = engineState->mProjectName;
    bool useRomfs = (platform == Platform::N3DS) && embedded;

    std::vector<std::pair<AssetStub*, std::string> > embeddedAssets;
    std::vector<ActionManager::EmbeddedRawAssetEntry> embeddedRawAssets;

    if (projectDir == "")
    {
        LogError("Project directory not set?");
        AppendBuildOutput("ERROR: Project directory not set.\n");
        mBuildState.mComplete.store(true);
        mBuildState.mSuccess.store(false);
        return;
    }

    // Store config for later phases
    mBuildState.mProjectDir = projectDir;
    mBuildState.mProjectName = projectName;
    mBuildState.mStandalone = standalone;
    mBuildState.mUseSteam = GetEngineConfig()->mPackageForSteam;

    std::string dkpPath = GetDevkitproPath();
    bool dkpInstalled = (dkpPath != "");
    LogDebug("DevkitPro is %s.", dkpInstalled ? "installed" : "not installed");

    // (1) Create a Packaged directory
    AppendBuildOutput("Creating packaged directory...\n");
    std::string packagedDir = projectDir + "Packaged/";

    if (!DoesDirExist(packagedDir.c_str()))
    {
        CreateDir(packagedDir.c_str());
    }

    packagedDir += GetPlatformString(platform);
    packagedDir += "/";
    if (DoesDirExist(packagedDir.c_str()))
    {
        RemoveDir(packagedDir.c_str());
    }

    CreateDir(packagedDir.c_str());
    mBuildState.mPackagedDir = packagedDir;

    // (2) Cook assets
    AppendBuildOutput("Cooking assets...\n");
    std::function<void(AssetDir*, bool)> saveDir = [&](AssetDir* dir, bool engine)
    {
        std::string packDir;
        if (engine)
        {
            packDir = packagedDir + dir->mPath + "/";
        }
        else
        {
            packDir = dir->mPath;
            packDir = packDir.substr(projectDir.length());
            packDir = packagedDir + projectName + "/" + packDir;
        }

        if (!DoesDirExist(packDir.c_str()))
        {
            CreateDir(packDir.c_str());
        }

        const uint32_t targetPlatformBit = GetPlatformBit(platform);

        for (uint32_t i = 0; i < dir->mAssetStubs.size(); ++i)
        {
            AssetStub* stub = dir->mAssetStubs[i];

            // Skip assets explicitly excluded from this platform via {asset}.oct.meta.
            // Default mPlatformMask is PlatformBit_All, so .meta-less assets always ship.
            if ((stub->mPlatformMask & targetPlatformBit) == 0u)
            {
                LogDebug("Skipping %s — excluded from %s by .meta", stub->mPath.c_str(), GetPlatformString(platform));
                continue;
            }

            bool alreadyLoaded = (stub->mAsset != nullptr);

            if (!alreadyLoaded)
            {
                AssetManager::Get()->LoadAsset(*stub);
            }

            // LoadAsset can fail silently (missing source file, factory missing
            // because its addon DLL didn't load, corrupt .oct). Skip the stub
            // rather than dereferencing null — the build continues for the rest
            // of the project instead of AV'ing in operator+.
            if (stub->mAsset == nullptr)
            {
                LogWarning("Cook: skipping stub '%s' — asset failed to load", stub->mPath.c_str());
                AppendBuildOutput("Warning: skipping asset (load failed): " + stub->mPath + "\n");
                continue;
            }

            std::string packFile = packDir + stub->mAsset->GetName() + ".oct";
            stub->mAsset->SaveFile(packFile.c_str(), platform);

            if (true)
            {
                AssetManager::Get()->SaveAsset(*stub);
            }

            if (embedded && !useRomfs)
            {
                embeddedAssets.push_back({ stub, packFile });
            }

            if (!alreadyLoaded)
            {
                AssetManager::Get()->UnloadAsset(*stub);
            }
        }

        for (uint32_t i = 0; i < dir->mChildDirs.size(); ++i)
        {
            saveDir(dir->mChildDirs[i], engine);
        }
    };

    AssetDir* engineAssetDir = AssetManager::Get()->FindEngineDirectory();
    AssetDir* projectAssetDir = AssetManager::Get()->FindProjectDirectory();
    std::string packEngineDir = packagedDir + engineAssetDir->mName + "/";
    std::string packProjectDir = packagedDir + projectAssetDir->mName + "/";
    CreateDir(packEngineDir.c_str());
    CreateDir(packProjectDir.c_str());

    saveDir(engineAssetDir, true);
    saveDir(projectAssetDir, false);

    // (2b) Copy raw (non-.oct) assets that AssetManager tracks via mRawAssetEntries.
    // These are .mp4 / .json / .png / .txt etc. — invisible to the runtime asset
    // system but needed at the on-disk path the game opens them by. saveDir above
    // only handles cooked .oct files, so this is the parallel step for raw files.
    //
    // Filtering rules (per-asset .meta sidecar):
    //   - Skip if (mPlatformMask & targetPlatformBit) == 0  -> excluded from this platform
    //   - Skip if mEmbed                                    -> handled by the embed pipeline below
    {
        AppendBuildOutput("Copying raw assets...\n");
        const uint32_t rawTargetBit = GetPlatformBit(platform);
        const std::vector<RawAssetEntry>& rawEntries = AssetManager::Get()->GetRawAssetEntries();

        for (const RawAssetEntry& entry : rawEntries)
        {
            if ((entry.mPlatformMask & rawTargetBit) == 0u)
            {
                continue;
            }
            if (entry.mEmbed && embedded && !useRomfs)
            {
                // Route into the embed pipeline below. The VFS lookup key is
                // the runtime path the game would pass to SYS_AcquireFileData,
                // which is relative to the shipped exe's working directory:
                //   engine    -> "Engine/Assets/<...>"           (path used as-is)
                //   project   -> "Assets/<...>"                  (after stripping projectDir)
                //   addon     -> "Packages/<addon>/Assets/<...>" (after stripping projectDir)
                ActionManager::EmbeddedRawAssetEntry rawEmbed;
                rawEmbed.mAbsolutePath = entry.mAbsolutePath;
                rawEmbed.mEngineAsset  = entry.mEngineAsset;

                if (entry.mEngineAsset)
                {
                    rawEmbed.mLookupKey = entry.mAbsolutePath;
                }
                else
                {
                    std::string rel = entry.mAbsolutePath;
                    if (rel.compare(0, projectDir.length(), projectDir) == 0)
                    {
                        rel = rel.substr(projectDir.length());
                    }
                    rawEmbed.mLookupKey = rel;
                }

                // Normalize to forward slashes — VFS shim canonicalizes the
                // same way at lookup time.
                std::replace(rawEmbed.mLookupKey.begin(), rawEmbed.mLookupKey.end(), '\\', '/');

                embeddedRawAssets.push_back(std::move(rawEmbed));
                continue;
            }
            if (entry.mEmbed)
            {
                // Embed flag set but not in embedded build mode (or romfs path).
                // Treat as loose — fall through to the copy branch.
            }

            // Destination mirrors saveDir's projectDir-prefix-strip rule:
            //   engine asset    -> packagedDir + "Engine/Assets/<...>"   (path used as-is)
            //   project asset   -> packagedDir + projectName + "/Assets/<...>"
            //   addon asset     -> packagedDir + projectName + "/Packages/<addon>/Assets/<...>"
            // Both project and addon strip projectDir from mAbsolutePath; the
            // remainder already contains "Packages/<addon>/Assets/..." for addons.
            std::string destPath;
            if (entry.mEngineAsset)
            {
                destPath = packagedDir + entry.mAbsolutePath;
            }
            else
            {
                std::string rel = entry.mAbsolutePath;
                if (rel.compare(0, projectDir.length(), projectDir) == 0)
                {
                    rel = rel.substr(projectDir.length());
                }
                destPath = packagedDir + projectName + "/" + rel;
            }

            // Ensure parent directory exists. Walk the path and CreateDir each
            // missing ancestor (CreateDir is idempotent).
            {
                size_t slash = 0;
                while ((slash = destPath.find('/', slash + 1)) != std::string::npos)
                {
                    std::string parent = destPath.substr(0, slash);
                    if (!DoesDirExist(parent.c_str()))
                    {
                        CreateDir(parent.c_str());
                    }
                }
            }

            SYS_CopyFile(entry.mAbsolutePath.c_str(), destPath.c_str());
        }
    }

    // (3) Generate embedded files and asset registry
    AppendBuildOutput("Generating asset registry...\n");
    std::unordered_map<std::string, AssetStub*>& assetMap = AssetManager::Get()->GetAssetMap();
    FILE* registryFile = nullptr;

    std::string registryFileName = packagedDir + projectName + "/AssetRegistry.txt";
    registryFile = fopen(registryFileName.c_str(), "w");

    const uint32_t registryPlatformBit = GetPlatformBit(platform);
    for (auto pair : assetMap)
    {
        // Skip assets excluded from this platform — they were not cooked to
        // packagedDir, so listing them in the registry would cause runtime
        // load failures.
        if ((pair.second->mPlatformMask & registryPlatformBit) == 0u)
        {
            continue;
        }

        if (pair.second->mAsset != nullptr &&
            pair.second->mEngineAsset)
        {
            AssetManager::Get()->SaveAsset(*pair.second);
        }

        if (registryFile != nullptr)
        {
            const char* regType = Asset::GetNameFromTypeId(pair.second->mType);

            std::string regPath = pair.second->mPath.c_str();
            if (!pair.second->mEngineAsset &&
                regPath.find(projectDir) != std::string::npos)
            {
                regPath = regPath.substr(projectDir.length());
                regPath = projectName + "/" + regPath;
            }

            fprintf(registryFile, "%s,%s\n", regType, regPath.c_str());
        }
    }

    if (registryFile != nullptr)
    {
        fclose(registryFile);
        registryFile = nullptr;
    }

    if (!DoesDirExist((projectDir + "Generated").c_str()))
    {
        CreateDir((projectDir + "Generated").c_str());
    }

    std::string embeddedHeaderPath = projectDir + "Generated/EmbeddedAssets.h";
    std::string embeddedSourcePath = projectDir + "Generated/EmbeddedAssets.cpp";
    GenerateEmbeddedAssetFiles(embeddedAssets, embeddedRawAssets, embeddedHeaderPath.c_str(), embeddedSourcePath.c_str());

    // Generate embedded script source files
    AppendBuildOutput("Copying scripts...\n");
    std::vector<std::string> scriptFiles;

    if (embedded && !useRomfs)
    {
        GatherScriptFiles((polyphaseDirectory + "Engine/Scripts/").c_str(), scriptFiles);
        GatherScriptFiles((projectDir + "/Scripts/").c_str(), scriptFiles);

        std::string packagesDir = projectDir + "Packages/";
        if (DoesDirExist(packagesDir.c_str()))
        {
            DirEntry dirEntry;
            SYS_OpenDirectory(packagesDir, dirEntry);

            while (dirEntry.mValid)
            {
                if (dirEntry.mDirectory &&
                    strcmp(dirEntry.mFilename, ".") != 0 &&
                    strcmp(dirEntry.mFilename, "..") != 0)
                {
                    std::string packageScriptsDir = packagesDir + dirEntry.mFilename + "/Scripts/";
                    if (DoesDirExist(packageScriptsDir.c_str()))
                    {
                        GatherScriptFiles(packageScriptsDir.c_str(), scriptFiles);
                    }
                }
                SYS_IterateDirectory(dirEntry);
            }
            SYS_CloseDirectory(dirEntry);
        }
    }
    else
    {
        SYS_CopyDirectory((polyphaseDirectory + "Engine/Scripts/").c_str(), (packagedDir + "Engine/Scripts/").c_str());
        SYS_CopyDirectory((projectDir + "Scripts/").c_str(), (packagedDir + projectName + "/Scripts/").c_str());

        std::string packagesDir = projectDir + "Packages/";
        if (DoesDirExist(packagesDir.c_str()))
        {
            DirEntry dirEntry;
            SYS_OpenDirectory(packagesDir, dirEntry);

            while (dirEntry.mValid)
            {
                if (dirEntry.mDirectory &&
                    strcmp(dirEntry.mFilename, ".") != 0 &&
                    strcmp(dirEntry.mFilename, "..") != 0)
                {
                    std::string packageName = dirEntry.mFilename;
                    std::string packageDestDir = packagedDir + projectName + "/Packages/" + packageName + "/";

                    std::string packageScriptsDir = packagesDir + packageName + "/Scripts/";
                    if (DoesDirExist(packageScriptsDir.c_str()))
                    {
                        // Ensure intermediate dirs exist — Linux SYS_CopyDirectory uses
                        // `cp -r` which fails silently if the destination's parent
                        // (Packages/<addon>/) hasn't been created yet.
                        size_t slash = 0;
                        while ((slash = packageDestDir.find('/', slash + 1)) != std::string::npos)
                        {
                            std::string parent = packageDestDir.substr(0, slash);
                            if (!DoesDirExist(parent.c_str()))
                            {
                                CreateDir(parent.c_str());
                            }
                        }
                        if (!DoesDirExist(packageDestDir.c_str()))
                        {
                            CreateDir(packageDestDir.c_str());
                        }
                        SYS_CopyDirectory(packageScriptsDir.c_str(), (packageDestDir + "Scripts/").c_str());
                    }

                    // Note: Packages/<addon>/Assets/ are NOT bulk-copied here anymore.
                    // Each file is discovered into AssetManager::mRawAssetEntries
                    // (or mAssetStubs for .oct) and copied/embedded by the unified
                    // raw-asset and saveDir steps above, which apply per-platform .meta
                    // filtering. This block intentionally only handles Scripts.
                }
                SYS_IterateDirectory(dirEntry);
            }
            SYS_CloseDirectory(dirEntry);
        }

        if (platform != Platform::Windows && platform != Platform::Linux)
        {
            SYS_RemoveFile((packagedDir + "Engine/Scripts" + "/LuaPanda.lua").c_str());
        }
    }

    std::string scriptHeaderPath = projectDir + "Generated/EmbeddedScripts.h";
    std::string scriptSourcePath = projectDir + "Generated/EmbeddedScripts.cpp";
    GenerateEmbeddedScriptFiles(scriptFiles, scriptHeaderPath.c_str(), scriptSourcePath.c_str());

    if (standalone)
    {
        // Copy the project's Generated/ contents into the engine's Standalone/Generated/
        // — that's what Standalone.vcxproj's <ClCompile Include="Generated\..."> entries
        // resolve to. Earlier this used "Standalone" (root), which flattened the files
        // and left the .vcxproj's `Generated\EmbeddedAssets.cpp` pointing at stale
        // output from a previous packaging run.
        SYS_CopyDirectory((projectDir + "Generated").c_str(),
                          (polyphaseDirectory + "Standalone/Generated").c_str());
    }

    // Copy .octp and Config.ini.
    //
    // Originally both were copied ONLY to the root of packagedDir. That's enough for
    // the initial CWD-relative `Config.ini` read (which discovers `mProjectName`), but
    // NOT for the subsequent `LoadProject("{projectName}/{projectName}.octp", ...)` at
    // `Engine.cpp:431` — which needs the .octp and its sibling Config.ini living in a
    // subdirectory named after the project. Windows shipped builds were silently failing
    // here with "Failed to open file: {projectName}/{projectName}.octp" etc.
    //
    // Fix: keep the original root copies (additive, so nothing that depended on them
    // breaks) AND add the project-subdirectory copies the loader actually needs.
    {
        // Root-level copies — unchanged from original behaviour. Config.ini here is
        // mandatory for the first read; the .octp is kept for any external tooling
        // that might reference it.
        SYS_CopyFile((projectDir + projectName + ".octp").c_str(), (packagedDir + projectName + ".octp").c_str());
        SYS_CopyFile((projectDir + "Config.ini").c_str(), (packagedDir + "Config.ini").c_str());

        // Project-subdirectory copies — what the standalone LoadProject() call at
        // Engine.cpp:431 actually opens. Required for shipped exe to boot its project.
        const std::string projSubdir = packagedDir + projectName + "/";
        if (!DoesDirExist(projSubdir.c_str()))
        {
            CreateDir(projSubdir.c_str());
        }
        SYS_CopyFile((projectDir + projectName + ".octp").c_str(), (projSubdir + projectName + ".octp").c_str());
        SYS_CopyFile((projectDir + "Config.ini").c_str(), (projSubdir + "Config.ini").c_str());
    }

    // Copy custom icon for Windows packaging
    if (platform == Platform::Windows && !GetEngineConfig()->mIconPath.empty())
    {
        std::string iconSrc = projectDir + GetEngineConfig()->mIconPath;
        if (SYS_DoesFileExist(iconSrc.c_str(), false))
        {
            // Validate the source is actually a Windows ICO before overwriting
            // Standalone/Polyphase.ico — a PNG (or any non-ICO file) silently
            // copied over the engine icon makes rc.exe fail with RC2175 and
            // breaks the entire Windows build for everyone using the engine
            // until the .ico is restored. Magic for ICO: 00 00 01 00.
            bool validIco = false;
            FILE* iconFile = fopen(iconSrc.c_str(), "rb");
            if (iconFile != nullptr)
            {
                uint8_t header[4] = {};
                size_t read = fread(header, 1, 4, iconFile);
                fclose(iconFile);
                validIco = (read == 4 &&
                            header[0] == 0x00 && header[1] == 0x00 &&
                            header[2] == 0x01 && header[3] == 0x00);
            }

            if (!validIco)
            {
                LogWarning("Custom project icon '%s' is not a valid Windows .ico file (expected magic 00 00 01 00). Falling back to engine default.",
                           iconSrc.c_str());
                AppendBuildOutput("WARNING: Custom icon ignored (not a valid .ico).\n");
            }
            else
            {
                std::string iconDst = polyphaseDirectory + "Standalone/Polyphase.ico";
                std::string iconBackup = polyphaseDirectory + "Standalone/Polyphase.ico.bak";

                // Back up original icon
                SYS_CopyFile(iconDst.c_str(), iconBackup.c_str());

                // Replace with project icon
                SYS_CopyFile(iconSrc.c_str(), iconDst.c_str());
                AppendBuildOutput("Using custom project icon.\n");

                // Also copy icon into packaged directory
                SYS_CopyFile(iconSrc.c_str(), (packagedDir + GetEngineConfig()->mIconPath).c_str());
            }
        }
    }

    // Handle SpirV shaders on Vulkan platforms
    if (platform == Platform::Windows ||
        platform == Platform::Linux ||
        platform == Platform::Android)
    {
        AppendBuildOutput("Compiling shaders...\n");
#if PLATFORM_WINDOWS
        std::string shaderCmd = "cd \"" + polyphaseDirectory + "Engine/Shaders/GLSL\" && \"./compile.bat\"";
#else
        std::string shaderCmd = "cd \"" + polyphaseDirectory + "Engine/Shaders/GLSL\" && \"./compile.sh\"";
#endif
        mBuildState.mShaderCompileCommand = shaderCmd;

        // In headless mode, compile shaders synchronously here
        if (IsHeadless())
        {
            SYS_Exec(shaderCmd.c_str());
        }

        CreateDir((packagedDir + "Engine/Shaders/").c_str());
        CreateDir((packagedDir + "Engine/Shaders/GLSL/").c_str());

        SYS_CopyDirectory((polyphaseDirectory+"Engine/Shaders/GLSL/bin/").c_str(), (packagedDir + "Engine/Shaders/GLSL/bin/").c_str());
    }

    // Romfs setup for 3DS
    std::string intermediateDir = standalone ? polyphaseDirectory+"Standalone/Intermediate" : (projectDir + "/Intermediate");
    std::string romfsDir = intermediateDir + "/Romfs";
    RemoveDir(romfsDir.c_str());
    CreateDir(intermediateDir.c_str());
    CreateDir(romfsDir.c_str());

    if (useRomfs)
    {
    #if PLATFORM_WINDOWS
        SYS_CopyDirectoryRecursive(packagedDir.c_str(), romfsDir.c_str());
    #else
        SYS_Exec(std::string("cp -R " + packagedDir + "/* " + romfsDir).c_str());
    #endif
    }

    // Determine compilation needs
    bool needCompile = true;

    std::string buildProjName = standalone ? "Standalone" : projectName;
    std::string buildProjDir = standalone ? polyphaseDirectory+"Standalone/" : projectDir;
    std::string buildDstExeName = standalone ? "Polyphase" : projectName;

    bool useSteam = mBuildState.mUseSteam;

    std::string prebuiltExePath;
    if (platform == Platform::Windows)
    {
        prebuiltExePath = buildProjDir + "Build/Windows/x64/"
            + (useSteam ? "ReleaseSteam/" : "Release/")
            + "Polyphase.exe";
    }
    else if (platform == Platform::Linux)
    {
        prebuiltExePath = buildProjDir + "Build/Linux/Polyphase.elf";
    }

    if (standalone && !IsHeadless() &&
        (platform == Platform::Windows || platform == Platform::Linux))
    {
        // Force Rebuild must invalidate the prebuilt-exe shortcut too, otherwise
        // BuildData silently reuses a stale Polyphase.exe that predates the user's
        // latest source edit. mForceCompile is the snapshot BuildData took before
        // mForceRebuild was cleared.
        needCompile = mBuildState.mForceCompile
            || !SYS_DoesFileExist(prebuiltExePath.c_str(), false);
    }

    mBuildState.mNeedCompile = needCompile;
    mBuildState.mBuildProjDir = buildProjDir;
    mBuildState.mPrebuiltExePath = prebuiltExePath;

    LogWarning("[BUILD] needCompile=%d plat=%d", needCompile ? 1 : 0, (int)platform);

    if (needCompile)
    {
        AppendBuildOutput("Setting up compilation...\n");

        if (platform == Platform::Windows)
        {
            std::string solutionPath = "Polyphase.sln";
            if (engineState->mSolutionPath != "")
            {
                solutionPath = engineState->mSolutionPath;
            }

            // --- Native addon injection for Windows shipped build ---
            // Standalone.vcxproj is explicit-list (no wildcards). Inject <ClCompile> items
            // for the generated plugin-registrar + every addon .cpp, plus per-config
            // AdditionalIncludeDirectories / AdditionalDependencies / AdditionalLibraryDirectories
            // / PreprocessorDefinitions entries from each addon's package.json extras.
            {
                NativeAddonManager* nam = NativeAddonManager::Get();
                if (nam != nullptr)
                {
                    nam->DiscoverNativeAddons();
                    std::vector<NativeAddonState> engineAddons = nam->GetEngineAddons();
                    if (!engineAddons.empty())
                    {
                        SYS_CreateDirectory((buildProjDir + "Generated").c_str());
                        const std::string registrarPath = buildProjDir + "Generated/AddonPlugins.cpp";
                        GenerateAddonPluginsRegistrar(registrarPath, engineAddons);

                        const std::string vcxprojPath = buildProjDir + buildProjName + ".vcxproj";
                        InjectNativeAddonsIntoVcxproj(vcxprojPath, engineAddons, "Generated\\AddonPlugins.cpp");
                    }
                }
            }

            // If the user asked for a Force Rebuild, wipe MSBuild's incremental state for
            // this config before invoking devenv. MSBuild's own .tlog-driven
            // up-to-date check will otherwise declare "nothing to do" even after we've
            // edited sources, because the injected addon source mtimes match .tlog
            // expectations. Clearing the intermediate dir + the output exe forces a
            // real full compile and link.
            if (mBuildState.mForceCompile)
            {
                const std::string config = useSteam ? "ReleaseSteam" : "Release";
                const std::string intDir = buildProjDir + "Intermediate/Windows/x64/" + config + "/" + buildProjName + "/";
                const std::string buildDir = buildProjDir + "Build/Windows/x64/" + config + "/";

                LogDebug("[BUILD] Force Rebuild: wiping %s and %sPolyphase.exe",
                         intDir.c_str(), buildDir.c_str());
                AppendBuildOutput("Force Rebuild: clearing intermediates for " + config + "\n");

                if (DoesDirExist(intDir.c_str()))
                {
                    RemoveDir(intDir.c_str());
                }
                // Only remove the exe + incremental-link companion files; keep the rest
                // of Build/ in case other artifacts in that folder are wanted (shaders,
                // side-by-side libs). link.exe will regenerate these.
                SYS_RemoveFile((buildDir + "Polyphase.exe").c_str());
                SYS_RemoveFile((buildDir + "Polyphase.ilk").c_str());
                SYS_RemoveFile((buildDir + "Polyphase.pdb").c_str());

                // Engine.lib is what Standalone links against. devenv /Build (not
                // /Rebuild) will only re-lib Engine if its .lib output is missing -
                // otherwise MSBuild's up-to-date check hands the linker a stale
                // Engine.lib whose exports may not match the user's latest edits.
                // Delete it so the dependency check forces a fresh re-lib.
                //
                // Engine.pdb is cl.exe's compile-time PDB. If it's locked when the
                // next build starts (devenv with a debug session of Polyphase.exe
                // attached, leaked mspdbsrv.exe from a killed prior build, AV scan
                // in flight, ...) every cpp file fails with C1033 and the entire
                // Engine compile aborts. Try to delete it; if the delete fails the
                // lock is real, so abort here with a clear message rather than let
                // devenv spin up a doomed compile.
                const std::string engineBuildDir =
                    polyphaseDirectory + "Engine/Build/Windows/x64/" + config + "/";
                const std::string enginePdb = engineBuildDir + "Engine.pdb";
                const std::string engineLib = engineBuildDir + "Engine.lib";

                SYS_RemoveFile(engineLib.c_str());
                SYS_RemoveFile(enginePdb.c_str());

                if (SYS_DoesFileExist(enginePdb.c_str(), false))
                {
                    LogError("[BUILD] Force Rebuild aborted: %s is locked",
                             enginePdb.c_str());
                    AppendBuildOutput(
                        "ERROR: Could not delete " + enginePdb +
                        " (file is locked).\n"
                        "  Visual Studio likely has a debug session of Polyphase.exe\n"
                        "  attached, or a leaked mspdbsrv.exe is holding the PDB.\n"
                        "  Stop debugging (or close devenv.exe) and retry Force Rebuild.\n");
                    mBuildState.mComplete.store(true);
                    mBuildState.mSuccess.store(false);
                    return;
                }
            }

            std::string devenvCmd = "\"" + GetDevenvPath();
            if (useSteam)
            {
                devenvCmd += std::string(" ") + solutionPath + " /Build \"ReleaseSteam|x64\" /Project " + buildProjName;
            }
            else
            {
                devenvCmd += std::string(" ") + solutionPath + " /Build \"Release|x64\" /Project " + buildProjName;
            }
            devenvCmd += "\"";
            mBuildState.mCompileCommand = devenvCmd;
        }
        else if (platform == Platform::Android)
        {
            std::string androidAssetsDir = buildProjDir + "Android/app/src/main/assets/";
            if (!DoesDirExist(androidAssetsDir.c_str()))
            {
                CreateDir(androidAssetsDir.c_str());
            }
            SYS_CopyDirectory(packagedDir.c_str(), androidAssetsDir.c_str());

            std::string gradleDir = buildProjDir + "Android/";
#if PLATFORM_WINDOWS
            std::string gradleCmd = "cd " + gradleDir + " && gradlew.bat assembleRelease";
#else
            std::string gradleCmd = "cd " + gradleDir + " && \"./gradlew assembleRelease\"";
#endif
            mBuildState.mCompileCommand = gradleCmd;
        }
        else
        {
            LogWarning("[BUILD] Makefile build path");
            std::string makefilePath = "Makefile_";

            switch (platform)
            {
            case Platform::Linux: makefilePath += "Linux_Game"; break;
            case Platform::GameCube: makefilePath += "GCN"; break;
            case Platform::Wii: makefilePath += "Wii"; break;
            case Platform::N3DS: makefilePath += "3DS"; break;
            default: OCT_ASSERT(0); break;
            }

            std::string srcMakefile = buildProjDir + makefilePath;
            std::string tmpMakefile = buildProjDir + "Makefile_TEMP";

            if (!SYS_DoesFileExist(srcMakefile.c_str(), false))
            {
                std::string standaloneMakefile = "Standalone/" + makefilePath;
                if (SYS_DoesFileExist(standaloneMakefile.c_str(), false))
                {
                    LogDebug("Makefile not found in project, copying from Standalone: %s", standaloneMakefile.c_str());
                    SYS_CopyFile(standaloneMakefile.c_str(), srcMakefile.c_str());
                }
                else
                {
                    LogError("Makefile not found: %s or %s", srcMakefile.c_str(), standaloneMakefile.c_str());
                    AppendBuildOutput("ERROR: Makefile not found: " + srcMakefile + "\n");
                    mBuildState.mComplete.store(true);
                    mBuildState.mSuccess.store(false);
                    return;
                }
            }

            SYS_CopyFile(srcMakefile.c_str(), tmpMakefile.c_str());

            if (platform == Platform::N3DS)
            {
                ReplaceStringInFile(tmpMakefile, "PolyphaseApp", projectName);
                ReplaceStringInFile(tmpMakefile, "$(CURDIR)/Makefile_3DS", "$(CURDIR)/Makefile_TEMP");
            }

            std::string projSourceDir = buildProjDir + "Source";
            if (!DoesDirExist(projSourceDir.c_str()))
            {
                std::string standaloneSourceDir = "Standalone/Source";
                if (DoesDirExist(standaloneSourceDir.c_str()))
                {
                    LogDebug("Source folder not found in project, copying from Standalone");
                    SYS_CopyDirectory(standaloneSourceDir.c_str(), projSourceDir.c_str());
                }
            }

            SYS_CreateDirectory((buildProjDir + "Generated").c_str());

            SYS_CopyFile(embeddedHeaderPath.c_str(), (buildProjDir + "Generated/EmbeddedAssets.h").c_str());
            SYS_CopyFile(embeddedSourcePath.c_str(), (buildProjDir + "Generated/EmbeddedAssets.cpp").c_str());
            SYS_CopyFile(scriptHeaderPath.c_str(), (buildProjDir + "Generated/EmbeddedScripts.h").c_str());
            SYS_CopyFile(scriptSourcePath.c_str(), (buildProjDir + "Generated/EmbeddedScripts.cpp").c_str());

            InjectNativeAddonSources(tmpMakefile, buildProjDir, platform);

            // Force Rebuild for Makefile platforms: make's timestamp-only dependency
            // tracking won't notice CFLAGS/LIBS changes from the just-injected addon
            // metadata. If a previous build cached .o files compiled WITHOUT the
            // addon's extras (e.g. -DPOLYPHASE_WITH_FFMPEG=1), make happily reuses
            // them and the resulting .elf silently misses the addon's gated code.
            // Wipe Intermediate/<platform>/ so every source recompiles from scratch.
            if (mBuildState.mForceCompile)
            {
                // Force Rebuild for Makefile-based platforms wipes EVERY cached
                // artifact this target depends on, not just the game intermediates.
                // Stale partial caches in External/Bullet/Intermediate/<plat>/
                // (e.g. an interrupted prior build that left some .o files
                // missing) otherwise survive Force Rebuild and trigger
                // "ar: <file>.o: No such file or directory" at link time, which
                // is impossible to diagnose without knowing the cache layout.
                // Slower than wiping just the game intermediates, but matches
                // what users expect from a button labelled "Force Rebuild".
                const std::string platDir = [&]() -> std::string {
                    switch (platform)
                    {
                    case Platform::Linux:    return "Linux";
                    case Platform::GameCube: return "GCN";
                    case Platform::Wii:      return "Wii";
                    case Platform::N3DS:     return "3DS";
                    default: return "";
                    }
                }();

                if (!platDir.empty())
                {
                    const std::string exeName = standalone ? "Polyphase" : projectName;
                    const std::string exeExt = (platform == Platform::Linux)    ? ".elf"
                                             : (platform == Platform::N3DS)    ? ".3dsx"
                                             : /* GCN/Wii */                     ".dol";

                    // Per-platform wipe targets. For each: a directory to nuke
                    // recursively, and an optional file to delete. Empty paths
                    // are skipped (keeps logic linear).
                    struct Target { std::string dir; std::string file; };
                    std::vector<Target> targets;

                    // Standalone game intermediate + final exe.
                    targets.push_back({
                        buildProjDir + "Intermediate/" + platDir + (platform == Platform::Linux ? "/Game/" : "/"),
                        buildProjDir + "Build/" + platDir + "/" + exeName + exeExt
                    });

                    // Engine library intermediate + .a. Linux splits Editor/Game
                    // intermediates; consoles share a single dir.
                    if (platform == Platform::Linux)
                    {
                        targets.push_back({
                            polyphaseDirectory + "Engine/Intermediate/Linux/EngineGame/",
                            polyphaseDirectory + "Engine/Build/Linux/libEngineGame.a"
                        });
                    }
                    else
                    {
                        targets.push_back({
                            polyphaseDirectory + "Engine/Intermediate/" + platDir + "/",
                            polyphaseDirectory + "Engine/Build/" + platDir + "/libEngine.a"
                        });
                    }

                    // External/Bullet — Bullet's interrupted-build partial cache
                    // is the actual reported failure mode this block addresses.
                    targets.push_back({
                        polyphaseDirectory + "External/Bullet/Intermediate/" + platDir + "/",
                        polyphaseDirectory + "External/Bullet/Build/" + platDir + "/libBullet.a"
                    });

                    // External/Assimp — only on platforms that build it (Linux).
                    if (platform == Platform::Linux)
                    {
                        targets.push_back({
                            polyphaseDirectory + "External/Assimp/Intermediate/Linux/",
                            polyphaseDirectory + "External/Assimp/Build/Linux/libAssimp.a"
                        });
                    }

                    AppendBuildOutput("Force Rebuild: clearing all caches for " + platDir + "\n");
                    for (const Target& t : targets)
                    {
                        if (!t.dir.empty() && DoesDirExist(t.dir.c_str()))
                        {
                            LogDebug("[BUILD] Force Rebuild: wiping %s", t.dir.c_str());
                            RemoveDir(t.dir.c_str());
                        }
                        if (!t.file.empty())
                        {
                            SYS_RemoveFile(t.file.c_str());
                        }
                    }
                }
            }

            mBuildState.mCompileCommand = std::string("make -C \"") + buildProjDir + "\" -f Makefile_TEMP -j 12";
            mBuildState.mTmpMakefile = tmpMakefile;

            if (platform == Platform::Linux)
            {
                std::string exeName = standalone ? "Polyphase" : projectName;
                mBuildState.mStripCommand = std::string("strip --strip-debug \"") + buildProjDir + "Build/Linux/" + exeName + ".elf\"";
            }
        }

        // Compute exe source path and extension for post-build
        std::string exeSrc = buildProjDir + "/Build/";
        if (platform == Platform::Android)
        {
            exeSrc = buildProjDir;
        }

        switch (platform)
        {
        case Platform::Windows: exeSrc += (useSteam ? "Windows/x64/ReleaseSteam/" : "Windows/x64/Release/"); break;
        case Platform::Linux: exeSrc += "Linux/"; break;
        case Platform::Android: exeSrc += "Android/app/build/outputs/apk/release/"; break;
        case Platform::GameCube: exeSrc += "GCN/"; break;
        case Platform::Wii: exeSrc += "Wii/"; break;
        case Platform::N3DS: exeSrc += "3DS/"; break;
        default: OCT_ASSERT(0); break;
        }

        std::string exeNameBase = standalone ? "Polyphase" : projectName;
        std::string extension = ".exe";

        switch (platform)
        {
        case Platform::Windows: extension = ".exe"; break;
        case Platform::Linux: extension = ".elf"; break;
        case Platform::Android: extension = ".apk"; break;
        case Platform::GameCube: extension = ".dol"; break;
        case Platform::Wii: extension = ".dol"; break;
        case Platform::N3DS: extension = ".3dsx"; break;
        default: OCT_ASSERT(0); break;
        }

        exeSrc += exeNameBase + extension;
        mBuildState.mExeSrc = exeSrc;
        mBuildState.mExtension = extension;

        // Snapshot the existing exe's mtime so FinalizeLocalBuild can detect a
        // silent compile/link failure that leaves a stale file on disk
        // (e.g. MSVC LTCG link.exe crash). Zero if the file doesn't exist yet.
        mBuildState.mExeMTimeBeforeBuild = CheckExeIntegrity(exeSrc).mtime;

        // Launch compile thread (or run synchronously in headless)
        if (IsHeadless())
        {
            // Headless: handled by caller (BuildData)
        }
        else
        {
            AppendBuildOutput("Compiling...\n");
            mBuildState.mRunning.store(true);
            mBuildState.mBuildThread = std::thread(&ActionManager::BuildCompileThreadFunc, this);
        }
    }
    else
    {
        LogDebug("Reusing pre-compiled game executable.");
        AppendBuildOutput("Reusing pre-compiled executable.\n");

        // Compute exe path for the no-compile case
        std::string exeSrc = buildProjDir + "/Build/";
        if (platform == Platform::Android) exeSrc = buildProjDir;

        switch (platform)
        {
        case Platform::Windows: exeSrc += (useSteam ? "Windows/x64/ReleaseSteam/" : "Windows/x64/Release/"); break;
        case Platform::Linux: exeSrc += "Linux/"; break;
        case Platform::Android: exeSrc += "Android/app/build/outputs/apk/release/"; break;
        case Platform::GameCube: exeSrc += "GCN/"; break;
        case Platform::Wii: exeSrc += "Wii/"; break;
        case Platform::N3DS: exeSrc += "3DS/"; break;
        default: OCT_ASSERT(0); break;
        }

        std::string exeNameBase = standalone ? "Polyphase" : projectName;
        std::string extension = ".exe";
        switch (platform)
        {
        case Platform::Windows: extension = ".exe"; break;
        case Platform::Linux: extension = ".elf"; break;
        case Platform::Android: extension = ".apk"; break;
        case Platform::GameCube: extension = ".dol"; break;
        case Platform::Wii: extension = ".dol"; break;
        case Platform::N3DS: extension = ".3dsx"; break;
        default: OCT_ASSERT(0); break;
        }

        mBuildState.mExeSrc = prebuiltExePath;
        mBuildState.mExtension = extension;

        // No compilation needed — mark complete and go straight to finalize
        mBuildState.mComplete.store(true);
        mBuildState.mSuccess.store(true);

        if (IsHeadless())
        {
            // Headless: handled by caller (BuildData)
        }
        else
        {
            // Auto-finalize on next modal draw
        }
    }
}

void ActionManager::BuildCompileThreadFunc()
{
#if PLATFORM_WINDOWS
#define popen _popen
#define pclose _pclose
#endif

    auto runStreamedCommand = [this](const std::string& cmd, const std::string& label) -> int
    {
        AppendBuildOutput(label + "\n");
        LogDebug("[BuildThread] %s", cmd.c_str());

        std::string fullCmd = cmd + " 2>&1";
        FILE* pipe = popen(fullCmd.c_str(), "r");
        if (pipe == nullptr)
        {
            AppendBuildOutput("ERROR: Failed to run command: " + cmd + "\n");
            return -1;
        }

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            if (mBuildState.mCancelRequested.load())
            {
                pclose(pipe);
                AppendBuildOutput("Build cancelled.\n");
                return -1;
            }
            AppendBuildOutput(buffer);
        }

        int status = pclose(pipe);
#if PLATFORM_WINDOWS || PLATFORM_3DS || PLATFORM_DOLPHIN
        int exitCode = status;
#else
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
        return exitCode;
    };

    // Compile shaders if needed (Vulkan platforms)
    if (!mBuildState.mShaderCompileCommand.empty() && !mBuildState.mCancelRequested.load())
    {
        int shaderExit = runStreamedCommand(mBuildState.mShaderCompileCommand, "--- Compiling shaders ---");
        if (shaderExit != 0)
        {
            AppendBuildOutput("Shader compilation failed (exit code: " + std::to_string(shaderExit) + ")\n");
            mBuildState.mExitCode.store(shaderExit);
            mBuildState.mSuccess.store(false);
            mBuildState.mComplete.store(true);
            mBuildState.mRunning.store(false);
            return;
        }
    }

    if (mBuildState.mCancelRequested.load())
    {
        AppendBuildOutput("Build cancelled.\n");
        mBuildState.mSuccess.store(false);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);
        return;
    }

    // Run the main compile command
    int exitCode = runStreamedCommand(mBuildState.mCompileCommand, "--- Compiling game ---");

    if (exitCode != 0 || mBuildState.mCancelRequested.load())
    {
        if (mBuildState.mCancelRequested.load())
        {
            AppendBuildOutput("Build cancelled.\n");
        }
        else
        {
            AppendBuildOutput("Compilation failed (exit code: " + std::to_string(exitCode) + ")\n");
        }
        mBuildState.mExitCode.store(exitCode);
        mBuildState.mSuccess.store(false);
        mBuildState.mComplete.store(true);
        mBuildState.mRunning.store(false);

        // Clean up temp makefile even on failure
        if (!mBuildState.mTmpMakefile.empty())
        {
            SYS_RemoveFile(mBuildState.mTmpMakefile.c_str());
        }
        return;
    }

    // Clean up temp makefile
    if (!mBuildState.mTmpMakefile.empty())
    {
        SYS_RemoveFile(mBuildState.mTmpMakefile.c_str());
    }

    // Strip on Linux
    if (!mBuildState.mStripCommand.empty())
    {
        AppendBuildOutput("Stripping debug symbols...\n");
        runStreamedCommand(mBuildState.mStripCommand, "");
    }

    AppendBuildOutput("Compilation successful.\n");
    mBuildState.mExitCode.store(0);
    mBuildState.mSuccess.store(true);
    mBuildState.mComplete.store(true);
    mBuildState.mRunning.store(false);

#if PLATFORM_WINDOWS
#undef popen
#undef pclose
#endif
}

void ActionManager::FinalizeLocalBuild()
{
    // Join thread if joinable
    if (mBuildState.mBuildThread.joinable())
    {
        mBuildState.mBuildThread.join();
    }

    Platform platform = mBuildState.mPlatform;
    bool standalone = mBuildState.mStandalone;
    std::string packagedDir = mBuildState.mPackagedDir;
    std::string projectName = mBuildState.mProjectName;
    std::string projectDir = mBuildState.mProjectDir;
    std::string extension = mBuildState.mExtension;
    std::string exeSrc = mBuildState.mExeSrc;
    bool useSteam = mBuildState.mUseSteam;
    bool needCompile = mBuildState.mNeedCompile;

    // Restore original Standalone icon if it was backed up
    {
        std::string polyphaseDir = SYS_GetPolyphasePath();
        std::string iconBackup = polyphaseDir + "Standalone/Polyphase.ico.bak";
        if (SYS_DoesFileExist(iconBackup.c_str(), false))
        {
            SYS_CopyFile(iconBackup.c_str(), (polyphaseDir + "Standalone/Polyphase.ico").c_str());
            SYS_RemoveFile(iconBackup.c_str());
        }
    }

    if (!mBuildState.mSuccess.load())
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->FireOnPackageFinished((int32_t)platform, false);
        if (hookMgr != nullptr) hookMgr->FireOnPostBuild((int32_t)platform, false);
        mShowBuildModal = false;
        return;
    }

    if (!needCompile)
    {
        exeSrc = mBuildState.mPrebuiltExePath;
    }

    // Integrity check the compiled executable before packaging.
    // msbuild / devenv can exit 0 even when link.exe silently crashes,
    // leaving a stale or zero-filled binary on disk. Without this check the
    // broken file gets copied into the packaged output.
    if (needCompile)
    {
        ExeIntegrity info = CheckExeIntegrity(exeSrc);

        bool missing       = !info.exists;
        bool tooSmall      = info.size < 1024;
        bool notRewritten  = info.mtime != 0 &&
                             mBuildState.mExeMTimeBeforeBuild != 0 &&
                             info.mtime <= mBuildState.mExeMTimeBeforeBuild;
        bool badPE         = (platform == Platform::Windows) && info.exists && !info.validPE;

        if (missing || tooSmall || notRewritten || badPE)
        {
            LogError("Build produced an invalid executable: %s", exeSrc.c_str());
            LogError("  exists=%d size=%llu bytes mtime=%lld preMtime=%lld validPE=%d",
                     (int)info.exists,
                     (unsigned long long)info.size,
                     (long long)info.mtime,
                     (long long)mBuildState.mExeMTimeBeforeBuild,
                     (int)info.validPE);
            if (notRewritten)
                LogError("  The compile step claimed success but did not rewrite the executable.");
            if (badPE)
                LogError("  The executable is not a valid PE/MZ image — link.exe may have crashed mid-write.");
            LogError("  Packaging aborted. Clean Standalone/Intermediate and Standalone/Build, then rebuild.");

            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->FireOnPackageFinished((int32_t)platform, false);
            if (hookMgr != nullptr) hookMgr->FireOnPostBuild((int32_t)platform, false);
            mShowBuildModal = false;
            return;
        }
    }

    // Copy the executable into the Packaged folder
    SYS_CopyDirectory(exeSrc.c_str(), packagedDir.c_str());

    // Delete the built binary so a stale file isn't copied on a subsequent failed build
    if (needCompile)
    {
        SYS_RemoveFile(exeSrc.c_str());
    }

    // Copy each addon's `copyBinaries` directories (e.g. FFmpeg DLLs) next to the exe so
    // runtime DLL imports resolve. Resolved per target platform — addons that bundle
    // host-specific binaries should put them under nativePerPlatform.<P>.copyBinaries.
    CopyAddonBinariesToPackaged(packagedDir, platform);

    if (standalone)
    {
        SYS_MoveFile((packagedDir + "Polyphase" + extension).c_str(), (packagedDir + projectName + extension).c_str());
    }

    if (platform == Platform::Windows && useSteam)
    {
        SYS_CopyDirectory((projectDir + (standalone ? "../" : "../Polyphase/") + "External/Steam/redistributable_bin/win64/steam_api64.dll").c_str(), packagedDir.c_str());

        Stream idStream;
        const char* txtId = "480";
        idStream.WriteBytes((uint8_t*)txtId, (uint32_t)strlen(txtId));
        idStream.WriteFile((packagedDir + "steam_appid.txt").c_str());
    }

    // Verify executable
    if (!SYS_DoesFileExist((packagedDir + projectName + extension).c_str(), false))
    {
        LogError("Packaged executable not found: %s", (packagedDir + projectName + extension).c_str());
        LogError("Packaging failed. Please check the log for errors.");

        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->FireOnPackageFinished((int32_t)platform, false);
        if (hookMgr != nullptr) hookMgr->FireOnPostBuild((int32_t)platform, false);
        return;
    }

    if (!IsHeadless())
    {
        SYS_ExplorerOpenDirectory(packagedDir.c_str());
    }

    LogDebug("Finished packaging!");

    // Save build manifest for incremental builds
    BuildCache* cache = BuildCache::Get();
    if (cache != nullptr)
    {
        cache->BuildCurrentManifest(platform, mBuildState.mEmbedded);
        cache->SaveManifest();
    }

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->FireOnPackageFinished((int32_t)platform, true);
    if (hookMgr != nullptr) hookMgr->FireOnPostBuild((int32_t)platform, true);

    // Handle run-after-build
    if (mBuildState.mRunAfterBuild)
    {
        std::string outputPath = packagedDir + projectName + extension;

        if (mBuildState.mRunOnDevice && platform == Platform::N3DS)
        {
            // Delegate to PackagingWindow for 3dslink
            GetPackagingWindow()->BuildAndRunWithProfile(platform, mBuildState.mEmbedded, true);
        }
        else if (mBuildState.mRunOnDevice && platform == Platform::Wii)
        {
            // Launch wiiload directly
            LaunchersModule* launchers = static_cast<LaunchersModule*>(
                PreferencesManager::Get()->FindModule("External/Launchers"));

            if (launchers != nullptr && launchers->IsWiiloadConfigured())
            {
                std::string cmd = launchers->BuildWiiloadCommand(outputPath);
                if (!cmd.empty())
                {
                    LogDebug("Launching wiiload: %s", cmd.c_str());
                    SYS_Exec(cmd.c_str());
                }
            }
            else
            {
                LogError("wiiload not configured. Set Wii IP in Preferences > External > Launchers.");
            }
        }
        else
        {
            LaunchersModule* launchers = static_cast<LaunchersModule*>(
                PreferencesManager::Get()->FindModule("External/Launchers"));

            if (launchers != nullptr && launchers->IsEmulatorConfigured(platform))
            {
                std::string cmd = launchers->BuildLaunchCommand(platform, outputPath);
                LogDebug("Launching emulator: %s", cmd.c_str());
                SYS_Exec(cmd.c_str());
            }
            else
            {
                LogError("Emulator not configured for %s", GetPlatformString(platform));
            }
        }
    }

    mShowBuildModal = false;
}

void ActionManager::DrawBuildModal()
{
    if (!mShowBuildModal)
    {
        return;
    }

    // Update display output from shared buffer
    {
        std::lock_guard<std::mutex> lock(mBuildState.mOutputMutex);
        if (mBuildState.mOutputDirty)
        {
            mBuildDisplayOutput = mBuildState.mOutput;
            mBuildState.mOutputDirty = false;
        }
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoCollapse;
    bool finalized = false;

    if (ImGui::Begin("Local Build", &mShowBuildModal, modalFlags))
    {
        bool isComplete = mBuildState.mComplete.load();
        bool isSuccess = mBuildState.mSuccess.load();
        bool isCancelled = mBuildState.mCancelRequested.load();
        bool isRunning = mBuildState.mRunning.load();

        // Status header
        if (!isComplete && !isRunning)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Preparing...");
        }
        else if (!isComplete)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Compiling...");
        }
        else if (isCancelled)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Build Cancelled");
        }
        else if (isSuccess)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Build Successful!");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Build Failed (exit code: %d)", mBuildState.mExitCode.load());
        }

        ImGui::Separator();

        // Collapsible output section
        if (ImGui::CollapsingHeader("Build Output", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float footerHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
            ImVec2 outputSize(0, -footerHeight);

            ImGui::BeginChild("BuildOutput", outputSize, true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(mBuildDisplayOutput.c_str());

            if (mBuildAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
            {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }

        // Footer buttons
        Polyphase::Checkbox("Auto-scroll", &mBuildAutoScroll);

        ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            ImGui::SetClipboardText(mBuildDisplayOutput.c_str());
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Log"))
        {
            std::string logPath = mBuildState.mProjectDir + "build_log.txt";
            Stream logStream;
            logStream.WriteBytes((uint8_t*)mBuildDisplayOutput.c_str(), (uint32_t)mBuildDisplayOutput.size());
            logStream.WriteFile(logPath.c_str());
            AppendBuildOutput("Log saved to: " + logPath + "\n");
        }

        ImGui::SameLine();
        float buttonWidth = 80.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - buttonWidth);

        // Auto-finalize on successful completion
        if (isComplete && isSuccess && !isCancelled)
        {
            FinalizeLocalBuild();
            finalized = true;
        }
        else if (isComplete)
        {
            if (ImGui::Button("Close", ImVec2(buttonWidth, 0)))
            {
                FinalizeLocalBuild();
                finalized = true;
            }
        }
        else if (isRunning)
        {
            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                CancelBuild();
            }
        }
    }
    ImGui::End();

    // Handle window close via X button (skip if already finalized above)
    if (!mShowBuildModal && !finalized)
    {
        if (mBuildState.mRunning.load())
        {
            CancelBuild();
            mShowBuildModal = true; // Re-show until build completes
        }
        else if (mBuildState.mComplete.load())
        {
            FinalizeLocalBuild();
        }
        else
        {
            mShowBuildModal = true;
        }
    }
}

void ActionManager::PrepareRelease()
{
    // Make sure to change the POLYPHASE_VERSION definition in Constants.h first.

    // [ ] Package the project for current platform
    std::string platformName = "";
#if PLATFORM_WINDOWS
    ActionManager::Get()->BuildData(Platform::Windows, false);
    platformName = "Windows";
#else
    ActionManager::Get()->BuildData(Platform::Linux, false);
    platformName = "Linux";

    // On Linux, make the editor too, in case we are using a cmake-build
    SYS_Exec("make -j 12 -C Standalone -f Makefile_Linux_Editor");
#endif

    // [ ] Destroy/Create directory in CWD for release
    const char* homePath = nullptr;
#if PLATFORM_WINDOWS
    homePath = getenv("USERPROFILE");
#else
    homePath = getenv("HOME");
#endif
    OCT_ASSERT(homePath != nullptr);
    LogDebug("Home path = %s", homePath);

    std::string stagingDir = std::string(homePath) + "/PolyphaseRelease/";
    SYS_RemoveDirectory(stagingDir.c_str());
    SYS_CreateDirectory(stagingDir.c_str());

    // [ ] Copy specific directories. See if we can use rsync on windows?
    std::string cpCmd = std::string("cp -R . ") + stagingDir;
    SYS_Exec(cpCmd.c_str());
    std::string cleanCmd = std::string("cd ") + stagingDir + " && git clean -xdf";
    SYS_Exec(cleanCmd.c_str());

    // [ ] Copy this exectubable (Release Editor) to the staging directory, rename to PolyphaseEditor
#if PLATFORM_WINDOWS
    std::string cpEditorCmd = std::string("cp Standalone/Build/Windows/x64/ReleaseEditor/Polyphase.exe ") + stagingDir + "/PolyphaseEditor.exe";
#else
    std::string cpEditorCmd = std::string("cp Standalone/Build/Linux/PolyphaseEditor.elf ") + stagingDir;// + std::string("PolyphaseEditor");
#endif

    SYS_Exec(cpEditorCmd.c_str());

    // [ ] Copy the packaged platform's Polyphase exe to the staging directory
#if PLATFORM_WINDOWS
    std::string cpGameCmd = std::string("cp Standalone/Build/Windows/x64/Release/Polyphase.exe ") + stagingDir;
#else
    std::string cpGameCmd = std::string("cp Standalone/Build/Linux/Polyphase.elf ") + stagingDir;// + std::string("Polyphase");
#endif

    SYS_Exec(cpGameCmd.c_str());

#if PLATFORM_LINUX
    // Strip out debug symbols to keep the file size small.
    std::string stripEditorCmd = std::string("strip --strip-debug ") + stagingDir + "PolyphaseEditor.elf";
    SYS_Exec(stripEditorCmd.c_str());
    std::string stripCmd = std::string("strip --strip-debug ") + stagingDir + "Polyphase.elf";
    SYS_Exec(stripCmd.c_str());
#endif

    // Compile shaders
#if PLATFORM_WINDOWS
    SYS_Exec("cd Engine/Shaders/GLSL && \"./compile.bat\"");
    std::string shadersCmd = "cd " + stagingDir + "/Engine/Shaders/GLSL && \"./compile.bat\"";
#else
    std::string shadersCmd = "cd " + stagingDir + "/Engine/Shaders/GLSL && \"./compile.sh\"";
#endif
    SYS_Exec(shadersCmd.c_str());

    // Remove git submodules
    std::string rmBulletSubmodule = std::string("rm -rf ") + stagingDir + "/External/bullet3";
    SYS_Exec(rmBulletSubmodule.c_str());
    std::string rmAssimpSubmodule = std::string("rm -rf ") + stagingDir + "/External/assimp";
    SYS_Exec(rmAssimpSubmodule.c_str());

    // Delete all dot files
    std::string deleteGitCmd = std::string("find ") + stagingDir + " -name '.*' -exec rm -rf \"{}\" \\;";
    SYS_Exec(deleteGitCmd.c_str());
}

void ActionManager::OnSelectedNodeChanged()
{
    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr)
    {
        hookMgr->FireOnSelectionChanged();
    }
}

Node* ActionManager::SpawnNode(TypeId nodeType, Node* parent)
{
    if (parent && parent->IsSceneLinked())
    {
        LogWarning("Cannot add child to scene-linked node. Unlink the scene first.");
        return nullptr;
    }

    Node* spawnedNode = EXE_SpawnNode(nodeType);

    OCT_ASSERT(spawnedNode != nullptr);
    if (spawnedNode != nullptr)
    {
        parent = parent ? parent : GetWorld(0)->GetRootNode();
        if (parent != nullptr)
        {
            parent->AddChild(spawnedNode);
        }
        else
        {
            GetWorld(0)->SetRootNode(spawnedNode);
        }

        GetEditorState()->SetSelectedNode(spawnedNode);
    }

    return spawnedNode;
}

Node* ActionManager::SpawnNode(TypeId nodeType, glm::vec3 position)
{
    Node* node = SpawnNode(nodeType, nullptr);
    Node3D* node3d = node ? node->As<Node3D>() : nullptr;

    if (node3d)
    {
        node3d->SetWorldPosition(position);
    }

    return node;
}

static void ConfigureSkyboxNode(StaticMesh3D* skyNode, Material* srcMaterial = nullptr)
{
    StaticMesh* mesh = (StaticMesh*)LoadAsset("SM_Sphere");
    skyNode->SetStaticMesh(mesh);

    // Caller-provided material wins (e.g. user had a material selected when invoking the
    // BASIC_SKYBOX_TEXTURED menu). Otherwise apply the bundled engine default that already
    // has the skybox render properties baked in.
    Material* skyMat = srcMaterial;
    if (skyMat == nullptr)
    {
        skyMat = (Material*)LoadAsset("M_SkyboxDay");
    }
    if (skyMat != nullptr)
    {
        skyNode->SetMaterialOverride(skyMat);
    }

    skyNode->EnableCollision(false);
    skyNode->EnableOverlaps(false);
    skyNode->EnablePhysics(false);
    skyNode->EnableCastShadows(false);
    skyNode->EnableReceiveShadows(false);
    skyNode->EnableReceiveSimpleShadows(false);
    skyNode->SetScale(glm::vec3(500.0f));
    skyNode->SetTargetScreen(0xFF);
}

Node* ActionManager::SpawnBasicNode(const std::string& name, Node* parent, Asset* srcAsset, bool setWorldPos, glm::vec3 worldPos)
{
    Node* spawnedNode = nullptr;

    if (parent && parent->IsSceneLinked())
    {
        LogWarning("Cannot add child to scene-linked node. Unlink the scene first.");
        return nullptr;
    }

    if (srcAsset == nullptr)
    {
        srcAsset = GetEditorState()->GetSelectedAsset();
    }

    if (name == BASIC_STATIC_MESH)
    {
        StaticMesh3D* meshNode = EXE_SpawnNode(StaticMesh3D::GetStaticType())->As<StaticMesh3D>();

        StaticMesh* mesh = (StaticMesh*) LoadAsset("SM_Cube");

        if (srcAsset != nullptr &&
            srcAsset->GetType() == StaticMesh::GetStaticType())
        {
            mesh = static_cast<StaticMesh*>(srcAsset);
            meshNode->SetName(mesh->GetName());
        }

        // When spawned by the editor, static meshes have collision enabled on colgroup1
        meshNode->SetStaticMesh(mesh);
        meshNode->EnableOverlaps(false);
        meshNode->EnableCollision(true);
        meshNode->EnablePhysics(false);
        meshNode->SetCollisionGroup(ColGroup1);
        meshNode->SetCollisionMask(~ColGroup1);
        meshNode->SetBakeLighting(true);

        spawnedNode = meshNode;
    }
    else if (name == BASIC_POINT_LIGHT)
    {
        // Spawn point light actor
        PointLight3D* pointLight = EXE_SpawnNode(PointLight3D::GetStaticType())->As<PointLight3D>();

        pointLight->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        pointLight->SetRadius(10.0f);
        pointLight->SetLightingDomain(LightingDomain::All);
        
        spawnedNode = pointLight;
    }
    else if (name == BASIC_NODE_3D)
    {
        spawnedNode = EXE_SpawnNode(Node3D::GetStaticType())->As<Node3D>();
    }
    else if (name == BASIC_DIRECTIONAL_LIGHT)
    {
        DirectionalLight3D* dirLight = EXE_SpawnNode(DirectionalLight3D::GetStaticType())->As<DirectionalLight3D>();
        dirLight->SetLightingDomain(LightingDomain::All);

        spawnedNode = dirLight;
    }
    else if (name == BASIC_SKELETAL_MESH)
    {
        SkeletalMesh3D* skNode = EXE_SpawnNode(SkeletalMesh3D::GetStaticType())->As<SkeletalMesh3D>();

        SkeletalMesh* mesh = LoadAsset<SkeletalMesh>("SK_EighthNote");

        if (srcAsset != nullptr &&
            srcAsset->GetType() == SkeletalMesh::GetStaticType())
        {
            mesh = static_cast<SkeletalMesh*>(srcAsset);

            // Spawn skeletal mesh actor.
            skNode->SetSkeletalMesh(mesh);
        }

        spawnedNode = skNode;
    }
    else if (name == BASIC_BOX)
    {
        spawnedNode = EXE_SpawnNode(Box3D::GetStaticType())->As<Box3D>();
    }
    else if (name == BASIC_NAV_MESH)
    {
        spawnedNode = EXE_SpawnNode(NavMesh3D::GetStaticType())->As<NavMesh3D>();
    }
    else if (name == BASIC_SPHERE)
    {
        spawnedNode = EXE_SpawnNode(Sphere3D::GetStaticType())->As<Sphere3D>();
    }
    else if (name == BASIC_CAPSULE)
    {
        spawnedNode = EXE_SpawnNode(Capsule3D::GetStaticType())->As<Capsule3D>();
    }
    else if (name == BASIC_PARTICLE)
    {
        ParticleSystem* particleSystem = (ParticleSystem*) LoadAsset("P_DefaultParticle");

        if (srcAsset != nullptr &&
            srcAsset->GetType() == ParticleSystem::GetStaticType())
        {
            particleSystem = static_cast<ParticleSystem*>(srcAsset);
        }

        // Spawn a Particle actor
        Particle3D* particleNode = EXE_SpawnNode(Particle3D::GetStaticType())->As<Particle3D>();
        particleNode->SetParticleSystem(particleSystem);

        spawnedNode = particleNode;
    }
    else if (name == BASIC_AUDIO)
    {
        // TODO: Add a default sound in Engine assets
        SoundWave* soundWave = nullptr;

        if (srcAsset != nullptr &&
            srcAsset->GetType() == SoundWave::GetStaticType())
        {
            soundWave = static_cast<SoundWave*>(srcAsset);
        }

        // Spawn an Audio actor
        Audio3D* audioNode = EXE_SpawnNode(Audio3D::GetStaticType())->As<Audio3D>();
        audioNode->SetSoundWave(soundWave);
        audioNode->SetLoop(true);
        audioNode->SetAutoPlay(true);

        spawnedNode = audioNode;
    }
    else if (name == BASIC_SCENE)
    {
        Scene* scene = nullptr;

        if (srcAsset != nullptr &&
            srcAsset->GetType() == Scene::GetStaticType())
        {
            scene = static_cast<Scene*>(srcAsset);
        }

        if (scene != nullptr)
        {
            spawnedNode = EXE_SpawnNode(scene);
        }
    }
    else if (name == BASIC_CAMERA)
    {
        spawnedNode = EXE_SpawnNode(Camera3D::GetStaticType())->As<Camera3D>();
    }
    else if (name == BASIC_TEXT_MESH)
    {
        spawnedNode = EXE_SpawnNode(TextMesh3D::GetStaticType())->As<TextMesh3D>();
    }
    else if (name == BASIC_INSTANCED_MESH)
    {
        InstancedMesh3D* instMeshNode = EXE_SpawnNode(InstancedMesh3D::GetStaticType())->As<InstancedMesh3D>();
        spawnedNode = instMeshNode;

        StaticMesh* mesh = (StaticMesh*)LoadAsset("SM_Cube");

        if (srcAsset != nullptr &&
            srcAsset->GetType() == StaticMesh::GetStaticType())
        {
            mesh = static_cast<StaticMesh*>(srcAsset);
            instMeshNode->SetName(mesh->GetName());
        }

        instMeshNode->SetStaticMesh(mesh);
        instMeshNode->EnableOverlaps(false);
        instMeshNode->EnableCollision(false);
        instMeshNode->EnablePhysics(false);
        instMeshNode->SetCollisionGroup(ColGroup1);
        instMeshNode->SetCollisionMask(~ColGroup1);
        instMeshNode->AddInstanceData(MeshInstanceData());
    }
    else if (name == BASIC_SKYBOX_TEXTURED || name == BASIC_SKYBOX_VERTEX_COLOR)
    {
        StaticMesh3D* skyNode = EXE_SpawnNode(StaticMesh3D::GetStaticType())->As<StaticMesh3D>();
        skyNode->SetName("Skybox");

        Material* srcMat = (srcAsset != nullptr && srcAsset->Is(Material::ClassRuntimeId()))
            ? static_cast<Material*>(srcAsset) : nullptr;
        ConfigureSkyboxNode(skyNode, srcMat);

        spawnedNode = skyNode;
    }
    else if (name == BASIC_SPLINE)
    {
        spawnedNode = EXE_SpawnNode(Spline3D::GetStaticType())->As<Spline3D>();
    }
    else if (name == BASIC_TERRAIN)
    {
        Terrain3D* terrainNode = EXE_SpawnNode(Terrain3D::GetStaticType())->As<Terrain3D>();
        terrainNode->SetName("Terrain");
        terrainNode->EnableCollision(true);
        terrainNode->EnableOverlaps(false);
        terrainNode->EnablePhysics(false);
        terrainNode->SetCollisionGroup(ColGroup1);
        terrainNode->SetCollisionMask(~ColGroup1);

        // Prompt user for save location — creates _BakedMap and _Heightmap companion files
        std::string savePath = SYS_SaveFileDialog();
        if (!savePath.empty())
        {
            std::replace(savePath.begin(), savePath.end(), '\\', '/');

            // Strip extension if user typed one
            std::string basePath = savePath;
            size_t dotPos = basePath.rfind('.');
            size_t slashPos = basePath.rfind('/');
            if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
                basePath = basePath.substr(0, dotPos);

            std::string baseName = Asset::GetNameFromPath(basePath);
            terrainNode->SetName(baseName);

            // Enable atlas + material slots for the companion files
            terrainNode->mUseMaterialSlots = true;
            terrainNode->mEnableAtlasTexturing = true;
            terrainNode->mBakeSplatmap = true;

            // Save heightmap (flat by default)
            terrainNode->BakeAndSaveHeightmap(basePath + "_Heightmap.oct");

            // Save default splatmap (white — user assigns atlas and rebakes later)
            terrainNode->BakeAndSaveMap(basePath + "_Splatmap.oct");
        }

        spawnedNode = terrainNode;
    }

    if (spawnedNode != nullptr)
    {
        parent = parent ? parent : GetWorld(0)->GetRootNode();
        if (parent != nullptr)
        {
            parent->AddChild(spawnedNode);
        }
        else
        {
            GetWorld(0)->SetRootNode(spawnedNode);
        }

        GetEditorState()->SetSelectedNode(spawnedNode);

        Node3D* node3d = spawnedNode ? spawnedNode->As<Node3D>() : nullptr;

        if (setWorldPos && node3d)
        {
            node3d->SetWorldPosition(worldPos);
        }
    }
    else
    {
        LogError("Failed to spawn basic actor: %s", name.c_str());
    }

    return spawnedNode;
}

void ActionManager::ExecuteAction(Action* action)
{
    OCT_ASSERT(std::find(mActionHistory.begin(), mActionHistory.end(), action) == mActionHistory.end());
    OCT_ASSERT(std::find(mActionFuture.begin(), mActionFuture.end(), action) == mActionFuture.end());

    action->Execute();

    // Limit max number of history?
    const uint32_t MaxActionHistoryCount = 100;
    if (mActionHistory.size() >= MaxActionHistoryCount)
    {
        mActionHistory.erase(mActionHistory.begin());
    }

    mActionHistory.push_back(action);
    ClearActionFuture();
}

void ActionManager::Undo()
{
    if (!IsPlaying() &&
        mActionHistory.size() > 0 &&
        !ImGui::GetIO().WantTextInput)
    {
        Action* action = mActionHistory.back();
        mActionHistory.pop_back();

        LogDebug("Undo %s", action->GetName());
        action->Reverse();

        mActionFuture.push_back(action);

        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->FireOnUndoRedo();
    }
}

void ActionManager::Redo()
{
    if (!IsPlaying() &&
        mActionFuture.size() > 0 &&
        !ImGui::GetIO().WantTextInput)
    {
        Action* action = mActionFuture.back();
        mActionFuture.pop_back();

        LogDebug("Redo %s", action->GetName());
        action->Execute();

        mActionHistory.push_back(action);

        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->FireOnUndoRedo();
    }
}

void ActionManager::EXE_EditProperty(void* owner, PropertyOwnerType ownerType, const std::string& name, uint32_t index, Datum newValue)
{
    ActionEditProperty* action = new ActionEditProperty(owner, ownerType, name, index, newValue);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_EditTransform(Node3D* transComp, const glm::mat4& transform)
{
    std::vector<Node3D*> transComps;
    transComps.push_back(transComp);

    std::vector<glm::mat4> transforms;
    transforms.push_back(transform);

    ActionEditTransforms* action = new ActionEditTransforms(transComps, transforms);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_EditTransforms(const std::vector<Node3D*>& transComps, const std::vector<glm::mat4>& newTransforms)
{
    ActionEditTransforms* action = new ActionEditTransforms(transComps, newTransforms);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_EditWidgetTransforms(const std::vector<Widget*>& widgets, const std::vector<WidgetTransformData>& newTransforms)
{
    ActionEditWidgetTransforms* action = new ActionEditWidgetTransforms(widgets, newTransforms);
    ActionManager::Get()->ExecuteAction(action);
}

Node* ActionManager::EXE_SpawnNode(TypeId srcType)
{
    std::vector<TypeId> srcTypes;
    srcTypes.push_back(srcType);

    GetEditorState()->EnsureActiveScene();

    ActionSpawnNodes* action = new ActionSpawnNodes(srcTypes);
    ActionManager::Get()->ExecuteAction(action);

    OCT_ASSERT(action->GetNodes().size() == 1);
    Node* retNode = action->GetNodes()[0].Get();
    return retNode;
}

Node* ActionManager::EXE_SpawnNode(const char* srcTypeName)
{
    std::vector<const char*> srcTypeNames;
    srcTypeNames.push_back(srcTypeName);

    GetEditorState()->EnsureActiveScene();

    ActionSpawnNodes* action = new ActionSpawnNodes(srcTypeNames);
    ActionManager::Get()->ExecuteAction(action);

    OCT_ASSERT(action->GetNodes().size() == 1);
    Node* retNode = action->GetNodes()[0].Get();
	retNode->SetName(srcTypeName);
    return retNode;
}

Node* ActionManager::EXE_SpawnNode(Scene* srcScene)
{
    std::vector<SceneRef> srcScenes;
    srcScenes.push_back(srcScene);

    GetEditorState()->EnsureActiveScene();

    ActionSpawnNodes* action = new ActionSpawnNodes(srcScenes);
    ActionManager::Get()->ExecuteAction(action);

    OCT_ASSERT(action->GetNodes().size() == 1);
    Node* retNode = action->GetNodes()[0].Get();
    return retNode;
}

Node* ActionManager::EXE_SpawnNode(Node* srcNode)
{
    if (srcNode->IsSceneLinkedChild())
    {
        LogError(SUB_SCENE_HIER_WARN_TEXT);
        return nullptr;
    }

    std::vector<Node*> srcNodes;
    srcNodes.push_back(srcNode);

    GetEditorState()->EnsureActiveScene();

    ActionSpawnNodes* action = new ActionSpawnNodes(srcNodes);
    ActionManager::Get()->ExecuteAction(action);

    OCT_ASSERT(action->GetNodes().size() == 1);
    Node* retNode = action->GetNodes()[0].Get();
    return retNode;
}

void ActionManager::EXE_DeleteNode(Node* node)
{
    if (node->IsSceneLinkedChild())
    {
        LogError(SUB_SCENE_HIER_WARN_TEXT);
        return;
    }

    std::vector<Node*> nodes;
    nodes.push_back(node);

    ActionDeleteNodes* action = new ActionDeleteNodes(nodes);
    ActionManager::Get()->ExecuteAction(action);
}

std::vector<Node*> ActionManager::EXE_SpawnNodes(const std::vector<Node*>& srcNodes)
{
    OCT_ASSERT(srcNodes.size() > 0);
    std::vector<Node*> retNodes;

    std::vector<Node*> trimmedSrcNodes = srcNodes;
    RemoveRedundantDescendants(trimmedSrcNodes);

    for (auto node : trimmedSrcNodes)
    {
        if (node->IsSceneLinkedChild())
        {
            LogError(SUB_SCENE_HIER_WARN_TEXT);
            return retNodes;
        }
    }

    GetEditorState()->EnsureActiveScene();

    ActionSpawnNodes* action = new ActionSpawnNodes(trimmedSrcNodes);
    ActionManager::Get()->ExecuteAction(action);

    OCT_ASSERT(action->GetNodes().size() > 0);

    retNodes.resize(action->GetNodes().size());

    for (uint32_t i = 0; i < retNodes.size(); ++i)
    {
        retNodes[i] = action->GetNodes()[i].Get();
    }

    return retNodes;
}

void ActionManager::EXE_DeleteNodes(const std::vector<Node*>& nodes)
{
    std::vector<Node*> trimmedNodes = nodes;
    RemoveRedundantDescendants(trimmedNodes);

    for (auto node : trimmedNodes)
    {
        if (node == nullptr)
        {
            LogError("Delete Node: Invalid node(s)");
            return;
        }

        if (node->IsSceneLinkedChild())
        {
            LogError(SUB_SCENE_HIER_WARN_TEXT);
            return;
        }
    }

    ActionDeleteNodes* action = new ActionDeleteNodes(trimmedNodes);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_AttachNode(Node* node, Node* newParent, int32_t childIndex, int32_t boneIndex)
{
    if (node->IsSceneLinkedChild() || newParent->IsSceneLinkedChild())
    {
        LogError(SUB_SCENE_HIER_WARN_TEXT);
        return;
    }

    ActionAttachNode* action = new ActionAttachNode(node, newParent, childIndex, boneIndex);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetRootNode(Node* newRoot)
{
    if (newRoot->IsSceneLinkedChild())
    {
        LogError(SUB_SCENE_HIER_WARN_TEXT);
        return;
    }

    ActionSetRootNode* action = new ActionSetRootNode(newRoot);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetWorldRotation(Node3D* comp, glm::quat rot)
{
    ActionSetWorldRotation* action = new ActionSetWorldRotation(comp, rot);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetWorldPosition(Node3D* comp, glm::vec3 pos)
{
    ActionSetWorldPosition* action = new ActionSetWorldPosition(comp, pos);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetWorldScale(Node3D* comp, glm::vec3 scale)
{
    ActionSetWorldScale* action = new ActionSetWorldScale(comp, scale);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_UnlinkScene(Node* node)
{
    if (node->IsSceneLinked())
    {
        ActionUnlinkScene* action = new ActionUnlinkScene(node);
        ActionManager::Get()->ExecuteAction(action);
    }
}

void ActionManager::EXE_ResetScene(Node* node)
{
    if (node->IsSceneLinked() && node->GetScene() != nullptr)
    {
        ActionResetScene* action = new ActionResetScene(node);
        ActionManager::Get()->ExecuteAction(action);
    }
}

void ActionManager::EXE_SetInstanceColors(const std::vector<ActionSetInstanceColorsData>& data)
{
    ActionSetInstanceColors* action = new ActionSetInstanceColors(data);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetVoxels(Voxel3D* voxel, const std::vector<VoxelChange>& changes)
{
    ActionSetVoxels* action = new ActionSetVoxels(voxel, changes);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_SetInstanceData(InstancedMesh3D* instMesh, int32_t startIndex, const std::vector<MeshInstanceData>& data)
{
    ActionSetInstanceData* action = new ActionSetInstanceData(instMesh, startIndex, data);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::ClearActionHistory()
{
    for (uint32_t i = 0; i < mActionHistory.size(); ++i)
    {
        delete mActionHistory[i];
        mActionHistory[i] = nullptr;
    }

    mActionHistory.clear();
}

void ActionManager::ClearActionFuture()
{
    for (uint32_t i = 0; i < mActionFuture.size(); ++i)
    {
        delete mActionFuture[i];
        mActionFuture[i] = nullptr;
    }

    mActionFuture.clear();
}

void ActionManager::ResetUndoRedo()
{
    ClearActionHistory();
    ClearActionFuture();

    for (int32_t i = (int32_t)mExiledNodes.size() - 1; i >= 0; --i)
    {
        mExiledNodes[i]->Destroy();
        mExiledNodes.erase(mExiledNodes.begin() + i);
    }

    // Clear property inspection history also.
    GetEditorState()->ClearInspectHistory();
}

void ActionManager::ExileNode(NodePtr node)
{
    OCT_ASSERT(std::find(mExiledNodes.begin(), mExiledNodes.end(), node) == mExiledNodes.end());
    OCT_ASSERT(node->GetParent() == nullptr);

    mExiledNodes.push_back(node);

    if (GetEditorState()->IsNodeSelected(node.Get()))
    {
        GetEditorState()->SetSelectedNode(nullptr);
    }

    if (GetEditorState()->GetInspectedObject() == node.Get())
    {
        GetEditorState()->InspectObject(nullptr, true);
    }
}

void ActionManager::RestoreExiledNode(NodePtr node)
{
    bool restored = false;

    for (uint32_t i = 0; i < mExiledNodes.size(); ++i)
    {
        if (mExiledNodes[i] == node)
        {
            mExiledNodes.erase(mExiledNodes.begin() + i);
            restored = true;
            break;
        }
    }

    OCT_ASSERT(restored);
}

void CpyFile(const std::string& srcFile, const std::string& dstFile)
{
	SYS_CopyFile(srcFile.c_str(), dstFile.c_str());
    //SYS_Exec((std::string("cp ") + srcFile + " " + dstFile).c_str());
}

void CpyDir(const std::string& srcFile, const std::string& dstFile)
{
	SYS_CopyDirectory(srcFile.c_str(), dstFile.c_str());
    //SYS_Exec((std::string("cp -r ") + srcFile + " " + dstFile).c_str());
}

void CopyFileAndReplaceString(const std::string& srcFile, const std::string& dstFile, const std::string& srcString, const std::string& dstString)
{
    CpyFile(srcFile, dstFile);
    ReplaceStringInFile(dstFile, srcString, dstString);
}

void CpyDirWithExclusions(const std::string& srcDir, const std::string& dstDir, const std::vector<std::string> exclusions)
{
    DirEntry dirEntry;
    std::string fullPath = SYS_GetAbsolutePath(srcDir);
    SYS_OpenDirectory(fullPath, dirEntry);

    // Create dir if it doesn't exist.
    CreateDir(dstDir.c_str());

    std::vector<std::string> subDirs;

    while (dirEntry.mValid)
    {
        std::string fileName = dirEntry.mFilename;
        if (std::find(exclusions.begin(), exclusions.end(), fileName) == exclusions.end())
        {
            if (dirEntry.mDirectory)
            {
                if (fileName != "." && fileName != "..")
                {
                    subDirs.push_back(fileName);
                }
            }
            else
            {
                CpyFile(srcDir + fileName, dstDir + fileName);
            }
        }

        SYS_IterateDirectory(dirEntry);
    }

    SYS_CloseDirectory(dirEntry);

    for (uint32_t i = 0; i < subDirs.size(); ++i)
    {
        CpyDirWithExclusions(srcDir + subDirs[i] + "/", dstDir + subDirs[i] + "/", exclusions);
    }
}

static void CreateConfigIni(const std::string& projDir, const std::string projName)
{
    // Write out an Engine.ini file which is used by Standalone game exe.
    FILE* engineIni = fopen(std::string(projDir + "Config.ini").c_str(), "w");
    if (engineIni != nullptr)
    {
        fprintf(engineIni, "project=%s", projName.c_str());

        fclose(engineIni);
        engineIni = nullptr;
    }
}

static void CreateAndSaveDefaultScene(const std::string& scenePath, const std::string& sceneName = "SC_Default")
{
    // Create a root Node3D using proper construction
    NodePtr root = Node::Construct(Node3D::GetStaticType());
    root->SetName((sceneName +"_Root").c_str());

    NodePtr camera = Node::Construct(Camera3D::GetStaticType());
    camera->SetName("Camera");
    camera->As<Camera3D>()->SetPosition(glm::vec3(0.0f, 0.0f, 10.0f));
    root->AddChild(camera.Get());

    Scene scene;
    scene.Create();
    scene.SetName(sceneName);
    scene.Capture(root.Get());

    scene.SaveFile(scenePath.c_str(), Platform::Count);

}

void ActionManager::CreateNewProject(const char* folderPath, bool cpp, const char* defaultSceneName)
{
    std::string folderPathStr = folderPath ? folderPath : "";

    if (folderPathStr == "")
    {
        folderPathStr = SYS_SelectFolderDialog();
    }

    if (folderPathStr != "")
    {
        std::string newProjDir = folderPathStr;
        std::replace(newProjDir.begin(), newProjDir.end(), '\\', '/');

        // Remove trailing slash
        if (newProjDir.size() > 0 && newProjDir.back() == '/')
        {
            newProjDir.pop_back();
        }

        std::string newProjName = newProjDir;
        size_t slashLoc = newProjName.find_last_of('/');
        if (slashLoc != std::string::npos)
        {
            newProjName = newProjName.substr(slashLoc + 1);
        }

        LogDebug("CreateNewProject: %s @ %s", newProjName.c_str(), newProjDir.c_str());
        {
            if (!DoesDirExist(newProjDir.c_str()))
            {
                LogDebug("The specified folder was not found, it will now be created @ %s",newProjDir.c_str());
                SYS_CreateDirectory(newProjDir.c_str());
            }
        }

        newProjDir += "/";

        // Now that we have the folder, we need to populate it with an Assets and Scripts folder
        std::string assetsFolder = newProjDir + "Assets";
        std::string scriptsFolder = newProjDir + "Scripts";
        SYS_CreateDirectory(assetsFolder.c_str());
        SYS_CreateDirectory(scriptsFolder.c_str());

        // Also we need to create an octp so that user can open the project with Ctrl+P
        std::string projectFile = newProjDir + newProjName.c_str() + ".octp";
        FILE* octpFile = fopen(projectFile.c_str(), "w");
        if (octpFile != nullptr)
        {
            fprintf(octpFile, "name=%s", newProjName.c_str());

            fclose(octpFile);
            octpFile = nullptr;
        }
		std::string subProjFolder = ""; 
        std::string polyphaseDirectory = SYS_GetPolyphasePath();
      
        if (cpp)
        {
            std::string standaloneDir = polyphaseDirectory+ "Standalone/";

            subProjFolder = newProjDir + newProjName.c_str();
            SYS_CreateDirectory(subProjFolder.c_str());
            subProjFolder += "/";
			projectFile = subProjFolder + newProjName.c_str() + ".octp";

            // Copy .octp folder into the subfolder
			SYS_MoveFile((newProjDir + newProjName.c_str() + ".octp").c_str(), (subProjFolder + newProjName + ".octp").c_str());
            //SYS_Exec(("mv " + newProjDir + newProjName.c_str() + ".octp" + " " + subProjFolder + newProjName.c_str() + ".octp").c_str());

            // Create Proj subfolder (Assets and Scripts folders will need to be moved in the subfolder)
			SYS_MoveDirectory(assetsFolder.c_str(), (subProjFolder + "Assets").c_str());
            //SYS_Exec((std::string("mv ") + assetsFolder.c_str() + " " + subProjFolder + "Assets").c_str());
			SYS_MoveDirectory(scriptsFolder.c_str(), (subProjFolder + "Scripts").c_str());
            //SYS_Exec((std::string("mv ") + scriptsFolder.c_str() + " " + subProjFolder + "Scripts").c_str());
            assetsFolder = subProjFolder + "Assets";
            scriptsFolder = subProjFolder + "Scripts";

            // Copy Makefiles and replace "Polyphase"
            CopyFileAndReplaceString(standaloneDir + "Makefile_3DS", subProjFolder + "Makefile_3DS", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Makefile_3DS", "../Engine", "../Polyphase/Engine");
            ReplaceStringInFile(subProjFolder + "Makefile_3DS", "../External", "../Polyphase/External");
            CopyFileAndReplaceString(standaloneDir + "Makefile_GCN", subProjFolder + "Makefile_GCN", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Makefile_GCN", "../Engine", "../Polyphase/Engine");
            ReplaceStringInFile(subProjFolder + "Makefile_GCN", "../External", "../Polyphase/External");
            CopyFileAndReplaceString(standaloneDir + "Makefile_Wii", subProjFolder + "Makefile_Wii", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Makefile_Wii", "../Engine", "../Polyphase/Engine");
            ReplaceStringInFile(subProjFolder + "Makefile_Wii", "../External", "../Polyphase/External");
            CopyFileAndReplaceString(standaloneDir + "Makefile_Linux_Game", subProjFolder + "Makefile_Linux_Game", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Makefile_Linux_Game", "../Engine", "../Polyphase/Engine");
            ReplaceStringInFile(subProjFolder + "Makefile_Linux_Game", "../External", "../Polyphase/External");
            CopyFileAndReplaceString(standaloneDir + "Makefile_Linux_Editor", subProjFolder + "Makefile_Linux_Editor", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Makefile_Linux_Editor", "../Engine", "../Polyphase/Engine");
            ReplaceStringInFile(subProjFolder + "Makefile_Linux_Editor", "../External", "../Polyphase/External");

            // Copy and replace Standalone.vcxproj/filters and replace "Standalone" 
            CopyFileAndReplaceString(standaloneDir + "Standalone.vcxproj", subProjFolder + newProjName + ".vcxproj", "Standlone", newProjName);
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "../", "../Polyphase/");
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "$(SolutionDir)Engine", "$(SolutionDir)Polyphase\\Engine");
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "$(SolutionDir)External", "$(SolutionDir)Polyphase\\External");
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "..\\Engine\\", "..\\Polyphase\\Engine\\");
            ReplaceStringInFile(subProjFolder + newProjName + ".vcxproj", "Standalone.rc", "$(SolutionDir)Polyphase\\Standalone\\Standalone.rc");
            CpyFile(standaloneDir + "Standalone.vcxproj.filters", subProjFolder + newProjName + ".vcxproj.filters");

            // Copy Source folder
            CpyDir(standaloneDir + "Source", subProjFolder + "Source");
            ReplaceStringInFile(subProjFolder + "Source/Main.cpp", "Polyphase", newProjName);
            ReplaceStringInFile(subProjFolder + "Source/Main.cpp", "config.mStandalone = true", "config.mStandalone = false");

            // Copy android files (whatevers in git, exclude .cxx and build)
            CpyDirWithExclusions(standaloneDir + "Android/", subProjFolder + "Android/", { "Build", "build", "Intermediate", ".gradle", ".cxx", ".vs", ".git", "imgui.ini", "assets"});

#if 0
            // Create a symlink to the Polyphase directory
            CreateSymLink(polyphaseDirectory, newProjDir + "Polyphase");
#else
       
            // Copy Engine, External, to Proj folder (EXCLUDE Build and Intermediate folders)
            CpyDirWithExclusions(polyphaseDirectory, newProjDir + "Polyphase/", {"Build", "build", "Intermediate", ".gradle", ".cxx", ".vs", ".git", "imgui.ini"});
#endif

            // Copy Polyphase.sln  - Replace "Standalone" with Proj Name
            CpyFile(standaloneDir + "../Polyphase.sln", newProjDir + newProjName + ".sln");
            ReplaceStringInFile(newProjDir + newProjName + ".sln", "Engine\\Engine.vcxproj", "Polyphase\\Engine\\Engine.vcxproj");
            ReplaceStringInFile(newProjDir + newProjName + ".sln", "External\\", "Polyphase\\External\\");
            ReplaceStringInFile(newProjDir + newProjName + ".sln", "Standalone", newProjName);
            ReplaceStringInFile(newProjDir + newProjName + ".sln", "\"External\", \"External\",", "\"External\", \"Polyphase\\External\",");

            // Copy .vscode folder from Polyphase to SubProjDir
            CpyDir(standaloneDir + "../.vscode", newProjDir + ".vscode");
            ReplaceStringInFile(newProjDir + ".vscode/tasks.json", "Standalone", newProjName);
            ReplaceStringInFile(newProjDir + ".vscode/launch.json", "Standalone", newProjName);
            ReplaceStringInFile(newProjDir + ".vscode/launch.json", "\"name\": \"Polyphase" , "\"name\": \"" + newProjName);

        }

        ResetEngineConfig();
        GetMutableEngineConfig()->mProjectName = newProjName;

        if (cpp) {
            // correct path with subfolder if cpp
            WriteEngineConfig(subProjFolder + "/Config.ini");

        }
        else {
            // correct path with subfolder if cpp
            WriteEngineConfig(newProjDir + +"/Config.ini");
        }

        // Create the Scenes folder and default scene for the new project
        std::string scenesFolder = assetsFolder + "/Scenes/";
        SYS_CreateDirectory(scenesFolder.c_str());
        CreateAndSaveDefaultScene(scenesFolder + std::string(defaultSceneName) + ".oct", defaultSceneName);

        // Finally, open the project
        OpenProject(projectFile.c_str());
    }
}

static std::string ResolveProjectPath(const std::string& inputPath)
{
    if (inputPath.empty())
        return "";

    std::string path = inputPath;
    std::replace(path.begin(), path.end(), '\\', '/');

    // Remove trailing slash
    if (!path.empty() && path.back() == '/')
        path.pop_back();

    std::string directory;
    std::string octpPath;

    if (DoesDirExist(path.c_str()))
    {
        // Input is a directory — derive {dirName}.octp
        directory = path + "/";
        std::string dirName = path;
        size_t slashPos = dirName.find_last_of('/');
        if (slashPos != std::string::npos)
            dirName = dirName.substr(slashPos + 1);

        octpPath = directory + dirName + ".octp";
    }
    else
    {
        // Treat as a file path — extract directory
        size_t dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos && path.substr(dotPos) == ".octp")
        {
            octpPath = path;
            size_t slashPos = path.find_last_of('/');
            directory = (slashPos != std::string::npos) ? path.substr(0, slashPos + 1) : "";
        }
        else
        {
            return "";
        }
    }

    // If .octp already exists, return it
    if (SYS_DoesFileExist(octpPath.c_str(), false))
        return octpPath;

    // .octp missing — check if directory looks like a project
    if (directory.empty())
        return "";

    bool hasAssets = DoesDirExist((directory + "Assets").c_str());
    bool hasScripts = DoesDirExist((directory + "Scripts").c_str());

    if (!hasAssets && !hasScripts)
    {
        LogWarning("Directory does not appear to be a project (no Assets/ or Scripts/): %s", directory.c_str());
        return "";
    }

    // Derive project name from directory
    std::string dirName = directory;
    if (!dirName.empty() && dirName.back() == '/')
        dirName.pop_back();
    size_t slashPos = dirName.find_last_of('/');
    if (slashPos != std::string::npos)
        dirName = dirName.substr(slashPos + 1);

    octpPath = directory + dirName + ".octp";

    // Auto-create the .octp
    LogDebug("Auto-creating missing project file: %s", octpPath.c_str());
    FILE* f = fopen(octpPath.c_str(), "w");
    if (f != nullptr)
    {
        fprintf(f, "name=%s", dirName.c_str());
        fclose(f);
    }
    else
    {
        LogError("Failed to create project file: %s", octpPath.c_str());
        return "";
    }

    // Auto-create Config.ini if missing
    std::string configPath = directory + "Config.ini";
    if (!SYS_DoesFileExist(configPath.c_str(), false))
    {
        LogDebug("Auto-creating missing Config.ini: %s", configPath.c_str());
        FILE* configFile = fopen(configPath.c_str(), "w");
        if (configFile != nullptr)
        {
            fprintf(configFile, "Project=%s\n", dirName.c_str());
            fclose(configFile);
        }
    }

    return octpPath;
}

void ActionManager::OpenProject(const char* path)
{
    std::string pathStr = path ? path : "";

    if (pathStr.empty())
    {
        pathStr = SYS_SelectFolderDialog();
    }

    if (!pathStr.empty())
    {
        pathStr = ResolveProjectPath(pathStr);
    }

    if (!pathStr.empty())
    {
        // Save and destroy previous PackagingSettings before loading new project
        if (PackagingSettings::Get() != nullptr)
        {
            PackagingSettings::Get()->SaveSettings();
            PackagingSettings::Destroy();
        }

        LoadProject(pathStr);

        // Handle new project directory
        GetEditorState()->ClearAssetDirHistory();
        GetEditorState()->SetAssetDirectory(AssetManager::Get()->FindProjectDirectory(), true);
        GetEditorState()->SetSelectedAssetStub(nullptr);
        GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Addons] = AssetManager::Get()->FindPackagesDirectory();

        // Initialize PackagingSettings for the new project
        PackagingSettings::Create();
        PackagingSettings::Get()->LoadSettings();

        // Load player input actions for the new project
        if (PlayerInputSystem::Get() != nullptr)
        {
            PlayerInputSystem::Get()->LoadProjectActions();
        }

        // Check if project needs upgrade to new UUID format
        CheckProjectNeedsUpgrade();
        // Note: native addons are loaded inside LoadProject() before asset discovery so that
        // addon-provided node types (registered via DEFINE_NODE static initializers) are
        // available when scenes deserialize. Calling ReloadAllNativeAddons again here would
        // re-enter LoadLibrary on the addon DLL, which re-runs its static initializers -
        // those hit a duplicate-class-name assert in Node::RegisterFactory because
        // UnloadNativeAddon doesn't currently remove the DLL's factory pointers from the
        // global factory list. Proper fix belongs in UnloadNativeAddon (strip addon factories
        // before FreeLibrary) but is out of scope here.

        // Write project-local config files that record this editor's
        // engine path so addons inside <project>/Packages/ can build
        // standalone (Visual Studio / cmake) without requiring the user to
        // set POLYPHASE_PATH. MSBuild auto-imports Directory.Build.props
        // walking up from any .vcxproj, so the addon's vcxproj inherits
        // <PolyphasePath> with no edits to the addon. CMake addons include
        // PolyphaseConfig.cmake explicitly via the walk-up search.
        WriteProjectLocalPolyphaseConfig();

        // Fire OnProjectOpen hook
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr)
        {
            hookMgr->FireOnProjectOpen(pathStr.c_str());
        }
    }
}

// Drop a Directory.Build.props (auto-imported by MSBuild) and a
// PolyphaseConfig.cmake (explicitly included by addon CMakeLists) at the
// project root, both setting the PolyphasePath/POLYPHASE_PATH variable to
// the engine path the editor itself is running from. Lets addons in
// <project>/Packages/ link without env vars or hand-edited paths.
void ActionManager::WriteProjectLocalPolyphaseConfig()
{
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    if (projectDir.empty())
        return;

    std::string polyphasePath = SYS_GetPolyphasePath();
    if (polyphasePath.empty())
        return;

    // Strip trailing slashes for cleanliness in both formats.
    while (!polyphasePath.empty() &&
           (polyphasePath.back() == '/' || polyphasePath.back() == '\\'))
    {
        polyphasePath.pop_back();
    }

    // Write Directory.Build.props with backslashes (MSBuild convention).
    //
    // The Exists() guard on the path lets this file be committed to source
    // control safely: if a teammate clones the project on a machine where
    // this exact engine path doesn't exist, the rule no-ops and the
    // addon's own UserMacros chain (env var, install probes, ...) takes
    // over instead of pinning PolyphasePath to a dead path.
    {
        std::string winPath;
        winPath.reserve(polyphasePath.size());
        for (char c : polyphasePath)
            winPath.push_back(c == '/' ? '\\' : c);

        std::stringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        xml << "<!-- Auto-generated by Polyphase editor on project open.\n";
        xml << "     Sets PolyphasePath for any .vcxproj under this folder so\n";
        xml << "     native addons in Packages/ build without per-machine setup.\n";
        xml << "     Safe to commit: the path is only applied when it exists on\n";
        xml << "     the current machine, so teammates with a different engine\n";
        xml << "     location fall through to their own env-var/install probes.\n";
        xml << "     The editor rewrites this file each time the project is\n";
        xml << "     opened. -->\n";
        xml << "<Project>\n";
        xml << "  <PropertyGroup>\n";
        xml << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('"
            << winPath << "\\Polyphase.sln')\">"
            << winPath << "</PolyphasePath>\n";
        // Also accept an installed-editor layout (Polyphase.exe at the root)
        // since installs don't ship Polyphase.sln.
        xml << "    <PolyphasePath Condition=\"'$(PolyphasePath)' == '' and Exists('"
            << winPath << "\\Polyphase.exe')\">"
            << winPath << "</PolyphasePath>\n";
        xml << "  </PropertyGroup>\n";
        xml << "</Project>\n";

        std::string content = xml.str();
        std::string outPath = projectDir + "Directory.Build.props";
        Stream stream(content.c_str(), (uint32_t)content.size());
        stream.WriteFile(outPath.c_str());
    }

    // Write PolyphaseConfig.cmake with forward slashes.
    //
    // Same Exists() guard rationale as Directory.Build.props: don't claim
    // a path that doesn't exist on this machine.
    {
        std::string fwdPath;
        fwdPath.reserve(polyphasePath.size());
        for (char c : polyphasePath)
            fwdPath.push_back(c == '\\' ? '/' : c);

        std::stringstream cm;
        cm << "# Auto-generated by Polyphase editor on project open.\n";
        cm << "# Recorded engine path for addons in this project. Addon\n";
        cm << "# CMakeLists files walk up looking for this file and include\n";
        cm << "# it before their own POLYPHASE_PATH probe chain runs.\n";
        cm << "#\n";
        cm << "# Safe to commit: if the recorded path doesn't exist on the\n";
        cm << "# current machine (teammate with a different engine layout),\n";
        cm << "# this no-ops and the addon's own probe chain takes over.\n";
        cm << "if(NOT DEFINED POLYPHASE_PATH OR POLYPHASE_PATH STREQUAL \"\")\n";
        cm << "    if(EXISTS \"" << fwdPath << "/Polyphase.sln\""
           << " OR EXISTS \"" << fwdPath << "/Polyphase.exe\")\n";
        cm << "        set(POLYPHASE_PATH \"" << fwdPath << "\")\n";
        cm << "    endif()\n";
        cm << "endif()\n";

        std::string content = cm.str();
        std::string outPath = projectDir + "PolyphaseConfig.cmake";
        Stream stream(content.c_str(), (uint32_t)content.size());
        stream.WriteFile(outPath.c_str());
    }
}

void ActionManager::OpenScene()
{
    // TODO-NODE: This uses SYS_OpenFileDialog() and I think we should just get rid of this function.
    if (GetEngineState()->mProjectPath == "")
        return;

    std::vector<std::string> openPaths = SYS_OpenFileDialog();

    // Display the Open dialog box. 
    if (openPaths.size() > 0)
    {
        std::string filename = strrchr(openPaths[0].c_str(), '/') + 1;
        filename = filename.substr(0, filename.find_last_of('.'));
        AssetStub* stub = FetchAssetStub(filename);

        if (stub != nullptr &&
            stub->mType == Scene::GetStaticType())
        {
            AssetManager::Get()->LoadAsset(*stub);
            Scene* loadedScene = (Scene*)stub->mAsset;

            OpenScene(loadedScene);
        }
        else
        {
            LogError("Failed to fetch Level from AssetManager");
        }
    }
}

void ActionManager::OpenScene(Scene* scene)
{
    GetEditorState()->OpenEditScene(scene);
}

Scene* ActionManager::CreateNewScene(const char* sceneName, int sceneType, bool createCamera, bool createSkybox, AssetDir* targetDir)
{
    if (sceneName == nullptr || sceneName[0] == '\0')
        return nullptr;

    if (targetDir == nullptr)
        targetDir = GetEditorState()->GetAssetDirectory();

    if (targetDir == nullptr)
        return nullptr;

    // Plugin scene types (sceneType >= 2) are not supported here — those go through
    // the EditorImgui menu path which has access to plugin function pointers.
    if (sceneType != 0 && sceneType != 1)
        return nullptr;

    AssetStub* stub = EditorAddUniqueAsset(sceneName, targetDir, Scene::GetStaticType(), true);
    if (stub == nullptr || stub->mAsset == nullptr)
        return nullptr;

    Scene* scene = (Scene*)stub->mAsset;

    if (sceneType == 1) // 3D
    {
        SharedPtr<Node3D> root = Node::Construct<Node3D>();
        root->SetName(std::string(sceneName) + "_Root");

        if (createCamera)
        {
            Camera3D* cam = root->CreateChild<Camera3D>("Camera3D");
            cam->SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        }

        if (createSkybox)
        {
            StaticMesh3D* skyNode = root->CreateChild<StaticMesh3D>("Skybox");
            ConfigureSkyboxNode(skyNode);
        }

        scene->Capture(root.Get());
    }
    else // 2D
    {
        SharedPtr<Canvas> root = Node::Construct<Canvas>();
        root->SetName(std::string(sceneName) + "_Root");

        // Match the EditorImgui "New Scene" popup: size the Canvas to the current
        // Build Profile's platform resolution, with a 640x480 fallback.

        Quad* bgQuad = root->CreateChild<Quad>("Background");
        bgQuad->SetFullScreen();
        bgQuad->Centered();

        scene->Capture(root.Get());

    }

    AssetManager::Get()->SaveAsset(*stub);
    return scene;
}

void ActionManager::SaveScene(bool saveAs)
{
    EditScene* editScene = GetEditorState()->GetEditScene();

    if (GetEngineState()->mProjectPath == "" ||
        editScene == nullptr)
        return;

    if (saveAs || editScene->mSceneAsset == nullptr)
    {
        GetEditorState()->mRequestSaveSceneAs = true;

#if 0
        // Old method of bringing up OS file browser
        std::string savePath = SYS_SaveFileDialog();

        // Display the Open dialog box. 
        if (savePath != "")
        {
            std::replace(savePath.begin(), savePath.end(), '\\', '/');
            std::string newSceneName = Asset::GetNameFromPath(savePath);
            AssetStub* stub = AssetManager::Get()->CreateAndRegisterAsset(Scene::GetStaticType(), nullptr, newSceneName, false);

            if (stub != nullptr)
            {
                Scene* newScene = (Scene*)stub->mAsset;
                newScene->SetName(newSceneName);
                editScene->mSceneAsset = newScene;
            }
        }
#endif
    }
    else if (editScene->mSceneAsset != nullptr)
    {
        Scene* scene = editScene->mSceneAsset.Get<Scene>();
        Node* root = GetWorld(0)->GetRootNode();
        scene->Capture(root);
        root->SetScene(scene);
        AssetManager::Get()->SaveAsset(scene->GetName());

        // Fire OnProjectSave and OnAssetSaved hooks
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr)
        {
            hookMgr->FireOnProjectSave(scene->GetName().c_str());
            hookMgr->FireOnAssetSaved(scene->GetName().c_str());
        }
    }
}

void ActionManager::SaveSelectedAsset()
{
    AssetStub* selectedStub = GetEditorState()->GetSelectedAssetStub();
    if (selectedStub != nullptr &&
        selectedStub->mAsset != nullptr)
    {
        AssetManager::Get()->SaveAsset(*selectedStub);
    }
}

void ActionManager::DeleteSelectedNodes()
{
    std::vector<Node*> nodes = GetEditorState()->GetSelectedNodes();

    for (uint32_t i = 0; i < nodes.size(); ++i)
    {
        // TODO-NODE: Previously we checked here if the node was the World's active camera.
        //   Now that we use a totally separate editor camera in EditorState, I removed the check.
        //   Verify editor cam works as expected.
        if (nodes[i] == nullptr)
        {
            nodes.erase(nodes.begin() + i);
            --i;
        }
    }

    EXE_DeleteNodes(nodes);
    GetEditorState()->SetSelectedNode(nullptr);
}

void ActionManager::DeleteNode(Node* node)
{
    if (node != nullptr &&
        node != GetEditorState()->GetEditorCamera())
    {
        EXE_DeleteNode(node);
    }
}

static void HandleRunScriptCallback(const std::vector<std::string>& filePaths)
{
    lua_State* L = GetLua();

    for (uint32_t i = 0; i < filePaths.size(); ++i)
    {
        // Don't use ScriptUtils::RunScript(). Allow script to exist outside of the project.
        if (luaL_dofile(L, filePaths[i].c_str()))
        {
            LogError("3Lua Error: %s\n", lua_tostring(L, -1));
        }
    }
}

void ActionManager::RunScript()
{
    std::vector<std::string> filePaths = SYS_OpenFileDialog();
    HandleRunScriptCallback(filePaths);
}

static void HandleImportCallback(const std::vector<std::string>& filePaths)
{
    for (uint32_t i = 0; i < filePaths.size(); ++i)
    {
        if (filePaths[i] != "")
        {
            ActionManager::Get()->ImportAsset(filePaths[i].c_str());
        }
        else
        {
            LogError("Bad file for ImportAsset.");
        }
    }
}

static void HandleImportSceneCallback(const std::vector<std::string>& filePaths)
{
    if (filePaths.size() > 1)
    {
        LogWarning("Can only import one scene at a time. Ignoring extra scenes.");
    }

    if (filePaths.size() >= 1)
    {
        GetEditorState()->mPendingSceneImportPath = filePaths[0];
    }
}


static void HandleImportCameraCallback(const std::vector<std::string>& filePaths)
{
    if (filePaths.size() > 1)
    {
        LogWarning("Can only import one scene at a time. Ignoring extra scenes.");
    }

    if (filePaths.size() >= 1)
    {
        GetEditorState()->mIOAssetPath = filePaths[0];
    }
}

void ActionManager::ImportAsset()
{
    if (GetEngineState()->mProjectPath != "")
    {
        std::vector<std::string> filePaths = SYS_OpenFileDialog();
        HandleImportCallback(filePaths);
    }
    else
    {
        LogWarning("Cannot import asset. No project loaded.");
    }
}

Asset* ActionManager::ImportAsset(const std::string& path)
{
    Asset* retAsset = nullptr;

    std::string filename = path;
    size_t lastSlashIdx = filename.find_last_of("/\\");
    std::vector<TypeId> importTypes;
    std::vector<int32_t> meshIndices;

    if (lastSlashIdx != std::string::npos)
    {
        filename = filename.substr(lastSlashIdx + 1);
    }

    int32_t dotIndex = int32_t(filename.find_last_of('.'));
    std::string extension = filename.substr(dotIndex, filename.size() - dotIndex);

    if (extension == ".png" ||
        extension == ".bmp" ||
        extension == ".jpeg" ||
        extension == ".jpg" ||
        extension == ".tga")
    {
        importTypes.push_back(Texture::GetStaticType());
    }
    else if (extension == ".dae" ||
        extension == ".fbx" ||
        extension == ".glb" ||
        extension == ".gltf" ||
        extension == ".obj")
    {
        GatherMeshImportTypes(path.c_str(), importTypes, meshIndices);
    }
    else if (extension == ".wav")
    {
        importTypes.push_back(SoundWave::GetStaticType());
    }
    else if (extension == ".ttf" || extension == ".xml")
    {
        importTypes.push_back(Font::GetStaticType());
    }
    else
    {
        // Addon-registered extension dispatch (e.g. ".mp4" -> VideoClip from the
        // video addon). Registered via AssetManager::RegisterImportExtension in
        // each addon's plugin OnLoad.
        TypeId addonType = LookupImportExtension(extension);
        if (addonType != INVALID_TYPE_ID)
        {
            importTypes.push_back(addonType);
        }
    }

    if (importTypes.size() == 0)
    {
        LogError("Failed to import Asset. Unrecognized source asset extension.");
    }

    for (uint32_t i = 0; i < importTypes.size(); ++i)
    {
        Asset* newAsset = nullptr;
        bool success = false;
        TypeId typeId = importTypes[i];
        ImportOptions options;

        if (typeId == StaticMesh::GetStaticType() ||
            typeId == SkeletalMesh::GetStaticType())
        {
            OCT_ASSERT(meshIndices.size() == importTypes.size());
            if (importTypes.size() > 1)
            {
                options.SetOptionValue("meshIndex", meshIndices[i]);
            }
        }

        newAsset = Asset::CreateInstance(typeId);

        std::string assetName = filename.substr(0, dotIndex);
        newAsset->SetName(assetName);

        success = newAsset->Import(path, &options);

        if (success)
        {
            // Update the asset name in case it was changed by the Import() function.
            // This happens when importing multiple meshes within the same file
            assetName = newAsset->GetName();

            AssetDir* assetDir = GetEditorState()->GetAssetDirectory();
            filename = assetName + ".oct";

            // Clear inspected asset if we are reimporting that same asset.
            Asset* oldAsset = FetchAsset(assetName.c_str());

            if (oldAsset != nullptr)
            {
                if (GetEditorState()->GetInspectedObject() == oldAsset)
                {
                    GetEditorState()->InspectObject(nullptr, true);
                }
            }

#if ASSET_LIVE_REF_TRACKING
            // If this asset already exists, then we are about to delete it and replace it.
            // So let's fix up any references now or else they will be lost (replaced with nullptr).
            if (oldAsset != nullptr)
            {
                AssetRef::ReplaceReferencesToAsset(oldAsset, newAsset);
            }
#endif

            // If asset already exists, overwrite it. So delete existing asset.
            bool purged = AssetManager::Get()->PurgeAsset(assetName.c_str());
            if (purged)
            {
                LogWarning("Reimporting asset");
            }

            AssetStub* stub = AssetManager::Get()->RegisterAsset(filename, newAsset->GetType(), assetDir, nullptr, false);
            stub->mAsset = newAsset;
            newAsset->SetName(stub->mName);

            // If a StaticMesh/SkeletalMesh is being imported, and there is a selected material, then assign
            // the material to that static mesh.
            if (newAsset != nullptr &&
                (newAsset->Is(StaticMesh::ClassRuntimeId()) || newAsset->Is(SkeletalMesh::ClassRuntimeId())) &&
                GetEditorState()->GetSelectedAsset() != nullptr &&
                GetEditorState()->GetSelectedAsset()->Is(Material::ClassRuntimeId()))
            {
                Material* material = GetEditorState()->GetSelectedAsset()->As<Material>();

                if (newAsset->Is(StaticMesh::ClassRuntimeId()))
                {
                    StaticMesh* mesh = newAsset->As<StaticMesh>();
                    mesh->SetMaterial(material);
                }
                else if (newAsset->Is(SkeletalMesh::ClassRuntimeId()))
                {
                    SkeletalMesh* mesh = newAsset->As<SkeletalMesh>();
                    mesh->SetMaterial(material);
                }
            }

            AssetManager::Get()->SaveAsset(*stub);

            retAsset = newAsset;

            // Fire OnAssetImported hook
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->FireOnAssetImported(stub->mName.c_str());
        }
        else
        {
            LogError("Failed to import asset.");

            delete newAsset;
            newAsset = nullptr;
        }
    }

    return retAsset;
}

void ActionManager::ImportTinyLLMModel()
{
    if (GetEngineState()->mProjectPath == "")
    {
        LogWarning("Cannot import asset. No project loaded.");
        return;
    }

    std::vector<std::string> filePaths = SYS_OpenFileDialog();

    for (const std::string& path : filePaths)
    {
        // Get filename without path
        std::string filename = path;
        size_t lastSlashIdx = filename.find_last_of("/\\");
        if (lastSlashIdx != std::string::npos)
        {
            filename = filename.substr(lastSlashIdx + 1);
        }

        int32_t dotIndex = int32_t(filename.find_last_of('.'));
        std::string assetName = filename.substr(0, dotIndex);

        // Create TinyLLMAsset
        TinyLLMAsset* newAsset = new TinyLLMAsset();
        newAsset->SetName(assetName);

        bool success = newAsset->Import(path, nullptr);

        if (success)
        {
            AssetDir* assetDir = GetEditorState()->GetAssetDirectory();
            std::string octFilename = assetName + ".oct";

            AssetStub* stub = AssetManager::Get()->RegisterAsset(octFilename, newAsset->GetType(), assetDir, nullptr, false);
            stub->mAsset = newAsset;
            newAsset->SetName(stub->mName);
            AssetManager::Get()->SaveAsset(*stub);

            LogDebug("Imported TinyLLM model: %s", assetName.c_str());
        }
        else
        {
            LogError("Failed to import TinyLLM model.");
            delete newAsset;
        }
    }
}

void ActionManager::ImportTinyLLMTokenizer()
{
    if (GetEngineState()->mProjectPath == "")
    {
        LogWarning("Cannot import asset. No project loaded.");
        return;
    }

    std::vector<std::string> filePaths = SYS_OpenFileDialog();

    for (const std::string& path : filePaths)
    {
        // Get filename without path
        std::string filename = path;
        size_t lastSlashIdx = filename.find_last_of("/\\");
        if (lastSlashIdx != std::string::npos)
        {
            filename = filename.substr(lastSlashIdx + 1);
        }

        int32_t dotIndex = int32_t(filename.find_last_of('.'));
        std::string assetName = filename.substr(0, dotIndex);

        // Create TinyLLMTokenizerAsset
        TinyLLMTokenizerAsset* newAsset = new TinyLLMTokenizerAsset();
        newAsset->SetName(assetName);

        bool success = newAsset->Import(path, nullptr);

        if (success)
        {
            AssetDir* assetDir = GetEditorState()->GetAssetDirectory();
            std::string octFilename = assetName + ".oct";

            AssetStub* stub = AssetManager::Get()->RegisterAsset(octFilename, newAsset->GetType(), assetDir, nullptr, false);
            stub->mAsset = newAsset;
            newAsset->SetName(stub->mName);
            AssetManager::Get()->SaveAsset(*stub);

            LogDebug("Imported TinyLLM tokenizer: %s", assetName.c_str());
        }
        else
        {
            LogError("Failed to import TinyLLM tokenizer.");
            delete newAsset;
        }
    }
}

static std::string GetFixedFilename(const char* name, const char* prefix)
{
    std::string nameStr = name;
    size_t extPeriod = nameStr.find_last_of('.');
    if (extPeriod != std::string::npos)
    {
        nameStr = nameStr.substr(0, extPeriod);
    }

    if (nameStr.substr(0, 2) != prefix)
    {
        nameStr = prefix + nameStr;
    }

    return nameStr;
}

static std::string SanitizeCppIdentifier(const std::string& name)
{
    std::string result = name;
    for (char& c : result)
    {
        if (!isalnum(c) && c != '_')
            c = '_';
    }
    if (!result.empty() && isdigit(result[0]))
        result = "_" + result;
    return result;
}

static void ConvertFileToByteString(
    const std::string& filePath,
    const std::string& name,
    std::string& outString,
    uint32_t& outSize)
{
    Stream stream;

    stream.ReadFile(filePath.c_str(), false);
    outSize = uint32_t(stream.GetSize());
    char* data = stream.GetData();

    outString.clear();
    outString.reserve(2048);

    std::string assetDataVar = name;
    outString += "extern const char ";
    outString += assetDataVar;
    outString += "[] = \n{\n";

    for (uint32_t byte = 0; byte < outSize; ++byte)
    {
        char byteString[8] = {};
        sprintf(byteString, "'\\x%02X',", uint8_t(data[byte]));

        outString += byteString;

        if (byte % 8 == 7)
        {
            outString += "\n";
        }
    }

    outString += "\n};\n\n";
}

static void ParseGltfExtrasFromDoc(const rapidjson::Document& doc, std::unordered_map<std::string, PolyphaseNodeExtras>& result)
{
    if (!doc.HasMember("nodes") || !doc["nodes"].IsArray())
    {
        return;
    }

    const rapidjson::Value& nodes = doc["nodes"];
    for (rapidjson::SizeType i = 0; i < nodes.Size(); ++i)
    {
        const rapidjson::Value& node = nodes[i];
        if (!node.HasMember("name") || !node["name"].IsString())
            continue;
        if (!node.HasMember("extras") || !node["extras"].IsObject())
            continue;

        std::string nodeName = node["name"].GetString();
        const rapidjson::Value& extras = node["extras"];

        PolyphaseNodeExtras data;
        bool hasPolyphaseData = false;

        // New mesh_type string property
        if (extras.HasMember("mesh_type") && extras["mesh_type"].IsString())
        {
            std::string mt = extras["mesh_type"].GetString();
            if (mt == "Node3D")
                data.mMeshType = PolyphaseMeshType::Node3D;
            else if (mt == "InstancedMesh")
                data.mMeshType = PolyphaseMeshType::InstancedMesh;
            else
                data.mMeshType = PolyphaseMeshType::StaticMesh;
            hasPolyphaseData = true;
        }
        else
        {
            // Backward compat: check old instance_mesh / static_mesh bools
            bool instanceMesh = false;
            bool hasLegacy = false;

            if (extras.HasMember("instance_mesh") && extras["instance_mesh"].IsBool())
            {
                instanceMesh = extras["instance_mesh"].GetBool();
                hasLegacy = true;
            }
            else if (extras.HasMember("instance_mesh") && extras["instance_mesh"].IsInt())
            {
                instanceMesh = (extras["instance_mesh"].GetInt() != 0);
                hasLegacy = true;
            }

            if (extras.HasMember("static_mesh") && extras["static_mesh"].IsBool())
            {
                hasLegacy = true;
            }
            else if (extras.HasMember("static_mesh") && extras["static_mesh"].IsInt())
            {
                hasLegacy = true;
            }

            if (hasLegacy)
            {
                data.mMeshType = instanceMesh ? PolyphaseMeshType::InstancedMesh : PolyphaseMeshType::StaticMesh;
                hasPolyphaseData = true;
            }
        }

        // Dual-key support: prefer "polyphase_*" keys, fall back to "octave_*" for backwards compat
        {
            const char* assetKey = extras.HasMember("polyphase_asset") ? "polyphase_asset" : "octave_asset";
            if (extras.HasMember(assetKey) && extras[assetKey].IsString())
            {
                data.mAssetName = extras[assetKey].GetString();
                if (!data.mAssetName.empty())
                    hasPolyphaseData = true;
            }
        }

        {
            const char* uuidKey = extras.HasMember("polyphase_asset_uuid") ? "polyphase_asset_uuid" : "octave_asset_uuid";
            if (extras.HasMember(uuidKey) && extras[uuidKey].IsString())
            {
                data.mAssetUuid = strtoull(extras[uuidKey].GetString(), nullptr, 10);
                if (data.mAssetUuid != 0)
                    hasPolyphaseData = true;
            }
        }

        {
            const char* scriptKey = extras.HasMember("polyphase_script") ? "polyphase_script" : "octave_script";
            if (extras.HasMember(scriptKey) && extras[scriptKey].IsString())
            {
                data.mScriptPath = extras[scriptKey].GetString();
                if (!data.mScriptPath.empty())
                    hasPolyphaseData = true;
            }
        }

        {
            const char* camKey = extras.HasMember("polyphase_main_camera") ? "polyphase_main_camera" : "octave_main_camera";
            if (extras.HasMember(camKey) && extras[camKey].IsBool())
            {
                data.mMainCamera = extras[camKey].GetBool();
                if (data.mMainCamera)
                    hasPolyphaseData = true;
            }
            else if (extras.HasMember(camKey) && extras[camKey].IsInt())
            {
                data.mMainCamera = (extras[camKey].GetInt() != 0);
                if (data.mMainCamera)
                    hasPolyphaseData = true;
            }
        }

        {
            const char* propsKey = extras.HasMember("polyphase_script_props") ? "polyphase_script_props" : "octave_script_props";
            if (extras.HasMember(propsKey) && extras[propsKey].IsString())
            {
                data.mScriptPropsJson = extras[propsKey].GetString();
            }
        }
        {
            const char* propsTypesKey = extras.HasMember("polyphase_script_props_types") ? "polyphase_script_props_types" : "octave_script_props_types";
            if (extras.HasMember(propsTypesKey) && extras[propsTypesKey].IsString())
            {
                data.mScriptPropsTypesJson = extras[propsTypesKey].GetString();
            }
        }

        {
            const char* matKey = extras.HasMember("polyphase_material_type") ? "polyphase_material_type" : "octave_material_type";
            if (extras.HasMember(matKey) && extras[matKey].IsString())
            {
                data.mMaterialType = extras[matKey].GetString();
                hasPolyphaseData = true;
            }
        }

        if (hasPolyphaseData)
        {
            LogDebug("PolyphaseExtras: node='%s' asset='%s' uuid=%llu meshType=%d script='%s'",
                nodeName.c_str(), data.mAssetName.c_str(), data.mAssetUuid,
                (int)data.mMeshType, data.mScriptPath.c_str());
            result[nodeName] = data;
        }
    }
}

static Property* FindScriptProperty(Script* script, const char* name)
{
    std::vector<Property>& props = script->GetScriptProperties();
    for (uint32_t i = 0; i < props.size(); ++i)
    {
        if (props[i].mName == name)
            return &props[i];
    }
    return nullptr;
}

static void ApplyScriptPropertyOverrides(Node* node, const PolyphaseNodeExtras& extras)
{
    if (extras.mScriptPropsJson.empty() || extras.mScriptPropsTypesJson.empty())
        return;

    Script* script = node->GetScript();
    if (script == nullptr)
        return;

    rapidjson::Document propsDoc;
    propsDoc.Parse(extras.mScriptPropsJson.c_str());
    if (propsDoc.HasParseError() || !propsDoc.IsObject())
        return;

    rapidjson::Document typesDoc;
    typesDoc.Parse(extras.mScriptPropsTypesJson.c_str());
    if (typesDoc.HasParseError() || !typesDoc.IsObject())
        return;

    for (auto it = propsDoc.MemberBegin(); it != propsDoc.MemberEnd(); ++it)
    {
        const char* key = it->name.GetString();

        if (!typesDoc.HasMember(key) || !typesDoc[key].IsInt())
            continue;

        int datumTypeInt = typesDoc[key].GetInt();
        const rapidjson::Value& val = it->value;

        Property* prop = FindScriptProperty(script, key);
        if (prop == nullptr || prop->mType != static_cast<DatumType>(datumTypeInt))
            continue;

        switch (static_cast<DatumType>(datumTypeInt))
        {
        case DatumType::Integer:
            if (val.IsInt()) prop->SetInteger((int32_t)val.GetInt());
            break;
        case DatumType::Float:
            if (val.IsNumber()) prop->SetFloat((float)val.GetDouble());
            break;
        case DatumType::Bool:
            if (val.IsBool()) prop->SetBool(val.GetBool());
            break;
        case DatumType::String:
            if (val.IsString()) prop->SetString(val.GetString());
            break;
        case DatumType::Vector2D:
            if (val.IsArray() && val.Size() >= 2)
            {
                glm::vec2 v((float)val[0].GetDouble(), (float)val[1].GetDouble());
                prop->SetVector2D(v);
            }
            break;
        case DatumType::Vector:
            if (val.IsArray() && val.Size() >= 3)
            {
                glm::vec3 v((float)val[0].GetDouble(), (float)val[1].GetDouble(), (float)val[2].GetDouble());
                prop->SetVector(v);
            }
            break;
        case DatumType::Color:
            if (val.IsArray() && val.Size() >= 4)
            {
                glm::vec4 v((float)val[0].GetDouble(), (float)val[1].GetDouble(),
                            (float)val[2].GetDouble(), (float)val[3].GetDouble());
                prop->SetColor(v);
            }
            break;
        case DatumType::Asset:
            if (val.IsString())
            {
                Asset* asset = AssetManager::Get()->LoadAsset(val.GetString());
                prop->SetAsset(asset);
            }
            break;
        case DatumType::Byte:
            if (val.IsInt()) prop->SetByte((uint8_t)val.GetInt());
            break;
        case DatumType::Short:
            if (val.IsInt()) prop->SetShort((int16_t)val.GetInt());
            break;
        default:
            break;
        }
    }
}

static std::unordered_map<std::string, PolyphaseNodeExtras> ParseGltfExtras(const std::string& filePath)
{
    std::unordered_map<std::string, PolyphaseNodeExtras> result;

    int32_t dotIndex = int32_t(filePath.find_last_of('.'));
    std::string extension = filePath.substr(dotIndex, filePath.size() - dotIndex);

    Stream stream;
    if (!stream.ReadFile(filePath.c_str(), false))
    {
        LogWarning("ParseGltfExtras: Failed to read file %s", filePath.c_str());
        return result;
    }

    std::string jsonStr;

    if (extension == ".glb")
    {
        // GLB format: 12-byte header, then chunks
        // Header: magic(4) + version(4) + length(4)
        // Each chunk: chunkLength(4) + chunkType(4) + chunkData(chunkLength)
        // First chunk is JSON (type 0x4E4F534A)
        const char* rawData = stream.GetData();
        uint32_t fileSize = stream.GetSize();

        if (fileSize < 20) // 12 header + 8 chunk header minimum
        {
            LogWarning("ParseGltfExtras: GLB file too small %s", filePath.c_str());
            return result;
        }

        uint32_t jsonChunkLength;
        memcpy(&jsonChunkLength, rawData + 12, sizeof(uint32_t));

        uint32_t jsonChunkType;
        memcpy(&jsonChunkType, rawData + 16, sizeof(uint32_t));

        if (jsonChunkType != 0x4E4F534A) // "JSON" in little-endian
        {
            LogWarning("ParseGltfExtras: First GLB chunk is not JSON in %s", filePath.c_str());
            return result;
        }

        if (20 + jsonChunkLength > fileSize)
        {
            LogWarning("ParseGltfExtras: GLB JSON chunk exceeds file size in %s", filePath.c_str());
            return result;
        }

        jsonStr.assign(rawData + 20, jsonChunkLength);
    }
    else
    {
        // .gltf text JSON
        jsonStr.assign(stream.GetData(), stream.GetSize());
    }

    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError() || !doc.IsObject())
    {
        LogWarning("ParseGltfExtras: Failed to parse JSON from %s", filePath.c_str());
        return result;
    }

    ParseGltfExtrasFromDoc(doc, result);

    return result;
}

static void ApplyMaterialTypeOverride(StaticMesh* mesh, const std::string& materialType)
{
    if (mesh == nullptr)
        return;

    Material* mat = mesh->GetMaterial();
    if (mat == nullptr || !mat->IsLite())
        return;

    MaterialLite* matLite = static_cast<MaterialLite*>(mat);

    if (materialType == "UNLIT")
    {
        matLite->SetShadingModel(ShadingModel::Unlit);
    }
    else if (materialType == "LIT")
    {
        matLite->SetShadingModel(ShadingModel::Lit);
    }
    else if (materialType == "VERTEX_COLOR")
    {
        matLite->SetVertexColorMode(VertexColorMode::Modulate);
    }
}

static void SpawnAiNode(aiNode* node, Node* root, const glm::mat4& parentTransform, const std::vector<StaticMesh*>& meshList, const SceneImportOptions& options, const std::unordered_map<std::string, PolyphaseNodeExtras>& extrasMap)
{
    if (node == nullptr || root == nullptr)
        return;

    glm::mat4 transform = parentTransform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    std::string nodeName = node->mName.C_Str();

    PolyphaseNodeExtras extras;
    if (options.mApplyGltfExtras)
    {
        auto octIt = extrasMap.find(nodeName);
        if (octIt != extrasMap.end())
        {
            extras = octIt->second;
        }
    }

    // Resolve asset from extras (UUID first, then name)
    StaticMesh* assetMesh = nullptr;
    if (options.mApplyGltfExtras)
    {
        if (extras.mAssetUuid != 0 || !extras.mAssetName.empty())
        {
            LogDebug("PolyphaseExtras: Resolving asset for node '%s' — uuid=%llu name='%s'",
                nodeName.c_str(), extras.mAssetUuid, extras.mAssetName.c_str());
        }

        if (extras.mAssetUuid != 0)
        {
            Asset* a = AssetManager::Get()->GetAssetByUuid(extras.mAssetUuid);
            if (a)
            {
                assetMesh = a->As<StaticMesh>();
                LogDebug("PolyphaseExtras: UUID lookup SUCCESS for node '%s' -> asset '%s'",
                    nodeName.c_str(), a->GetName().c_str());
            }
            else
            {
                LogWarning("PolyphaseExtras: Could not find asset by UUID %llu for node '%s'", extras.mAssetUuid, nodeName.c_str());
            }
        }

        if (assetMesh == nullptr && !extras.mAssetName.empty())
        {
            // Try path-based lookup first (e.g. "Assets/Models/SM_Cube"), then name-based
            LogDebug("PolyphaseExtras: Trying path lookup '%s' for node '%s'", extras.mAssetName.c_str(), nodeName.c_str());
            Asset* a = AssetManager::Get()->LoadAssetByPath(extras.mAssetName);
            if (a)
            {
                assetMesh = a->As<StaticMesh>();
                LogDebug("PolyphaseExtras: Path lookup SUCCESS -> '%s' (type=%s)",
                    a->GetName().c_str(), Asset::GetNameFromTypeId(a->GetType()));
            }
            else
            {
                LogDebug("PolyphaseExtras: Path lookup FAILED, trying name lookup '%s'", extras.mAssetName.c_str());
                assetMesh = LoadAsset<StaticMesh>(extras.mAssetName);
                if (assetMesh)
                {
                    LogDebug("PolyphaseExtras: Name lookup SUCCESS -> '%s'", assetMesh->GetName().c_str());
                }
            }

            if (assetMesh == nullptr)
            {
                LogWarning("PolyphaseExtras: Could not load asset '%s' for node '%s'", extras.mAssetName.c_str(), nodeName.c_str());
            }
        }
    }

    if (node->mNumMeshes > 0)
    {
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            uint32_t meshIndex = node->mMeshes[i];
            StaticMesh* meshToUse = assetMesh ? assetMesh : meshList[meshIndex];

            Node* newNode = nullptr;

            switch (extras.mMeshType)
            {
            case PolyphaseMeshType::Node3D:
            {
                Node3D* newNode3D = root->CreateChild<Node3D>();
                newNode3D->SetTransform(transform);
                newNode3D->SetName(nodeName);
                newNode = newNode3D;
                break;
            }
            case PolyphaseMeshType::InstancedMesh:
            {
                InstancedMesh3D* newInst = root->CreateChild<InstancedMesh3D>();
                newInst->SetStaticMesh(meshToUse);
                newInst->SetTransform(transform);
                newInst->SetName(nodeName);
                newInst->EnableCastShadows(true);
                newInst->SetBakeLighting(true);
                newInst->EnableCollision(options.mEnableCollision);
                {
                    MeshInstanceData data;
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::quat rotation;
                    glm::vec3 scale;
                    glm::vec3 translation;
                    glm::decompose(transform, scale, rotation, translation, skew, perspective);
   
                    newInst->SetInstanceData({ data });
                }
                newNode = newInst;
                break;
            }
            case PolyphaseMeshType::StaticMesh:
            default:
            {
                StaticMesh3D* newMesh = root->CreateChild<StaticMesh3D>();
                newMesh->SetStaticMesh(meshToUse);
                newMesh->SetUseTriangleCollision(meshToUse->IsTriangleCollisionMeshEnabled());
                newMesh->SetTransform(transform);
                newMesh->SetName(nodeName);
                newMesh->EnableCastShadows(true);
                newMesh->SetBakeLighting(true);
                newMesh->SetUseTriangleCollision(true);
                newMesh->EnableCollision(options.mEnableCollision);
                newNode = newMesh;
                break;
            }
            }

            if (!assetMesh && !extras.mMaterialType.empty())
            {
                ApplyMaterialTypeOverride(meshToUse, extras.mMaterialType);
            }

            if (newNode != nullptr && !extras.mScriptPath.empty())
            {
                newNode->SetScriptFile(extras.mScriptPath);
                ApplyScriptPropertyOverrides(newNode, extras);
            }
        }
    }
    else if (options.mApplyGltfExtras && (!extras.mAssetName.empty() || extras.mAssetUuid != 0))
    {
        // Node has no embedded mesh but has a polyphase_asset reference
        Node* newNode = nullptr;

        switch (extras.mMeshType)
        {
        case PolyphaseMeshType::Node3D:
        {
            Node3D* newNode3D = root->CreateChild<Node3D>();
            newNode3D->SetTransform(transform);
            newNode3D->SetName(nodeName);
            newNode = newNode3D;
            break;
        }
        case PolyphaseMeshType::InstancedMesh:
        {
            if (assetMesh != nullptr)
            {
                InstancedMesh3D* newInst = root->CreateChild<InstancedMesh3D>();
                newInst->SetStaticMesh(assetMesh);
                newInst->SetTransform(transform);
                newInst->SetName(nodeName);
                newInst->EnableCastShadows(true);
                newInst->SetBakeLighting(true);
                newInst->EnableCollision(options.mEnableCollision);
                MeshInstanceData data;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::quat rotation;
                glm::vec3 scale;
                glm::vec3 translation;
                glm::decompose(transform, scale, rotation, translation, skew, perspective);
       /*         data.mPosition = translation;
                data.mRotation = glm::degrees(glm::eulerAngles(rotation));
                data.mScale = scale;*/
                newInst->SetInstanceData({ data });
				//newInst->SetPosition({ 0,0,0 });

                newNode = newInst;
            }
            break;
        }
        case PolyphaseMeshType::StaticMesh:
        default:
        {
            if (assetMesh != nullptr)
            {
                StaticMesh3D* newMesh = root->CreateChild<StaticMesh3D>();
                newMesh->SetStaticMesh(assetMesh);
                newMesh->SetUseTriangleCollision(assetMesh->IsTriangleCollisionMeshEnabled());
                newMesh->SetTransform(transform);
                newMesh->SetName(nodeName);
                newMesh->EnableCastShadows(true);
                newMesh->SetBakeLighting(true);
                newMesh->SetUseTriangleCollision(true);
                newMesh->EnableCollision(options.mEnableCollision);
                newNode = newMesh;
            }
            break;
        }
        }

        if (newNode != nullptr && !extras.mScriptPath.empty())
        {
            newNode->SetScriptFile(extras.mScriptPath);
            ApplyScriptPropertyOverrides(newNode, extras);
        }
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        SpawnAiNode(node->mChildren[i], root, transform, meshList, options, extrasMap);
    }
}

static glm::mat4 GetNodeTransform(aiNode* node)
{
    aiMatrix4x4 transform;

    while (node)
    {
        transform = transform * node->mTransformation;
        node = node->mParent;
    }

    glm::mat4 gMat = glm::transpose(glm::make_mat4(&transform.a1));
    return gMat;
}

void ActionManager::ImportCamera(const CameraImportOptions& options)
{
    if (GetEngineState()->mProjectPath == "")
        return;
    World* world = GetWorld(0);
    std::string openPath = options.mFilePath;
    // Display the Open dialog box. 
    if (openPath != "")
    {
        std::string filename = openPath;
        int32_t dotIndex = int32_t(filename.find_last_of('.'));
        std::string extension = filename.substr(dotIndex, filename.size() - dotIndex);
        if (extension == ".glb" ||
            extension == ".gltf" ||
            extension == ".dae")
        {
            LogDebug("Begin camera import...");
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filename, aiProcess_FlipUVs);
            if (scene == nullptr)
            {
                LogError("Failed to load scene file");
                return;
            }

            for (uint32_t i = 0; i < scene->mNumCameras; ++i)
            {
                aiCamera* aCamera = scene->mCameras[i];
				Node* rootNode = GetWorld(0)->GetRootNode();
                if(rootNode == nullptr)
                {
                    LogError("Failed to get root node from world");
					return;
				}
                Camera3D* newCamera = rootNode->CreateChild<Camera3D>();

                glm::mat4 camTransform(1);
                aiNode* camNode = scene->mRootNode->FindNode(aCamera->mName.C_Str());

                if (camNode)
                {
                    camTransform = GetNodeTransform(camNode);
                }
                float vfov = 2.0f * atanf(tanf(aCamera->mHorizontalFOV * 0.5f) / aCamera->mAspect);

                newCamera->SetFieldOfView(glm::degrees(vfov));
                newCamera->SetNearZ(aCamera->mClipPlaneNear);
                newCamera->SetFarZ(aCamera->mClipPlaneFar);
                newCamera->SetAspectRatio(aCamera->mAspect);

                newCamera->SetTransform(camTransform);
                newCamera->UpdateTransform(true);
                if (i == 0 && options.mIsMainCamera) {
					newCamera->SetIsMainCamera(true);
                }


                newCamera->SetName(options.mOverrideCameraName ? options.mCameraName : aCamera->mName.C_Str());

            }


        }
	}

}

void ActionManager::ImportScene(const SceneImportOptions& options)
{
    if (GetEngineState()->mProjectPath == "")
        return;

    World* world = GetWorld(0);
    std::string openPath = options.mFilePath;

    // Display the Open dialog box. 
    if (openPath != "")
    {
        std::string filename = openPath;
        int32_t dotIndex = int32_t(filename.find_last_of('.'));
        std::string extension = filename.substr(dotIndex, filename.size() - dotIndex);

        std::string importDir;
        size_t slashPos = openPath.find_last_of("/\\");
        if (slashPos != std::string::npos)
        {
            importDir = openPath.substr(0, slashPos + 1);
        }
        else
        {
            importDir = "./";
        }

        if (extension == ".glb" ||
            extension == ".gltf" ||
            extension == ".dae" ||
            extension == ".blend")
        {
            LogDebug("Begin scene import...");
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(filename, aiProcess_FlipUVs);

            if (scene == nullptr)
            {
                LogError("Failed to load scene file");
                return;
            }

            // Parse Polyphase extras from glTF files
            std::unordered_map<std::string, PolyphaseNodeExtras> extrasMap;
            if (options.mApplyGltfExtras && (extension == ".gltf" || extension == ".glb"))
            {
                extrasMap = ParseGltfExtras(filename);
                LogDebug("Parsed %zu glTF extras entries", extrasMap.size());
                for (auto& kv : extrasMap)
                {
                    LogDebug("  extras['%s']: asset='%s' uuid=%llu meshType=%d",
                        kv.first.c_str(), kv.second.mAssetName.c_str(),
                        kv.second.mAssetUuid, (int)kv.second.mMeshType);
                }
            }

            bool isReimport = (options.mReimportSceneStub != nullptr);

            // Get the current directory in the asset panel (all assets will be saved there)
            AssetDir* curDir = GetEditorState()->GetAssetDirectory();

            const std::string& sceneName = options.mSceneName;
            AssetDir* sceneDir = nullptr;
            if (isReimport)
            {
                sceneDir = options.mReimportSceneStub->mDirectory;
            }
            else
            {
                sceneDir = curDir->CreateSubdirectory(sceneName);
            }

            std::string fullSceneName = "SC_" + sceneName;
            for (uint32_t i = 0; i < GetEditorState()->mEditScenes.size(); ++i)
            {
                Scene* scene = GetEditorState()->mEditScenes[i].mSceneAsset.Get<Scene>();
                if (scene && scene->GetName() == fullSceneName)
                {
                    GetEditorState()->CloseEditScene((int32_t)i);
                    break;
                }
            }

            GarbageCollect();

            if (sceneDir == nullptr ||
                sceneDir->mParentDir == nullptr)
            {
                LogError("Invalid directory. Use the asset panel to navigate to a valid directory");
                return;
            }

            if (!isReimport)
            {
                LogDebug("Purging scene dir: %s (%zu assets)", sceneDir->mPath.c_str(), sceneDir->mAssetStubs.size());
                sceneDir->Purge();
            }
            GetEditorState()->SetAssetDirectory(sceneDir, true);

            SharedPtr<Node3D> rootNode = Node::Construct<Node3D>();
            rootNode->SetName(sceneName);

            std::vector<Texture*> textureList;
            std::vector<Material*> materialList;
            std::vector<StaticMesh*> meshList;
            std::unordered_map<std::string, Texture*> textureMap;

            // Pre-scan: determine which mesh indices actually need new assets created.
            // If ALL nodes referencing a mesh have a polyphase_asset extra, skip creating that mesh.
            std::set<uint32_t> neededMeshIndices;
            std::function<void(aiNode*)> collectNeededMeshes = [&](aiNode* n) {
                if (!n) return;
                std::string name = n->mName.C_Str();
                bool hasAssetExtra = false;
                if (options.mApplyGltfExtras)
                {
                    auto it = extrasMap.find(name);
                    if (it != extrasMap.end() && (!it->second.mAssetName.empty() || it->second.mAssetUuid != 0))
                    {
                        hasAssetExtra = true;
                    }
                }
                if (!hasAssetExtra)
                {
                    for (uint32_t m = 0; m < n->mNumMeshes; ++m)
                    {
                        neededMeshIndices.insert(n->mMeshes[m]);
                    }
                }
                for (uint32_t c = 0; c < n->mNumChildren; ++c)
                {
                    collectNeededMeshes(n->mChildren[c]);
                }
            };
            collectNeededMeshes(scene->mRootNode);

            // Derive which material indices are needed (only those used by needed meshes)
            std::set<uint32_t> neededMaterialIndices;
            for (uint32_t idx : neededMeshIndices)
            {
                neededMaterialIndices.insert(scene->mMeshes[idx]->mMaterialIndex);
            }

            LogDebug("Pre-scan: %zu/%u meshes needed, %zu/%u materials needed",
                neededMeshIndices.size(), scene->mNumMeshes,
                neededMaterialIndices.size(), scene->mNumMaterials);

            uint32_t numMaterials = scene->mNumMaterials;
            for (uint32_t i = 0; i < numMaterials; ++i)
            {
                // Skip materials not needed by any mesh we're actually creating
                if (neededMaterialIndices.find(i) == neededMaterialIndices.end())
                {
                    materialList.push_back(nullptr);
                    continue;
                }

                aiMaterial* aMaterial = scene->mMaterials[i];
                std::string materialName = options.mPrefix + aMaterial->GetName().C_Str();

                if (materialName.size() < 2 || (materialName.substr(0, 2) != "M_"))
                {
                    materialName = std::string("M_") + sceneName + std::string("_") + materialName;
                }

                AssetStub* materialStub = nullptr;
                MaterialLite* newMaterial = nullptr;
                if (options.mImportMaterials)
                {
                    if (isReimport)
                    {
                        AssetStub* existingStub = AssetManager::Get()->GetAssetStub(materialName);
                        if (existingStub && existingStub->mType == MaterialLite::GetStaticType())
                        {
                            materialStub = existingStub;
                            if (!existingStub->mAsset) AssetManager::Get()->LoadAsset(*existingStub);
                            newMaterial = static_cast<MaterialLite*>(existingStub->mAsset);
                        }
                    }
                    if (materialStub == nullptr)
                    {
                        materialStub = EditorAddUniqueAsset(materialName.c_str(), sceneDir, MaterialLite::GetStaticType(), true);
                        newMaterial = static_cast<MaterialLite*>(materialStub->mAsset);
                    }
                    newMaterial->SetShadingModel(options.mDefaultShadingModel);
                    newMaterial->SetVertexColorMode(options.mDefaultVertexColorMode);
                }

                uint32_t numBaseTextures = aMaterial->GetTextureCount(aiTextureType_DIFFUSE);
                numBaseTextures = glm::clamp(numBaseTextures, 0u, 4u);

                for (uint32_t t = 0; t < numBaseTextures; ++t)
                {
                    aiString path;
                    aiReturn ret = aMaterial->GetTexture(aiTextureType::aiTextureType_DIFFUSE, t, &path);

                    if (ret == aiReturn_SUCCESS)
                    {
                        std::string texturePath = path.C_Str();
                        Texture* textureToAssign = nullptr;
                        LogDebug("Scene Texture: %s", texturePath.c_str());

                        if (textureMap.find(texturePath) != textureMap.end())
                        {
                            // Case 1 - Texture has already been loaded by a previous material
                            textureToAssign = textureMap[texturePath];
                        }
                        else if (options.mImportTextures)
                        {
                            // Case 2 - Texture needs to be loaded.
                            // To make texturing sharing simpler, we only import the texture if
                            //  - There is no texture registered
                            //  - There is a texture registered, and it resides in the current AssetDir

                            bool importTexture = false;

                            std::string assetName;
                            if (texturePath.size() > 1 && texturePath[0] == '*')
                            {
                                assetName = "Texture" + texturePath.substr(1);
                            }
                            else
                            {
                                assetName = EditorGetAssetNameFromPath(texturePath);
                                if (assetName.size() >= 2 && (strncmp(assetName.c_str(), "T_", 2) == 0))
                                {
                                    assetName = assetName.substr(2);
                                }
                            }

                            assetName = options.mPrefix + assetName;

                            // Add T_ and sceneName to texture name like we do for materials
                            assetName = std::string("T_") + sceneName + std::string("_") + assetName;

                            AssetStub* existingStub = AssetManager::Get()->GetAssetStub(assetName);
                            if (existingStub && existingStub->mDirectory != sceneDir)
                            {
                                textureToAssign = LoadAsset<Texture>(assetName);
                            }

                            if (isReimport && textureToAssign == nullptr)
                            {
                                if (options.mReimportTextures)
                                {
                                    if (existingStub)
                                    {
                                        AssetManager::Get()->PurgeAsset(assetName.c_str());
                                        existingStub = nullptr;
                                    }
                                }
                                else
                                {
                                    textureToAssign = LoadAsset<Texture>(assetName);
                                }
                            }

                            bool embeddedTexturePath = (texturePath.size() > 1 && texturePath[0] == '*');

                            // Resolve the texture source to a file path
                            std::string resolvedTexturePath;
                            bool tempFileCreated = false;

                            if (textureToAssign == nullptr && embeddedTexturePath)
                            {
                                // GLB embedded texture — extract to a temp file for import
                                int embIndex = std::atoi(texturePath.c_str() + 1);
                                if (embIndex >= 0 && (uint32_t)embIndex < scene->mNumTextures)
                                {
                                    aiTexture* embTex = scene->mTextures[embIndex];
                                    if (embTex->mHeight == 0)
                                    {
                                        // Compressed format (PNG/JPG) — mWidth is the data size in bytes
                                        std::string ext = ".";
                                        ext += embTex->achFormatHint;
                                        resolvedTexturePath = importDir + "__octave_emb_" + std::to_string(embIndex) + ext;

                                        std::ofstream tmpFile(resolvedTexturePath, std::ios::binary);
                                        if (tmpFile)
                                        {
                                            tmpFile.write(reinterpret_cast<const char*>(embTex->pcData), embTex->mWidth);
                                            tmpFile.close();
                                            tempFileCreated = true;
                                        }
                                    }
                                }
                            }
                            else if (textureToAssign == nullptr)
                            {
                                resolvedTexturePath = importDir + texturePath;
                            }

                            if (textureToAssign == nullptr && !resolvedTexturePath.empty())
                            {
                                Asset* importedAsset = ImportAsset(resolvedTexturePath);
                                OCT_ASSERT(importedAsset == nullptr || importedAsset->GetType() == Texture::GetStaticType());

                                if (importedAsset == nullptr || importedAsset->GetType() == Texture::GetStaticType())
                                {
                                    textureToAssign = (Texture*)importedAsset;
                                }

                                if (importedAsset != nullptr)
                                {
                                    AssetManager::Get()->RenameAsset(importedAsset, assetName);
                                    AssetManager::Get()->SaveAsset(assetName);
                                }
                            }

                            if (tempFileCreated)
                            {
                                std::remove(resolvedTexturePath.c_str());
                            }

                            if (textureToAssign != nullptr)
                            {
                                textureMap.insert({ texturePath, textureToAssign });
                            }
                        }

                        if (newMaterial != nullptr && textureToAssign != nullptr)
                        {
                            newMaterial->SetTexture(t, textureToAssign);
                        }
                    }
                }

                if (materialStub != nullptr)
                {
                    AssetManager::Get()->SaveAsset(*materialStub);
                    materialList.push_back(newMaterial);
                }
            }

            // Create static mesh assets (assign corresponding material)
            uint32_t numMeshes = scene->mNumMeshes;

            for (uint32_t i = 0; i < numMeshes; ++i)
            {
                // Skip meshes not needed (all referencing nodes have polyphase_asset extras)
                if (neededMeshIndices.find(i) == neededMeshIndices.end())
                {
                    meshList.push_back(nullptr);
                    continue;
                }

                aiMesh* aMesh = scene->mMeshes[i];
                std::string meshName = options.mPrefix + aMesh->mName.C_Str();

                if (meshName.size() < 3 || (meshName.substr(0, 3) != "SM_"))
                {
                    meshName = std::string("SM_") + sceneName + std::string("_") + meshName;
                }

                // Ensure unique name (this normally happens when model has multiple materials).
                std::string uniqueName = meshName;
                int32_t uniqueNum = 1;
                for (int32_t u = 0; u < (int32_t)meshList.size(); ++u)
                {
                    if (meshList[u] != nullptr && meshList[u]->GetName() == uniqueName)
                    {
                        uniqueName = meshName + "_" + std::to_string(uniqueNum);
                        uniqueNum++;
                        u = -1;
                    }
                }
                meshName = uniqueName;

                StaticMesh* existingMesh = LoadAsset<StaticMesh>(meshName);
                StaticMesh* meshToAddToList = existingMesh;

                if (options.mImportMeshes)
                {
                    StaticMesh* newMesh = (StaticMesh*)Asset::CreateInstance(StaticMesh::GetStaticType());
                    newMesh->Create(scene, *aMesh, 0, nullptr); // Collision meshes currently not supported for scene import?
                    meshToAddToList = newMesh;

                    if (existingMesh)
                    {
                        newMesh->SetGenerateTriangleCollisionMesh(existingMesh->IsTriangleCollisionMeshEnabled());
                        newMesh->SetMaterial(existingMesh->GetMaterial());
                        AssetManager::Get()->PurgeAsset(existingMesh->GetName().c_str());
                        existingMesh = nullptr;
                    }

                    if (options.mImportMaterials)
                    {
                        // Find material to use...
                        uint32_t materialIndex = aMesh->mMaterialIndex;
                        OCT_ASSERT(materialIndex < materialList.size());
                        newMesh->SetMaterial(materialList[materialIndex]);
                    }

                    AssetStub* meshStub = EditorAddUniqueAsset(meshName.c_str(), sceneDir, StaticMesh::GetStaticType(), false);
                    meshStub->mAsset = newMesh;
                    newMesh->SetName(meshName);
                    newMesh->SetGenerateTriangleCollisionMesh(true);
                    AssetManager::Get()->SaveAsset(*meshStub);
                }

                meshList.push_back(meshToAddToList);
            }
            // Create Lights
            if (options.mImportLights)
            {
                uint32_t numLights = scene->mNumLights;
                for (uint32_t i = 0; i < numLights; ++i)
                {
                    aiLight* aLight = scene->mLights[i];

                    if (aLight->mType == aiLightSource_POINT)
                    {
                        PointLight3D* pointLight = rootNode->CreateChild<PointLight3D>();

                        glm::vec3 lightColor;
                        lightColor.r = aLight->mColorDiffuse.r;
                        lightColor.g = aLight->mColorDiffuse.g;
                        lightColor.b = aLight->mColorDiffuse.b;
                        lightColor = Maths::SafeNormalize(lightColor);
                        pointLight->SetColor(glm::vec4(lightColor, 1.0f));

                        pointLight->SetLightingDomain(LightingDomain::All);

                        // For now, set lights to a default radius to 50.0f.
                        // Not sure how to convert attenutation data into a radius.
                        // Maybe I need to rethink how light attenuation is specified.
                        // Using the constant/linear/quadratic coefficients is how GCN and 3DS do it IIRC.
                        pointLight->SetRadius(50.0f);

                        glm::mat4 lightTransform(1);
                        aiNode* lightNode = scene->mRootNode->FindNode(aLight->mName.C_Str());

                        if (lightNode)
                        {
                            lightTransform = GetNodeTransform(lightNode);
                        }

                        pointLight->SetTransform(lightTransform);
                        pointLight->UpdateTransform(true);

                        pointLight->SetName(aLight->mName.C_Str());
                    }
                }
                


            }

            // Create Cameras
            if (options.mImportCameras) {

                uint32_t numCameras = scene->mNumCameras;

                for (uint32_t i = 0; i < numCameras; ++i)
                {
                    aiCamera* aCamera = scene->mCameras[i];

                    Camera3D* newCamera = rootNode->CreateChild<Camera3D>();

                    glm::mat4 camTransform(1);
                    aiNode* camNode = scene->mRootNode->FindNode(aCamera->mName.C_Str());

                    if (camNode)
                    {
                        camTransform = GetNodeTransform(camNode);
                    }
                    float vfov = 2.0f * atanf(tanf(aCamera->mHorizontalFOV * 0.5f) / aCamera->mAspect);

                    newCamera->SetFieldOfView(glm::degrees(vfov));
                    newCamera->SetNearZ(aCamera->mClipPlaneNear);
                    newCamera->SetFarZ(aCamera->mClipPlaneFar);
                    newCamera->SetAspectRatio(aCamera->mAspect);

                    newCamera->SetTransform(camTransform);
                    newCamera->UpdateTransform(true);

                    newCamera->SetName(aCamera->mName.C_Str());

                    if (options.mApplyGltfExtras)
                    {
                        auto camExtIt = extrasMap.find(aCamera->mName.C_Str());
                        if (camExtIt != extrasMap.end() && camExtIt->second.mMainCamera)
                        {
                            newCamera->SetIsMainCamera(true);
                        }
                    }

                }
			}
            LogDebug("Spawning scene nodes — meshList has %zu entries, extrasMap has %zu entries",
                meshList.size(), extrasMap.size());
            // Dump all referenced asset names/UUIDs so we can cross-check with engine asset registry
            for (auto& kv : extrasMap)
            {
                if (!kv.second.mAssetName.empty())
                {
                    AssetStub* stub = AssetManager::Get()->GetAssetStub(kv.second.mAssetName);
                    AssetStub* stubByPath = AssetManager::Get()->GetAssetStubByPath(kv.second.mAssetName);
                    LogDebug("  Pre-spawn check '%s': byName=%s byPath=%s uuidInMap=%s",
                        kv.first.c_str(),
                        stub ? stub->mName.c_str() : "NULL",
                        stubByPath ? stubByPath->mName.c_str() : "NULL",
                        kv.second.mAssetUuid != 0 ?
                            (AssetManager::Get()->GetAssetByUuid(kv.second.mAssetUuid) ? "FOUND" : "MISS") : "N/A");
                }
            }
            aiNode* node = scene->mRootNode;
            SpawnAiNode(node, rootNode.Get(), glm::mat4(1), meshList, options, extrasMap);

            if (isReimport)
            {
                if (!options.mReimportSceneStub->mAsset)
                {
                    AssetManager::Get()->LoadAsset(*options.mReimportSceneStub);
                }
                Scene* existingScene = options.mReimportSceneStub->mAsset ?
                    options.mReimportSceneStub->mAsset->As<Scene>() : nullptr;
                if (existingScene)
                {
                    existingScene->Capture(rootNode.Get());
                    AssetManager::Get()->SaveAsset(*options.mReimportSceneStub);
                    GetEditorState()->OpenEditScene(existingScene);
                }
                else
                {
                    LogError("Failed to access existing scene asset for reimport");
                }
            }
            else
            {
                AssetStub* sceneStub = EditorAddUniqueAsset(fullSceneName.c_str(), sceneDir, Scene::GetStaticType(), true);
                Scene* newScene = sceneStub->mAsset ? sceneStub->mAsset->As<Scene>() : nullptr;

                if (newScene)
                {
                    newScene->Capture(rootNode.Get());
                    AssetManager::Get()->SaveAsset(*sceneStub);
                    GetEditorState()->OpenEditScene(newScene);
                }
                else
                {
                    LogError("Failed to create new scene asset for scene import");
                }
            }
        }
        else
        {
            LogError("Failed to import scene. File format must be .glb, .gltf, .dae, or .blend");
        }
    }
}

void ActionManager::BeginImportScene()
{
    if (GetEngineState()->mProjectPath != "")
    {
        std::vector<std::string> filePaths = SYS_OpenFileDialog();
        HandleImportSceneCallback(filePaths);
    }
    else
    {
        LogWarning("Cannot import scene. No project loaded.");
    }

}

void ActionManager::BeginImportCamera()
{
	std::string  prevIOPath = GetEngineState()->mProjectPath;
    if (prevIOPath != "")
    {
        std::vector<std::string> filePaths = SYS_OpenFileDialog();
        HandleImportCameraCallback(filePaths);
    }
    else
    {
        LogWarning("Cannot import scene. No project loaded.");
    }

}

static void HandleReimportSceneCallback(const std::vector<std::string>& filePaths)
{
    if (filePaths.size() > 1)
    {
        LogWarning("Can only reimport one scene at a time. Ignoring extra files.");
    }

    if (filePaths.size() >= 1)
    {
        GetEditorState()->mPendingReimportScenePath = filePaths[0];
    }
}

void ActionManager::BeginReimportScene(AssetStub* sceneStub)
{
    if (GetEngineState()->mProjectPath == "")
    {
        LogWarning("Cannot reimport scene. No project loaded.");
        return;
    }

    if (sceneStub == nullptr || sceneStub->mType != Scene::GetStaticType())
    {
        LogWarning("Cannot reimport scene. Invalid scene asset.");
        return;
    }

    GetEditorState()->mPendingReimportSceneStub = sceneStub;

    std::vector<std::string> filePaths = SYS_OpenFileDialog();
    HandleReimportSceneCallback(filePaths);
}

void ActionManager::GenerateEmbeddedAssetFiles(std::vector<std::pair<AssetStub*, std::string> >& assets,
    std::vector<EmbeddedRawAssetEntry>& rawAssets,
    const char* headerPath,
    const char* sourcePath)
{
    FILE* headerFile = fopen(headerPath, "w");
    FILE* sourceFile = fopen(sourcePath, "w");

    OCT_ASSERT(headerFile != nullptr && sourceFile != nullptr);

    if (headerFile != nullptr && sourceFile != nullptr)
    {
        fprintf(headerFile, "#include <stdint.h>\n");
        fprintf(headerFile, "#include \"EmbeddedFile.h\"\n\n");

        fprintf(headerFile, "extern uint32_t gNumEmbeddedAssets;\n");
        fprintf(headerFile, "extern EmbeddedFile gEmbeddedAssets[];\n");
        fprintf(headerFile, "extern uint32_t gNumEmbeddedRawAssets;\n");
        fprintf(headerFile, "extern EmbeddedFile gEmbeddedRawAssets[];\n\n");
        fprintf(headerFile, "extern const char gEmbeddedConfig_Data[];\n\n");
        // Match the const-ness of the source-side definition at line ~5615
        // (`extern const uint32_t gEmbeddedConfig_Size = ...`). C++ links
        // `extern T` and `extern const T` as different symbols, so a const/non-const
        // mismatch here produced an LNK2001 in shipped builds.
        fprintf(headerFile, "extern const uint32_t gEmbeddedConfig_Size;\n\n");

        fprintf(sourceFile, "#include <stdint.h>\n");
        fprintf(sourceFile, "#include \"EmbeddedFile.h\"\n\n");

        std::string initializer;

        for (int32_t i = 0; i < int32_t(assets.size()); ++i)
        {
            AssetStub* stub = assets[i].first;
            const std::string& packPath = assets[i].second;

            // Build unique variable name using full relative path to avoid collisions
            // when assets with the same name exist in different directories
            std::string uniqueName;
            AssetDir* projDir = AssetManager::Get()->FindProjectDirectory();
            AssetDir* engDir = AssetManager::Get()->FindEngineDirectory();
            AssetDir* rootDir = stub->mEngineAsset ? engDir : projDir;
            AssetDir* dir = stub->mDirectory;
            std::vector<std::string> pathParts;
            while (dir != nullptr && dir != rootDir)
            {
                pathParts.push_back(dir->mName);
                dir = dir->mParentDir;
            }
            for (int j = (int)pathParts.size() - 1; j >= 0; --j)
            {
                uniqueName += pathParts[j] + "_";
            }
            uniqueName += stub->mName;

            std::string dataVarName = SanitizeCppIdentifier(uniqueName) + "_Data";
            uint32_t dataSize = 0;

            std::string sourceString;
            ConvertFileToByteString(
                packPath,
                dataVarName,
                sourceString,
                dataSize);

            fprintf(sourceFile, "%s", sourceString.c_str());

            initializer += "{" + ("\"" + stub->mName + "\",") +
                                 (dataVarName + ",") +
                                 (std::to_string(dataSize) + ",") +
                                 (stub->mEngineAsset ? "true" : "false") +
                                 "}, \n";
        }

        fprintf(sourceFile, "\n\nuint32_t gNumEmbeddedAssets = %d;\n", uint32_t(assets.size()));

        if (assets.size() > 0)
        {
            fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedAssets[] = \n{\n");
            fprintf(sourceFile, "%s", initializer.c_str());
            fprintf(sourceFile, "\n};\n");
        }
        else
        {
            fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedAssets[] = { {} };\n");
        }

        // ---- Raw embedded assets (.mp4, .json, .png, …) ----
        // Lookup keys are projectDir-relative paths (forward slashes) to match
        // the canonicalisation the runtime VFS shim performs at SYS_OpenFile.
        // Hex-byte format is the same as the .oct path; warn on > 10 MB files
        // so devs notice compile-time blowup before it's a problem.
        {
            std::string rawInitializer;
            for (size_t i = 0; i < rawAssets.size(); ++i)
            {
                const EmbeddedRawAssetEntry& rawEntry = rawAssets[i];

                std::string rawDataVar = SanitizeCppIdentifier("RawAsset_" + std::to_string(i) + "_" + rawEntry.mLookupKey) + "_Data";
                uint32_t rawDataSize = 0;
                std::string rawSourceString;
                ConvertFileToByteString(
                    rawEntry.mAbsolutePath,
                    rawDataVar,
                    rawSourceString,
                    rawDataSize);

                if (rawDataSize > 10u * 1024u * 1024u)
                {
                    LogWarning("Embedded raw asset '%s' is %.1f MB. The hex-byte embed format scales poorly past ~10 MB; expect long compile times.",
                               rawEntry.mLookupKey.c_str(), double(rawDataSize) / (1024.0 * 1024.0));
                }

                fprintf(sourceFile, "%s", rawSourceString.c_str());

                rawInitializer += "{\"" + rawEntry.mLookupKey + "\"," +
                                  rawDataVar + "," +
                                  std::to_string(rawDataSize) + "," +
                                  (rawEntry.mEngineAsset ? "true" : "false") +
                                  "},\n";
            }

            fprintf(sourceFile, "\n\nuint32_t gNumEmbeddedRawAssets = %d;\n", uint32_t(rawAssets.size()));
            if (rawAssets.size() > 0)
            {
                fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedRawAssets[] = \n{\n");
                fprintf(sourceFile, "%s", rawInitializer.c_str());
                fprintf(sourceFile, "\n};\n");
            }
            else
            {
                fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedRawAssets[] = { {} };\n");
            }

            // Static initializer: register the embedded raw asset table with
            // SystemUtils so SYS_LookupEmbeddedRawAsset / SYS_AcquireFileData
            // can resolve embedded paths at runtime. Editor builds never link
            // this file in, so registration only happens in shipped builds.
            fprintf(sourceFile, "\nvoid SYS_RegisterEmbeddedRawAssets(EmbeddedFile* table, uint32_t count);\n");
            fprintf(sourceFile, "namespace { struct EmbeddedRawAssetsRegistrar { EmbeddedRawAssetsRegistrar() { SYS_RegisterEmbeddedRawAssets(gEmbeddedRawAssets, gNumEmbeddedRawAssets); } }; static EmbeddedRawAssetsRegistrar sEmbeddedRawAssetsRegistrar; }\n");
        }

        {
            std::string sourceString;
            uint32_t dataSize = 0;
            ConvertFileToByteString(
                GetEngineState()->mProjectDirectory + "Config.ini",
                "gEmbeddedConfig_Data",
                sourceString,
                dataSize);

            fprintf(sourceFile, "%s", sourceString.c_str());

            fprintf(sourceFile, "extern const uint32_t gEmbeddedConfig_Size = %d;\n", dataSize);

        }

        fclose(headerFile);
        headerFile = nullptr;
        fclose(sourceFile);
        sourceFile = nullptr;
    }
}

void ActionManager::GenerateEmbeddedScriptFiles(
    std::vector<std::string> files,
    const char* headerPath,
    const char* sourcePath)
{
    FILE* headerFile = fopen(headerPath, "w");
    FILE* sourceFile = fopen(sourcePath, "w");

    OCT_ASSERT(headerFile != nullptr && sourceFile != nullptr);

    if (headerFile != nullptr && sourceFile != nullptr)
    {
        fprintf(headerFile, "#include <stdint.h>\n");
        fprintf(headerFile, "#include \"EmbeddedFile.h\"\n\n");

        fprintf(headerFile, "extern uint32_t gNumEmbeddedScripts;\n");
        fprintf(headerFile, "extern EmbeddedFile gEmbeddedScripts[];\n\n");

        fprintf(sourceFile, "#include <stdint.h>\n");
        fprintf(sourceFile, "#include \"EmbeddedFile.h\"\n\n");

        std::string initializer;

        for (int32_t i = 0; i < int32_t(files.size()); ++i)
        {
            std::string luaFile = files[i];
            std::string luaClass = ScriptUtils::GetClassNameFromFileName(luaFile);

            Stream stream;

            // Handle special case for level
            stream.ReadFile(luaFile.c_str(), false);
            uint32_t size = uint32_t(stream.GetSize());
            char* data = stream.GetData();

            std::string sourceString;
            sourceString.reserve(2048);

            std::string fileDataVar = luaClass + "_Data";
            sourceString += "const char ";
            sourceString += fileDataVar;
            sourceString += "[] = \n{\n";

            for (uint32_t byte = 0; byte < size; ++byte)
            {
                char byteString[8] = {};
                sprintf(byteString, "'\\x%02X',", uint8_t(data[byte]));

                sourceString += byteString;

                if (byte % 8 == 7)
                {
                    sourceString += "\n";
                }
            }

            sourceString += "\n};\n\n";

            fprintf(sourceFile, "%s", sourceString.c_str());

            initializer += "{" + ("\"" + luaClass + "\",") +
                (fileDataVar + ",") +
                (std::to_string(size) + ",") +
                (/*engineFile ? "true" : */"false") +
                "}, \n";
        }

        fprintf(sourceFile, "\n\nuint32_t gNumEmbeddedScripts = %d;\n", uint32_t(files.size()));

        if (files.size() > 0)
        {
            fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedScripts[] = \n{\n");
            fprintf(sourceFile, "%s", initializer.c_str());
            fprintf(sourceFile, "\n};\n");
        }
        else
        {
            fprintf(sourceFile, "\n\nEmbeddedFile gEmbeddedScripts[] = { {} };\n");
        }

        fclose(headerFile);
        headerFile = nullptr;
        fclose(sourceFile);
        sourceFile = nullptr;
    }
}

void ActionManager::GatherScriptFiles(const std::string& dir, std::vector<std::string>& outFiles)
{
    // Recursively iterate through the Script directory and find .lua files.
    std::function<void(std::string)> searchDirectory = [&](std::string dirPath)
    {
        std::vector<std::string> subDirectories;
        DirEntry dirEntry = { };

        SYS_OpenDirectory(dirPath, dirEntry);

        while (dirEntry.mValid)
        {
            if (dirEntry.mDirectory)
            {
                // Ignore this directory and parent directory.
                if (dirEntry.mFilename[0] != '.')
                {
                    subDirectories.push_back(dirEntry.mFilename);
                }
            }
            else
            {
                const char* extension = strrchr(dirEntry.mFilename, '.');

                if (strcmp(dirEntry.mFilename, "LuaPanda.lua") != 0 &&
                    extension != nullptr &&
                    strcmp(extension, ".lua") == 0)
                {
                    std::string path = dirPath + dirEntry.mFilename;
                    outFiles.push_back(path);
                }
            }

            SYS_IterateDirectory(dirEntry);
        }

        SYS_CloseDirectory(dirEntry);

        // Discover files of subdirectories.
        for (uint32_t i = 0; i < subDirectories.size(); ++i)
        {
            std::string subDirPath = dirPath + subDirectories[i] + "/";
            searchDirectory(subDirPath);
        }
    };

    searchDirectory(dir);
}

void ActionManager::ClearWorld()
{
    GetEditorState()->SetSelectedNode(nullptr);
    //SetActiveLevel(nullptr);
    GetWorld(0)->Clear();

    ResetUndoRedo();
}

void ActionManager::DeleteAllNodes()
{
    if (!IsPlayingInEditor())
    {
        GetEditorState()->SetSelectedNode(nullptr);

        if (GetWorld(0)->GetRootNode() != nullptr)
        {
            EXE_DeleteNode(GetWorld(0)->GetRootNode());
        }
    }

}

void ActionManager::RecaptureAndSaveAllScenes()
{
    std::unordered_map<std::string, AssetStub*>& assetMap = AssetManager::Get()->GetAssetMap();

    // This will load all assets!
    for (auto& pair : assetMap)
    {
        if (pair.second->mType == Scene::GetStaticType())
        {
            Asset* asset = AssetManager::Get()->LoadAsset(*pair.second);
            Scene* scene = static_cast<Scene*>(asset);
            OCT_ASSERT(scene != nullptr);

            NodePtr temp = scene->Instantiate();
            scene->Capture(temp.Get());

            temp->Destroy();
            temp = nullptr;

            AssetManager::Get()->SaveAsset(*pair.second);
        }
    }
}

void ActionManager::ResaveAllAssets()
{
    std::unordered_map<std::string, AssetStub*>& assetMap = AssetManager::Get()->GetAssetMap();

    // This action is really meant for doing project-wide data updates when adding new serialized data to an asset.
    // It is important that assets are only loaded once if the LoadStream() / WriteStream() funcs are different
    for (auto& pair : assetMap)
    {
        Asset* asset = AssetManager::Get()->LoadAsset(*pair.second);
        AssetManager::Get()->SaveAsset(*pair.second);
    }

    // Refsweep afterwards to 
    AssetManager::Get()->RefSweep();
}

void ActionManager::DeleteAsset(AssetStub* stub)
{
    if (stub != nullptr)
    {
        if (GetEditorState()->GetSelectedAssetStub() == stub)
        {
            GetEditorState()->SetSelectedAssetStub(nullptr);
        }

        if (GetEditorState()->GetInspectedObject() == stub->mAsset)
        {
            GetEditorState()->InspectObject(nullptr, true);
        }

        GetEditorState()->RemoveFromInspectHistory(stub->mAsset);

        std::string path = stub->mPath;
        std::string assetName = stub->mName;
        AssetManager::Get()->PurgeAsset(stub->mName.c_str());
        SYS_RemoveFile(path.c_str());

        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->FireOnAssetDeleted(assetName.c_str());
    }
    else
    {
        LogWarning("Can't delete null asset stub.");
    }
}

void ActionManager::DeleteAssetDir(AssetDir* dir)
{
    if (dir != nullptr &&
        dir->mParentDir != nullptr &&
        dir->mParentDir != AssetManager::Get()->GetRootDirectory())
    {
        std::string path = dir->mPath;
        dir->mParentDir->DeleteSubdirectory(dir->mName.c_str());
        SYS_RemoveDirectory(path.c_str());
    }
    else
    {
        LogWarning("Can't delete null asset dir.");
    }
}

bool ActionManager::DuplicateNodes(std::vector<Node*> srcNodes)
{
    bool duplicated = false;

    // Don't use a vector reference for nodes param because we are going to modify the vector anyway.
    std::vector<Node*> dupedNodes;
    RemoveRedundantDescendants(srcNodes);

    OCT_ASSERT(srcNodes.size() > 0);
    dupedNodes = EXE_SpawnNodes(srcNodes);

    if (dupedNodes.size() > 0 &&
        dupedNodes.size() == srcNodes.size())
    {
        duplicated = true;

        for (uint32_t i = 0; i < srcNodes.size(); ++i)
        {
            Node* srcNode = srcNodes[i];
            Node* newNode = dupedNodes[i];
            Node* parent = srcNode->GetParent();
            if (parent == nullptr)
            {
                parent = srcNode;
            }

            parent->AddChild(newNode);
        }

        GetEditorState()->SetSelectedNode(nullptr);

        for (uint32_t i = 0; i < dupedNodes.size(); ++i)
        {
            GetEditorState()->AddSelectedNode(dupedNodes[i], false);
        }
    }

    return duplicated;
}

void ActionManager::AttachSelectedNodes(Node* newParent, int32_t boneIdx)
{
    if (newParent == nullptr)
        return;

    if (newParent->IsSceneLinked())
    {
        LogWarning("Cannot add child to scene-linked node. Unlink the scene first.");
        return;
    }

    std::vector<Node*> selNodes = GetEditorState()->GetSelectedNodes();

    if (selNodes.size() == 0)
        return;

    for (uint32_t i = 0; i < selNodes.size(); ++i)
    {
        Node* child = selNodes[i];
        Node* parent = newParent;

        if (child == parent)
            continue;

        if (parent->GetParent() == child)
            continue;

        int32_t boneIndex = -1;

        if (child->As<Node3D>())
        {
            boneIndex = child->As<Node3D>()->GetParentBoneIndex();
        }

        if (child->GetParent() != parent ||
            boneIdx != boneIndex)
        {
            // TODO: Do one EXE for all nodes so user doesn't have to CTRL+Z N times.
            ActionManager::Get()->EXE_AttachNode(child, parent, -1, boneIdx);

            // Reparenting components should break the scene link.
            // For now, you cannot override scene instance children
            if (newParent->GetParent() != GetWorld(0)->GetRootNode())
            {
                newParent->SetScene(nullptr);
            }
        }
    }

}

// ---------------------------
// --------- ACTIONS ---------
// ---------------------------

static void MarkEditSceneDirty()
{
    Scene* sceneToDirty = nullptr;
    EditScene* editScene = GetEditorState()->GetEditScene();
    if (editScene)
    {
        sceneToDirty = editScene->mSceneAsset.Get<Scene>();
    }

    if (sceneToDirty)
    {
        sceneToDirty->SetDirtyFlag();
    }
}
void Action::Execute()
{
    MarkEditSceneDirty();
}

void Action::Reverse()
{
    MarkEditSceneDirty();
}

ActionEditProperty::ActionEditProperty(
    void* owner,
    PropertyOwnerType ownerType,
    const std::string& propName,
    uint32_t index,
    Datum value)
{
    mOwner = owner;
    mOwnerType = ownerType;
    mPropertyName = propName;
    mIndex = index;
    mValue = value;

    // Keep a reference
    if (mOwnerType == PropertyOwnerType::Asset)
    {
        mReferencedAsset = (Asset*)mOwner;
    }
}

void ActionEditProperty::GatherProps(std::vector<Property>& props)
{
    if (mOwnerType == PropertyOwnerType::Node)
    {
        Node* node = (Node*)mOwner;
        node->GatherProperties(props);
    }
    else if (mOwnerType == PropertyOwnerType::Asset)
    {
        Asset* asset = (Asset*)mReferencedAsset.Get<Asset>();
        if (asset)
        {
            asset->GatherProperties(props);
        }
    }
    else if (mOwnerType == PropertyOwnerType::Global)
    {
        GatherGlobalProperties(props);
    }
}

Property* ActionEditProperty::FindProp(std::vector<Property>& props, const std::string& name)
{
    Property* prop = nullptr;

    for (uint32_t i = 0; i < props.size(); ++i)
    {
        if (props[i].mName == name)
        {
            prop = &props[i];
            break;
        }
    }

    return prop;
}

void ActionEditProperty::Execute()
{
    Action::Execute();

    std::vector<Property> sProps;
    GatherProps(sProps);

    Property* prop = FindProp(sProps, mPropertyName);

    if (prop != nullptr)
    {
        if (prop->IsVector() && !prop->IsExternal() && mIndex >= prop->GetCount())
        {
            // In the case of Asset properties, they will be nil if not set, thus
            // when we regather script properties at the start of this function, the count will not include
            // those nil values.
            mIndex = prop->GetCount();
            prop->SetCount(prop->GetCount() + 1);
        }

        // TODO-NODE: Support undo/redo for AddPropertyArray. Until then, don't set value on OOB elements.
        if (mIndex < prop->GetCount())
        {
            mPreviousValue.Destroy();
            mPreviousValue.SetType(prop->GetType());
            mPreviousValue.SetCount(1);
            mPreviousValue.SetValue(prop->GetValue(mIndex));

            prop->SetValue(mValue.mData.vp, mIndex, 1);

            // TODO-NODE: Delete this? Shouldn't be necessary anymore now that we are using Imgui
            //   and gathering properties every frame.
#if 0
            // Script properties are stored internally and propagated to scripts
            // so after setting the value we need to refresh property widgets.
                if (!prop->IsExternal())
                {
                    PanelManager::Get()->GetPropertiesPanel()->RefreshProperties();
            }
#endif
        }
    }
}

void ActionEditProperty::Reverse()
{
    Action::Reverse();

    std::vector<Property> sProps;
    GatherProps(sProps);

    Property* prop = FindProp(sProps, mPropertyName);

    if (prop != nullptr)
    {
        // TODO-NODE: Support undo/redo for AddPropertyArray. Until then, don't set value on OOB elements.
        if (prop->GetCount() > mIndex)
        {
            prop->SetValue(mPreviousValue.GetValue(0), mIndex, 1);
        }

        // TODO-NODE: Delete this? Shouldn't be necessary anymore now that we are using Imgui
        //   and gathering properties every frame.
#if 0
        // Script properties are stored internally and propagated to scripts
        // so after setting the value we need to refresh property widgets.
        if (!prop->IsExternal())
        {
            PanelManager::Get()->GetPropertiesPanel()->RefreshProperties();
        }
#endif
    }
}

ActionEditTransforms::ActionEditTransforms(
    const std::vector<Node3D*>& nodes,
    const std::vector<glm::mat4>& newTransforms)
{
    mNodes = nodes;
    mNewTransforms = newTransforms;

    OCT_ASSERT(mNodes.size() == mNewTransforms.size());
}

void ActionEditTransforms::Execute()
{
    Action::Execute();

    mPrevTransforms.clear();

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        mPrevTransforms.push_back(mNodes[i]->GetTransform());
        mNodes[i]->SetTransform(mNewTransforms[i]);
    }
}

void ActionEditTransforms::Reverse()
{
    Action::Reverse();

    OCT_ASSERT(mPrevTransforms.size() == mNodes.size());

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        mNodes[i]->SetTransform(mPrevTransforms[i]);
    }
}

ActionEditWidgetTransforms::ActionEditWidgetTransforms(
    const std::vector<Widget*>& widgets,
    const std::vector<WidgetTransformData>& newTransforms)
{
    mWidgets = widgets;
    mNewTransforms = newTransforms;

    OCT_ASSERT(mWidgets.size() == mNewTransforms.size());
}

void ActionEditWidgetTransforms::Execute()
{
    Action::Execute();

    mPrevTransforms.clear();

    for (uint32_t i = 0; i < mWidgets.size(); ++i)
    {
        WidgetTransformData prev;
        prev.mOffset = mWidgets[i]->GetOffset();
        prev.mSize = mWidgets[i]->GetSize();
        prev.mRotation = mWidgets[i]->GetRotation();
        mPrevTransforms.push_back(prev);

        mWidgets[i]->SetOffset(mNewTransforms[i].mOffset.x, mNewTransforms[i].mOffset.y);
        mWidgets[i]->SetSize(mNewTransforms[i].mSize.x, mNewTransforms[i].mSize.y);
        mWidgets[i]->SetRotation(mNewTransforms[i].mRotation);
    }
}

void ActionEditWidgetTransforms::Reverse()
{
    Action::Reverse();

    OCT_ASSERT(mPrevTransforms.size() == mWidgets.size());

    for (uint32_t i = 0; i < mWidgets.size(); ++i)
    {
        mWidgets[i]->SetOffset(mPrevTransforms[i].mOffset.x, mPrevTransforms[i].mOffset.y);
        mWidgets[i]->SetSize(mPrevTransforms[i].mSize.x, mPrevTransforms[i].mSize.y);
        mWidgets[i]->SetRotation(mPrevTransforms[i].mRotation);
    }
}

ActionSpawnNodes::ActionSpawnNodes(const std::vector<TypeId>& types)
{
    mSrcTypes = types;
}

ActionSpawnNodes::ActionSpawnNodes(const std::vector<const char*>& typeNames)
{
    mSrcTypeNames = typeNames;
}

ActionSpawnNodes::ActionSpawnNodes(const std::vector<SceneRef>& scenes)
{
    mSrcScenes = scenes;
}

ActionSpawnNodes::ActionSpawnNodes(const std::vector<Node*>& srcNodes)
{
    std::vector<Node*> trimmedSrcNodes = srcNodes;
    RemoveRedundantDescendants(trimmedSrcNodes);

    mSrcNodes.resize(trimmedSrcNodes.size());

    for (uint32_t i = 0; i < mSrcNodes.size(); ++i)
    {
        mSrcNodes[i] = ResolvePtr(trimmedSrcNodes[i]);
    }
}

void ActionSpawnNodes::Execute()
{
    Action::Execute();

    if (mNodes.size() == 0)
    {
        // First time executing this action. We need to create the nodes from the src data.
        if (mSrcTypes.size() > 0)
        {
            for (uint32_t i = 0; i < mSrcTypes.size(); ++i)
            {
                NodePtr newNode = Node::Construct(mSrcTypes[i]);
                OCT_ASSERT(newNode);
                mNodes.push_back(newNode);
            }
        }
        else if (mSrcTypeNames.size() > 0)
        {
            for (uint32_t i = 0; i < mSrcTypeNames.size(); ++i)
            {
                NodePtr newNode = Node::Construct(mSrcTypeNames[i]);
                OCT_ASSERT(newNode);
                mNodes.push_back(newNode);
            }
        }
        else if (mSrcScenes.size() > 0)
        {
            for (uint32_t i = 0; i < mSrcScenes.size(); ++i)
            {
                Scene* scene = mSrcScenes[i].Get<Scene>();
                if (scene != nullptr)
                {
                    NodePtr newNode = scene->Instantiate();
                    OCT_ASSERT(newNode);
                    mNodes.push_back(newNode);
                }
                else
                {
                    LogError("Null scene in ActionSpawnNodes::Execute()");
                }
            }
        }
        else if (mSrcNodes.size() > 0)
        {
            for (uint32_t i = 0; i < mSrcNodes.size(); ++i)
            {
                OCT_ASSERT(mSrcNodes[i] != nullptr);

                NodePtr newNode = mSrcNodes[i]->Clone(true, true, true);
                OCT_ASSERT(newNode);
                mNodes.push_back(newNode);
            }
        }
        else
        {
            LogError("Invalid src data for ActionSpawnNodes!");
        }
    }
    else 
    {
        // Second time and beyond. Restore exiled nodes and attach to correct parents.
        for (uint32_t i = 0; i < mNodes.size(); ++i)
        {
            ActionManager::Get()->RestoreExiledNode(mNodes[i]);

            if (mParents[i] != nullptr)
            {
                mNodes[i]->Attach(mParents[i].Get());
            }
            else
            {
                // This must have been the root node?
                OCT_ASSERT(mNodes.size() == 1);
                OCT_ASSERT(GetWorld(0)->GetRootNode() == nullptr);
                GetWorld(0)->SetRootNode(mNodes[i].Get());
            }
        }
    }
}

void ActionSpawnNodes::Reverse()
{
    Action::Reverse();

    if (mParents.size() == 0)
    {
        // First time reversing, track the parents and parent scene-links.
        for (uint32_t i = 0; i < mNodes.size(); ++i)
        {
            Node* parent = mNodes[i]->GetParent();
            mParents.push_back(ResolvePtr(parent));
        }
    }

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        if (mParents[i] != nullptr)
        {
            mNodes[i]->Detach();
        }
        else
        {
            OCT_ASSERT(mNodes.size() == 1);
            OCT_ASSERT(mNodes[i] == GetWorld(0)->GetRootNode());
            GetWorld(0)->SetRootNode(nullptr);
        }

        ActionManager::Get()->ExileNode(mNodes[i]);
    }
}

ActionDeleteNodes::ActionDeleteNodes(const std::vector<Node*>& nodes)
{
    std::vector<Node*> trimmedNodes = nodes;
    RemoveRedundantDescendants(trimmedNodes);

    mNodes.resize(trimmedNodes.size());
    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        mNodes[i] = ResolvePtr(nodes[i]);
    }

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        mParents.push_back(ResolvePtr(mNodes[i]->GetParent()));
        if (mParents[i] != nullptr)
        {
            int32_t childIdx = mParents[i]->FindChildIndex(mNodes[i].Get());
            OCT_ASSERT(childIdx != -1);
            mChildIndices.push_back(childIdx);

            Node3D* node3d = mNodes[i]->As<Node3D>();
            if (node3d != nullptr)
            {
                mBoneIndices.push_back(node3d->GetParentBoneIndex());
            }
            else
            {
                mBoneIndices.push_back(-1);
            }
        }
        else
        {
            // This must be the root node being deleted.
            OCT_ASSERT(mNodes.size() == 1);

            mChildIndices.push_back(-1);
            mBoneIndices.push_back(-1);
        }
    }
}

void ActionDeleteNodes::Execute()
{
    Action::Execute();

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        // Actor is already spawned at this point.
        OCT_ASSERT(mNodes[i]->GetWorld() != nullptr);

        if (IsPlayingInEditor())
        {
            mNodes[i]->Destroy();
        }
        else
        {
            if (mParents[i] != nullptr)
            {
                mNodes[i]->Detach();
            }
            else
            {
                // We must be deleting the root node
                OCT_ASSERT(mNodes.size() == 1);
                OCT_ASSERT(mNodes[i] == GetWorld(0)->GetRootNode());
                GetWorld(0)->SetRootNode(nullptr);
            }

            ActionManager::Get()->ExileNode(mNodes[i]);
        }
    }
}

void ActionDeleteNodes::Reverse()
{
    Action::Reverse();

    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        ActionManager::Get()->RestoreExiledNode(mNodes[i]);

        if (mParents[i] != nullptr)
        {
            if (mBoneIndices[i] >= 0 &&
                mParents[i]->As<SkeletalMesh3D>() &&
                mNodes[i]->As<Node3D>())
            {
                // This node was attached to a bone!
                mNodes[i]->As<Node3D>()->AttachToBone(
                    mParents[i]->As<SkeletalMesh3D>(),
                    mBoneIndices[i], 
                    false, 
                    mChildIndices[i]);
            }
            else
            {
                // Normal attachment
                mParents[i]->AddChild(mNodes[i].Get(), mChildIndices[i]);
            }
            // TODO: Support attaching to the correct bone. 
            // Probably need to add extra parameter to Attach() to include child index.
        }
        else
        {
            // Must have deleted the root node.
            OCT_ASSERT(mNodes.size() == 1);
            OCT_ASSERT(GetWorld(0)->GetRootNode() == nullptr);
            GetWorld(0)->SetRootNode(mNodes[i].Get());
        }
    }
}

ActionAttachNode::ActionAttachNode(Node* node, Node* newParent, int32_t childIndex, int32_t boneIndex)
{
    mNode = ResolvePtr(node);
    mNewParent = ResolvePtr(newParent);
    mPrevParent = ResolvePtr(node->GetParent());
    mChildIndex = childIndex;
    mPrevChildIndex = node->GetParent() ? node->GetParent()->FindChildIndex(node) : -1;
    mBoneIndex = boneIndex;
    mPrevBoneIndex = node->IsNode3D() ? node->As<Node3D>()->GetParentBoneIndex() : -1;
    OCT_ASSERT(mNode);
    OCT_ASSERT(mNewParent);
}

void ActionAttachNode::Execute()
{
    Action::Execute();

    if (mBoneIndex >= 0 &&
        mNewParent != nullptr &&
        mNewParent->As<SkeletalMesh3D>() &&
        mNode->As<Node3D>())
    {
        Node3D* node3d = mNode->As<Node3D>();
        SkeletalMesh3D* skParent = mNewParent->As<SkeletalMesh3D>();

        node3d->AttachToBone(skParent, mBoneIndex, true, mChildIndex);
    }
    else
    {
       mNode->Attach(mNewParent.Get(), true, mChildIndex);
    }
}

void ActionAttachNode::Reverse()
{
    Action::Reverse();

    if (mPrevBoneIndex >= 0 &&
        mPrevParent != nullptr &&
        mPrevParent->As<SkeletalMesh3D>() &&
        mNode->As<Node3D>())
    {
        Node3D* node3d = mNode->As<Node3D>();
        SkeletalMesh3D* skParent = mPrevParent->As<SkeletalMesh3D>();
        node3d->AttachToBone(skParent, mPrevBoneIndex, true, mPrevChildIndex);
    }
    else
    {
        mNode->Attach(mPrevParent.Get(), true, mPrevChildIndex);
    }
}

ActionSetRootNode::ActionSetRootNode(Node* newRoot)
{
    mNewRoot = ResolvePtr(newRoot);
    mOldRoot = ResolvePtr(GetWorld(0)->GetRootNode());
    mNewRootParent = ResolvePtr(mNewRoot->GetParent());
    mNewRootChildIndex = mNewRootParent ? mNewRootParent->FindChildIndex(mNewRoot.Get()) : -1;

    OCT_ASSERT(mNewRoot != mOldRoot);
    OCT_ASSERT(mNewRoot != nullptr);
    OCT_ASSERT(mOldRoot != nullptr);
    OCT_ASSERT(mNewRootParent != nullptr && mNewRootChildIndex != -1);
}

void ActionSetRootNode::Execute()
{
    Action::Execute();

    mNewRoot->Detach(true);
    GetWorld(0)->SetRootNode(mNewRoot.Get());
    mOldRoot->Attach(mNewRoot.Get(), true);
    mOldRoot->SetScene(nullptr);
}

void ActionSetRootNode::Reverse()
{
    Action::Reverse();

    mOldRoot->Detach(true);
    GetWorld(0)->SetRootNode(mOldRoot.Get());
    mNewRoot->Attach(mNewRootParent.Get(), true, mNewRootChildIndex);
    mNewRoot->SetScene(nullptr);
}

ActionSetWorldRotation::ActionSetWorldRotation(Node3D* node, glm::quat rot)
{
    mNode = node;
    mNewRotation = rot;
    mPrevRotation = node->GetWorldRotationQuat();
    OCT_ASSERT(mNode);
}

void ActionSetWorldRotation::Execute()
{
    Action::Execute();

    mNode->SetWorldRotation(mNewRotation);
}

void ActionSetWorldRotation::Reverse()
{
    Action::Reverse();

    mNode->SetWorldRotation(mPrevRotation);
}

ActionSetWorldPosition::ActionSetWorldPosition(Node3D* node, glm::vec3 pos)
{
    mNode = node;
    mNewPosition = pos;
    mPrevPosition = node->GetWorldPosition();
    OCT_ASSERT(mNode);
}

void ActionSetWorldPosition::Execute()
{
    Action::Execute();

    mNode->SetWorldPosition(mNewPosition);
}

void ActionSetWorldPosition::Reverse()
{
    Action::Reverse();

    mNode->SetWorldPosition(mPrevPosition);
}

ActionSetWorldScale::ActionSetWorldScale(Node3D* node, glm::vec3 scale)
{
    mNode = node;
    mNewScale = scale;
    mPrevScale = node->GetWorldScale();
    OCT_ASSERT(mNode);
}

void ActionSetWorldScale::Execute()
{
    Action::Execute();

    mNode->SetWorldScale(mNewScale);
}

void ActionSetWorldScale::Reverse()
{
    Action::Reverse();

    mNode->SetWorldScale(mPrevScale);
}

ActionUnlinkScene::ActionUnlinkScene(Node* node)
{
    mNode = node;
    mScene = node->GetScene();
    OCT_ASSERT(mNode);
}

void ActionUnlinkScene::Execute()
{
    Action::Execute();

    mNode->BreakSceneLink();
}

void ActionUnlinkScene::Reverse()
{
    Action::Reverse();

    mNode->SetScene(mScene.Get<Scene>());
}

ActionResetScene::ActionResetScene(Node* node)
{
    mNode = node;
    mScene = node->GetScene();
    OCT_ASSERT(mNode);
    OCT_ASSERT(mScene != nullptr);

    // Save current properties for undo
    mNode->GatherProperties(mPrevProperties);
}

void ActionResetScene::Execute()
{
    Action::Execute();

    Scene* scene = mScene.Get<Scene>();
    if (scene == nullptr)
        return;

    const std::vector<Property>* sceneProps = scene->GetRootNodeProperties();
    if (sceneProps == nullptr)
        return;

    // Get current node properties and copy scene values over them
    std::vector<Property> nodeProps;
    mNode->GatherProperties(nodeProps);
    CopyPropertyValues(nodeProps, *sceneProps);
}

void ActionResetScene::Reverse()
{
    Action::Reverse();

    // Restore the previous property values
    std::vector<Property> nodeProps;
    mNode->GatherProperties(nodeProps);
    CopyPropertyValues(nodeProps, mPrevProperties);
}

ActionSetInstanceColors::ActionSetInstanceColors(const std::vector<ActionSetInstanceColorsData>& data)
{
    // Don't call this action with no color data to change.
    OCT_ASSERT(data.size() > 0);

    mData = data;

    mPrevData.resize(mData.size());
    for (uint32_t i = 0; i < data.size(); ++i)
    {
        mPrevData[i].mMesh3d = mData[i].mMesh3d;
        mPrevData[i].mColors = mData[i].mMesh3d->GetInstanceColors();
        mPrevData[i].mBakedLight = mData[i].mMesh3d->HasBakedLighting();
    }
}

void ActionSetInstanceColors::Execute()
{
    Action::Execute();

    for (uint32_t i = 0; i < mData.size(); ++i)
    {
        mData[i].mMesh3d->SetInstanceColors(mData[i].mColors, mData[i].mBakedLight);
    }
}

void ActionSetInstanceColors::Reverse()
{
    Action::Reverse();

    for (uint32_t i = 0; i < mPrevData.size(); ++i)
    {
        mPrevData[i].mMesh3d->SetInstanceColors(mPrevData[i].mColors, mPrevData[i].mBakedLight);
    }
}

ActionSetInstanceData::ActionSetInstanceData(InstancedMesh3D* instMesh, int32_t startIndex, const std::vector<MeshInstanceData>& data)
{
    mInstMesh = instMesh;
    mData = data;
    mStartIndex = startIndex;
    mData = data;

    if (mStartIndex < 0)
    {
        // Negative start index means set entire array of instance data
        mPrevData = mInstMesh->GetInstanceData();
    }
    else
    {
        // We are only setting a subset of the data
        for (int32_t i = mStartIndex; (i < (int32_t)mInstMesh->GetNumInstances()) && (i < mStartIndex + data.size()); ++i)
        {
            mPrevData.push_back(mInstMesh->GetInstanceData(i));
        }
    }
}

void ActionSetInstanceData::Execute()
{
    Action::Execute();

    if (mStartIndex < 0)
    {
        mInstMesh->SetInstanceData(mData);
    }
    else
    {
        // Remove X num of instance data
        int32_t removeEnd = int32_t(mStartIndex + mData.size());
        removeEnd = glm::min<uint32_t>(removeEnd, (int32_t)mInstMesh->GetNumInstances());
        for (int32_t i = removeEnd - 1; i >= mStartIndex; --i)
        {
            mInstMesh->RemoveInstanceData(i);
        }

        // Add X num of new instance data
        for (int32_t i = 0; i < mData.size(); ++i)
        {
            mInstMesh->AddInstanceData(mData[i], mStartIndex + i);
        }
    }
}

void ActionSetInstanceData::Reverse()
{
    Action::Reverse();

    if (mStartIndex < 0)
    {
        mInstMesh->SetInstanceData(mPrevData);
    }
    else
    {
        // Remove X num of instance data
        int32_t removeEnd = int32_t(mStartIndex + mPrevData.size());
        removeEnd = glm::min<uint32_t>(removeEnd, (int32_t)mInstMesh->GetNumInstances());
        for (int32_t i = removeEnd - 1; i >= mStartIndex; --i)
        {
            mInstMesh->RemoveInstanceData(i);
        }

        // Add X num of new instance data
        for (int32_t i = 0; i < mPrevData.size(); ++i)
        {
            mInstMesh->AddInstanceData(mPrevData[i], mStartIndex + i);
        }
    }
}

// ---------------------------------------------------------------------------
// ActionReplaceWithAsset
// ---------------------------------------------------------------------------

ActionReplaceWithAsset::ActionReplaceWithAsset(Asset* asset, const std::vector<Node*>& nodes)
{
    OCT_ASSERT(asset != nullptr);
    mAsset = asset;

    TypeId assetType = asset->GetType();

    if (assetType == StaticMesh::GetStaticType())
    {
        mMode = ReplaceMode::StaticMesh;
        for (Node* n : nodes)
        {
            StaticMesh3D* sm = n->As<StaticMesh3D>();
            if (sm)
            {
                mMeshNodes.push_back(sm);
                mPrevMeshes.push_back(StaticMeshRef(sm->GetStaticMesh()));
            }
        }
    }
    else if (assetType == MaterialLite::GetStaticType() ||
             assetType == MaterialBase::GetStaticType() ||
             assetType == MaterialInstance::GetStaticType())
    {
        mMode = ReplaceMode::Material;
        for (Node* n : nodes)
        {
            Mesh3D* mesh = n->As<Mesh3D>();
            if (mesh)
            {
                mMatNodes.push_back(mesh);
                mPrevMaterials.push_back(MaterialRef(mesh->GetMaterialOverride()));
            }
        }
    }
    else if (assetType == Scene::GetStaticType())
    {
        mMode = ReplaceMode::Scene;
        for (Node* n : nodes)
        {
            Node3D* node3d = n->As<Node3D>();
            if (node3d && node3d->GetParent())
            {
                SceneReplaceEntry entry;
                entry.mOriginal = ResolvePtr(n);
                entry.mParent = ResolvePtr(n->GetParent());
                entry.mChildIndex = n->GetParent()->FindChildIndex(n);
                mSceneEntries.push_back(entry);
            }
        }
    }
    else
    {
        LogWarning("ActionReplaceWithAsset: Unsupported asset type.");
        mMode = ReplaceMode::Invalid;
    }
}

void ActionReplaceWithAsset::Execute()
{
    Action::Execute();

    switch (mMode)
    {
    case ReplaceMode::StaticMesh:
    {
        StaticMesh* mesh = mAsset.Get<StaticMesh>();
        for (uint32_t i = 0; i < mMeshNodes.size(); ++i)
        {
            mMeshNodes[i]->SetStaticMesh(mesh);
        }
        break;
    }
    case ReplaceMode::Material:
    {
        Material* mat = mAsset.Get<Material>();
        for (uint32_t i = 0; i < mMatNodes.size(); ++i)
        {
            mMatNodes[i]->SetMaterialOverride(mat);
        }
        break;
    }
    case ReplaceMode::Scene:
    {
        Scene* scene = mAsset.Get<Scene>();
        if (scene == nullptr)
            break;

        for (uint32_t i = 0; i < mSceneEntries.size(); ++i)
        {
            SceneReplaceEntry& entry = mSceneEntries[i];

            if (entry.mSpawnedNode == nullptr)
            {
                // First execute: spawn and position
                NodePtr spawned = scene->Instantiate();
                if (spawned == nullptr)
                    continue;

                Node3D* orig3d = entry.mOriginal->As<Node3D>();
                Node3D* spawned3d = spawned->As<Node3D>();

                entry.mParent->AddChild(spawned.Get(), entry.mChildIndex);

                if (orig3d && spawned3d)
                {
                    spawned3d->SetWorldPosition(orig3d->GetWorldPosition());
                    spawned3d->SetWorldRotation(orig3d->GetWorldRotationQuat());
                    spawned3d->SetWorldScale(orig3d->GetWorldScale());
                }

                entry.mOriginal->Detach();
                ActionManager::Get()->ExileNode(entry.mOriginal);
                entry.mSpawnedNode = spawned;
            }
            else
            {
                // Re-execute: restore spawned, exile original
                ActionManager::Get()->RestoreExiledNode(entry.mSpawnedNode);
                entry.mParent->AddChild(entry.mSpawnedNode.Get(), entry.mChildIndex);

                entry.mOriginal->Detach();
                ActionManager::Get()->ExileNode(entry.mOriginal);
            }
        }
        break;
    }
    default:
        break;
    }
}

void ActionReplaceWithAsset::Reverse()
{
    Action::Reverse();

    switch (mMode)
    {
    case ReplaceMode::StaticMesh:
    {
        for (uint32_t i = 0; i < mMeshNodes.size(); ++i)
        {
            mMeshNodes[i]->SetStaticMesh(mPrevMeshes[i].Get<StaticMesh>());
        }
        break;
    }
    case ReplaceMode::Material:
    {
        for (uint32_t i = 0; i < mMatNodes.size(); ++i)
        {
            mMatNodes[i]->SetMaterialOverride(mPrevMaterials[i].Get<Material>());
        }
        break;
    }
    case ReplaceMode::Scene:
    {
        for (uint32_t i = 0; i < mSceneEntries.size(); ++i)
        {
            SceneReplaceEntry& entry = mSceneEntries[i];

            if (entry.mSpawnedNode != nullptr)
            {
                entry.mSpawnedNode->Detach();
                ActionManager::Get()->ExileNode(entry.mSpawnedNode);
            }

            ActionManager::Get()->RestoreExiledNode(entry.mOriginal);
            entry.mParent->AddChild(entry.mOriginal.Get(), entry.mChildIndex);
        }
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// ActionReplaceWithInstancedMesh
// ---------------------------------------------------------------------------

ActionReplaceWithInstancedMesh::ActionReplaceWithInstancedMesh(const std::vector<Node*>& nodes, bool merge)
{
    // Collect valid StaticMesh3D nodes (excluding InstancedMesh3D)
    std::vector<StaticMesh3D*> validNodes;
    for (Node* n : nodes)
    {
        StaticMesh3D* sm = n->As<StaticMesh3D>();
        if (sm && !sm->IsInstancedMesh3D() && sm->GetStaticMesh() != nullptr)
        {
            validNodes.push_back(sm);
        }
    }

    if (merge)
    {
        // Group by mesh pointer: all nodes sharing a mesh become one InstancedMesh3D
        std::map<StaticMesh*, std::vector<StaticMesh3D*>> groups;
        for (StaticMesh3D* sm : validNodes)
        {
            groups[sm->GetStaticMesh()].push_back(sm);
        }

        for (auto& pair : groups)
        {
            GroupEntry group;
            StaticMesh3D* firstNode = pair.second[0];

            for (StaticMesh3D* sm : pair.second)
            {
                group.mOriginalNodes.push_back(ResolvePtr(sm));
                group.mOriginalParents.push_back(ResolvePtr(sm->GetParent()));
                group.mOriginalChildIndices.push_back(
                    sm->GetParent() ? sm->GetParent()->FindChildIndex(sm) : -1);
            }

            group.mInstancedParent = ResolvePtr(firstNode->GetParent());
            group.mInstancedChildIndex = firstNode->GetParent()
                ? firstNode->GetParent()->FindChildIndex(firstNode)
                : -1;

            mGroups.push_back(group);
        }
    }
    else
    {
        // 1:1 conversion: each StaticMesh3D becomes its own InstancedMesh3D
        for (StaticMesh3D* sm : validNodes)
        {
            GroupEntry group;
            group.mOriginalNodes.push_back(ResolvePtr(sm));
            group.mOriginalParents.push_back(ResolvePtr(sm->GetParent()));
            group.mOriginalChildIndices.push_back(
                sm->GetParent() ? sm->GetParent()->FindChildIndex(sm) : -1);

            group.mInstancedParent = ResolvePtr(sm->GetParent());
            group.mInstancedChildIndex = sm->GetParent()
                ? sm->GetParent()->FindChildIndex(sm)
                : -1;

            mGroups.push_back(group);
        }
    }
}

void ActionReplaceWithInstancedMesh::Execute()
{
    Action::Execute();

    if (mFirstExecute)
    {
        mFirstExecute = false;

        for (GroupEntry& group : mGroups)
        {
            // Create InstancedMesh3D
            NodePtr instNode = Node::Construct(InstancedMesh3D::GetStaticType());
            InstancedMesh3D* inst = instNode->As<InstancedMesh3D>();

            StaticMesh3D* firstSrc = group.mOriginalNodes[0]->As<StaticMesh3D>();
            inst->SetStaticMesh(firstSrc->GetStaticMesh());
            inst->SetMaterialOverride(firstSrc->GetMaterialOverride());
            inst->SetName(firstSrc->GetStaticMesh()->GetName() + "_Instanced");

            // Populate instance data from each source node
            for (uint32_t i = 0; i < group.mOriginalNodes.size(); ++i)
            {
                Node3D* src3d = group.mOriginalNodes[i]->As<Node3D>();
                if (src3d)
                {
                    MeshInstanceData data;
                    data.mPosition = src3d->GetPosition();
                    data.mRotation = src3d->GetWorldRotationEuler();
                    data.mScale = src3d->GetWorldScale();
                    inst->AddInstanceData(data);
                }
            }

            // Attach to parent
            if (group.mInstancedParent != nullptr)
            {
                group.mInstancedParent->AddChild(instNode.Get(), group.mInstancedChildIndex);
            }

            group.mInstancedNode = instNode;

            // Exile all original nodes
            for (uint32_t i = 0; i < group.mOriginalNodes.size(); ++i)
            {
                group.mOriginalNodes[i]->Detach();
                ActionManager::Get()->ExileNode(group.mOriginalNodes[i]);
            }
        }
    }
    else
    {
        // Re-execute: restore instanced nodes, exile originals
        for (GroupEntry& group : mGroups)
        {
            ActionManager::Get()->RestoreExiledNode(group.mInstancedNode);
            if (group.mInstancedParent != nullptr)
            {
                group.mInstancedParent->AddChild(group.mInstancedNode.Get(), group.mInstancedChildIndex);
            }

            for (uint32_t i = 0; i < group.mOriginalNodes.size(); ++i)
            {
                group.mOriginalNodes[i]->Detach();
                ActionManager::Get()->ExileNode(group.mOriginalNodes[i]);
            }
        }
    }

    // Select the first instanced mesh
    if (!mGroups.empty() && mGroups[0].mInstancedNode != nullptr)
    {
        GetEditorState()->SetSelectedNode(mGroups[0].mInstancedNode.Get());
    }
}

void ActionReplaceWithInstancedMesh::Reverse()
{
    Action::Reverse();

    for (GroupEntry& group : mGroups)
    {
        // Remove instanced node
        if (group.mInstancedNode != nullptr)
        {
            group.mInstancedNode->Detach();
            ActionManager::Get()->ExileNode(group.mInstancedNode);
        }

        // Restore originals
        for (uint32_t i = 0; i < group.mOriginalNodes.size(); ++i)
        {
            ActionManager::Get()->RestoreExiledNode(group.mOriginalNodes[i]);
            if (group.mOriginalParents[i] != nullptr)
            {
                group.mOriginalParents[i]->AddChild(
                    group.mOriginalNodes[i].Get(),
                    group.mOriginalChildIndices[i]);
            }
        }
    }

    // Select first original node
    if (!mGroups.empty() && !mGroups[0].mOriginalNodes.empty())
    {
        GetEditorState()->SetSelectedNode(mGroups[0].mOriginalNodes[0].Get());
    }
}

// ---------------------------------------------------------------------------
// ActionReplaceWithStaticMesh
// ---------------------------------------------------------------------------

ActionReplaceWithStaticMesh::ActionReplaceWithStaticMesh(const std::vector<Node*>& nodes)
{
    for (Node* n : nodes)
    {
        InstancedMesh3D* inst = n->As<InstancedMesh3D>();
        if (inst && inst->GetNumInstances() > 0)
        {
            SplitEntry entry;
            entry.mOriginalNode = ResolvePtr(n);
            entry.mOriginalParent = ResolvePtr(n->GetParent());
            entry.mOriginalChildIndex = n->GetParent()
                ? n->GetParent()->FindChildIndex(n)
                : -1;
            mEntries.push_back(entry);
        }
    }
}

void ActionReplaceWithStaticMesh::Execute()
{
    Action::Execute();

    if (mFirstExecute)
    {
        mFirstExecute = false;

        for (SplitEntry& entry : mEntries)
        {
            InstancedMesh3D* inst = entry.mOriginalNode->As<InstancedMesh3D>();
            StaticMesh* mesh = inst->GetStaticMesh();
            Material* mat = inst->GetMaterialOverride();

            for (uint32_t i = 0; i < inst->GetNumInstances(); ++i)
            {
                const MeshInstanceData& instData = inst->GetInstanceData(i);

                NodePtr newNode = Node::Construct(StaticMesh3D::GetStaticType());
                StaticMesh3D* sm = newNode->As<StaticMesh3D>();
                sm->SetStaticMesh(mesh);
                sm->SetMaterialOverride(mat);
                sm->SetName(mesh ? mesh->GetName() : "StaticMesh3D");

                // Compute world transform from instance data
                // InstancedMesh3D stores world-space transforms in instance data
                sm->SetWorldPosition(instData.mPosition);
                sm->SetWorldRotation(instData.mRotation);
                sm->SetWorldScale(instData.mScale);

                if (entry.mOriginalParent != nullptr)
                {
                    entry.mOriginalParent->AddChild(newNode.Get());
                }

                entry.mCreatedNodes.push_back(newNode);
            }

            // Exile original
            entry.mOriginalNode->Detach();
            ActionManager::Get()->ExileNode(entry.mOriginalNode);
        }
    }
    else
    {
        // Re-execute: restore created, exile originals
        for (SplitEntry& entry : mEntries)
        {
            for (uint32_t i = 0; i < entry.mCreatedNodes.size(); ++i)
            {
                ActionManager::Get()->RestoreExiledNode(entry.mCreatedNodes[i]);
                if (entry.mOriginalParent != nullptr)
                {
                    entry.mOriginalParent->AddChild(entry.mCreatedNodes[i].Get());
                }
            }

            entry.mOriginalNode->Detach();
            ActionManager::Get()->ExileNode(entry.mOriginalNode);
        }
    }

    // Select first created node
    if (!mEntries.empty() && !mEntries[0].mCreatedNodes.empty())
    {
        GetEditorState()->SetSelectedNode(mEntries[0].mCreatedNodes[0].Get());
    }
}

void ActionReplaceWithStaticMesh::Reverse()
{
    Action::Reverse();

    for (SplitEntry& entry : mEntries)
    {
        // Remove created nodes
        for (uint32_t i = 0; i < entry.mCreatedNodes.size(); ++i)
        {
            entry.mCreatedNodes[i]->Detach();
            ActionManager::Get()->ExileNode(entry.mCreatedNodes[i]);
        }

        // Restore original
        ActionManager::Get()->RestoreExiledNode(entry.mOriginalNode);
        if (entry.mOriginalParent != nullptr)
        {
            entry.mOriginalParent->AddChild(
                entry.mOriginalNode.Get(),
                entry.mOriginalChildIndex);
        }
    }

    // Select first original
    if (!mEntries.empty())
    {
        GetEditorState()->SetSelectedNode(mEntries[0].mOriginalNode.Get());
    }
}

// ---------------------------------------------------------------------------
// ActionParentSelectedWith
// ---------------------------------------------------------------------------

ActionParentSelectedWith::ActionParentSelectedWith(
    const std::vector<Node*>& nodes,
    TypeId newParentType,
    ArrayOrientation arrayOrientation)
    : mNewParentType(newParentType)
    , mArrayOrientation(arrayOrientation)
{
    for (Node* node : nodes)
    {
        mNodes.push_back(ResolvePtr(node));
        mPrevParents.push_back(ResolvePtr(node->GetParent()));
        mPrevChildIndices.push_back(
            node->GetParent() ? node->GetParent()->FindChildIndex(node) : -1);
        mPrevBoneIndices.push_back(
            node->IsNode3D() ? node->As<Node3D>()->GetParentBoneIndex() : -1);
    }
}

void ActionParentSelectedWith::Execute()
{
    Action::Execute();

    if (mNodes.empty())
        return;

    if (mFirstExecute)
    {
        mFirstExecute = false;

        // Find insertion point: use first selected node's parent and position
        Node* firstNode = mNodes[0].Get();
        Node* insertionParent = firstNode->GetParent();
        int32_t insertionIndex = insertionParent ?
            insertionParent->FindChildIndex(firstNode) : -1;

        // Create the new parent node
        mCreatedParent = Node::Construct(mNewParentType);
        mCreatedParent->SetName("Parent");

        // Configure ArrayWidget if needed
        if (ArrayWidget* arrayWidget = mCreatedParent->As<ArrayWidget>())
        {
            arrayWidget->SetOrientation(mArrayOrientation);
        }

        // Store where we're inserting
        mCreatedParentParent = ResolvePtr(insertionParent);
        mCreatedParentChildIndex = insertionIndex;

        // Insert new parent at the first node's position
        if (insertionParent)
        {
            insertionParent->AddChild(mCreatedParent.Get(), insertionIndex);
        }

        // Reparent all selected nodes under the new parent
        for (uint32_t i = 0; i < mNodes.size(); ++i)
        {
            mNodes[i]->Attach(mCreatedParent.Get(), true, -1);
        }
    }
    else
    {
        // Re-execute: restore from exile
        ActionManager::Get()->RestoreExiledNode(mCreatedParent);

        if (mCreatedParentParent)
        {
            mCreatedParentParent->AddChild(
                mCreatedParent.Get(),
                mCreatedParentChildIndex);
        }

        // Reparent nodes again
        for (uint32_t i = 0; i < mNodes.size(); ++i)
        {
            mNodes[i]->Attach(mCreatedParent.Get(), true, -1);
        }
    }

    // Select the new parent
    GetEditorState()->SetSelectedNode(mCreatedParent.Get());
}

void ActionParentSelectedWith::Reverse()
{
    Action::Reverse();

    // Restore each node to its original parent
    for (uint32_t i = 0; i < mNodes.size(); ++i)
    {
        if (mPrevBoneIndices[i] >= 0 &&
            mPrevParents[i] &&
            mPrevParents[i]->As<SkeletalMesh3D>() &&
            mNodes[i]->As<Node3D>())
        {
            mNodes[i]->As<Node3D>()->AttachToBone(
                mPrevParents[i]->As<SkeletalMesh3D>(),
                mPrevBoneIndices[i],
                true,
                mPrevChildIndices[i]);
        }
        else if (mPrevParents[i])
        {
            mPrevParents[i]->AddChild(mNodes[i].Get(), mPrevChildIndices[i]);
        }
    }

    // Remove and exile the created parent
    mCreatedParent->Detach();
    ActionManager::Get()->ExileNode(mCreatedParent);

    // Restore selection to first original node
    if (!mNodes.empty())
    {
        GetEditorState()->SetSelectedNode(mNodes[0].Get());
    }
}

// ---------------------------------------------------------------------------
// EXE_ methods for replace actions
// ---------------------------------------------------------------------------

void ActionManager::EXE_ReplaceSelectedWithAsset(Asset* asset, const std::vector<Node*>& nodes)
{
    if (asset == nullptr || nodes.empty())
    {
        LogWarning("EXE_ReplaceSelectedWithAsset: Invalid asset or empty node list.");
        return;
    }

    ActionReplaceWithAsset* action = new ActionReplaceWithAsset(asset, nodes);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_ReplaceWithInstancedMesh(const std::vector<Node*>& nodes, bool merge)
{
    if (nodes.empty())
        return;

    ActionReplaceWithInstancedMesh* action = new ActionReplaceWithInstancedMesh(nodes, merge);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_ReplaceWithStaticMesh(const std::vector<Node*>& nodes)
{
    if (nodes.empty())
        return;

    ActionReplaceWithStaticMesh* action = new ActionReplaceWithStaticMesh(nodes);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_ParentSelectedWith(
    const std::vector<Node*>& nodes,
    TypeId parentType,
    ArrayOrientation arrayOrientation)
{
    if (nodes.size() < 1)
    {
        LogWarning("Parent Selected With requires at least 1 selected node.");
        return;
    }

    // Check for scene-linked nodes
    for (Node* node : nodes)
    {
        if (node->IsSceneLinkedChild())
        {
            LogError("Cannot reparent scene-linked nodes. Unlink the scene first.");
            return;
        }
    }

    ActionParentSelectedWith* action = new ActionParentSelectedWith(
        nodes, parentType, arrayOrientation);
    ActionManager::Get()->ExecuteAction(action);
}

// ======= Timeline Actions =======

void ActionManager::EXE_TimelineAddTrack(Timeline* timeline, TypeId trackType)
{
    ActionTimelineAddTrack* action = new ActionTimelineAddTrack(timeline, trackType);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_TimelineRemoveTrack(Timeline* timeline, int32_t trackIndex)
{
    ActionTimelineRemoveTrack* action = new ActionTimelineRemoveTrack(timeline, trackIndex);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_TimelineAddClip(Timeline* timeline, int32_t trackIndex, TypeId clipType, float startTime, float duration)
{
    ActionTimelineAddClip* action = new ActionTimelineAddClip(timeline, trackIndex, clipType, startTime, duration);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_TimelineRemoveClip(Timeline* timeline, int32_t trackIndex, int32_t clipIndex)
{
    ActionTimelineRemoveClip* action = new ActionTimelineRemoveClip(timeline, trackIndex, clipIndex);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_TimelineMoveClip(Timeline* timeline, int32_t trackIndex, int32_t clipIndex, float oldStartTime, float newStartTime)
{
    ActionTimelineMoveClip* action = new ActionTimelineMoveClip(timeline, trackIndex, clipIndex, oldStartTime, newStartTime);
    ActionManager::Get()->ExecuteAction(action);
}

void ActionManager::EXE_TimelineBindTrack(Timeline* timeline, int32_t trackIndex, uint64_t oldUuid, uint64_t newUuid, const std::string& oldName, const std::string& newName)
{
    ActionTimelineBindTrack* action = new ActionTimelineBindTrack(timeline, trackIndex, oldUuid, newUuid, oldName, newName);
    ActionManager::Get()->ExecuteAction(action);
}

// Static storage for assets needing upgrade (used in both headless and editor modes)
static std::vector<AssetStub*> sAssetsNeedingUpgrade;

bool ActionManager::CheckProjectNeedsUpgrade()
{
    // Check if any assets in the project are using old versions that need upgrading
    // Specifically, check for assets < ASSET_VERSION_UUID_WITH_NAME_FALLBACK (13)
    // This ensures UUID references have name fallback for reliable loading

    sAssetsNeedingUpgrade.clear();

    std::unordered_map<std::string, AssetStub*>& assetMap = AssetManager::Get()->GetAssetMap();

    for (auto& pair : assetMap)
    {
        AssetStub* stub = pair.second;
        if (stub == nullptr || stub->mEngineAsset)
            continue;

        // Read the asset header to check version
        Stream stream;
        if (stub->mPath.empty())
            continue;

        stream.ReadFile(stub->mPath.c_str(), true, sizeof(uint32_t) * 3 + sizeof(uint8_t) + sizeof(uint64_t));
        if (stream.GetSize() == 0)
            continue;

        AssetHeader header = Asset::ReadHeader(stream);

        // Check if asset needs upgrade (version < 13)
        if (header.mVersion < ASSET_VERSION_UUID_WITH_NAME_FALLBACK)
        {
            sAssetsNeedingUpgrade.push_back(stub);
        }
    }

    if (sAssetsNeedingUpgrade.size() > 0)
    {
        // Only show modal in non-headless mode (headless mode auto-upgrades)
        if (!IsHeadless())
        {
            EditorState* editorState = GetEditorState();
            editorState->mAssetsNeedingUpgrade = sAssetsNeedingUpgrade;
            editorState->mShowProjectUpgradeModal = true;
        }
        LogWarning("Project has %d assets that need to be upgraded to the new UUID format.", (int)sAssetsNeedingUpgrade.size());
        return true;
    }

    return false;
}

void ActionManager::UpgradeProject()
{
    // Use static storage in headless mode, EditorState in editor mode
    std::vector<AssetStub*>& assetsToUpgrade = IsHeadless() ? sAssetsNeedingUpgrade : GetEditorState()->mAssetsNeedingUpgrade;

    if (assetsToUpgrade.empty())
    {
        LogDebug("No assets need upgrading.");
        if (!IsHeadless())
        {
            GetEditorState()->mProjectUpgradeInProgress = false;
        }
        return;
    }

    if (!IsHeadless())
    {
        GetEditorState()->mProjectUpgradeInProgress = true;
    }

    LogDebug("Upgrading %d assets to new format...", (int)assetsToUpgrade.size());

    uint32_t upgradedCount = 0;
    uint32_t failedCount = 0;

    for (AssetStub* stub : assetsToUpgrade)
    {
        if (stub == nullptr)
            continue;

        // Load the asset if not already loaded
        bool wasLoaded = (stub->mAsset != nullptr);
        if (!wasLoaded)
        {
            AssetManager::Get()->LoadAsset(*stub);
        }

        if (stub->mAsset == nullptr)
        {
            LogError("Failed to load asset for upgrade: %s", stub->mPath.c_str());
            failedCount++;
            continue;
        }

        // Save the asset (this will write the new format with UUID + name fallback)
        AssetManager::Get()->SaveAsset(*stub);
        upgradedCount++;

        LogDebug("Upgraded asset: %s", stub->mAsset->GetName().c_str());

        // Unload if it wasn't loaded before
        if (!wasLoaded)
        {
            AssetManager::Get()->UnloadAsset(*stub);
        }
    }

    assetsToUpgrade.clear();
    sAssetsNeedingUpgrade.clear();

    if (!IsHeadless())
    {
        GetEditorState()->mProjectUpgradeInProgress = false;
    }

    LogDebug("Project upgrade complete. Upgraded: %d, Failed: %d", upgradedCount, failedCount);
}

// ---------------------------------------------------------------------------
// ActionSetVoxels
// ---------------------------------------------------------------------------

ActionSetVoxels::ActionSetVoxels(Voxel3D* voxel, const std::vector<VoxelChange>& changes)
    : mVoxel(voxel), mChanges(changes)
{
}

void ActionSetVoxels::Execute()
{
    if (mVoxel == nullptr) return;

    for (const auto& c : mChanges)
    {
        mVoxel->SetVoxel(c.x, c.y, c.z, c.newValue);
    }
}

void ActionSetVoxels::Reverse()
{
    if (mVoxel == nullptr) return;

    for (const auto& c : mChanges)
    {
        mVoxel->SetVoxel(c.x, c.y, c.z, c.oldValue);
    }
}

// ---------------------------------------------------------------------------
// ActionSetTerrainHeights
// ---------------------------------------------------------------------------

ActionSetTerrainHeights::ActionSetTerrainHeights(Terrain3D* terrain, const std::vector<TerrainHeightChange>& changes)
    : mTerrain(terrain), mChanges(changes)
{
}

void ActionSetTerrainHeights::Execute()
{
    if (mTerrain == nullptr) return;

    for (const auto& c : mChanges)
    {
        mTerrain->SetHeight(c.x, c.z, c.newHeight);
        mTerrain->mSplatmap[c.z * mTerrain->GetResolutionX() + c.x] = c.newSplat;
    }
    mTerrain->MarkDirty();
}

void ActionSetTerrainHeights::Reverse()
{
    if (mTerrain == nullptr) return;

    for (const auto& c : mChanges)
    {
        mTerrain->SetHeight(c.x, c.z, c.oldHeight);
        mTerrain->mSplatmap[c.z * mTerrain->GetResolutionX() + c.x] = c.oldSplat;
    }
    mTerrain->MarkDirty();
}

void ActionManager::EXE_SetTerrainHeights(Terrain3D* terrain, const std::vector<TerrainHeightChange>& changes)
{
    ActionSetTerrainHeights* action = new ActionSetTerrainHeights(terrain, changes);
    ActionManager::Get()->ExecuteAction(action);
}

// ---------------------------------------------------------------------------
// ActionPaintTiles
// ---------------------------------------------------------------------------

ActionPaintTiles::ActionPaintTiles(TileMap2D* tileMapNode, const std::vector<TilePaintChange>& changes)
    : mTarget(tileMapNode), mChanges(changes)
{
}

void ActionPaintTiles::Execute()
{
    if (mTarget == nullptr) return;
    TileMap* tileMap = mTarget->GetTileMap();
    if (tileMap == nullptr) return;

    for (const auto& c : mChanges)
    {
        tileMap->SetCell(c.mCellX, c.mCellY, c.mNewCell, c.mLayer);
    }
    mTarget->MarkDirty();
}

void ActionPaintTiles::Reverse()
{
    if (mTarget == nullptr) return;
    TileMap* tileMap = mTarget->GetTileMap();
    if (tileMap == nullptr) return;

    for (const auto& c : mChanges)
    {
        tileMap->SetCell(c.mCellX, c.mCellY, c.mOldCell, c.mLayer);
    }
    mTarget->MarkDirty();
}

void ActionManager::EXE_PaintTiles(TileMap2D* tileMapNode, const std::vector<TilePaintChange>& changes)
{
    ActionPaintTiles* action = new ActionPaintTiles(tileMapNode, changes);
    ActionManager::Get()->ExecuteAction(action);
}

#endif
