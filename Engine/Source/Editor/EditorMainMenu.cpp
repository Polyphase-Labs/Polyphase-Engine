#if EDITOR

#include "EditorMainMenu.h"

#include "EditorImgui.h"
#include "EditorState.h"
#include "EditorUtils.h"
#include "EditorConstants.h"
#include "ActionManager.h"
#include "Engine.h"
#include "System/System.h"
#include "Renderer.h"
#include "InputDevices.h"
#include "Log.h"
#include "Grid.h"
#include "FeatureFlags.h"
#include "World.h"
#include "Viewport3d.h"
#include "Viewport2d.h"

#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/Primitive3d.h"
#include "Nodes/3D/Spline3d.h"
#include "Nodes/3D/Camera3d.h"

#include "Assets/Scene.h"

#include "ProjectSelect/ProjectSelectWindow.h"
#include "Packaging/PackagingWindow.h"
#include "CSharp/CSharpManager.h"
#include "AppSettings/AppSettingsWindow.h"
#include "Addons/AddonsWindow.h"
#include "Addons/AddonsMenu.h"
#include "MemorySnapshot/MemorySnapshotWindow.h"
#include "Addons/NativeAddonManager.h"
#include "EditorUIHookManager.h"
#include "BuildDependencyWindow.h"
#include "InputMapWindow.h"
#include "InputTester/InputTesterPanel.h"
#include "Hotkeys/EditorHotkeysWindow.h"
#include "Hotkeys/EditorHotkeyMap.h"
#include "Hotkeys/EditorAction.h"
#include "PlayerInputEditor.h"
#include "PlayerInputDebugger.h"
#include "ThemeEditor/ThemeEditorWindow.h"
#include "AutoUpdater/AutoUpdater.h"
#include "Preferences/PreferencesWindow.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/External/EditorsModule.h"
#include "Timeline/TimelinePanel.h"
#include "CliTerminal/TerminalPanel.h"

#include "Git/GitWorkspaceWindow.h"
#include "Git/GitService.h"
#include "Git/GitRepository.h"
#include "Git/GitOperationQueue.h"

#include "imgui.h"

#include <string>
#include <vector>

void DrawAddNodeMenu(Node* node);
void DrawSpawnBasic3dMenu(Node* node, bool setFocusPos);
void EditorImgui_ResetSaveSceneAsBuffer();
void EditorImgui_RequestDockReset();

static int32_t sDevModeClicks = 0;

static void OpenUrl(const std::string& url)
{
#if PLATFORM_WINDOWS
    SYS_ExecDetached(("start \"\" " + url).c_str());
#else
    SYS_ExecDetached(("xdg-open " + url).c_str());
#endif
}

static void DocsMenuItem(const char* label, const char* relPath)
{
    if (ImGui::MenuItem(label))
    {
        std::string url = "https://polyphase-labs.github.io/Polyphase-Engine/";
        url += relPath;
        OpenUrl(url);
    }
}

