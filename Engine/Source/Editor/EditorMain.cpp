#if EDITOR

#include <stdint.h>
#include <stdio.h>
#include <chrono>

#if PLATFORM_WINDOWS
#include <Windows.h>
#endif

#undef min
#undef max

#include "Engine.h"
#include "World.h"
#include "Clock.h"
#include "Renderer.h"
#include "Log.h"

#include "Nodes/Widgets/Quad.h"
#include "Nodes/Widgets/Text.h"
#include "Nodes/Widgets/Canvas.h"

#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/PointLight3d.h"
#include "Nodes/3D/TestSpinner.h"

#include "ActionManager.h"
#include "BuildCache.h"
#include "InputManager.h"
#include "CliTerminal/TerminalPanel.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/General/GeneralModule.h"
#include "Preferences/JsonSettings.h"
#include "ProjectSelect/TemplateManager.h"
#include "ProjectSelect/ProjectSelectWindow.h"
#include "Addons/AddonManager.h"
#include "Addons/NativeAddonManager.h"
#include "EditorUIHookManager.h"
#include "ControllerServer/ControllerServer.h"
#include "Preferences/Network/NetworkModule.h"
#include "Preferences/Updates/UpdatesModule.h"
#include "AutoUpdater/AutoUpdater.h"
#include "AutoUpdater/HttpClient.h"
#include "Profiling/ProfilingWindow.h"
#include "Grid.h"
#include "Assets/StaticMesh.h"
#include "Assets/Font.h"
#include "EditorState.h"
#include "EditorImgui.h"
#include "BuildDependencyWindow.h"
#include "Input/InputMap.h"
#include "Input/PlayerInputSystem.h"
#include "Hotkeys/EditorHotkeyMap.h"
#include "Git/GitService.h"
#include "Utilities.h"
#include "Stream.h"

#include <string>

#if PLATFORM_WINDOWS
// If a prior packaging build (or a Visual Studio link) was interrupted by a BSOD,
// power loss, or hard kill, `Standalone/Standalone.vcxproj` can be left truncated
// — ends in a run of null bytes instead of `</Project>` — which both VS and our
// own packager choke on. Our injection keeps a pristine `.orig` next to it, so
// detecting a malformed `.vcxproj` and restoring from `.orig` is safe and
// automatic. Same crash tends to corrupt whatever was mid-write in the
// Standalone build/intermediate trees, so wipe those too — they regenerate on
// the next build, and leaving garbage behind silently fails later links.
static void SelfHealStandaloneIfCrashed()
{
    const std::string vcxprojPath = "Standalone/Standalone.vcxproj";
    const std::string origPath    = "Standalone/Standalone.vcxproj.orig";

    if (!SYS_DoesFileExist(origPath.c_str(), false))
        return;

    Stream live;
    if (!live.ReadFile(vcxprojPath.c_str(), false) || live.GetSize() == 0)
    {
        // Live file missing or zero-length — treat as corrupt, fall through to restore.
    }
    else
    {
        // A healthy vcxproj ends with `</Project>` (possibly followed by whitespace).
        // Anything else — truncation, null-byte tail — is treated as corrupt.
        const std::string content(live.GetData(), live.GetSize());
        if (content.find("</Project>") != std::string::npos)
            return;
    }

    LogWarning("Standalone.vcxproj appears corrupt (likely interrupted by crash). Restoring from .orig.");

    Stream orig;
    if (!orig.ReadFile(origPath.c_str(), false) || orig.GetSize() == 0)
    {
        LogWarning("SelfHealStandaloneIfCrashed: .orig unreadable; skipping.");
        return;
    }
    Stream writeBack(orig.GetData(), orig.GetSize());
    writeBack.WriteFile(vcxprojPath.c_str());

    // Wipe per-config Standalone intermediate + output artifacts. Engine.vcxproj is
    // shared and expensive to rebuild; leave it alone — its intermediates usually
    // survive crashes intact, and a Force Rebuild from the editor can clear them.
    const char* configs[] = { "Release", "ReleaseSteam", "DebugEditor", "ReleaseEditor", "Debug" };
    for (const char* cfg : configs)
    {
        const std::string intDir = std::string("Standalone/Intermediate/Windows/x64/") + cfg + "/Standalone/";
        if (DoesDirExist(intDir.c_str()))
        {
            RemoveDir(intDir.c_str());
        }
        const std::string buildDir = std::string("Standalone/Build/Windows/x64/") + cfg + "/";
        SYS_RemoveFile((buildDir + "Polyphase.exe").c_str());
        SYS_RemoveFile((buildDir + "Polyphase.ilk").c_str());
        SYS_RemoveFile((buildDir + "Polyphase.pdb").c_str());
    }

    LogWarning("Self-heal complete. Standalone build state reset.");
}
#endif

