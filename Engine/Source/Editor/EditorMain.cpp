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

void OctPreInitialize(EngineConfig& config);

void EditorMain(int32_t argc, char** argv)
{
    GetEngineState()->mArgC = argc;
    GetEngineState()->mArgV = argv;

    ReadCommandLineArgs(argc, argv);

    {
        EngineConfig* mutableConfig = GetMutableEngineConfig();
        OctPreInitialize(*mutableConfig);
    }

    ReadEngineConfig();

    Initialize();

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
            OctPreUpdate();
        }

        ret = Update();

        // Tick native addon plugins
        if (NativeAddonManager::Get() != nullptr)
        {
            float deltaTime = GetAppClock()->DeltaTime();

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
            OctPostUpdate();
        }
    }

    // Fire OnEditorShutdown before cleanup
    if (EditorUIHookManager::Get() != nullptr)
    {
        EditorUIHookManager::Get()->FireOnEditorShutdown();
    }

    GitService::Destroy();
    AutoUpdater::Destroy();
    // NOTE: NativeAddonManager::Destroy() is intentionally deferred until AFTER Shutdown().
    // Shutdown destroys every world and recursively destroys their nodes. If addon DLLs
    // are freed first, any scene node whose type came from an addon (e.g. VideoPlayer3D)
    // has a dangling vtable pointer into unmapped memory; the destructor call crashes.
    EditorUIHookManager::Destroy();
    AddonManager::Destroy();
    TemplateManager::Destroy();
    BuildCache::Destroy();
    PreferencesManager::Destroy();
    PlayerInputSystem::Destroy();
    EditorHotkeyMap::Destroy();
    InputMap::Destroy();
    GetEditorState()->Shutdown();
    Shutdown();
    // Now that all worlds/nodes are destroyed and the Lua state is closed, it's safe to
    // FreeLibrary the addon DLLs.
    NativeAddonManager::Destroy();
}

#endif