static void DrawHelpDocumentationLuaMenu()
{
    DocsMenuItem("Lua API", "Lua/");

    if (ImGui::BeginMenu("Node"))
    {
        if (ImGui::BeginMenu("Animation"))
        {
            DocsMenuItem("SpriteAnimator", "Lua/Nodes/SpriteAnimator/");
            DocsMenuItem("AnimatedSprite3D", "Lua/Nodes/3D/AnimatedSprite3D/");
            DocsMenuItem("AnimatedWidget", "Lua/Nodes/Widgets/AnimatedWidget/");
            ImGui::EndMenu();
        }
        DocsMenuItem("Node", "Lua/Nodes/Node/");
        DocsMenuItem("Node3D", "Lua/Nodes/3D/Node3D/");
        if (ImGui::BeginMenu("Widget"))
        {
            DocsMenuItem("Widget", "Lua/Nodes/Widgets/Widget/");
            DocsMenuItem("Button", "Lua/Nodes/Widgets/Button/");
            DocsMenuItem("Poly", "Lua/Nodes/Widgets/Poly/");
            DocsMenuItem("PolyRect", "Lua/Nodes/Widgets/PolyRect/");
            DocsMenuItem("Quad", "Lua/Nodes/Widgets/Quad/");
            DocsMenuItem("Text", "Lua/Nodes/Widgets/Text/");
            DocsMenuItem("DialogWindow", "UI/DialogWindow/");
            DocsMenuItem("ScrollContainer", "UI/ScrollContainer/");
            DocsMenuItem("Window", "UI/Window/");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("System"))
    {
        DocsMenuItem("World", "Lua/Misc/World/");
        DocsMenuItem("Engine", "Lua/Systems/Engine/");
        DocsMenuItem("Audio", "Lua/Systems/Audio/");
        DocsMenuItem("Network", "Lua/Systems/Network/");
        DocsMenuItem("TimerManager", "Lua/Systems/TimerManager/");
        DocsMenuItem("System", "Lua/Systems/System/");
        DocsMenuItem("Log", "Lua/Systems/Log/");
        DocsMenuItem("Math", "Lua/Systems/Math/");
        DocsMenuItem("Script", "Lua/Systems/Script/");
        DocsMenuItem("AssetManager", "Lua/Systems/AssetManager/");
        ImGui::Separator();
        DocsMenuItem("Signal", "Lua/Misc/Signal/");
        DocsMenuItem("SaveData", "Lua/Misc/SaveData/");
        DocsMenuItem("Globals", "Lua/Misc/Globals/");
        DocsMenuItem("Property", "Lua/Misc/Property/");
        DocsMenuItem("Vector", "Lua/Misc/Vector/");
        DocsMenuItem("Stream", "Lua/Misc/Stream/");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Asset"))
    {
        DocsMenuItem("Asset", "Lua/Assets/Asset/");
        if (ImGui::BeginMenu("Material"))
        {
            DocsMenuItem("Material", "Lua/Assets/Material/");
            DocsMenuItem("MaterialBase", "Lua/Assets/MaterialBase/");
            DocsMenuItem("MaterialInstance", "Lua/Assets/MaterialInstance/");
            DocsMenuItem("MaterialLite", "Lua/Assets/MaterialLite/");
            ImGui::EndMenu();
        }
        DocsMenuItem("ParticleSystem", "Lua/Assets/ParticleSystem/");
        DocsMenuItem("ParticleSystemInstance", "Lua/Assets/ParticleSystemInstance/");
        DocsMenuItem("Scene", "Lua/Assets/Scene/");
        DocsMenuItem("SkeletalMesh", "Lua/Assets/SkeletalMesh/");
        DocsMenuItem("SoundWave", "Lua/Assets/SoundWave/");
        DocsMenuItem("StaticMesh", "Lua/Assets/StaticMesh/");
        DocsMenuItem("Texture", "Lua/Assets/Texture/");
        ImGui::EndMenu();
    }
}

static void DrawHelpDocumentationDevelopmentMenu()
{
    if (ImGui::BeginMenu("Setup Environment"))
    {
        DocsMenuItem("Windows", "Development/SetupEnvironment/Windows/");
        DocsMenuItem("Linux", "Development/SetupEnvironment/Linux/");
        DocsMenuItem("VS Code", "Development/SetupEnvironment/VSCode/");
        DocsMenuItem("Terminal", "Development/SetupEnvironment/Terminal/");
        DocsMenuItem("Compiling", "Development/SetupEnvironment/Compiling/");
        ImGui::EndMenu();
    }

    DocsMenuItem("Mesh Instancing", "Development/MeshInstancing/");

    if (ImGui::BeginMenu("3D Nodes"))
    {
        DocsMenuItem("Terrain3D", "Development/Terrain3D/");
        DocsMenuItem("TileMap2D", "Development/TileMap2D/");
        DocsMenuItem("Voxel3D", "Development/Voxel3D/");
        ImGui::EndMenu();
    }

    DocsMenuItem("Packaging Flow", "Development/PackagingFlow/");
    DocsMenuItem("Themes / CSS", "Development/ThemesCss/");
    DocsMenuItem("Save Data", "Development/SaveData/");

    if (ImGui::BeginMenu("UI"))
    {
        DocsMenuItem("Overview", "Development/UI/Overview/");
        DocsMenuItem("Displaying Images", "Development/UI/DisplayingImages/");
        DocsMenuItem("Text", "Development/UI/Text/");
        DocsMenuItem("Buttons", "Development/UI/Buttons/");
        DocsMenuItem("Animation", "Development/UI/Animation/");
        DocsMenuItem("Building UI", "Development/UI/BuildingUI/");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Input"))
    {
        DocsMenuItem("Overview", "Development/Input/Overview/");
        DocsMenuItem("Keyboard", "Development/Input/Keyboard/");
        DocsMenuItem("Mouse", "Development/Input/Mouse/");
        DocsMenuItem("Gamepad", "Development/Input/Gamepad/");
        DocsMenuItem("Touch", "Development/Input/Touch/");
        DocsMenuItem("Platform Specific", "Development/Input/PlatformSpecific/");
        DocsMenuItem("Known Gaps", "Development/Input/KnownGaps/");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Logging"))
    {
        DocsMenuItem("Overview", "Development/Logging/Overview/");
        DocsMenuItem("Configuration", "Development/Logging/Configuration/");
        DocsMenuItem("C++ API", "Development/Logging/CppAPI/");
        DocsMenuItem("Lua API", "Development/Logging/LuaAPI/");
        DocsMenuItem("Editor Debug Log", "Development/Logging/EditorDebugLog/");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Node Graph"))
    {
        DocsMenuItem("Overview", "Development/NodeGraph/Overview/");
        DocsMenuItem("Graph Types", "Development/NodeGraph/GraphTypes/");
        DocsMenuItem("Creating Domains", "Development/NodeGraph/CreatingDomains/");
        DocsMenuItem("Creating Nodes", "Development/NodeGraph/CreatingNodes/");
        DocsMenuItem("NodeGraphPlayer", "Development/NodeGraph/NodeGraphPlayer/");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Timeline"))
    {
        DocsMenuItem("Overview", "Development/Timeline/Overview/");
        DocsMenuItem("TimelinePlayer", "Development/Timeline/TimelinePlayer/");
        DocsMenuItem("Tracks and Clips", "Development/Timeline/TracksAndClips/");
        DocsMenuItem("Lua API", "Development/Timeline/LuaAPI/");
        DocsMenuItem("Creating Custom Tracks", "Development/Timeline/CreatingCustomTracks/");
        ImGui::EndMenu();
    }

    DocsMenuItem("Serial", "Development/Serial/");

    if (ImGui::BeginMenu("Platforms"))
    {
        if (ImGui::BeginMenu("3DS"))
        {
            DocsMenuItem("Overview", "Development/Platforms/3DS/Overview/");
            DocsMenuItem("Screens", "Development/Platforms/3DS/Screens/");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Native Addons"))
    {
        DocsMenuItem("Overview", "Development/NativeAddon/NativeAddon/");

        if (ImGui::BeginMenu("General Examples"))
        {
            DocsMenuItem("Custom Context Menu Item", "Development/NativeAddon/Examples/CustomContextMenuItem/");
            DocsMenuItem("Custom Debug Window", "Development/NativeAddon/Examples/CustomDebugWindow/");
            DocsMenuItem("Custom Menu Item", "Development/NativeAddon/Examples/CustomMenuItem/");
            DocsMenuItem("Custom Script Inspector", "Development/NativeAddon/Examples/CustomScriptInspector/");
            DocsMenuItem("Custom Theme", "Development/NativeAddon/Examples/CustomTheme/");
            DocsMenuItem("Debug Log", "Development/NativeAddon/Examples/DebugLog/");
            DocsMenuItem("Coin", "Development/NativeAddon/Examples/Coin/");
            DocsMenuItem("Select Handler", "Development/NativeAddon/Examples/SelectHandler/");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Editor Examples"))
        {
            DocsMenuItem("Add Node Menu", "Development/NativeAddon/Examples/Editor/AddNodeMenu/");
            DocsMenuItem("Asset Importer", "Development/NativeAddon/Examples/Editor/AssetImporter/");
            DocsMenuItem("Asset Open Hooks", "Development/NativeAddon/Examples/Editor/AssetOpenHooks/");
            DocsMenuItem("Asset Pipeline Hooks", "Development/NativeAddon/Examples/Editor/AssetPipelineHooks/");
            DocsMenuItem("Build Pipeline", "Development/NativeAddon/Examples/Editor/BuildPipeline/");
            DocsMenuItem("Custom Dock Panel", "Development/NativeAddon/Examples/Editor/CustomDockablePanel/");
            DocsMenuItem("Custom Play Target", "Development/NativeAddon/Examples/Editor/CustomPlayTarget/");
            DocsMenuItem("Drag and Drop Asset", "Development/NativeAddon/Examples/Editor/DragAndDropAsset/");
            DocsMenuItem("Editor Init Hooks", "Development/NativeAddon/Examples/Editor/EditorInitHooks/");
            DocsMenuItem("Editor Shutdown Hook", "Development/NativeAddon/Examples/Editor/EditorShutdownHook/");
            DocsMenuItem("Game Preview Resolution", "Development/NativeAddon/Examples/Editor/GamePreviewResolution/");
            DocsMenuItem("Gizmo Tool", "Development/NativeAddon/Examples/Editor/GizmoTool/");
            DocsMenuItem("Hierarchy Item GUI", "Development/NativeAddon/Examples/Editor/HierarchyItemGUI/");
            DocsMenuItem("Keyboard Shortcuts", "Development/NativeAddon/Examples/Editor/KeyboardShortcuts/");
            DocsMenuItem("Menu Positioning", "Development/NativeAddon/Examples/Editor/MenuPositioning/");
            DocsMenuItem("Packaging Hooks", "Development/NativeAddon/Examples/Editor/PackagingHooks/");
            DocsMenuItem("Play Mode Hooks", "Development/NativeAddon/Examples/Editor/PlayModeHooks/");
            DocsMenuItem("Preferences Panel", "Development/NativeAddon/Examples/Editor/PreferencesPanel/");
            DocsMenuItem("Project Lifecycle", "Development/NativeAddon/Examples/Editor/ProjectLifecycle/");
            DocsMenuItem("Property Drawer", "Development/NativeAddon/Examples/Editor/PropertyDrawer/");
            DocsMenuItem("Scene Lifecycle", "Development/NativeAddon/Examples/Editor/SceneLifecycle/");
            DocsMenuItem("Scene Type Registration", "Development/NativeAddon/Examples/Editor/SceneTypeRegistration/");
            DocsMenuItem("Selection Handler", "Development/NativeAddon/Examples/Editor/SelectionHandler/");
            DocsMenuItem("Toolbar Extension", "Development/NativeAddon/Examples/Editor/ToolbarExtension/");
            DocsMenuItem("Top Level Menu", "Development/NativeAddon/Examples/Editor/TopLevelMenu/");
            DocsMenuItem("Undo / Redo", "Development/NativeAddon/Examples/Editor/UndoRedoHook/");
            DocsMenuItem("Viewport Overlay", "Development/NativeAddon/Examples/Editor/ViewportOverlay/");

            if (ImGui::BeginMenu("Node Graph"))
            {
                DocsMenuItem("Custom Domain", "Development/NativeAddon/Examples/Editor/NodeGraph/CustomDomain/");
                DocsMenuItem("Custom Material Type", "Development/NativeAddon/Examples/Editor/NodeGraph/CustomMaterialType/");
                DocsMenuItem("Custom Node", "Development/NativeAddon/Examples/Editor/NodeGraph/CustomNode/");
                DocsMenuItem("Domain Callbacks", "Development/NativeAddon/Examples/Editor/NodeGraph/DomainCallbacks/");
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

static void DrawHelpDocumentationMenu()
{
    DocsMenuItem("Home Page", "");
    ImGui::Separator();
    DocsMenuItem("Quick Start", "Info/QuickStart/");
    DocsMenuItem("Editor", "Info/Editor/");
    DocsMenuItem("Scripting", "Info/Scripting/");
    DocsMenuItem("Build Profiles", "Info/BuildProfiles/");
    DocsMenuItem("Addons", "Info/Addons/");
    DocsMenuItem("FAQ", "Info/FAQ/");
    ImGui::Separator();
    DocsMenuItem("C++ Reference", "api/");
    ImGui::Separator();

    if (ImGui::BeginMenu("Lua"))
    {
        DrawHelpDocumentationLuaMenu();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Development"))
    {
        DrawHelpDocumentationDevelopmentMenu();
        ImGui::EndMenu();
    }
}

static void DrawFileMenu(bool& openSaveSceneAsModal)
{
    ActionManager* am = ActionManager::Get();
    EditScene* editScene = GetEditorState()->GetEditScene();

    if (ImGui::MenuItem("Project Select..."))
        GetProjectSelectWindow()->Open();

    if (ImGui::BeginMenu("Recent Projects"))
    {
        const std::vector<std::string>& recentProjects = GetEditorState()->mRecentProjects;

        if (recentProjects.empty())
        {
            ImGui::TextDisabled("(No recent projects)");
        }
        else
        {
            for (size_t i = 0; i < recentProjects.size(); ++i)
            {
                const std::string& projPath = recentProjects[i];

                std::string label = projPath;
                size_t slash = label.find_last_of("/\\");
                if (slash != std::string::npos)
                    label = label.substr(slash + 1);

                ImGui::PushID((int)i);
                if (ImGui::MenuItem(label.c_str()))
                {
                    am->RequestOpenProject(projPath.c_str());
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", projPath.c_str());
                ImGui::PopID();
            }
        }

        ImGui::EndMenu();
    }

    // Close-current-project — symmetric with Project Select / Recent Projects.
    // Drops every loaded native addon DLL on its way out so the user can then
    // wipe Intermediate/Plugins/ from Explorer and reopen with a clean rebuild.
    // Useful both as an explicit user action and as the precondition for the
    // Tier-2 Recover from stuck addons flow.
    {
        const bool hasOpenProject = !GetEngineState()->mProjectDirectory.empty();
        if (ImGui::MenuItem("Close Project", nullptr, false, hasOpenProject))
        {
            // Defer the actual close until end-of-frame so we're not closing
            // scenes / unloading addons while a menu draw is still in flight.
            // Matches the deferred pattern used by RequestOpenProject.
            am->RequestCloseProject();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Close the current project. Unloads every native addon DLL "
                "so the addon's Intermediate/Plugins folder is no longer "
                "locked. Returns to the project-select window.");
        }
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("New Scene"))
            GetEditorState()->OpenEditScene(nullptr);
        if (ImGui::MenuItem("Save Scene", nullptr, false, editScene != nullptr))
        {
            Scene* scene = editScene ? editScene->mSceneAsset.Get<Scene>() : nullptr;
            AssetStub* sceneStub = scene ? AssetManager::Get()->GetAssetStub(scene->GetName()) : nullptr;
            if (sceneStub != nullptr)
            {
                GetEditorState()->CaptureAndSaveScene(sceneStub, nullptr);
            }
            else
            {
                openSaveSceneAsModal = true;
                EditorImgui_ResetSaveSceneAsBuffer();
            }
        }
        if (ImGui::MenuItem("Save Scene As...", nullptr, false, editScene != nullptr))
        {
            openSaveSceneAsModal = true;
            EditorImgui_ResetSaveSceneAsBuffer();
        }
        if (ImGui::MenuItem("Recapture All Scenes"))
            am->RecaptureAndSaveAllScenes();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Recent Scenes"))
    {
        const std::vector<RecentScene>& recentScenes = GetEditorState()->mRecentScenes;
        AssetManager* assetMgr = AssetManager::Get();
        bool hasValid = false;

        for (const RecentScene& r : recentScenes)
        {
            if (assetMgr && assetMgr->DoesAssetExist(r.mSceneName))
            {
                hasValid = true;
                if (ImGui::MenuItem(r.mSceneName.c_str()))
                {
                    AssetStub* stub = assetMgr->GetAssetStub(r.mSceneName);
                    if (stub)
                    {
                        if (!stub->mAsset)
                            assetMgr->LoadAsset(*stub);
                        Scene* scene = stub->mAsset ? stub->mAsset->As<Scene>() : nullptr;
                        if (scene)
                            GetEditorState()->OpenEditScene(scene);
                    }
                }
            }
        }

        if (!hasValid)
            ImGui::TextDisabled("(No recent scenes)");

        ImGui::EndMenu();
    }

    ImGui::Separator();


    if (ImGui::BeginMenu("Import"))
    {
        if (ImGui::MenuItem("Scene"))
            am->BeginImportScene();
        if (ImGui::MenuItem("Asset"))
            am->ImportAsset();
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Build Profiles"))
        GetPackagingWindow()->Open();

   

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawMenuItems("File");
}

static void DrawEditMenu()
{
    ActionManager* am = ActionManager::Get();

    if (ImGui::MenuItem("Undo"))
        am->Undo();
    if (ImGui::MenuItem("Redo"))
        am->Redo();

    ImGui::Separator();

    if (ImGui::MenuItem("Preferences..."))
        GetPreferencesWindow()->Open();
    if (ImGui::MenuItem("Editor Hotkeys..."))
        GetEditorHotkeysWindow()->Open();
    if (ImGui::MenuItem("App Settings..."))
        GetAppSettingsWindow()->Open();

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawMenuItems("Edit");
}

static void DrawVersionControlMenu()
{
    if (ImGui::BeginMenu("Git"))
    {
        if (ImGui::MenuItem("Open Git Panel"))
        {
            GetGitWorkspaceWindow()->Open();
        }

        ImGui::Separator();

        bool repoOpen = GitService::Get() && GitService::Get()->IsRepositoryOpen();

        if (ImGui::MenuItem("Fetch", nullptr, false, repoOpen))
        {
            if (GitService::Get()->GetCurrentRepo())
            {
                GitOperationRequest req;
                req.mKind = GitOperationKind::Fetch;
                req.mRepoPath = GitService::Get()->GetCurrentRepo()->GetPath();
                req.mCancelToken = CreateCancelToken();
                GitService::Get()->GetOperationQueue()->Enqueue(req);
            }
        }

        if (ImGui::MenuItem("Pull...", nullptr, false, repoOpen))
        {
            if (GitService::Get()->GetCurrentRepo())
            {
                GitOperationRequest req;
                req.mKind = GitOperationKind::Pull;
                req.mRepoPath = GitService::Get()->GetCurrentRepo()->GetPath();
                req.mCancelToken = CreateCancelToken();
                GitService::Get()->GetOperationQueue()->Enqueue(req);
            }
        }

        if (ImGui::MenuItem("Push...", nullptr, false, repoOpen))
        {
            GitRepository* pushRepo = GitService::Get()->GetCurrentRepo();
            if (pushRepo)
            {
                GitOperationRequest req;
                req.mKind = GitOperationKind::Push;
                req.mRepoPath = pushRepo->GetPath();
                req.mBranchName = pushRepo->GetCurrentBranch();
                std::vector<GitRemoteInfo> pushRemotes = pushRepo->GetRemotes();
                if (!pushRemotes.empty())
                    req.mRemoteName = pushRemotes[0].mName;
                req.mCancelToken = CreateCancelToken();
                GitService::Get()->GetOperationQueue()->Enqueue(req);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh", nullptr, false, repoOpen))
        {
            if (GitService::Get()->GetCurrentRepo())
            {
                GitService::Get()->GetCurrentRepo()->RefreshStatus();
            }
        }

        ImGui::EndMenu();
    }
}

static void DrawViewMenu()
{
    Renderer* renderer = Renderer::Get();
    Camera3D* cam = GetEditorState()->GetEditorCamera();
    ProjectionMode projMode = cam ? cam->GetProjectionMode() : ProjectionMode::PERSPECTIVE;
    if ((projMode == ProjectionMode::PERSPECTIVE && ImGui::MenuItem("Orthographic")) ||
        (projMode == ProjectionMode::ORTHOGRAPHIC && ImGui::MenuItem("Perspective")))
    {
        GetEditorState()->ToggleEditorCameraProjection();
    }
    if (ImGui::MenuItem("Wireframe"))
        renderer->SetDebugMode(renderer->GetDebugMode() == DEBUG_WIREFRAME ? DEBUG_NONE : DEBUG_WIREFRAME);
    if (ImGui::MenuItem("Collision"))
        renderer->SetDebugMode(renderer->GetDebugMode() == DEBUG_COLLISION ? DEBUG_NONE : DEBUG_COLLISION);
    if (ImGui::MenuItem("Proxy"))
        renderer->EnableProxyRendering(!renderer->IsProxyRenderingEnabled());
    if (ImGui::MenuItem("Spline Lines"))
        Spline3D::SetSplineLinesVisible(!Spline3D::IsSplineLinesVisible());
    if (ImGui::MenuItem("Bounds"))
    {
        uint32_t newMode = (uint32_t(renderer->GetBoundsDebugMode()) + 1) % uint32_t(BoundsDebugMode::Count);
        renderer->SetBoundsDebugMode((BoundsDebugMode)newMode);
    }
    if (ImGui::MenuItem("Occlusion Preview", nullptr, renderer->IsOcclusionPreviewEnabled()))
    {
        renderer->EnableOcclusionPreview(!renderer->IsOcclusionPreviewEnabled());
        LogDebug("Occlusion preview %s (enable Bounds to see cells).", renderer->IsOcclusionPreviewEnabled() ? "enabled" : "disabled");
    }
    if (ImGui::MenuItem("Grid"))
        ToggleGrid();
    if (ImGui::MenuItem("Stats"))
        renderer->EnableStatsOverlay(!renderer->IsStatsOverlayEnabled());
    {
        bool consolePreview = GetEditorState()->ShouldPreviewConsoleLighting();
        const char* previewLightingLabel = consolePreview ? "Preview Lighting (off: console target)" : "Preview Lighting";
        if (ImGui::MenuItem(previewLightingLabel, nullptr, false, !consolePreview))
        {
            GetEditorState()->mPreviewLighting = !GetEditorState()->mPreviewLighting;
            LogDebug("Preview lighting %s", GetEditorState()->mPreviewLighting ? "enabled." : "disabled.");
        }
    }

    if (GetEditorState()->GetEditorMode() == EditorMode::Scene2D)
    {
        if (ImGui::MenuItem("Reset 2D Viewport"))
        {
            GetEditorState()->GetViewport2D()->ResetViewport();
        }
    }

    if (ImGui::BeginMenu("Interface Scale"))
    {
        static float sInterfaceScale = GetEngineConfig()->mEditorInterfaceScale;
        ImGui::SliderFloat("IntScale", &sInterfaceScale, 0.5f, 3.0f);
        if (ImGui::Button("Apply"))
        {
            GetMutableEngineConfig()->mEditorInterfaceScale = sInterfaceScale;
            WriteEngineConfig();
        }
        ImGui::EndMenu();
    }

    if (GetFeatureFlagsEditor().mShowTheming == true) {
        if (ImGui::MenuItem("Theme Editor..."))
        {
            GetThemeEditorWindow()->Open();
        }
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Camera Bookmarks"))
    {
        EditorHotkeyMap* hk = EditorHotkeyMap::Get();
        EditorState* es = GetEditorState();

        // User-facing slot labels follow the keyboard order: 1..9 then 0.
        static const int32_t kBookmarkSlot[kEditorCameraBookmarkSlotCount] = { 0,1,2,3,4,5,6,7,8,9 };
        static const char    kBookmarkLabel[kEditorCameraBookmarkSlotCount] = { '1','2','3','4','5','6','7','8','9','0' };
        static const EditorAction kSaveAction[kEditorCameraBookmarkSlotCount] =
        {
            EditorAction::View_SaveBookmark1, EditorAction::View_SaveBookmark2,
            EditorAction::View_SaveBookmark3, EditorAction::View_SaveBookmark4,
            EditorAction::View_SaveBookmark5, EditorAction::View_SaveBookmark6,
            EditorAction::View_SaveBookmark7, EditorAction::View_SaveBookmark8,
            EditorAction::View_SaveBookmark9, EditorAction::View_SaveBookmark0,
        };
        static const EditorAction kGotoAction[kEditorCameraBookmarkSlotCount] =
        {
            EditorAction::View_GotoBookmark1, EditorAction::View_GotoBookmark2,
            EditorAction::View_GotoBookmark3, EditorAction::View_GotoBookmark4,
            EditorAction::View_GotoBookmark5, EditorAction::View_GotoBookmark6,
            EditorAction::View_GotoBookmark7, EditorAction::View_GotoBookmark8,
            EditorAction::View_GotoBookmark9, EditorAction::View_GotoBookmark0,
        };

        if (ImGui::BeginMenu("Save"))
        {
            for (int32_t i = 0; i < kEditorCameraBookmarkSlotCount; ++i)
            {
                char label[32];
                snprintf(label, sizeof(label), "Save Bookmark %c", kBookmarkLabel[i]);
                std::string shortcut = hk ? EditorHotkeyMap::BindingToDisplayString(hk->GetBinding(kSaveAction[i])) : std::string();
                if (ImGui::MenuItem(label, shortcut.c_str()))
                    es->SaveCameraBookmark(kBookmarkSlot[i]);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Jump"))
        {
            for (int32_t i = 0; i < kEditorCameraBookmarkSlotCount; ++i)
            {
                char label[32];
                bool valid = es->HasCameraBookmark(kBookmarkSlot[i]);
                snprintf(label, sizeof(label), "Jump to Bookmark %c%s", kBookmarkLabel[i], valid ? "" : " (empty)");
                std::string shortcut = hk ? EditorHotkeyMap::BindingToDisplayString(hk->GetBinding(kGotoAction[i])) : std::string();
                if (ImGui::MenuItem(label, shortcut.c_str(), false, valid))
                    es->RestoreCameraBookmark(kBookmarkSlot[i]);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Panels"))
    {
        if (ImGui::MenuItem("Scene"))
            GetEditorState()->mShowLeftPane = !GetEditorState()->mShowLeftPane;
        if (ImGui::MenuItem("Assets"))
            GetEditorState()->mShowLeftPane = !GetEditorState()->mShowLeftPane;
        if (ImGui::MenuItem("Properties"))
            GetEditorState()->mShowRightPane = !GetEditorState()->mShowRightPane;
        if (ImGui::MenuItem("Debug Log"))
            GetEditorState()->mShowBottomPane = !GetEditorState()->mShowBottomPane;
        if (ImGui::MenuItem("Timeline"))
            GetEditorState()->mShowTimelinePanel = !GetEditorState()->mShowTimelinePanel;
        if (ImGui::MenuItem("3DS Preview"))
            GetEditorState()->mShow3DSPreview = !GetEditorState()->mShow3DSPreview;
        if (ImGui::MenuItem("Game Preview"))
            GetEditorState()->mShowGamePreview = !GetEditorState()->mShowGamePreview;
        if (ImGui::MenuItem("Node Graph"))
            GetEditorState()->mShowNodeGraphPanel = !GetEditorState()->mShowNodeGraphPanel;
        if (ImGui::MenuItem("Animation Browser"))
            GetEditorState()->mShowAnimationBrowser = !GetEditorState()->mShowAnimationBrowser;
        if (ImGui::MenuItem("Bone Mask Editor"))
            GetEditorState()->mShowBoneMaskEditor = !GetEditorState()->mShowBoneMaskEditor;
        if (ImGui::MenuItem("Profiling"))
            GetEditorState()->mShowProfilingPanel = !GetEditorState()->mShowProfilingPanel;
        if (ImGui::MenuItem("CLI Terminal"))
            GetTerminalPanel()->mVisible = !GetTerminalPanel()->mVisible;

        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout"))
            EditorImgui_RequestDockReset();

        ImGui::EndMenu();
    }

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawMenuItems("View");
}

static void DrawWorldMenu()
{
    ActionManager* am = ActionManager::Get();
    Renderer* renderer = Renderer::Get();

    if (ImGui::BeginMenu("Spawn Node"))
    {
        DrawAddNodeMenu(nullptr);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Spawn Basic 3D"))
    {
        DrawSpawnBasic3dMenu(nullptr, true);
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Clear World"))
        am->DeleteAllNodes();
    if (ImGui::MenuItem("Bake Lighting"))
        renderer->BeginLightBake();
    if (ImGui::MenuItem("Clear Baked Lighting"))
    {
        const std::vector<Node*>& nodes = GetWorld(0)->GatherNodes();
        for (uint32_t a = 0; a < nodes.size(); ++a)
        {
            StaticMesh3D* meshNode = nodes[a]->As<StaticMesh3D>();
            if (meshNode != nullptr && meshNode->GetBakeLighting())
            {
                meshNode->ClearInstanceColors();
            }
        }
    }
    {
        bool canBake = !IsPlayingInEditor();
        if (ImGui::MenuItem("Bake Occlusion Culling", nullptr, false, canBake))
            am->RequestBakeOcclusion();
        if (ImGui::MenuItem("Clear Occlusion Data", nullptr, false, canBake))
            am->RequestClearOcclusion();
        if (ImGui::MenuItem("Mark Selected Occluder + Occludee"))
        {
            const std::vector<Node*>& selected = GetEditorState()->GetSelectedNodes();
            for (Node* node : selected)
            {
                if (node != nullptr && node->IsPrimitive3D())
                {
                    am->EXE_EditProperty(node, PropertyOwnerType::Node, "Occluder Static", 0, Datum(true));
                    am->EXE_EditProperty(node, PropertyOwnerType::Node, "Occludee Static", 0, Datum(true));
                }
            }
        }
    }
    if (ImGui::MenuItem("Toggle Transform Mode"))
        GetEditorState()->GetViewport3D()->ToggleTransformMode();

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawMenuItems("World");
}

static void DrawToolsAddonsMenu()
{
    if (ImGui::MenuItem("Addons Manager"))
    {
        GetAddonsWindow()->Open();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Reload Native Addons"))
    {
        NativeAddonManager* nam = NativeAddonManager::Get();
        if (nam != nullptr)
        {
            // Empty addon list = "all installed enabled native addons".
            // Routed through the project-restart chokepoint so live nodes
            // don't keep vtable pointers into about-to-be-unloaded DLLs.
            //
            // forceRebuild=true so Reload always source-compiles, even for
            // resolveMode=binary addons whose synced-binary cache would
            // otherwise short-circuit the build. Matches the per-addon
            // Reload button in AddonsWindow.cpp:OnReloadNativeAddon. The
            // restart machinery also sets mForceSourceForNextLoad for each
            // target so LoadNativeAddon ignores binary mode on the next load
            // and picks up the freshly built source DLL.
            nam->ReloadNativeAddonsWithProjectRestart({}, /*forceRebuild*/true,
                                                      "Edit > Reload Native Addons");
        }
    }

    // Tier-2 escalation: when Reload Native Addons fails because mspdbsrv /
    // a stray linker is pinning the .pdb, this does a heavier reset — closes
    // the project entirely (drops every addon DLL handle), kills the locker
    // processes, deletes Intermediate/Plugins/ outright, then reopens the
    // project. The build-blocked modal surfaces a button for the same flow
    // after two unsuccessful Retries.
    if (ImGui::MenuItem("Recover from stuck addons"))
    {
        // Deferred so close-project / unload-addons runs at end-of-frame,
        // safely past the menu draw rather than mid-popup.
        ActionManager::Get()->RequestAddonRecovery("Tools > Recover from stuck addons");
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Tier-2 recovery: close project, taskkill mspdbsrv/link, wipe\n"
            "Intermediate/Plugins, reopen project. Use when Reload Native\n"
            "Addons leaves the build blocked on locked .pdb / .dll files.");
    }

    // Tier-3 escalation: when even Tier-2 can't free the locked files (rare —
    // typically VS debugger or Defender holding the .pdb), restart the editor
    // process entirely. The Windows Job Object's KILL_ON_JOB_CLOSE guarantees
    // mspdbsrv dies with the editor; the new instance gets --addon-recovery
    // and re-opens the project with a clean wipe. Implementation lives in
    // EditorAddonRecovery; this menu just kicks it off.
    if (ImGui::MenuItem("Restart Editor (Clean Addon Rebuild)"))
    {
        ActionManager* am = ActionManager::Get();
        if (am != nullptr)
        {
            am->RequestEditorRestartForAddonRecovery(
                "Tools > Restart Editor (Clean Addon Rebuild)");
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Tier-3 recovery: spawn a fresh editor, exit this one. The OS\n"
            "tears down every build subprocess (mspdbsrv etc.) when this\n"
            "instance exits, then the new instance wipes Intermediate/Plugins\n"
            "before opening the project. Use only when Recover from stuck\n"
            "addons still can't unlock the .pdb.");
    }

    if (ImGui::MenuItem("Discover Native Addons"))
    {
        NativeAddonManager* nam = NativeAddonManager::Get();
        if (nam != nullptr)
        {
            nam->DiscoverNativeAddons();
            LogDebug("Native addons discovered.");
        }
    }

    if (ImGui::MenuItem("Regenerate Native Addon Dependencies"))
    {
        NativeAddonManager* nam = NativeAddonManager::Get();
        if (nam != nullptr)
        {
            std::vector<std::string> localIds = nam->GetLocalPackageIds();
            for (const std::string& id : localIds)
            {
                std::string addonPath = nam->GetAddonSourcePath(id);
                if (!addonPath.empty())
                {
                    nam->GenerateIDEConfig(addonPath);
                }
            }
            LogDebug("Native addon dependencies regenerated for %d addon(s).", (int)localIds.size());
        }
    }

    ImGui::Separator();

    DrawAddonsPopupContent();

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawAddonsMenuItems();
}

static void DrawToolsMenu()
{
    // Directory paths shared between submenus
    const std::string& projectDir = GetEngineState()->mProjectDirectory;
    bool hasProject = !projectDir.empty();
    std::string assetsDir = projectDir + "Assets/";
    std::string scriptsDir = projectDir + "Scripts/";
    std::string addonsDir = projectDir + "Packages/";
    std::string polyphaseDir = SYS_GetPolyphasePath();

    if (ImGui::BeginMenu("Addons"))
    {
        DrawToolsAddonsMenu();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Version Control"))
    {
        DrawVersionControlMenu();
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Memory Snapshot"))
    {
        GetMemorySnapshotWindow()->Open();
    }

    if (ImGui::BeginMenu("Open In..."))
    {
        if (ImGui::BeginMenu("VS Code"))
        {
            auto openInVSCode = [](const std::string& dir) {
                std::string absPath = SYS_GetAbsolutePath(dir);
                // Strip trailing slashes — on Windows cmd.exe interprets a
                // trailing backslash inside double quotes as an escape for the
                // closing quote ("...\\\"" -> mangled args), which makes
                // `code` receive a corrupted path and crash with an ICU error.
                while (!absPath.empty() && (absPath.back() == '/' || absPath.back() == '\\'))
                    absPath.pop_back();
#if PLATFORM_WINDOWS
                SYS_ExecDetached(("code \"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
                SYS_ExecDetached(("code \"" + absPath + "\"").c_str());
#endif
            };

            if (ImGui::MenuItem("Project Directory", nullptr, false, hasProject))
                openInVSCode(projectDir);
            if (ImGui::MenuItem("Project Assets Directory", nullptr, false, hasProject))
                openInVSCode(assetsDir);
            if (ImGui::MenuItem("Project Scripts Directory", nullptr, false, hasProject))
                openInVSCode(scriptsDir);
            if (ImGui::MenuItem("Project Addons Directory", nullptr, false, hasProject))
                openInVSCode(addonsDir);
            if (ImGui::MenuItem("Polyphase Engine Directory"))
                openInVSCode(polyphaseDir);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Code Editor"))
        {
            PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
            EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
            bool hasEditor = editors && editors->IsLuaEditorConfigured();

            auto openInEditor = [&](const std::string& dir) {
                if (editors)
                {
                    std::string absPath = SYS_GetAbsolutePath(dir);
                    while (!absPath.empty() && (absPath.back() == '/' || absPath.back() == '\\'))
                        absPath.pop_back();
                    std::string cmd = editors->BuildLuaOpenCommand(absPath);
                    SYS_ExecDetached(cmd.c_str());
                }
            };

            if (ImGui::MenuItem("Project Directory", nullptr, false, hasProject && hasEditor))
                openInEditor(projectDir);
            if (ImGui::MenuItem("Project Assets Directory", nullptr, false, hasProject && hasEditor))
                openInEditor(assetsDir);
            if (ImGui::MenuItem("Project Scripts Directory", nullptr, false, hasProject && hasEditor))
                openInEditor(scriptsDir);
            if (ImGui::MenuItem("Project Addons Directory", nullptr, false, hasProject && hasEditor))
                openInEditor(addonsDir);
            if (ImGui::MenuItem("Polyphase Engine Directory", nullptr, false, hasEditor))
                openInEditor(polyphaseDir);

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Reveal in Explorer"))
    {
        auto revealDir = [](const std::string& dir) {
            std::string absPath = SYS_GetAbsolutePath(dir);
            while (!absPath.empty() && (absPath.back() == '/' || absPath.back() == '\\'))
                absPath.pop_back();
#if PLATFORM_WINDOWS
            for (char& c : absPath) { if (c == '/') c = '\\'; }
            SYS_ExecDetached(("explorer \"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
            SYS_ExecDetached(("xdg-open \"" + absPath + "\"").c_str());
#endif
        };

        if (ImGui::MenuItem("Project Directory", nullptr, false, hasProject))
            revealDir(projectDir);
        if (ImGui::MenuItem("Project Assets Directory", nullptr, false, hasProject))
            revealDir(assetsDir);
        if (ImGui::MenuItem("Project Scripts Directory", nullptr, false, hasProject))
            revealDir(scriptsDir);
        if (ImGui::MenuItem("Project Addons Directory", nullptr, false, hasProject))
            revealDir(addonsDir);
        if (ImGui::MenuItem("Polyphase Engine Directory"))
            revealDir(polyphaseDir);

        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Check Build Dependencies"))
    {
        GetBuildDependencyWindow()->Open();
    }

    if (ImGui::MenuItem("Texture Atlas Viewer"))
    {
        GetEditorState()->mShowTextureAtlasViewer = !GetEditorState()->mShowTextureAtlasViewer;
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Write Config"))
    {
        WriteEngineConfig();
    }

    ImGui::Separator();
    ActionManager* am = ActionManager::Get();

    if (ImGui::MenuItem("Run Script"))
        am->RunScript();
    if (ImGui::MenuItem("Resave All Assets"))
        am->RequestResaveAllAssets();
    if (ImGui::MenuItem("Reload All Scripts"))
    {
        // Lua-only. Native addon reload is gated behind the project-restart
        // chokepoint and lives in the Edit > Reload Native Addons menu and
        // the AddonsWindow per-row Reload button.
        GetEditorState()->RequestReloadAllScripts();
        InvalidateScriptPickerCache();
    }

    {
        bool hasProject = GetEngineState()->mProjectDirectory != "";
        bool csharpEnabled = GetEngineConfig()->mCSharpScripting;

        if (hasProject && ImGui::BeginMenu("CSharp"))
        {
            if (csharpEnabled)
            {
                if (ImGui::MenuItem("Build C# Scripts", "Ctrl+R"))
                {
                    // Same path Ctrl+R takes: transpile, then reload everything so
                    // the regenerated .lua is live.
                    if (CSharpManager::Get()->Transpile())
                    {
                        GetEditorState()->RequestReloadAllScripts();
                        InvalidateScriptPickerCache();
                    }
                }

                if (ImGui::MenuItem("Open C# Solution"))
                {
                    // VS Code folder-open > Visual Studio Game.sln > OS fallback.
                    // (A bare .csproj opened as a FILE in VS Code gives no
                    // IntelliSense — its C# extension needs the workspace.)
                    CSharpManager::Get()->OpenIde();
                }
            }

            if (ImGui::MenuItem("Check Dependencies"))
            {
                std::string version;
                if (CSharpManager::Get()->CheckDotnet(&version, true))
                {
                    LogDebug("[CSharp] .NET SDK %s found.", version.c_str());
                }
                else
                {
                    LogError("[CSharp] .NET SDK 8+ not found. Install with: %s  (or download: %s)",
                        CSharpManager::GetDotnetInstallCommand(),
                        CSharpManager::GetDotnetInstallUrl());
                }
                GetBuildDependencyWindow()->Open();
            }

            if (!csharpEnabled && ImGui::MenuItem("Enable C# for This Project"))
            {
                CSharpManager::Get()->EnableForProject();
            }

            ImGui::EndMenu();
        }
    }

    ImGui::Separator();



    if (GetEditorState()->mDevMode &&
        GetEngineState()->mStandalone &&
        ImGui::MenuItem("Prepare Release"))
    {
        ActionManager::Get()->PrepareRelease();
    }

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr)
    {
        // Backwards-compat: addons that registered against the legacy
        // "Developer" and "Extra" menu names continue to render here under
        // the renamed Tools menu (Developer was renamed; Extra was removed
        // and its developer-flavoured items migrated to Tools).
        hookMgr->DrawMenuItems("Developer");
        hookMgr->DrawMenuItems("Extra");
        hookMgr->DrawMenuItems("Tools");
    }
}

static void DrawHelpMenu()
{
    if (ImGui::MenuItem("Check for Updates..."))
    {
        AutoUpdater::Get()->CheckForUpdates(true);
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Documentation"))
    {
        DrawHelpDocumentationMenu();
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Report Issue"))
    {
        OpenUrl("https://github.com/Polyphase-Labs/Polyphase-Engine/issues");
    }

    ImGui::Separator();

    char versionStr[32];
    snprintf(versionStr, 31, "Version: %d", POLYPHASE_VERSION);
    ImGui::MenuItem(versionStr, nullptr, false, false);

    if (ImGui::IsItemHovered() && IsMouseButtonJustUp(MOUSE_RIGHT))
    {
        sDevModeClicks++;
        if (sDevModeClicks >= 5)
        {
            GetEditorState()->mDevMode = true;
        }
    }

    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr) hookMgr->DrawMenuItems("Help");
}

void DrawMainMenuBarMenus(bool& openSaveSceneAsModal)
{
    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();

    if (ImGui::BeginMenu("File"))
    {
        DrawFileMenu(openSaveSceneAsModal);
        ImGui::EndMenu();
    }
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::AfterFile);

    if (ImGui::BeginMenu("Edit"))
    {
        DrawEditMenu();
        ImGui::EndMenu();
    }
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::AfterEdit);

    if (ImGui::BeginMenu("View"))
    {
        DrawViewMenu();
        ImGui::EndMenu();
    }
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::AfterView);

    if (ImGui::BeginMenu("World"))
    {
        DrawWorldMenu();
        ImGui::EndMenu();
    }
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::AfterWorld);

    if (ImGui::BeginMenu("Tools"))
    {
        DrawToolsMenu();
        ImGui::EndMenu();
    }
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::AfterTools);

    // Legacy slots — render at the same anchor (between Tools and Help) to
    // preserve compatibility with addons registered against positions 5/6
    // before the top-level Addons and Extra menus were folded into Tools.
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::Legacy_AfterAddons);
    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenusAtPosition(EditorMenuPos::Legacy_AfterExtra);

    if (ImGui::BeginMenu("Help"))
    {
        DrawHelpMenu();
        ImGui::EndMenu();
    }

    if (hookMgr != nullptr) hookMgr->DrawTopLevelMenus();
}

#endif