// Persisted across sessions in %APPDATA%/PolyphaseEditor/Preferences/WindowState.json
// (or ~/.config/... on Linux). Captured on close, applied right after the OS window
// exists on next startup. Per-app, not per-project — the window is shared across all
// projects and the project-select screen.
static const char* kWindowStateFile = "WindowState.json";

static void RestoreEditorWindowState()
{
    std::string path = JsonSettings::GetPreferencesDirectory() + "/" + kWindowStateFile;
    rapidjson::Document doc;
    if (!JsonSettings::LoadFromFile(path, doc) || !doc.IsObject())
    {
        return;
    }

    int width = JsonSettings::GetInt(doc, "width", -1);
    int height = JsonSettings::GetInt(doc, "height", -1);
    int x = JsonSettings::GetInt(doc, "x", 0);
    int y = JsonSettings::GetInt(doc, "y", 0);
    bool maximized = JsonSettings::GetBool(doc, "maximized", false);

    if (width > 0 && height > 0)
    {
        SYS_SetWindowRect(x, y, width, height);
    }
    if (maximized)
    {
        SYS_MaximizeWindow();
    }
}

static void SaveEditorWindowState()
{
    // Bail if Play-in-Editor is using the editor window — EditorState::BeginPlayInEditor
    // temporarily resizes it to game preview resolution and queryng now would record the
    // wrong rect. Tolerable edge case to skip the save; the previous saved state stays.
    EditorState* es = GetEditorState();
    if (es != nullptr && es->mPlayInEditor && !es->mPlayInGameWindow)
    {
        return;
    }

    // SYS_GetWindowRect on Windows uses GetWindowPlacement.rcNormalPosition, so the
    // rect we save is the un-maximized rect even when currently maximized — restore
    // then puts the window back to the right size if the user un-maximizes.
    int x = 0, y = 0, w = 0, h = 0;
    SYS_GetWindowRect(x, y, w, h);
    bool maximized = SYS_IsWindowMaximized();

    rapidjson::Document doc;
    doc.SetObject();
    JsonSettings::SetInt(doc, "x", x);
    JsonSettings::SetInt(doc, "y", y);
    JsonSettings::SetInt(doc, "width", w);
    JsonSettings::SetInt(doc, "height", h);
    JsonSettings::SetBool(doc, "maximized", maximized);

    JsonSettings::EnsurePreferencesDirectory();
    std::string path = JsonSettings::GetPreferencesDirectory() + "/" + kWindowStateFile;
    JsonSettings::SaveToFile(path, doc);
}

void OctPreInitialize(EngineConfig& config);

void EditorMain(int32_t argc, char** argv)
{
    GetEngineState()->mArgC = argc;
    GetEngineState()->mArgV = argv;

    ReadCommandLineArgs(argc, argv);

    // If the editor's built-in assets aren't reachable from the current working
    // directory (e.g. the OS launched the editor by double-clicking a .octp file
    // and set cwd to the project folder), pivot cwd to the engine root so every
    // "Engine/Assets/..." relative-path lookup resolves. ReadCommandLineArgs has
    // already absolutized the project path, so the pivot doesn't strand it.
    //
    // W1: For DLL editor builds the exe lives several directories deep under
    // Standalone/Build/Windows/x64/<config>/, so the old "pivot to exe dir"
    // fallback didn't help — Engine/Assets was still unreachable. SYS_GetPolyphasePath()
    // walks back from the exe to find the engine root (looks for Engine/Source/Engine/Engine.h
    // and similar markers); use that as the primary pivot target, falling back
    // to the exe dir for compatibility with whatever the previous behavior was.
    if (!SYS_DoesFileExist("Engine/Assets/Fonts/F_InterRegular18.ttf", false))
    {
        const char* kAssetMarker = "Engine/Assets/Fonts/F_InterRegular18.ttf";
        bool pivoted = false;

        std::string polyphaseRoot = SYS_GetPolyphasePath();
        if (!polyphaseRoot.empty() &&
            SYS_DoesFileExist((polyphaseRoot + kAssetMarker).c_str(), false))
        {
            SYS_SetWorkingDirectory(polyphaseRoot);
            pivoted = true;
        }

        // W1: For DLL editor builds the exe lives several dirs below the engine root
        // (Standalone/Build/Windows/x64/<config>/), so SYS_GetPolyphasePath's
        // "look in exe dir" heuristic doesn't find Engine/. Walk up from the exe
        // directory checking each ancestor for the marker asset before giving up.
        if (!pivoted)
        {
            std::string exePath = SYS_GetExecutablePath();
            size_t lastSlash = exePath.find_last_of("\\/");
            std::string dir = (lastSlash != std::string::npos)
                                  ? exePath.substr(0, lastSlash)
                                  : std::string(".");
            for (int hops = 0; hops < 8 && !pivoted; ++hops)
            {
                if (SYS_DoesFileExist((dir + "/" + kAssetMarker).c_str(), false))
                {
                    SYS_SetWorkingDirectory(dir);
                    pivoted = true;
                    break;
                }
                size_t up = dir.find_last_of("\\/");
                if (up == std::string::npos) break;
                dir = dir.substr(0, up);
            }
        }

        if (!pivoted)
        {
            // Last-resort: pivot to the exe dir so at least the binary's own
            // side-by-side assets are reachable (preserves the previous fallback
            // behavior for setups where engine assets just don't exist anywhere
            // walkable from the exe).
            std::string exePath = SYS_GetExecutablePath();
            size_t lastSlash = exePath.find_last_of("\\/");
            if (lastSlash != std::string::npos)
            {
                SYS_SetWorkingDirectory(exePath.substr(0, lastSlash));
            }
        }
    }

    {
        EngineConfig* mutableConfig = GetMutableEngineConfig();
        // W1: route through OctGameHooks so this works inside the editor DLL too.
        const OctGameHooks& hooks = GetOctHooks();
        if (hooks.preInitialize) hooks.preInitialize(*mutableConfig);
    }

    ReadEngineConfig();

    Initialize();

#if PLATFORM_WINDOWS
    // Must run before anything looks at Standalone.vcxproj (headless BuildData, interactive
    // packaging, etc). No-op on a clean install — only acts when the last run crashed.
    SelfHealStandaloneIfCrashed();
#endif

    const EngineConfig* engineConfig = GetEngineConfig();

    // Headless build mode - minimal initialization, run build, and exit
    if (IsHeadless())
    {
        LogDebug("Headless mode: Starting");
        LogDebug("Headless mode: Project path = %s", engineConfig->mProjectPath.c_str());
        LogDebug("Headless mode: Build platform = %d", (int)engineConfig->mBuildPlatform);

        ActionManager::Create();
        BuildCache::Create();

        // Load project directly without editor state
        if (engineConfig->mProjectPath != "")
        {
            LoadProject(engineConfig->mProjectPath);

            // Check and auto-upgrade assets to new UUID format
            if (ActionManager::Get()->CheckProjectNeedsUpgrade())
            {
                LogDebug("Headless mode: Auto-upgrading assets to new UUID format...");
                ActionManager::Get()->UpgradeProject();
            }
        }

        if (engineConfig->mBuildPlatform != Platform::Count)
        {
            LogDebug("Headless mode: Building for %s (embedded=%d)",
                     GetPlatformString(engineConfig->mBuildPlatform),
                     engineConfig->mBuildEmbedded ? 1 : 0);

            ActionManager::Get()->BuildData(engineConfig->mBuildPlatform, engineConfig->mBuildEmbedded);

            LogDebug("Headless mode: Build complete");
        }
        else
        {
            LogError("Headless mode: No build platform specified. Use -build <platform>");
        }

        // Cleanup and exit
        BuildCache::Destroy();
        ActionManager::Destroy();
        Shutdown();
        return;
    }

    // Normal editor initialization
    GetEditorState()->Init();

    // Window already exists from Initialize() above. Apply last-session size/maximize
    // state if we have one — overrides Config.ini's WindowWidth/Height for the editor.
    RestoreEditorWindowState();

    ActionManager::Create();
    BuildCache::Create();
    InputManager::Create();
    InputMap::Create();
    EditorHotkeyMap::Create();
    GitService::Create();
    PlayerInputSystem::Create();
    PreferencesManager::Create();

    // Auto-start controller server if enabled in preferences (must be after PreferencesManager::Create)
    NetworkModule* netModule = NetworkModule::Get();
    if (netModule && netModule->GetControllerServerEnabled())
    {
        ControllerServer::Get()->Start(netModule->GetPort());
        ControllerServer::Get()->SetLogRequests(netModule->GetLogRequests());
    }

    TemplateManager::Create();
    AddonManager::Create();
    EditorUIHookManager::Create();
    NativeAddonManager::Create();
    AutoUpdater::Create();

    // Connect EditorUIHooks to NativeAddonManager's engine API
    if (NativeAddonManager::Get() && EditorUIHookManager::Get())
    {
        NativeAddonManager::Get()->GetEngineAPI()->editorUI = EditorUIHookManager::Get()->GetHooks();
    }

    InitializeGrid();

    if (engineConfig->mProjectPath != "")
    {
        // Clear the project path so we don't overwrite the EditorProject.sav file with default data.
        // This would have been set earlier in Initialize() to ensure that shader cache is loaded correctly.
        // TODO: Seems like we don't need to be storing shader cache in project folder when running the editor?
        GetEngineState()->mProjectName = "";
        GetEngineState()->mProjectPath = "";
        GetEngineState()->mProjectDirectory = "";

        ActionManager::Get()->OpenProject(engineConfig->mProjectPath.c_str());

        if (PlayerInputSystem::Get() != nullptr)
        {
            PlayerInputSystem::Get()->LoadProjectActions();
        }
    }

    // Spawn starting scene if a default wasn't loaded
    if (GetEditorState()->GetEditScene() == nullptr)
    {
        GetEditorState()->OpenEditScene(nullptr);
        GetWorld(0)->SpawnNode<TestSpinner>();
    }

    // Show Project Select window if no project is loaded
    if (GetEngineState()->mProjectPath.empty())
    {
        GetProjectSelectWindow()->Open();
    }

    // Fire OnEditorReady on all loaded plugins
    if (NativeAddonManager::Get() != nullptr)
    {
        NativeAddonManager::Get()->CallOnEditorReady();
    }

    Renderer::Get()->EnableConsole(true);
    Renderer::Get()->EnableStatsOverlay(false);

    GeneralModule* generalModule = static_cast<GeneralModule*>(
        PreferencesManager::Get()->FindModule("General"));
    if (generalModule != nullptr && generalModule->GetCheckBuildDepsOnStartup())
    {
        GetBuildDependencyWindow()->RunChecks();
        if (GetBuildDependencyWindow()->HasMissing())
        {
            GetBuildDependencyWindow()->Open();
        }
    }

    // Check for updates on startup if enabled
    UpdatesModule* updatesModule = UpdatesModule::Get();
    if (updatesModule && updatesModule->GetCheckOnStartup() && HttpClient::IsAvailable())
    {
        // Check if enough time has passed since last check
        int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t lastCheck = updatesModule->GetLastCheckTime();
        int64_t intervalNs = (int64_t)updatesModule->GetCheckIntervalHours() * 3600LL * 1000000000LL;

        if (lastCheck == 0 || (now - lastCheck) > intervalNs)
        {
            AutoUpdater::Get()->CheckForUpdates(false);
        }
    }

    bool ret = true;

    while (ret)
    {
        InputManager::Get()->Update();
        ActionManager::Get()->Update();
        AutoUpdater::Get()->Update();
        GitService::Get()->Update();

        bool playInEditor = GetEditorState()->mPlayInEditor;

        if (playInEditor)
        {
            // W1: route through OctGameHooks (see Engine.h).
            const OctGameHooks& hooks = GetOctHooks();
            if (hooks.preUpdate) hooks.preUpdate();
        }

        ret = Update();

        // Tick native addon plugins
        if (NativeAddonManager::Get() != nullptr)
        {
            float deltaTime = GetAppClock()->DeltaTime();

            // Drive any in-flight async addon builds (worker thread →
            // main-thread finalize → MOD_Load + register types).
            NativeAddonManager::Get()->TickAsyncBuilds();

            // TickEditor runs every frame in editor (regardless of play state)
            NativeAddonManager::Get()->TickEditorAllPlugins(deltaTime);

            // Tick only runs during gameplay (Play In Editor)
            if (playInEditor)
            {
                NativeAddonManager::Get()->TickAllPlugins(deltaTime);
            }
        }

        // Tick profiling window to record frame time history
        GetProfilingWindow()->Tick();

        // Tick CLI terminal panel: drains output buffer and advances session state
        GetTerminalPanel()->Tick();

        if (GetEditorState()->mEndPieAtEndOfFrame)
        {
            GetEditorState()->EndPlayInEditor();
        }

        // We are trying to quit, and haven't done the shutdown check yet
        if (!ret && !GetEditorState()->mShutdownUnsavedCheck)
        {
            GetEditorState()->mShutdownUnsavedCheck = true;
            std::vector<AssetStub*> unsavedAssets = AssetManager::Get()->GatherDirtyAssets();

            if (unsavedAssets.size() > 0)
            {
                // Need to wait on user response.
                ret = true;
                GetEngineState()->mQuit = false;

                // Have the imgui callbacks set / clear the Quit and Shutdown check flags as appropriate.
                EditorShowUnsavedAssetsModal(unsavedAssets);
            }
        }

        if (playInEditor)
        {
            // W1: route through OctGameHooks (see Engine.h).
            const OctGameHooks& hooks = GetOctHooks();
            if (hooks.postUpdate) hooks.postUpdate();
        }
    }

    // Fire OnEditorShutdown before cleanup
    if (EditorUIHookManager::Get() != nullptr)
    {
        EditorUIHookManager::Get()->FireOnEditorShutdown();
    }

    // Capture window size/maximize state before Shutdown() destroys the OS window.
    SaveEditorWindowState();

    GitService::Destroy();
    AutoUpdater::Destroy();
    // NOTE: NativeAddonManager::Destroy() is sandwiched between Shutdown() and
    // EditorUIHookManager::Destroy() to satisfy two ordering constraints:
    //   (A) Worlds must be destroyed BEFORE addon DLLs are unloaded — scene nodes
    //       whose type came from an addon (e.g. VideoPlayer3D) have vtables in the
    //       addon DLL; freeing the DLL first leaves dangling vtables and ~Node crashes.
    //   (B) EditorUIHookManager must outlive addon unload — each addon's OnUnload()
    //       calls hooks->RemoveAllHooks(hookId), and UnloadNativeAddon itself also
    //       does the same on the way out. Both dereference the editorUI pointer that
    //       points into EditorUIHookManager's mHooks struct.
    AddonManager::Destroy();
    TemplateManager::Destroy();
    BuildCache::Destroy();
    PreferencesManager::Destroy();
    PlayerInputSystem::Destroy();
    EditorHotkeyMap::Destroy();
    InputMap::Destroy();
    GetEditorState()->Shutdown();
    Shutdown();
    // Worlds/nodes destroyed and Lua state closed — safe to FreeLibrary addon DLLs.
    NativeAddonManager::Destroy();
    // Addons are gone, no one will dereference editorUI any more.
    EditorUIHookManager::Destroy();
}

#endif
