#if EDITOR

#include "EditorImgui.h"
#include "EditorWidgets.h"
#include "System/System.h"
#include "Engine.h"
#include "EditorUtils.h"
#include "EditorConstants.h"
#include "Renderer.h"
#include "InputDevices.h"
#include "Log.h"
#include "AssetDir.h"
#include "Grid.h"
#include "Script.h"
#include "PaintManager.h"
#include "NodePath.h"
#include "FeatureFlags.h"
#include "EditorIcons.h"
#include "EditorIconRegistry.h"

#include "Nodes/NodeClipboard.h"
#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/InstancedMesh3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Nodes/3D/Camera3d.h"
#include "Nodes/3D/Spline3d.h"
#include "Nodes/3D/PointLight3d.h"
#include "Nodes/3D/DirectionalLight3d.h"
#include "Nodes/3D/Particle3d.h"
#include "Nodes/3D/Audio3d.h"
#include "Nodes/3D/Box3d.h"
#include "Nodes/3D/Sphere3d.h"
#include "Nodes/3D/Capsule3d.h"
#include "Nodes/3D/ShadowMesh3d.h"
#include "Nodes/3D/TextMesh3d.h"
#include "Nodes/3D/Voxel3d.h"
#include "Nodes/3D/Terrain3d.h"
#include "Nodes/3D/TileMap2d.h"
#include "Assets/TileMap.h"
#include "Assets/TileSet.h"
#include "World.h"

#include "Assets/Scene.h"
#include "Assets/SoundWave.h"
#include "Assets/ParticleSystem.h"
#include "Assets/StaticMesh.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/MaterialBase.h"
#include "Assets/MaterialInstance.h"
#include "Assets/MaterialLite.h"
#include "Assets/Timeline.h"
#include "Assets/Font.h"
#include "Assets/NodeGraphAsset.h"
#include "Assets/DataAsset.h"
#include "Assets/SpriteAnimation.h"
#include "UI/UIDocument.h"
#include "UI/UITypes.h"

#include "Viewport3d.h"
#include "Viewport2d.h"
#include "ActionManager.h"
#include "EditorState.h"
#include "Preferences/PreferencesWindow.h"
#include "Preferences/Appearance/Theme/ThemeModule.h"
#include "Preferences/Appearance/Viewport/ViewportModule.h"
#include "Packaging/PackagingWindow.h"
#include "AppSettings/AppSettingsWindow.h"
#include "ProjectSelect/ProjectSelectWindow.h"
#include "Addons/AddonsWindow.h"
#include "Addons/NativeAddonManager.h"
#include "Addons/AddonsMenu.h"
#include "EditorUIHookManager.h"
#include "BuildDependencyWindow.h"
#include "InputMapWindow.h"
#include "Hotkeys/EditorHotkeyMap.h"
#include "Hotkeys/EditorHotkeysWindow.h"
#include "Git/GitWorkspaceWindow.h"
#include "Git/GitService.h"
#include "Git/GitRepository.h"
#include "Git/GitOperationQueue.h"
#include "Git/GitStatusBarWidget.h"
#include "Git/Dialogs/GitCloneDialog.h"
#include "Git/Dialogs/GitOpenDialog.h"
#include "Git/Dialogs/GitInitDialog.h"
#include "Git/Dialogs/GitCreateBranchDialog.h"
#include "Git/Dialogs/GitCheckoutConfirmDialog.h"
#include "Git/Dialogs/GitCreateTagDialog.h"
#include "Git/Dialogs/GitDeleteConfirmDialog.h"
#include "Git/Dialogs/GitPushDialog.h"
#include "Git/Dialogs/GitPullDialog.h"
#include "Git/Dialogs/GitFetchDialog.h"
#include "Git/Dialogs/GitRemoteEditDialog.h"
#include "Git/Dialogs/GitMergeDialog.h"
#include "Git/Dialogs/GitCliStatusDialog.h"
#include "Git/Dialogs/GitOperationProgressDialog.h"
#include "Git/Dialogs/GitSyncBranchDialog.h"
#include "PlayerInputEditor.h"
#include "PlayerInputDebugger.h"
#include "DebugLog/DebugLogWindow.h"
#include "CliTerminal/TerminalPanel.h"
#include "LuaDebugger/LuaDebuggerPanel.h"
#include "ScriptEditor/ScriptEditorWindow.h"
#include "ThemeEditor/ThemeEditorWindow.h"
#include "Preferences/Appearance/Theme/CssThemeParser.h"
#include "Timeline/TimelinePanel.h"
#include "NodeGraph/NodeGraphPanel.h"
#include "Profiling/ProfilingWindow.h"
#include "InputTester/InputTesterPanel.h"
#include "TextureAtlas/TextureAtlasViewer.h"
#include "VoxelSculpt/VoxelSculptManager.h"
#include "TerrainSculpt/TerrainSculptManager.h"
#include "TilePaint/TilePaintManager.h"
#include "TilePaint/TilePicker.h"
#include "Preferences/General/GeneralModule.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/External/LaunchersModule.h"
#include "Preferences/External/EditorsModule.h"
#include "Packaging/PackagingSettings.h"
#include "ScriptCreator/ScriptCreatorDialog.h"
#include "ControllerServer/ControllerServer.h"
#include "Preferences/Network/NetworkModule.h"
#include "AutoUpdater/AutoUpdater.h"
#include "AutoUpdater/AutoUpdaterWindow.h"
#include "EditorMainMenu.h"

#include <functional>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

// TODO: If we ever support an OpenGL backend, gotta change this.
#include "backends/imgui_impl_vulkan.cpp"
#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/VulkanUtils.h"

#include "CustomImgui.h"
#include "./ImGuizmo/ImGuizmo.h"
#include "imgui_dock.h"
#include "imgui_internal.h"

#if PLATFORM_WINDOWS
#include "backends/imgui_impl_win32.cpp"
#elif PLATFORM_LINUX
#include "imgui_impl_xcb.h"
#endif

// Cross-platform case-insensitive string comparison
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
#include <Nodes/TimelinePlayer.h>
#include <Nodes/Widgets/Widget.h>
#include <Nodes/Widgets/Button.h>
#include <Nodes/Widgets/Quad.h>
#include <Nodes/Widgets/Canvas.h>
#include <Nodes/Widgets/Console.h>
#include <Nodes/Widgets/ArrayWidget.h>
#include <Nodes/Widgets/Window.h>
#include <Nodes/Widgets/DialogWindow.h>

#include "SecondScreenPreview/SecondScreenPreview.h"
#include "GamePreview/GamePreview.h"
#include "AnimationBrowser/AnimationBrowser.h"


static const char* GetNodeIcon(Node* node)
{
    // Defensive: parent->mChildren has been observed handing out stale pointers
    // after script hot-reload (a Node gets `delete`d via its Lua wrapper's GC
    // path without going through Node::Destroy → Attach(nullptr), so the parent
    // never erases it). Returning a default icon here is a band-aid; root cause
    // lives in the Node_Lua GC / SharedPtr lifecycle.
    if (node == nullptr || node->IsDestroyed()) return ICON_STREAMLINE_PLUMP_WORLD_REMIX;

    if (node->As<InstancedMesh3D>())          return ICON_INSTANCE_MESH;
    if (node->As<ShadowMesh3D>())             return ICON_SHADOW;
    if (node->As<SkeletalMesh3D>())           return ICON_SKELETON;
    if (node->As<TextMesh3D>())               return ICON_TEXTMESH;
    if (node->As<StaticMesh3D>())             return ICON_STATIC_MESH;
    if (node->As<PointLight3D>())             return ICON_LIGHTBULB;
    if (node->As<DirectionalLight3D>())       return ICON_SUN;
    if (node->As<Camera3D>())                 return ICON_IX_VIDEO_CAMERA_FILLED;
    if (node->As<Particle3D>())               return ICON_FIREWORK;
    if (node->As<Audio3D>())                  return ICON_RIVET_ICONS_AUDIO_SOLID;
    if (node->As<Sphere3D>())                 return ICON_PH_SPHERE;
    if (node->As<Box3D>())                    return ICON_FLUENT_MDL2_CUBE_SHAPE;
    if (node->As<Capsule3D>())                return ICON_CAPSULE;
    if (node->As<Spline3D>())                 return ICON_IC_BASELINE_LINK;
    if (node->As<Text>())                     return ICON_TEXTWIDGET;
    if (node->As<Button>())                   return ICON_BUTTON;
    if (node->As<Console>())                  return ICON_CONSOLE;
    if (node->As<Canvas>())                   return ICON_CANVAS;
    if (node->As<Quad>())                     return ICON_RECT;
    if (node->As<ArrayWidget>())              return ICON_LAYERS;
    if (node->As<TimelinePlayer>())           return ICON_CLAPPERBOARD_CLOSE;
    if (node->IsSceneLinked())
    {
        Scene* scene = node->GetScene();
        if (scene != nullptr)
        {
            const char* customIcon = GetSceneIconByIndex(scene->GetIconOverride());
            if (customIcon != nullptr)
                return customIcon;
        }
        return ICON_STREAMLINE_PLUMP_WORLD_REMIX;
    }
    if (node->IsNode3D())                     return ICON_PH_SPHERE;
    if (node->IsWidget())                     return ICON_UIWIDGET;
    return ICON_STREAMLINE_PLUMP_WORLD_REMIX;
}

static const char* GetAssetIcon(TypeId type)
{
    if (type == StaticMesh::GetStaticType())       return ICON_STATIC_MESH;
    if (type == SkeletalMesh::GetStaticType())      return ICON_SKELETON;
    if (type == Texture::GetStaticType())            return ICON_TDESIGN_IMAGE_FILLED;
    if (type == Scene::GetStaticType())              return ICON_STREAMLINE_PLUMP_WORLD_REMIX;
    if (type == SoundWave::GetStaticType())          return ICON_RIVET_ICONS_AUDIO_SOLID;
    if (type == MaterialLite::GetStaticType())       return ICON_HUGEICONS_MATERIAL_AND_TEXTURE;
    if (type == MaterialBase::GetStaticType())       return ICON_HUGEICONS_MATERIAL_AND_TEXTURE;
    if (type == MaterialInstance::GetStaticType())   return ICON_HUGEICONS_MATERIAL_AND_TEXTURE;
    if (type == ParticleSystem::GetStaticType())     return ICON_FIREWORK;
    if (type == Timeline::GetStaticType())           return ICON_TIMELINE;
    if (type == Font::GetStaticType())               return ICON_TEXTMESH;
    if (type == NodeGraphAsset::GetStaticType())    return ICON_IC_BASELINE_SHARE;
    if (type == UIDocument::GetStaticType())         return ICON_MATERIAL_SYMBOLS_WIDGET_MEDIUM;
    return ICON_STREAMLINE_SHARP_NEW_FILE_REMIX;
}

struct FileBrowserDirEntry
{
    std::string mName;
    std::string mDirPath;
    bool mFolder = false;
    bool mSelected = false;
};

static float GetMenuBarPadding()
{
    ViewportModule* vm = ViewportModule::Get();
    return vm ? vm->GetMenuBarPadding() : 8.0f;
}

static float GetMainMenuBarHeight()
{
    float padding = GetMenuBarPadding();
    return ImGui::GetFontSize() + padding * 2.0f + 1.0f;
}
static float GetTopBarHeight()
{
    return GetMainMenuBarHeight();
}
static const ImVec4 kSelectedColor = ImVec4(0.12f, 0.50f, 0.47f, 1.00f);
static const ImVec4 kBgInactive = ImVec4(0.20f, 0.20f, 0.68f, 1.00f);
static const ImVec4 kBgHover = ImVec4(0.26f, 0.61f, 0.98f, 0.80f);
static const ImVec4 kToggledColor = ImVec4(0.7f, 0.2f, 0.2f, 1.0f);

constexpr const uint32_t kPopupInputBufferSize = 256;
static char sPopupInputBuffer[kPopupInputBufferSize] = {};

// XML to UIDocument conversion state
static std::string sConvertXmlSourcePath;
static bool sShowUIDocumentPicker = false;

static int sNewSceneType = 1;        // 0=2D, 1=3D
static bool sNewSceneCreateCamera = true;
static bool sNewSceneCreateSkybox = true;
static int32_t sZooColumns = 5;
static float sZooSpacing = 3.0f;

static bool sF2RenameAssetFocus = false;
static bool sDuplicateAssetFocus = false;
static bool sDuplicateNodeFocus = false;
static AssetStub* sDuplicateAssetStub = nullptr;
static bool sNodesDiscovered = false;
static bool showTheming = false;

// Optional monospace font with extended Unicode coverage, loaded for the
// CLI Terminal panel only. May remain null if F_RobotoMono16.ttf isn't
// shipped — callers must check.
static ImFont* sTerminalFont = nullptr;
static std::vector<std::string> sNode3dNames;
static std::vector<std::string> sNodeWidgetNames;
static std::vector<std::string> sNodeOtherNames;

static ImTextureID sInspectTexId = 0;
static Texture* sPrevInspectTexture = nullptr;

static bool sFileBrowserOpen = false;
static bool sFileBrowserFolderMode = false;
static FileBrowserCallbackFP sFileBrowserCallback = nullptr;
static std::vector<std::string> sFileBrowserPaths;
static std::string sFileBrowserCurDir;
static std::vector<FileBrowserDirEntry> sFileBrowserEntries;
static float sFileBrowserDoubleClickBlock = 0.0f;
static bool sFileBrowserNeedsRefresh = false;

static bool sObjectTabOpen = false;

static SceneImportOptions sSceneImportOptions;
static SceneImportOptions sReimportSceneOptions;
static CameraImportOptions sCameraImportOptions;

static std::vector<AssetStub*> sUnsavedAssets;
static std::vector<bool> sUnsavedAssetsSelected;
static bool sUnsavedModalActive = false;

static std::vector<std::string> sSceneList;
static std::unordered_map<std::string, std::vector<std::string>> sScenesByCategory;

static std::string sReplaceAssetInput;
static std::vector<std::string> sReplaceAssetSuggestions;

static Node* sCreateSceneTargetNode = nullptr;
static AssetDir* sCreateSceneTargetDir = nullptr;

static AssetStub* sAssetDropStub = nullptr;
static Node* sAssetDropParentNode = nullptr;
static bool sAssetDropInViewport = false;
static ImVec2 sAssetDropPopupPos = ImVec2(0.0f, 0.0f);
static bool sAssetDropPopupPending = false;
static int32_t sAssetDropScreenX = 0;
static int32_t sAssetDropScreenY = 0;

static bool IsBottomPaneVisible()
{
    if (!GetEditorState()->mShowBottomPane) return false;
    PreferencesManager* pm = PreferencesManager::Get();
    if (pm)
    {
        GeneralModule* gm = static_cast<GeneralModule*>(pm->FindModule("General"));
        if (gm && !gm->GetShowDebugInEditor()) return false;
    }
    return true;
}

static bool sDockInitialized = false;
static bool sDockResetRequested = false;
static int  sDockActivateViewportFrames = 0;
static ImVec2 sViewportDockPos = ImVec2(0, 0);
static ImVec2 sViewportDockSize = ImVec2(800, 600);
static ImGuiWindow* sViewportDockWindow = nullptr;
static bool sViewportDockActive = false;

// A known dock label that must exist in a valid layout.
// If imgui.ini has dock data but this label is missing, the layout is stale.
// Update kDockLayoutVersion when dock panel names change to force a reset.
static constexpr uint32_t kDockLayoutVersion = 12;

static void ValidateDockLayoutIni()
{
    const char* iniPath = ImGui::GetIO().IniFilename;
    if (!iniPath)
        return;

    FILE* f = fopen(iniPath, "rb");
    if (!f)
        return; // No INI yet — first launch, nothing to validate.

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 1024 * 1024)
    {
        fclose(f);
        return;
    }

    std::string contents(fileSize, '\0');
    fread(&contents[0], 1, fileSize, f);
    fclose(f);

    // Check if the file has a dock section at all.
    if (contents.find("[Dock]") == std::string::npos)
        return; // No dock data saved — nothing to invalidate.

    // Check for a version marker we embed: "##DockLayoutVersion=N"
    char versionTag[64];
    snprintf(versionTag, sizeof(versionTag), "##DockLayoutVersion=%u", kDockLayoutVersion);

    if (contents.find(versionTag) != std::string::npos)
        return; // Version matches — layout is current.

    // Layout is stale (version missing or wrong). Delete the file so ImGui
    // starts fresh.  The default layout will be built by DrawDockspace()
    // and saved on shutdown.
    LogDebug("Dock layout version mismatch -- resetting imgui.ini");
    remove(iniPath);
}

// Append the dock layout version marker to imgui.ini after ImGui saves it.
static void StampDockLayoutVersion()
{
    const char* iniPath = ImGui::GetIO().IniFilename;
    if (!iniPath)
        return;

    FILE* f = fopen(iniPath, "a");
    if (!f)
        return;

    fprintf(f, "\n##DockLayoutVersion=%u\n", kDockLayoutVersion);
    fclose(f);
}

static std::string ReadFileToString(const char* path)
{
    std::string result;
    FILE* f = fopen(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0)
        {
            result.resize(sz);
            fread(&result[0], 1, sz, f);
        }
        fclose(f);
    }
    return result;
}

static void RefreshUIDocuments()
{
    AssetManager* assetMgr = AssetManager::Get();
    if (!assetMgr)
        return;

    auto& assetMap = assetMgr->GetAssetMap();
    TypeId uiDocType = UIDocument::GetStaticType();

    for (auto& pair : assetMap)
    {
        AssetStub* stub = pair.second;
        if (!stub || stub->mType != uiDocType)
            continue;

        // Ensure asset is loaded so we can inspect it
        if (!stub->mAsset)
            assetMgr->LoadAsset(*stub);
        if (!stub->mAsset)
            continue;

        UIDocument* doc = stub->mAsset->As<UIDocument>();
        if (!doc)
            continue;

        const std::string& srcPath = doc->GetSourceFilePath();
        if (srcPath.empty() || !SYS_DoesFileExist(srcPath.c_str(), false))
            continue;

        // Read the .xml from disk and compare with the loaded asset.
        // If they differ, reimport from the .xml (the editable source of truth).
        std::string diskXml = ReadFileToString(srcPath.c_str());

        if (!diskXml.empty() && diskXml != doc->GetXmlSource())
        {
            doc->Import(srcPath, nullptr);
            assetMgr->SaveAsset(*stub);
            LogDebug("UIDocument reimported from XML: %s", stub->mName.c_str());
        }
    }
}

// Forward declarations for panel content functions (called from DrawDockspace)
static void DrawScenePanel();
static void DrawAssetsPanel();
static void DrawPropertiesPanel();
static void DrawScriptsPanel();
static void DrawDirPickerTree(AssetDir* dir, AssetDir** selectedDir);

// Alternating row background helpers
static void BeginAlternatingRows();
static void AlternatingRowBackground();

static void DrawDockspace()
{
    float topBarH = GetTopBarHeight();
    float displayW = ImGui::GetIO().DisplaySize.x;
    float displayH = ImGui::GetIO().DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(0.0f, topBarH));
    ImGui::SetNextWindowSize(ImVec2(displayW, displayH - topBarH));

    ImGuiWindowFlags dockHostFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("EditorDock", nullptr, dockHostFlags);
    ImGui::PopStyleVar();

    // Make dockspace child transparent so 3D render shows through Viewport area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginDockspace();
    ImGui::PopStyleColor();

    // Visibility bools synced with EditorState
    bool sceneOpen = GetEditorState()->mShowLeftPane;
    bool assetsOpen = GetEditorState()->mShowLeftPane;
    bool propsOpen = GetEditorState()->mShowRightPane;
    bool debugLogOpen = IsBottomPaneVisible();

    // --- Viewport dock (transparent so 3D render shows through) ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    sViewportDockActive = ImGui::BeginDock(ICON_VIEWPORT3D "  Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (sViewportDockActive)
    {
        sViewportDockPos = ImGui::GetWindowPos();
        sViewportDockSize = ImGui::GetWindowSize();
        sViewportDockWindow = ImGui::GetCurrentWindow();

        // Drop target for asset drag-and-drop into the 3D viewport
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        if (activePayload != nullptr && activePayload->IsDataType(DRAGDROP_ASSET))
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x > 0.0f && avail.y > 0.0f)
            {
                ImGui::InvisibleButton("##ViewportDropTarget", avail);
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
                    {
                        AssetStub* droppedStub = *(AssetStub**)payload->Data;
                        if (droppedStub != nullptr && droppedStub->mType == StaticMesh::GetStaticType())
                        {
                            sAssetDropStub = droppedStub;
                            sAssetDropParentNode = nullptr;
                            sAssetDropInViewport = true;
                            sAssetDropPopupPos = ImGui::GetMousePos();
                            sAssetDropPopupPending = true;

                            ImVec2 dropPos = ImGui::GetMousePos();
                            float scale = GetEngineConfig()->mEditorInterfaceScale;
                            if (scale == 0.0f) scale = 1.0f;
                            sAssetDropScreenX = (int32_t)(dropPos.x * scale);
                            sAssetDropScreenY = (int32_t)(dropPos.y * scale);
                        }
                        else if (droppedStub != nullptr && droppedStub->mType == Scene::GetStaticType())
                        {
                            if (droppedStub->mAsset == nullptr)
                                AssetManager::Get()->LoadAsset(*droppedStub);
                            if (droppedStub->mAsset != nullptr)
                            {
                                glm::vec3 spawnPos = EditorGetFocusPosition();
                                Camera3D* camera = GetEditorState()->GetEditorCamera();
                                if (camera != nullptr)
                                {
                                    ImVec2 dropPos = ImGui::GetMousePos();
                                    float scale = GetEngineConfig()->mEditorInterfaceScale;
                                    if (scale == 0.0f) scale = 1.0f;
                                    int32_t screenX = (int32_t)(dropPos.x * scale);
                                    int32_t screenY = (int32_t)(dropPos.y * scale);

                                    RayTestResult rayResult;
                                    camera->TraceScreenToWorld(screenX, screenY, ColGroupAll, rayResult);
                                    if (rayResult.mHitNode != nullptr)
                                        spawnPos = rayResult.mHitPosition;
                                }
                                ActionManager::Get()->SpawnBasicNode(BASIC_SCENE,
                                    nullptr, droppedStub->mAsset, true, spawnPos);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        // Viewport right-click context menu (Batch 3)
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr && hookMgr->HasViewportContextItems())
            {
                if (ImGui::BeginPopupContextWindow("ViewportContextMenu", 1))
                {
                    hookMgr->DrawViewportContextItems();
                    ImGui::EndPopup();
                }
            }
        }

        // Viewport overlays (Batch 3)
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                hookMgr->DrawViewportOverlays(
                    sViewportDockPos.x, sViewportDockPos.y,
                    sViewportDockSize.x, sViewportDockSize.y);
            }
        }
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Assets dock ---
    {
        ImVec4 assetsBg;
        ImVec4 bg = CssThemeParser::GetPanelAssetsBg(assetsBg) ? assetsBg : ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_ASSETS "  Assets", &assetsOpen))
    {
        DrawAssetsPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Scene dock ---
    {
        ImVec4 sceneBg;
        ImVec4 bg = CssThemeParser::GetPanelSceneBg(sceneBg) ? sceneBg : ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_HIERARCHY "  Scene", &sceneOpen))
    {
        DrawScenePanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Properties dock ---
    {
        ImVec4 propsPanelBg;
        ImVec4 propsBg = CssThemeParser::GetPanelPropertiesBg(propsPanelBg) ? propsPanelBg : ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, propsBg);
    }
    if (ImGui::BeginDock(ICON_INFO "  Properties", &propsOpen))
    {
        DrawPropertiesPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Debug Log dock ---
    {
        ImVec4 debugBg;
        ImVec4 bg = CssThemeParser::GetPanelDebugLogBg(debugBg) ? debugBg : ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_STREAMLINE_LOG_SOLID "  Debug Log", &debugLogOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        GetDebugLogWindow()->DrawContent();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Scripts dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_IX_CODE "  Scripts", nullptr))
    {
        DrawScriptsPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Script Editor dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_IX_CODE "  Script Editor", nullptr))
    {
        GetScriptEditorWindow()->DrawContent();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- 3DS Preview dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_CIB_NINTENDO_3DS "  3DS Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        GetSecondScreenPreview()->DrawPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Game Preview dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_IX_VIDEO_CAMERA_FILLED "  Game Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        GetGamePreview()->DrawPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

   
    {
        static bool sAnimBrowserForceHiddenOnce = true;
        if (sAnimBrowserForceHiddenOnce)
        {
            GetEditorState()->mShowAnimationBrowser = false;
            sAnimBrowserForceHiddenOnce = false;
        }
    }
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    {
        // If Open() was called this/last frame, re-dock the AnimationBrowser
        // tab next to CLI Terminal before BeginDock runs. The browser is
        // force-hidden on the editor's first frame which leaves it Float and
        // detached from the layout, so without this it would auto-dock to
        // whatever the dock root happens to be (typically the bottom-left
        // tab group with Assets/Scripts/Debug Log) instead of following the
        // CLI Terminal location.
        if (GetAnimationBrowser()->ConsumePendingDock())
        {
            ImGui::DockTo("EditorDock",
                          ICON_SKELETON "  Animation Browser",
                          ICON_CONSOLE "  CLI Terminal",
                          ImGuiDockSlot_Tab);
            ImGui::SetDockActive("EditorDock", ICON_SKELETON "  Animation Browser");
        }

        bool animOpen = GetEditorState()->mShowAnimationBrowser;
        if (ImGui::BeginDock(ICON_SKELETON "  Animation Browser", &animOpen,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            GetAnimationBrowser()->DrawPanel();
        }
        ImGui::EndDock();
        // AND with the current state so a mid-frame close from inside
        // DrawPanel() (e.g. the in-panel "Close" button calling
        // AnimationBrowser::Close() which clears mShowAnimationBrowser) isn't
        // clobbered by the stale animOpen captured at the start of the frame.
        GetEditorState()->mShowAnimationBrowser =
            animOpen && GetEditorState()->mShowAnimationBrowser;
    }
    ImGui::PopStyleColor();

    // --- Node Graph dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_IC_BASELINE_SHARE "  Node Graph", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawNodeGraphContent();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Profiling dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock(ICON_CURVEGRAPH "  Profiling", nullptr, 0))
    {
        GetProfilingWindow()->DrawContent();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // --- Input Tester dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    {
        bool testerOpen = GetEditorState()->mShowInputTesterPanel;
        if (testerOpen)
        {
            if (ImGui::BeginDock(ICON_FE_GAMEPAD "  Input Tester", &testerOpen, 0))
            {
                GetInputTesterPanel()->DrawContent();
            }
            ImGui::EndDock();
        }
        GetEditorState()->mShowInputTesterPanel =
            testerOpen && GetEditorState()->mShowInputTesterPanel;
    }
    ImGui::PopStyleColor();

    // --- CLI Terminal dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    {
        bool cliOpen = GetTerminalPanel()->mVisible;
        if (ImGui::BeginDock(ICON_CONSOLE "  CLI Terminal", &cliOpen,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            GetTerminalPanel()->DrawContent();
        }
        ImGui::EndDock();
        GetTerminalPanel()->mVisible = cliOpen;
    }
    ImGui::PopStyleColor();

    // --- Lua Debugger dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    {
        bool debugOpen = GetLuaDebuggerPanel()->mVisible;
        if (ImGui::BeginDock(ICON_MDI_BUG "  Lua Debugger", &debugOpen, ImGuiWindowFlags_NoScrollbar))
        {
            GetLuaDebuggerPanel()->DrawContent();
        }
        ImGui::EndDock();
        GetLuaDebuggerPanel()->mVisible = debugOpen;
    }
    ImGui::PopStyleColor();

    // --- Texture Atlas Viewer dock ---
    {
        ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    }
    if (ImGui::BeginDock("Texture Atlas Viewer", nullptr, 0))
    {
        GetTextureAtlasViewer()->DrawPanel();
    }
    ImGui::EndDock();
    ImGui::PopStyleColor();

    // Draw plugin/addon dockable windows
    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr)
    {
        hookMgr->DrawWindows();
    }

    ImGui::EndDockspace();
    ImGui::End();

    // Explicit layout setup on first frame or when reset is requested
    if (!sDockInitialized || sDockResetRequested)
    {
        bool isReset = sDockResetRequested;
        sDockInitialized = true;
        sDockResetRequested = false;

        if (isReset || !ImGui::HasDockLayout("EditorDock"))
        {
            ImGui::UndockAll("EditorDock");
            ImGui::DockToRoot("EditorDock", ICON_HIERARCHY "  Scene", ImGuiDockSlot_Left, 0.2f);
            ImGui::DockToRoot("EditorDock", ICON_INFO "  Properties", ImGuiDockSlot_Right, 0.45f);
            ImGui::DockToRoot("EditorDock",ICON_ASSETS "  Assets", ImGuiDockSlot_Bottom, 0.33f);
            ImGui::DockTo("EditorDock", ICON_IX_CODE "  Scripts", ICON_ASSETS "  Assets", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_STREAMLINE_LOG_SOLID "  Debug Log", ICON_ASSETS "  Assets", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_IX_CODE "  Script Editor", ICON_VIEWPORT3D "  Viewport", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_CIB_NINTENDO_3DS "  3DS Preview", ICON_INFO "  Properties", ImGuiDockSlot_Left, 0.6f);
            ImGui::DockTo("EditorDock", ICON_IX_VIDEO_CAMERA_FILLED "  Game Preview",ICON_CIB_NINTENDO_3DS "  3DS Preview",  ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_IC_BASELINE_SHARE "  Node Graph", ICON_ASSETS "  Assets", ImGuiDockSlot_Right, 0.5f);
            ImGui::DockTo("EditorDock", ICON_CURVEGRAPH "  Profiling", ICON_IC_BASELINE_SHARE "  Node Graph", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", "Texture Atlas Viewer", ICON_CURVEGRAPH "  Profiling", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_CONSOLE "  CLI Terminal", ICON_STREAMLINE_LOG_SOLID "  Debug Log", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_SKELETON "  Animation Browser", ICON_CONSOLE "  CLI Terminal", ImGuiDockSlot_Tab);
            ImGui::DockTo("EditorDock", ICON_MDI_BUG "  Lua Debugger", ICON_STREAMLINE_LOG_SOLID "  Debug Log", ImGuiDockSlot_Tab);

            // Defer activating the Viewport tab — docks call setActive() on their
            // first BeginDock frame, so we need to wait a couple frames for all
            // docks to finish their first-time setup before overriding.
            sDockActivateViewportFrames = 3;

            // Persist the fresh layout immediately so a crash won't leave a stale INI.
            if (ImGui::GetIO().IniFilename)
            {
                ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
                StampDockLayoutVersion();
            }

            if (isReset)
            {
                GetEditorState()->mShowLeftPane = true;
                GetEditorState()->mShowRightPane = true;
                GetEditorState()->mShowBottomPane = true;
            }
        }
    }

    // After layout reset, wait for all docks to finish first-frame setup,
    // then force the Viewport as the active tab in its group.
    if (sDockActivateViewportFrames > 0)
    {
        --sDockActivateViewportFrames;
        if (sDockActivateViewportFrames == 0)
        {
            ImGui::SetDockActive("EditorDock", ICON_VIEWPORT3D "  Viewport");
        }
    }

    // Sync visibility flags back from dock close buttons
    if (!sceneOpen)
        GetEditorState()->mShowLeftPane = false;
    if (!propsOpen)
        GetEditorState()->mShowRightPane = false;
    if (!debugLogOpen)
        GetEditorState()->mShowBottomPane = false;
}

static void PopulateFileBrowserDirs()
{
    sFileBrowserDoubleClickBlock = 0.2f;

    sFileBrowserEntries.clear();

    DirEntry dirEntry;
    SYS_OpenDirectory(sFileBrowserCurDir, dirEntry);

    if (!dirEntry.mValid)
    {

        GetEditorState()->RemoveFavoriteDir(sFileBrowserCurDir);

        sFileBrowserCurDir = SYS_GetAbsolutePath(GetEngineState()->mProjectDirectory);
        SYS_OpenDirectory(sFileBrowserCurDir, dirEntry);
    }

    while (dirEntry.mValid)
    {
        FileBrowserDirEntry entry;
        entry.mName = dirEntry.mFilename;
        entry.mDirPath = sFileBrowserCurDir;
        entry.mFolder = dirEntry.mDirectory;
        sFileBrowserEntries.push_back(entry);

        SYS_IterateDirectory(dirEntry);
    }

    SYS_CloseDirectory(dirEntry);

    auto alphaComp = [&](const FileBrowserDirEntry& l, const FileBrowserDirEntry& r)
    {
        if (l.mName == "..")
            return true;
        else if (r.mName == "..")
            return false;
        return l.mName < r.mName;
    };

    std::sort(sFileBrowserEntries.begin(), sFileBrowserEntries.end(), alphaComp);
}

void EditorOpenFileBrowser(FileBrowserCallbackFP callback, bool folderMode)
{
    if (!sFileBrowserOpen)
    {
        sFileBrowserOpen = true;
        sFileBrowserFolderMode = folderMode;
        sFileBrowserCallback = callback;
        sFileBrowserPaths.clear();

        if (sFileBrowserCurDir == "")
        {
            if (GetEngineState()->mProjectDirectory != "")
            {
                sFileBrowserCurDir = GetEngineState()->mProjectDirectory;
            }
            else
            {
                sFileBrowserCurDir = "./";
            }
        }

        sFileBrowserCurDir = SYS_GetAbsolutePath(sFileBrowserCurDir);

        PopulateFileBrowserDirs();

        if (sFileBrowserEntries.size() == 0)
        {
            LogWarning("No directory entries found. Reseting to working dir.");
            sFileBrowserCurDir = "./";
            PopulateFileBrowserDirs();

            if (sFileBrowserEntries.size() == 0)
            {
                LogError("Still couldn't find directory entries...");
            }
        }
    }
    else
    {
        LogWarning("Failed to open file browser. It is already open.");
    }
}

void EditorSetFileBrowserDir(const std::string& dir)
{
    sFileBrowserCurDir = dir;
}

void EditorShowUnsavedAssetsModal(const std::vector<AssetStub*>& unsavedStubs)
{
    if (!sUnsavedModalActive)
    {
        sUnsavedModalActive = true;
        sUnsavedAssets = unsavedStubs;
        sUnsavedAssetsSelected.clear();
        sUnsavedAssetsSelected.resize(sUnsavedAssets.size());
    }
}

static void DrawFileBrowserContextPopup(FileBrowserDirEntry* entry)
{
    // New Folder
    // Rename 
    // Delete

    bool closeContextPopup = false;
    bool setTextInputFocus = false;
    bool isParentDir = (entry && entry->mName == "..");

    if (entry && !isParentDir && ImGui::Selectable("Rename", false, ImGuiSelectableFlags_DontClosePopups))
    {
        ImGui::OpenPopup("Rename File");
        strncpy(sPopupInputBuffer, entry->mName.c_str(), kPopupInputBufferSize - 1);
        sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
        setTextInputFocus = true;
    }
    if (ImGui::Selectable("New Folder", false, ImGuiSelectableFlags_DontClosePopups))
    {
        ImGui::OpenPopup("New Folder");
        sPopupInputBuffer[0] = '\0';
        setTextInputFocus = true;
    }
    if (entry && !isParentDir && ImGui::Selectable("Delete"))
    {
        if (entry->mFolder)
        {
            std::string dirPath = sFileBrowserCurDir + entry->mName;
            SYS_RemoveDirectory(dirPath.c_str());
            LogDebug("Deleted directory: %s", dirPath.c_str());
        }
        else
        {
            std::string filePath = sFileBrowserCurDir + entry->mName;
            SYS_RemoveFile(filePath.c_str());
            LogDebug("Deleted file: %s", filePath.c_str());
        }

        sFileBrowserNeedsRefresh = true;
    }

    if (ImGui::BeginPopup("Rename File"))
    {
        if (setTextInputFocus)
            ImGui::SetKeyboardFocusHere();

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            // Rename!
            std::string newName = sPopupInputBuffer;
            std::string oldFilePath = entry->mDirPath + entry->mName;
            std::string newFilePath = entry->mDirPath + newName;
            SYS_Rename(oldFilePath.c_str(), newFilePath.c_str());
            sFileBrowserNeedsRefresh = true;

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("New Folder"))
    {
        if (setTextInputFocus)
            ImGui::SetKeyboardFocusHere();

        if (ImGui::InputText("Folder Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            const std::string folderName = sPopupInputBuffer;

            if (folderName != "")
            {
                if (!SYS_CreateDirectory((sFileBrowserCurDir + folderName).c_str()))
                {
                    LogError("Failed to create folder");
                }
            }

            sFileBrowserNeedsRefresh = true;

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (closeContextPopup)
    {
        ImGui::CloseCurrentPopup();
    }
}

static void DrawFileBrowser()
{
    bool contextPopupOpen = false;

    if (sFileBrowserNeedsRefresh)
    {
        PopulateFileBrowserDirs();
        sFileBrowserNeedsRefresh = false;
    }

    sFileBrowserDoubleClickBlock -= GetEngineState()->mRealDeltaTime;
    sFileBrowserDoubleClickBlock = glm::max(sFileBrowserDoubleClickBlock, 0.0f);

    if (sFileBrowserOpen)
    {
        ImGui::OpenPopup("File Browser");
    }

    // Center the modal
    if (ImGui::IsPopupOpen("File Browser"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(465, 490));
    }

    if (ImGui::BeginPopupModal("File Browser", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        bool confirmOpen = false;

        // Text that shows current directory.
        ImGui::Text(sFileBrowserCurDir.c_str());

        bool dirFaved = GetEditorState()->IsDirFavorited(sFileBrowserCurDir);
        if (dirFaved)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, kToggledColor);
        }

        if (ImGui::Button("F", ImVec2(20, 20)))
        {
            if (dirFaved)
                GetEditorState()->RemoveFavoriteDir(sFileBrowserCurDir);
            else
                GetEditorState()->AddFavoriteDir(sFileBrowserCurDir);
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            ImGui::SetTooltip("Favorite");

        if (dirFaved)
        {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        std::string curDirShortName = GetDirShortName(sFileBrowserCurDir);

        if (ImGui::BeginCombo("Favorites", dirFaved ? curDirShortName.c_str() : ""))
        {
            const std::vector<std::string>& favedDirs = GetEditorState()->mFavoritedDirs;
            for (uint32_t i = 0; i < favedDirs.size(); ++i)
            {
                std::string dispName = GetDirShortName(favedDirs[i]);
                dispName += " --- " + favedDirs[i];

                if (ImGui::Selectable(dispName.c_str()))
                {
                    sFileBrowserCurDir = favedDirs[i];
                    sFileBrowserNeedsRefresh = true;
                }
            }

            ImGui::EndCombo();
        }

        // List of selectables that let you navigate/open/delete.
        {
            ImGuiWindowFlags childFlags = ImGuiWindowFlags_None;
            ImGui::BeginChild("File List", ImVec2(450, 350), true, childFlags);

            bool changedDir = false;

            // Show folders
            for (uint32_t i = 0; i < sFileBrowserEntries.size(); ++i)
            {
                if (sFileBrowserEntries[i].mFolder &&
                    sFileBrowserEntries[i].mName != ".")
                {
                    if (ImGui::Selectable(sFileBrowserEntries[i].mName.c_str(), true))
                    {
                        // If a folder is selected, then we need to switch to that directory,
                        // and also set the sFileBrowserPath if in folder mode.
                        sFileBrowserCurDir = SYS_GetAbsolutePath(sFileBrowserCurDir + sFileBrowserEntries[i].mName + "/");
                        changedDir = true;

                        if (sFileBrowserFolderMode)
                        {
                            sFileBrowserPaths.clear();
                            sFileBrowserPaths.push_back(sFileBrowserCurDir);
                        }
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                        contextPopupOpen = true;
                        DrawFileBrowserContextPopup(&sFileBrowserEntries[i]);
                        ImGui::EndPopup();
                    }
                }
            }

            // Show files
            bool doubleClicked = ImGui::IsMouseDoubleClicked(0) && sFileBrowserDoubleClickBlock <= 0.0f;

            for (uint32_t i = 0; i < sFileBrowserEntries.size(); ++i)
            {
                if (!sFileBrowserEntries[i].mFolder)
                {
                    bool selected = sFileBrowserEntries[i].mSelected;

                    if (selected)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Header, kToggledColor);
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kToggledColor);
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, kToggledColor);
                    }

                    if (ImGui::Selectable(sFileBrowserEntries[i].mName.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (!sFileBrowserFolderMode)
                        {
                            if (IsControlDown())
                            {
                                if (selected)
                                {
                                    sFileBrowserEntries[i].mSelected = false;
                                    sFileBrowserPaths.erase(std::find(sFileBrowserPaths.begin(), sFileBrowserPaths.end(), sFileBrowserCurDir + sFileBrowserEntries[i].mName));
                                }
                                else
                                {
                                    sFileBrowserEntries[i].mSelected = true;
                                    sFileBrowserPaths.push_back(sFileBrowserCurDir + sFileBrowserEntries[i].mName);
                                }
                            }
                            else
                            {
                                if (doubleClicked)
                                {
                                    confirmOpen = true;
                                }

                                sFileBrowserPaths.clear();
                                sFileBrowserPaths.push_back(sFileBrowserCurDir + sFileBrowserEntries[i].mName);

                                for (uint32_t x = 0; x < sFileBrowserEntries.size(); ++x)
                                {
                                    sFileBrowserEntries[x].mSelected = false;
                                }

                                sFileBrowserEntries[i].mSelected = true;
                            }
                        }
                    }

                    if (selected)
                    {
                        ImGui::PopStyleColor(3);
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                        contextPopupOpen = true;
                        DrawFileBrowserContextPopup(&sFileBrowserEntries[i]);
                        ImGui::EndPopup();
                    }
                }
            }

            ImGui::EndChild();

            // If no popup is open and we aren't inputting text...
            if (!contextPopupOpen &&
                ImGui::IsItemHovered() &&
                !ImGui::GetIO().WantTextInput)
            {
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                {
                    ImGui::OpenPopup("Null Context");
                }
            }

            if (ImGui::BeginPopup("Null Context"))
            {
                contextPopupOpen = true;
                DrawFileBrowserContextPopup(nullptr);
                ImGui::EndPopup();
            }

            if (changedDir)
            {
                PopulateFileBrowserDirs();
            }
        }

        std::string fileFolderName = "";
        if (sFileBrowserPaths.size() > 0)
        {
            fileFolderName = sFileBrowserPaths.back();
        }

        if (sFileBrowserFolderMode && 
            fileFolderName.size() > 0 &&
            (fileFolderName.back() == '/' || fileFolderName.back() == '\\'))
        {
            fileFolderName.pop_back();
        }

        size_t lastSlashIdx = fileFolderName.find_last_of("/\\");
        if (lastSlashIdx != std::string::npos)
        {
            fileFolderName = fileFolderName.substr(lastSlashIdx + 1);
        }

        ImGui::SetNextItemWidth(400);
        if (ImGui::InputText(sFileBrowserFolderMode ? "Folder" : "File", &fileFolderName))
        {
            sFileBrowserPaths.clear();
            sFileBrowserPaths.push_back(sFileBrowserCurDir + fileFolderName);

            // Add the trailing slash if its a folder.
            if (sFileBrowserFolderMode &&
                sFileBrowserPaths[0].size() > 0 &&
                (sFileBrowserPaths[0].back() != '/' && sFileBrowserPaths[0].back() != '\\'))
            {
#if PLATFORM_WINDOWS
                sFileBrowserPaths[0].push_back('\\');
#else
                sFileBrowserPaths[0].push_back('/');
#endif
            }
        }

        {
            std::string curPath = "";
            if (sFileBrowserPaths.size() > 0)
            {
                curPath = sFileBrowserPaths[0];
            }
            ImGui::Text(curPath.c_str());
        }

        if (ImGui::Button("Open"))
        {
            confirmOpen = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            sFileBrowserOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (!contextPopupOpen && IsKeyJustDown(POLYPHASE_KEY_ENTER))
        {
            confirmOpen = true;
        }

        if (confirmOpen)
        {
            if (sFileBrowserCallback != nullptr)
            {
                sFileBrowserCallback(sFileBrowserPaths);
            }

            sFileBrowserOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void DrawUnsavedCheck()
{
    if (sUnsavedModalActive)
    {
        ImGui::OpenPopup("Unsaved Changes");
    }

    // Center the modal
    if (ImGui::IsPopupOpen("Unsaved Changes"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400,300));
    }

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        // Scrolling list
        if (ImGui::BeginListBox("##UnsavedAssetListBox", ImVec2(390, 200)))
        {
            for (uint32_t i = 0; i < sUnsavedAssets.size(); ++i)
            {
                if (sUnsavedAssets[i] == nullptr ||
                    sUnsavedAssets[i]->mAsset == nullptr)
                {
                    continue;
                }

                glm::vec4 assetColor = AssetManager::Get()->GetEditorAssetColor(sUnsavedAssets[i]->mType);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(assetColor.r, assetColor.g, assetColor.b, assetColor.a));

                if (ImGui::Selectable(sUnsavedAssets[i]->mAsset->GetName().c_str(), sUnsavedAssetsSelected[i]))
                {
                    sUnsavedAssetsSelected[i] = !sUnsavedAssetsSelected[i];
                }

                ImGui::PopStyleColor();
            }
            ImGui::EndListBox();
        }

        bool closePopup = false;

        if (ImGui::Button("Save All"))
        {
            for (uint32_t i = 0; i < sUnsavedAssets.size(); ++i)
            {
                if (sUnsavedAssets[i] != nullptr)
                {
                    AssetManager::Get()->SaveAsset(*sUnsavedAssets[i]);
                }
            }

            GetEditorState()->mShutdownUnsavedCheck = true;
            GetEngineState()->mQuit = true;
            closePopup = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Selected"))
        {
            for (uint32_t i = 0; i < sUnsavedAssets.size(); ++i)
            {
                if (sUnsavedAssets[i] != nullptr &&
                    sUnsavedAssetsSelected[i])
                {
                    AssetManager::Get()->SaveAsset(*sUnsavedAssets[i]);
                }
            }
            GetEditorState()->mShutdownUnsavedCheck = true;
            GetEngineState()->mQuit = true;
            closePopup = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            GetEditorState()->mShutdownUnsavedCheck = true;
            GetEngineState()->mQuit = true;
            closePopup = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            GetEditorState()->mShutdownUnsavedCheck = false;
            GetEngineState()->mQuit = false;
            closePopup = true;
        }

        if (IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
        {
            GetEditorState()->mShutdownUnsavedCheck = false;
            GetEngineState()->mQuit = false;
            closePopup = true;
        }

        if (closePopup)
        {
            sUnsavedModalActive = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void DrawProjectUpgradeModal()
{
    EditorState* editorState = GetEditorState();

    if (editorState->mShowProjectUpgradeModal)
    {
        ImGui::OpenPopup("Project Upgrade Required");
    }

    // Center the modal
    if (ImGui::IsPopupOpen("Project Upgrade Required"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 350));
    }

    if (ImGui::BeginPopupModal("Project Upgrade Required", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextWrapped("This project contains assets that were created with an older version of the engine.");
        ImGui::Spacing();
        ImGui::TextWrapped("The new version includes improved asset reference handling with UUID + name fallback, which prevents broken references in packaged builds.");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Assets needing upgrade: %d", (int)editorState->mAssetsNeedingUpgrade.size());
        ImGui::Spacing();

        // Show list of assets needing upgrade (scrollable)
        if (ImGui::BeginListBox("##UpgradeAssetList", ImVec2(480, 150)))
        {
            for (uint32_t i = 0; i < editorState->mAssetsNeedingUpgrade.size(); ++i)
            {
                AssetStub* stub = editorState->mAssetsNeedingUpgrade[i];
                if (stub == nullptr)
                    continue;

                glm::vec4 assetColor = AssetManager::Get()->GetEditorAssetColor(stub->mType);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(assetColor.r, assetColor.g, assetColor.b, assetColor.a));
                ImGui::Text("%s", stub->mName.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndListBox();
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Click 'Upgrade Now' to automatically resave all assets with the new format.");
        ImGui::Spacing();

        bool closePopup = false;

        if (ImGui::Button("Upgrade Now", ImVec2(120, 0)))
        {
            closePopup = true;
            ActionManager::Get()->UpgradeProject();
            LogDebug("Project upgrade completed successfully!");
        }

        ImGui::SameLine();
        if (ImGui::Button("Skip", ImVec2(120, 0)))
        {
            closePopup = true;
            editorState->mAssetsNeedingUpgrade.clear();
            LogWarning("Project upgrade skipped. Packaged builds may have issues with asset references.");
        }

        if (IsKeyJustDown(POLYPHASE_KEY_ESCAPE))
        {
            closePopup = true;
            editorState->mAssetsNeedingUpgrade.clear();
        }

        if (closePopup)
        {
            editorState->mShowProjectUpgradeModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void DiscoverNodeClasses()
{
    sNode3dNames.clear();
    sNodeWidgetNames.clear();

    const std::vector<Factory*>& nodeFactories = Node::GetFactoryList();
    for (uint32_t i = 0; i < nodeFactories.size(); ++i)
    {
        Node* node = Node::CreateInstance(nodeFactories[i]->GetType());
        if (node->As<Node3D>())
        {
            if (strcmp(node->GetClassName(), "Node3D") == 0)
            {
                sNode3dNames.insert(sNode3dNames.begin(), node->GetClassName());
            }
            else
            {
                sNode3dNames.push_back(nodeFactories[i]->GetClassName());
            }
        }
        else if (node->As<Widget>())
        {
			auto className = node->GetClassName();
			auto factoryClassName = nodeFactories[i]->GetClassName();
            if (strcmp(node->GetClassName(), "Widget") == 0)
            {
                sNodeWidgetNames.insert(sNodeWidgetNames.begin(), node->GetClassName());
            }
            else
            {
                sNodeWidgetNames.push_back(nodeFactories[i]->GetClassName());
            }
        }
        else if (strcmp(node->GetClassName(), "Node") != 0)
        {
            sNodeOtherNames.push_back(nodeFactories[i]->GetClassName());
        }

        delete node;
    }
}

static bool MoveAssetToDirectory(AssetStub* stub, AssetDir* newDir)
{
    if (stub == nullptr || newDir == nullptr || stub->mDirectory == newDir)
        return false;
    if (stub->mEngineAsset || newDir->mEngineDir || newDir->mAddonDir)
        return false;

    AssetDir* oldDir = stub->mDirectory;

    // Build new path
    std::string newPath = newDir->mPath + stub->mName + ".oct";

    // Move file on disk
    if (!SYS_Rename(stub->mPath.c_str(), newPath.c_str()))
        return false;

    // Update in-memory structures
    oldDir->RemoveAssetStub(stub);
    newDir->mAssetStubs.push_back(stub);
    stub->mDirectory = newDir;
    stub->mPath = newPath;

    return true;
}

static bool MoveDirectoryToDirectory(AssetDir* dir, AssetDir* newParent)
{
    if (dir == nullptr || newParent == nullptr)
        return false;
    if (dir->mEngineDir || dir->mAddonDir || newParent->mEngineDir || newParent->mAddonDir)
        return false;
    if (dir->mParentDir == newParent)
        return false;
    // Prevent moving a dir into itself or a descendant
    for (AssetDir* p = newParent; p != nullptr; p = p->mParentDir)
    {
        if (p == dir)
            return false;
    }

    std::string oldPath = dir->mPath;
    std::string newPath = newParent->mPath + dir->mName + "/";

    // Move on disk
    SYS_MoveDirectory(oldPath.c_str(), newPath.c_str());

    // Update tree: remove from old parent
    AssetDir* oldParent = dir->mParentDir;
    auto& oldChildren = oldParent->mChildDirs;
    oldChildren.erase(std::find(oldChildren.begin(), oldChildren.end(), dir));

    // Add to new parent
    newParent->mChildDirs.push_back(dir);
    dir->mParentDir = newParent;

    // Recursively update paths
    std::function<void(AssetDir*)> updatePaths = [&](AssetDir* d) {
        d->mPath = d->mParentDir->mPath + d->mName + "/";
        for (AssetStub* stub : d->mAssetStubs)
            stub->mPath = d->mPath + stub->mName + ".oct";
        for (AssetDir* child : d->mChildDirs)
            updatePaths(child);
    };
    updatePaths(dir);

    return true;
}

static const char* AnchorModeToCSS(AnchorMode mode)
{
    switch (mode)
    {
    case AnchorMode::TopLeft:                return "top-left";
    case AnchorMode::TopMid:                 return "top-center";
    case AnchorMode::TopRight:               return "top-right";
    case AnchorMode::MidLeft:                return "center-left";
    case AnchorMode::Mid:                    return "center";
    case AnchorMode::MidRight:               return "center-right";
    case AnchorMode::BottomLeft:             return "bottom-left";
    case AnchorMode::BottomMid:              return "bottom-center";
    case AnchorMode::BottomRight:            return "bottom-right";
    case AnchorMode::TopStretch:             return "top-stretch";
    case AnchorMode::MidHorizontalStretch:   return "center-h-stretch";
    case AnchorMode::BottomStretch:          return "bottom-stretch";
    case AnchorMode::LeftStretch:            return "left-stretch";
    case AnchorMode::MidVerticalStretch:     return "center-v-stretch";
    case AnchorMode::RightStretch:           return "right-stretch";
    case AnchorMode::FullStretch:            return "full-stretch";
    default:                                 return "top-left";
    }
}

static void ExportWidgetTreeToXML(Widget* widget, std::string& outXml, int indent = 0)
{
    if (widget == nullptr)
        return;

    const char* typeName = widget->GetTypeName();
    const char* element = UIWidgetTypeToElement(typeName);

    std::string pad(indent * 2, ' ');
    outXml += pad + "<" + element;

    // id attribute from widget name
    const std::string& name = widget->GetName();
    if (!name.empty())
    {
        outXml += " id=\"" + name + "\"";
    }

    // Build inline style
    std::string style;

    // Anchor mode (skip default top-left)
    AnchorMode anchor = widget->GetAnchorMode();
    if (anchor != AnchorMode::TopLeft)
    {
        style += "anchor: " + std::string(AnchorModeToCSS(anchor)) + "; ";
    }

    // Position
    glm::vec2 offset = widget->GetOffset();
    if (offset.x != 0.0f || offset.y != 0.0f)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "left: %.0fpx; top: %.0fpx; ", offset.x, offset.y);
        style += buf;
    }

    // Size
    glm::vec2 size = widget->GetSize();
    if (size.x != 0.0f || size.y != 0.0f)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "width: %.0fpx; height: %.0fpx; ", size.x, size.y);
        style += buf;
    }

    // Color (skip default white)
    glm::vec4 color = widget->GetColor();
    if (color.r != 1.0f || color.g != 1.0f || color.b != 1.0f || color.a != 1.0f)
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "color: rgba(%.0f, %.0f, %.0f, %.2f); ",
                 color.r * 255.0f, color.g * 255.0f, color.b * 255.0f, color.a);
        style += buf;
    }

    // Opacity (skip default 255)
    uint8_t opacity = widget->GetOpacity();
    if (opacity != 255)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "opacity: %.2f; ", opacity / 255.0f);
        style += buf;
    }

    // Visibility
    if (!widget->IsVisible())
    {
        style += "display: none; ";
    }

    // Text-specific: font-size
    Text* textWidget = widget->As<Text>();
    if (textWidget)
    {
        float textSize = textWidget->GetTextSize();
        if (textSize != 32.0f) // skip default
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "font-size: %.0fpx; ", textSize);
            style += buf;
        }
    }

    // Quad-specific: src attribute
    Quad* quadWidget = widget->As<Quad>();
    if (quadWidget && quadWidget->GetTexture())
    {
        outXml += " src=\"" + quadWidget->GetTexture()->GetName() + "\"";
    }

    // Trim trailing space from style
    if (!style.empty() && style.back() == ' ')
        style.pop_back();

    if (!style.empty())
    {
        outXml += " style=\"" + style + "\"";
    }

    // Check for text content and children
    std::string textContent;
    if (textWidget)
    {
        textContent = textWidget->GetText();
    }

    // Button: skip internal Quad/Text children (they're auto-created)
    bool isButton = (widget->As<Button>() != nullptr);
    uint32_t childCount = isButton ? 0 : widget->GetNumChildren();

    if (textContent.empty() && childCount == 0)
    {
        outXml += " />\n";
    }
    else
    {
        outXml += ">";

        if (!textContent.empty())
        {
            outXml += textContent;
        }

        if (childCount > 0)
        {
            outXml += "\n";
            for (uint32_t i = 0; i < widget->GetNumChildren(); ++i)
            {
                Widget* child = widget->GetChild(i)->As<Widget>();
                if (child)
                {
                    ExportWidgetTreeToXML(child, outXml, indent + 1);
                }
            }
            outXml += pad;
        }

        outXml += "</" + std::string(element) + ">\n";
    }
}

static void CreateNewAsset(TypeId assetType, const char* assetName, bool isSkybox = false, bool userProvidedName = false)
{
    AssetStub* stub = nullptr;
    AssetDir* currentDir = GetEditorState()->GetAssetDirectory();

    if (currentDir == nullptr)
        return;

    stub = EditorAddUniqueAsset(assetName, currentDir, assetType, true);

    if (isSkybox && stub != nullptr && stub->mAsset != nullptr)
    {
        MaterialLite* skyMat = stub->mAsset->As<MaterialLite>();
        if (skyMat != nullptr)
        {
            skyMat->SetShadingModel(ShadingModel::Unlit);
            skyMat->SetCullMode(CullMode::Front);
            skyMat->SetDepthTestDisabled(true);
            skyMat->SetSortPriority(-1000);
            skyMat->SetApplyFog(false);
            skyMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            AssetManager::Get()->SaveAsset(*stub);
        }
    }

    if (assetType == MaterialLite::GetStaticType())
    {
        Asset* selAsset = GetEditorState()->GetSelectedAsset();

        if (stub != nullptr &&
            stub->mAsset != nullptr &&
            selAsset != nullptr &&
            selAsset->GetType() == Texture::GetStaticType())
        {
            MaterialLite* matLite = stub->mAsset->As<MaterialLite>();
            Texture* texture = selAsset->As<Texture>();

            // Auto assign the selected texture to index 0
            matLite->SetTexture(0, texture);

            // Only auto-rename if user didn't provide a custom name
            if (!userProvidedName)
            {
                std::string newMatName = texture->GetName();

                if (newMatName.length() >= 2 && newMatName[0] == 'T' && newMatName[1] == '_')
                {
                    newMatName[0] = 'M';
                }
                else
                {
                    newMatName = std::string("M_") + newMatName;
                }

                AssetManager::Get()->RenameAsset(matLite, newMatName);
            }
        }
    }

    if (assetType == UIDocument::GetStaticType())
    {
        if (stub != nullptr && stub->mAsset != nullptr)
        {
            UIDocument* doc = stub->mAsset->As<UIDocument>();

            // Write the .xml source file alongside the .oct asset
            std::string xmlPath = currentDir->mPath + stub->mName + ".xml";
            FILE* xmlFile = fopen(xmlPath.c_str(), "w");
            if (xmlFile != nullptr)
            {
                fputs(doc->GetXmlSource().c_str(), xmlFile);
                fclose(xmlFile);
            }

            doc->Import(xmlPath, nullptr);
        }
    }

    if (stub != nullptr)
    {
        AssetManager::Get()->SaveAsset(*stub);
    }
}

static void CreateNewScene(const char* sceneName, int sceneType, bool createCamera, bool createSkybox = true)
{
    // Plugin-registered scene types (sceneType >= 2) call into addon function pointers
    // that aren't safe to invoke from non-editor contexts (e.g. the REST controller).
    // Keep that branch here in EditorImgui; defer the built-in 2D/3D path to ActionManager
    // so the REST POST /api/scene/new endpoint can share the same implementation.
    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
    if (hookMgr != nullptr && sceneType >= 2)
    {
        AssetDir* currentDir = GetEditorState()->GetAssetDirectory();
        if (currentDir == nullptr)
            return;

        const auto& sceneTypes = hookMgr->GetSceneTypes();
        int hookIndex = sceneType - 2;
        if (hookIndex >= 0 && hookIndex < (int)sceneTypes.size())
        {
            AssetStub* stub = EditorAddUniqueAsset(sceneName, currentDir, Scene::GetStaticType(), true);
            if (stub == nullptr)
                return;

            SharedPtr<Node3D> root = Node::Construct<Node3D>();
            root->SetName("Root");

            sceneTypes[hookIndex].mCreateFunc(sceneName, root.Get(), sceneTypes[hookIndex].mUserData);

            Scene* scene = (Scene*)stub->mAsset;
            scene->Capture(root.Get());
            AssetManager::Get()->SaveAsset(*stub);
            return;
        }
    }

    ActionManager::Get()->CreateNewScene(sceneName, sceneType, createCamera, createSkybox);
}

static void AssignAssetToProperty(Object* owner, PropertyOwnerType ownerType, Property& prop, uint32_t index, Asset* newAsset)
{
    if (newAsset != nullptr &&
        newAsset != prop.GetAsset())
    {
        TypeId newType = newAsset->GetType();
        TypeId propId = prop.mExtra ? (TypeId)prop.mExtra->GetInteger() : 0;

        bool matchingType = (prop.mExtra == nullptr || newType == TypeId(prop.mExtra->GetInteger()));

        // HACK: Handle derived class types.
        if (propId == Material::GetStaticType() &&
            (newType == MaterialBase::GetStaticType() ||
             newType == MaterialInstance::GetStaticType() ||
             newType == MaterialLite::GetStaticType()))
        {
            matchingType = true;
        }

        if (matchingType)
        {
            if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
            {
                ActionManager::Get()->EXE_EditProperty(owner, ownerType, prop.mName, index, newAsset);
            }
            else
            {
                prop.SetAsset(newAsset, index);
            }
        }
    }
}

static bool NodeMatchesProperty(const Property& prop, Node* node, Object* owner)
{
    if (node == nullptr)
        return true;

    // Spline3D attachment filters
    if (owner && owner->Is("Spline3D"))
    {
        if (prop.mName == "Camera") return node->Is("Camera3D");
        if (prop.mName == "Static Mesh") return node->Is("StaticMesh3D");
        if (prop.mName == "Skeletal Mesh") return node->Is("SkeletalMesh3D");
        if (prop.mName == "Particle") return node->Is("Particle3D");
        if (prop.mName == "Point Light") return node->Is("PointLight3D");
        if (prop.mName == "Audio") return node->Is("Audio3D");
        if (prop.mName == "Point Speed Target") return node->GetName().rfind("point", 0) == 0;
    }

    if (prop.mExtra && prop.mExtra->GetType() == DatumType::String)
    {
        const std::string& typeName = prop.mExtra->GetString();
        if (!typeName.empty())
        {
            return node->Is(typeName.c_str());
        }
    }

    return true;
}

static void DrawNodeProperty(Property& prop, uint32_t index, Object* owner, PropertyOwnerType ownerType)
{
    Node* node = prop.GetNode(index).Get();
    ActionManager* am = ActionManager::Get();

    // Group the entire property row (target/info/picker button + InputText)
    // so we can attach a single drop target that covers the whole hit-box
    // rather than only the (small) InputText. Without this, hitting the drop
    // zone exactly was finicky and dropped payloads were silently missed.
    ImGui::BeginGroup();

    if (IsControlDown())
    {
        if (ImGui::Button(ICON_MATERIAL_SYMBOLS_TARGET) && node)
        {
            GetEditorState()->SetSelectedNode(node);
        }
    }
    else if (IsAltDown())
    {
        if (ImGui::Button(ICON_INFO) && node)
        {
            GetEditorState()->InspectObject(node);
        }
    }
    else
    {
        if (ImGui::Button(ICON_MDI_TARGET))
        {
            if (GetEditorState()->mNodePropertySelect)
            {
                GetEditorState()->ClearNodePropertySelect();
            }
            else
            {
                GetEditorState()->SetNodePropertySelect(true, index, prop.mName);
            }
        }

        if (node != nullptr &&
            ImGui::IsItemHovered() &&
            EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::Edit_DeleteSelected))
        {
            am->EXE_EditProperty(owner, ownerType, prop.mName, index, (Node*) nullptr);
        }
    }

    ImGui::SameLine();

    static std::string sTempString;
    static ImGuiID sActiveInputId = 0;

    Node* src = owner->As<Node>();

    if (!src && owner->As<Script>())
    {
        src = owner->As<Script>()->GetOwner();
    }

    // Only update the temp string when not actively editing this field
    // Otherwise user's input gets overwritten every frame
    ImGuiID inputId = ImGui::GetID("##NodeNameStr");
    if (sActiveInputId != inputId)
    {
        sTempString = FindRelativeNodePath(src, node);
    }

    ImGui::InputText("##NodeNameStr", &sTempString);

    if (ImGui::IsItemActive())
    {
        sActiveInputId = inputId;
    }
    else if (sActiveInputId == inputId)
    {
        sActiveInputId = 0;
    }

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        if (sTempString == "null" ||
            sTempString == "NULL" ||
            sTempString == "Null")
        {
            Node* nullNode = nullptr;
            if (ownerType != PropertyOwnerType::Count)
            {
                am->EXE_EditProperty(owner, ownerType, prop.mName, index, nullNode);
            }
        }
        else
        {
            Node* newNode = ResolveNodePath(src, sTempString);
            if (NodeMatchesProperty(prop, newNode, owner))
            {
                am->EXE_EditProperty(owner, ownerType, prop.mName, index, newNode);
            }
        }
    }

    // Per-element delete button for vector properties
    if (prop.IsVector() && prop.GetCount() > prop.mMinCount)
    {
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(ICON_RAPHAEL_NO "##NodeDelete"))
        {
            if (prop.IsExternal())
            {
                prop.EraseVector(index);
            }
            else
            {
                prop.Erase(index);
                if (prop.GetCount() > 0)
                {
                    prop.SetValue(prop.GetValue(0), 0, prop.GetCount());
                }
                else if (prop.mChangeHandler)
                {
                    prop.mChangeHandler(&prop, 0, nullptr);
                }
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove element [%d]", index);
    }

    ImGui::EndGroup();

    // Drop target across the entire property row (target/info button +
    // InputText + optional vector-delete button). Accepts a node payload
    // dragged from the scene tree (DRAGDROP_NODE) and assigns it to this
    // property slot.
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_NODE))
        {
            Node* droppedNode = (payload->Data != nullptr) ? *(Node**)payload->Data : nullptr;
            if (droppedNode != nullptr &&
                ownerType != PropertyOwnerType::Count &&
                NodeMatchesProperty(prop, droppedNode, owner))
            {
                am->EXE_EditProperty(owner, ownerType, prop.mName, index, droppedNode);
                sTempString = FindRelativeNodePath(src, droppedNode);
            }
        }
        ImGui::EndDragDropTarget();
    }
}

// A reusable autocomplete dropdown function that works with various input types
// Returns true if a selection was made, and updates the input string
template<typename FilterFuncType>
static bool DrawAutocompleteDropdown(const char* dropdownId,
                                     std::string& inputText,
                                     const std::vector<std::string>& suggestions,
                                     FilterFuncType filterFunc,
                                     bool forceActive = false,
                                     ImGuiID overrideInputId = 0,
                                     ImVec2 overrideRectMin = ImVec2(0, 0),
                                     ImVec2 overrideRectMax = ImVec2(0, 0))
{
    bool selectionMade = false;

    // Use static variables with the ID to handle multiple instances correctly
    static ImGuiID activeDropdownId = 0;
    static size_t selectedIndex = 0;
    static bool hasSelection = false;
    static bool dropdownActive = false;
    static std::string lastInputText = "";
    static std::vector<std::string> filteredItems;
    static bool selectionJustMade = false;

    // Use overridden input identity/rect if provided (when other widgets are drawn between InputText and this call)
    bool hasOverride = (overrideInputId != 0);
    ImGuiID inputId = hasOverride ? overrideInputId : ImGui::GetItemID();
    bool isInputActive = hasOverride ? forceActive : ImGui::IsItemActive();
    bool isInputFocused = hasOverride ? forceActive : ImGui::IsItemFocused();
    ImVec2 inputRectMin = (overrideRectMax.x > overrideRectMin.x) ? overrideRectMin : ImGui::GetItemRectMin();
    ImVec2 inputRectMax = (overrideRectMax.x > overrideRectMin.x) ? overrideRectMax : ImGui::GetItemRectMax();

    // If forceActive is true, force the dropdown to show
    // But not if a selection was just made (prevents immediate reopen)
    if (forceActive && !selectionJustMade) {
        dropdownActive = true;
    }
    // Reset the flag after one frame
    selectionJustMade = false;

    // If we have a new active item, reset state
    if (activeDropdownId != inputId && (isInputActive || isInputFocused))
    {
        activeDropdownId = inputId;
        hasSelection = false;
        dropdownActive = true; // Set to true to ensure dropdown shows
        selectedIndex = 0;
    }

    // Always update filtered items before bounds checks so we use the actual item count
    filteredItems.clear();
    for (const auto& suggestion : suggestions)
    {
        if (filterFunc(suggestion, inputText))
        {
            filteredItems.push_back(suggestion);
        }
    }

    // Reset selection state when filter text changes
    if (inputText != lastInputText)
    {
        lastInputText = inputText;
        selectedIndex = 0;
        hasSelection = false;
    }
    if (hasSelection && selectedIndex >= filteredItems.size() && !filteredItems.empty())
    {
        selectedIndex = filteredItems.size() - 1;
    }

    // Hide dropdown when input loses focus (but not if mouse is over dropdown)
    if (!isInputActive && !isInputFocused && activeDropdownId == inputId)
    {
        // Calculate dropdown bounds to check if mouse is hovering over it
        ImVec2 inputSize = ImVec2(inputRectMax.x - inputRectMin.x, inputRectMax.y - inputRectMin.y);
        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const size_t visibleItems = std::min(filteredItems.size(), (size_t)4);
        const float maxDropdownHeight = itemHeight * visibleItems + ImGui::GetStyle().WindowPadding.y * 2;
        ImVec2 dropdownMin = ImVec2(inputRectMin.x, inputRectMin.y + inputSize.y);
        ImVec2 dropdownMax = ImVec2(inputRectMax.x, inputRectMin.y + inputSize.y + maxDropdownHeight);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool mouseOverDropdown = mousePos.x >= dropdownMin.x && mousePos.x <= dropdownMax.x &&
                                 mousePos.y >= dropdownMin.y && mousePos.y <= dropdownMax.y;

        // Only close if mouse is not over dropdown (allows click selection)
        if (!mouseOverDropdown)
        {
            dropdownActive = false;
        }
    }

    // Close dropdown when user clicks outside BOTH the input field AND dropdown
    if (dropdownActive && activeDropdownId == inputId && ImGui::IsMouseClicked(0))
    {
        // Calculate dropdown bounds (same calculation as later in the function)
        ImVec2 inputSize = ImVec2(inputRectMax.x - inputRectMin.x, inputRectMax.y - inputRectMin.y);
        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const size_t visibleItems = std::min(filteredItems.size(), (size_t)4);
        const float maxDropdownHeight = itemHeight * visibleItems + ImGui::GetStyle().WindowPadding.y * 2;

        // Dropdown rect is below the input
        ImVec2 dropdownMin = ImVec2(inputRectMin.x, inputRectMin.y + inputSize.y);
        ImVec2 dropdownMax = ImVec2(inputRectMax.x, inputRectMin.y + inputSize.y + maxDropdownHeight);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool outsideInput = mousePos.x < inputRectMin.x || mousePos.x > inputRectMax.x ||
                            mousePos.y < inputRectMin.y || mousePos.y > inputRectMax.y;
        bool outsideDropdown = mousePos.x < dropdownMin.x || mousePos.x > dropdownMax.x ||
                               mousePos.y < dropdownMin.y || mousePos.y > dropdownMax.y;
        if (outsideInput && outsideDropdown)
        {
            dropdownActive = false;
        }
    }
    
    // Only show dropdown when we have filtered items and the dropdown is active
    // Either the input should be active/focused OR we're in the process of selecting an item
    if (!filteredItems.empty() && dropdownActive && (isInputActive || isInputFocused || activeDropdownId == inputId ))
    {
        activeDropdownId = inputId; // Keep track of which dropdown is active

        // Calculate popup position below the input field
        ImVec2 inputSize = ImVec2(inputRectMax.x - inputRectMin.x, inputRectMax.y - inputRectMin.y);
        ImGui::SetNextWindowPos(ImVec2(inputRectMin.x, inputRectMin.y + inputSize.y));

        // Set maximum height for 4 items plus a bit of padding
        const float itemHeight = ImGui::GetTextLineHeightWithSpacing();
        const float maxHeight = itemHeight * 4 + ImGui::GetStyle().WindowPadding.y * 2;
        ImGui::SetNextWindowSizeConstraints(ImVec2(inputSize.x, 0), ImVec2(inputSize.x, maxHeight));

        // Make sure the dropdown appears above other elements
        ImGui::SetNextWindowBgAlpha(1.0f); // Fully opaque background
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f); // Add a border for better visibility

        // Use flags to ensure the dropdown stays on top
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_Tooltip | // Use tooltip flag to ensure it draws on top
            ImGuiWindowFlags_NoScrollbar;
        
        if (ImGui::Begin(dropdownId, nullptr, flags))
        {
            // Get current key state
            bool upArrowPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow));
            bool downArrowPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow));
            bool tabPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab));
            bool enterPressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) ||
                                  ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
            bool escapePressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape));
            bool backspacePressed = ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace));
            
            // Handle keyboard navigation
            if (upArrowPressed)
            {
                if (!hasSelection && !filteredItems.empty())
                {
                    selectedIndex = filteredItems.size() - 1;
                    hasSelection = true;
                }
                else if (hasSelection)
                {
                    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : filteredItems.size() - 1;
                }
                
                // Consume the event
                ImGui::GetIO().KeysDown[ImGui::GetKeyIndex(ImGuiKey_UpArrow)] = false;
            }
            else if (downArrowPressed || tabPressed)
            {
                if (!hasSelection && !filteredItems.empty())
                {
                    selectedIndex = 0;
                    hasSelection = true;
                }
                else if (hasSelection)
                {
                    selectedIndex = (selectedIndex + 1) % filteredItems.size();
                }
                
                // Consume the events
                if (downArrowPressed)
                    ImGui::GetIO().KeysDown[ImGui::GetKeyIndex(ImGuiKey_DownArrow)] = false;
                if (tabPressed)
                    ImGui::GetIO().KeysDown[ImGui::GetKeyIndex(ImGuiKey_Tab)] = false;
            }
            else if (enterPressed)
            {
                // Always hide dropdown when Enter is pressed
                dropdownActive = false;

                if (hasSelection && selectedIndex < filteredItems.size())
                {
                    inputText = filteredItems[selectedIndex];
                    selectionMade = true;
                }
                else if (inputText.empty())
                {
                    // If Enter is pressed with empty input, set to null
                    inputText = "null";
                    selectionMade = true;
                }

                ImGui::CloseCurrentPopup();

                // Consume the event
                ImGui::GetIO().KeysDown[ImGui::GetKeyIndex(ImGuiKey_Enter)] = false;
            }
            else if (escapePressed)
            {
                dropdownActive = false;
                ImGui::CloseCurrentPopup();
                
                // Consume the event
                ImGui::GetIO().KeysDown[ImGui::GetKeyIndex(ImGuiKey_Escape)] = false;
            }

            if (inputText == "" && backspacePressed)
            {
                selectedIndex = -1;
            }
            
            // Display filtered items
            // Ensure the selected item is always visible by scrolling to it
            if (hasSelection && selectedIndex < filteredItems.size())
            {
                // Calculate where we need to scroll to make the selected item visible
                float itemHeight = ImGui::GetTextLineHeightWithSpacing();
                float targetY = itemHeight * selectedIndex;
                
                // ImGui will handle the scrolling if we set focus on the selected item
                // We'll also make sure to scroll into view if arrow keys are pressed
                if (upArrowPressed || downArrowPressed)
                {
                    ImGui::SetScrollY(targetY - itemHeight); // Position selected item one row from top
                }
            }

            // Display all filtered items in a scrollable area
            for (size_t i = 0; i < filteredItems.size(); i++)
            {
                bool isSelected = (hasSelection && i == selectedIndex);

                if (isSelected)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));

                ImGui::Selectable(filteredItems[i].c_str(), isSelected);

                // Use AllowWhenBlockedByActiveItem to detect hover even when
                // another widget (InputText) holds g.ActiveId
                bool itemHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

                // Mouse hover - update selection
                if (itemHovered)
                {
                    selectedIndex = i;
                    hasSelection = true;
                }

                // Mouse click - manual detection that bypasses Selectable's internal
                // hover check which fails when InputText holds ActiveId
                if (itemHovered && ImGui::IsMouseClicked(0))
                {
                    inputText = filteredItems[i];
                    selectionMade = true;
                    dropdownActive = false;
                    selectionJustMade = true;
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus(); // This also scrolls to make the item visible
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(); // Pop the window border style
    }
    else
    {

        if (filteredItems.empty() && isInputActive)
        {
            // Only hide the dropdown if there are no filtered items AND the input is active
            // This way we don't hide it when the user is trying to interact with it
            dropdownActive = false;
        }
    }

    return selectionMade;
}


void DrawAssetProperty(Property& prop, uint32_t index, Object* owner, PropertyOwnerType ownerType)
{
    Asset* asset = prop.GetAsset(index);
    ActionManager* am = ActionManager::Get();
 
    bool useAssetColor = (prop.mExtra != 0);
    if (useAssetColor)
    {
        glm::vec4 assetColor = AssetManager::Get()->GetEditorAssetColor(prop.mExtra ? (TypeId)prop.mExtra->GetInteger() : 0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(assetColor.r, assetColor.g, assetColor.b, assetColor.a));
    }

    if (IsControlDown())
    {
        if (ImGui::Button(ICON_IC_BASELINE_UPLOAD) && asset)
        {
            GetEditorState()->BrowseToAsset(asset->GetName());
        }
    }
    else if (IsAltDown())
    {
        if (ImGui::Button(ICON_INFO) && asset)
        {
            GetEditorState()->InspectObject(asset);
        }
    }
    else
    {
        if (ImGui::Button(ICON_IC_BASELINE_DOWNLOAD))
        {
            Asset* selAsset = GetEditorState()->GetSelectedAsset();
            if (selAsset != nullptr)
            {
                AssignAssetToProperty(owner, ownerType, prop, index, selAsset);
            }
        }

        if (asset != nullptr &&
            ImGui::IsItemHovered() &&
            EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::Edit_DeleteSelected))
        {
            if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
            {
                am->EXE_EditProperty(owner, ownerType, prop.mName, index, (Asset*) nullptr);
            }
            else
            {
                prop.SetAsset(nullptr, index);
            }
        }
    }

    ImGui::SameLine();

    // Each asset property gets its own string buffer keyed by a unique ID
    static std::unordered_map<std::string, std::string> sTempStrings;
    std::string propKey = prop.mName + "_" + std::to_string(index) + "_" + std::to_string((uintptr_t)owner);
    std::string& sTempString = sTempStrings[propKey];

    // Create a unique ID for this input
    ImGui::PushID((prop.mName + std::to_string(index)).c_str());

    // Use ImGui::InputText and get the active status for the dropdown
    // Capture keys to prevent input field from intercepting arrow keys
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    if (ImGui::IsItemFocused() || ImGui::IsItemActive()) {
        // If dropdown is about to be shown, don't allow up/down keys to affect the input
        flags |= ImGuiInputTextFlags_CharsNoBlank;
    }

    bool textActive = ImGui::InputText("##AssetNameStr", &sTempString, flags);
    bool isInputFocused = ImGui::IsItemFocused();
    bool isInputActivated = ImGui::IsItemActivated();

    // Save InputText rect/ID for the autocomplete dropdown positioning
    ImGuiID inputTextId = ImGui::GetItemID();
    ImVec2 inputTextRectMin = ImGui::GetItemRectMin();
    ImVec2 inputTextRectMax = ImGui::GetItemRectMax();

    // Calculate dropdown bounds to check if mouse is hovering over it
    ImVec2 inputSize = ImVec2(inputTextRectMax.x - inputTextRectMin.x, inputTextRectMax.y - inputTextRectMin.y);
    const float dropdownItemHeight = ImGui::GetTextLineHeightWithSpacing();
    const float maxDropdownHeight = dropdownItemHeight * 4 + ImGui::GetStyle().WindowPadding.y * 2;
    ImVec2 dropdownMin = ImVec2(inputTextRectMin.x, inputTextRectMin.y + inputSize.y);
    ImVec2 dropdownMax = ImVec2(inputTextRectMax.x, inputTextRectMin.y + inputSize.y + maxDropdownHeight);

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseOverDropdown = mousePos.x >= dropdownMin.x && mousePos.x <= dropdownMax.x &&
                             mousePos.y >= dropdownMin.y && mousePos.y <= dropdownMax.y;

    // Only reset temp string when NOT focused and NOT hovering over dropdown
    // This preserves the filter text when clicking on dropdown items
    if (isInputActivated)
    {
        // Reset when first activating the input
        sTempString = asset ? asset->GetName() : "";
    }
    else if (!isInputFocused && !mouseOverDropdown)
    {
        // Reset when not interacting
        sTempString = asset ? asset->GetName() : "";
    }

    // Drag-and-drop target on the input text field
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
        {
            AssetStub* droppedStub = *(AssetStub**)payload->Data;
            if (droppedStub != nullptr)
            {
                Asset* droppedAsset = LoadAsset(droppedStub->mName);
                AssignAssetToProperty(owner, ownerType, prop, index, droppedAsset);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Clear button
    ImGui::SameLine(0.0f, 2.0f);
    if (ImGui::Button(ICON_DASHICONS_NO_ALT "##AssetClear"))
    {
        Asset* nullAsset = nullptr;
        if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
        {
            am->EXE_EditProperty(owner, ownerType, prop.mName, index, nullAsset);
        }
        else
        {
            prop.SetAsset(nullAsset, index);
        }
    }

    // Inspect and Reveal buttons - only show when asset is assigned
    if (asset != nullptr)
    {
        // Inspect button - view asset properties
        ImGui::SameLine(0.0f, 2.0f);
        if (ImGui::Button(ICON_INFO "##Inspect"))
        {
            GetEditorState()->InspectObject(asset);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Inspect asset properties");

        // Reveal button - navigate to asset in Asset Panel
        ImGui::SameLine(0.0f, 2.0f);
        if (ImGui::Button(ICON_MDI_TARGET "##Reveal"))
        {
            GetEditorState()->BrowseToAsset(asset->GetName());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reveal in Asset Panel");
    }

    // Per-element delete button for vector properties
    if (prop.IsVector() && prop.GetCount() > prop.mMinCount)
    {
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(ICON_RAPHAEL_NO "##AssetDelete"))
        {
            if (prop.IsExternal())
            {
                prop.EraseVector(index);
            }
            else
            {
                prop.Erase(index);
                if (prop.GetCount() > 0)
                {
                    prop.SetValue(prop.GetValue(0), 0, prop.GetCount());
                }
                else if (prop.mChangeHandler)
                {
                    prop.mChangeHandler(&prop, 0, nullptr);
                }
            }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove element [%d]", index);
    }

    // If we have an extra property with type information, use it to filter assets
    TypeId assetTypeFilter = 0;
    if (prop.mExtra)
    {
        assetTypeFilter = (TypeId)prop.mExtra->GetInteger();
    }

    // Get asset suggestions based on type
    static std::vector<std::string> assetSuggestions;
    static TypeId lastAssetTypeFilter = 0;
    static double lastUpdateTime = 0.0;
    double currentTime = ImGui::GetTime();

    // Only refresh the suggestions list occasionally to avoid performance issues
    if (assetTypeFilter != lastAssetTypeFilter || currentTime - lastUpdateTime > 2.0)
    {
        assetSuggestions.clear();
        const auto& assetMap = AssetManager::Get()->GetAssetMap();
        
        for (const auto& pair : assetMap)
        {
            AssetStub* stub = pair.second;
            // If assetTypeFilter is 0 or matches the asset type, add it to suggestions
            bool typeMatches = false;
            
            if (assetTypeFilter == 0)
            {
                typeMatches = true; // Accept all types when no filter
            }
            else if (stub)
            {
                // Check for exact type match
                if (stub->mType == assetTypeFilter)
                {
                    typeMatches = true;
                }
                // Special handling for Material types
                else if (assetTypeFilter == Material::GetStaticType() && 
                         (stub->mType == MaterialBase::GetStaticType() ||
                          stub->mType == MaterialInstance::GetStaticType() ||
                          stub->mType == MaterialLite::GetStaticType()))
                {
                    typeMatches = true;
                }
            }
            
            if (stub && typeMatches)
            {
                assetSuggestions.push_back(stub->mName);
            }
        }
        
        lastAssetTypeFilter = assetTypeFilter;
        lastUpdateTime = currentTime;
    }

    // Define a filter function for our assets
    auto assetFilter = [](const std::string& suggestion, const std::string& input) {
        if (input.empty()) return true;
        // Case-insensitive substring search
        std::string lowerSuggestion = suggestion;
        std::string lowerInput = input;
        std::transform(lowerSuggestion.begin(), lowerSuggestion.end(), lowerSuggestion.begin(), ::tolower);
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        return lowerSuggestion.find(lowerInput) != std::string::npos;
    };

    // Draw the autocomplete dropdown for asset selection
    // Force the dropdown to be active when the input is activated or text changes
    // Use saved InputText rect so the dropdown positions below the input field, not the clear button
    bool selectionMade = DrawAutocompleteDropdown("AssetAutocomplete", sTempString, assetSuggestions, assetFilter,
                                                isInputActivated || textActive || isInputFocused,
                                                inputTextId, inputTextRectMin, inputTextRectMax);
    
    // Pop the ID we pushed earlier
    ImGui::PopID();

    // If a selection was made or the input was deactivated after editing, apply the change
    if (selectionMade || ImGui::IsItemDeactivatedAfterEdit())
    {
        if (sTempString == "null" ||
            sTempString == "NULL" ||
            sTempString == "Null")
        {
            Asset* nullAsset = nullptr;
            if (ownerType != PropertyOwnerType::Count)
            {
                am->EXE_EditProperty(owner, ownerType, prop.mName, index, nullAsset);
            }
            else
            {
                prop.SetAsset(nullAsset, index);
            }
        }
        else
        {
            Asset* newAsset = LoadAsset(sTempString);
            AssignAssetToProperty(owner, ownerType, prop, index, newAsset);
        }
    }

    if (useAssetColor)
    {
        ImGui::PopStyleColor();
    }
}

static bool HandleScriptSelection( std::string sTempString,
 std::string sOrigVal) {
    // Get available script files
    static std::vector<std::string> scriptSuggestions;
    static double lastUpdateTime = 0.0;
    double currentTime = ImGui::GetTime();



    // Case-insensitive filter
    auto scriptFilter = [](const std::string& suggestion, const std::string& input) {
        if (input.empty()) return true;
        std::string lowerSuggestion = suggestion, lowerInput = input;
        std::transform(lowerSuggestion.begin(), lowerSuggestion.end(), lowerSuggestion.begin(), ::tolower);
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        return lowerSuggestion.find(lowerInput) != std::string::npos;
        };

    // Draw the input field first
    bool textActive = ImGui::InputText("##ScriptInput", &sTempString);

    // Capture input state IMMEDIATELY after InputText
    bool isInputFocused = ImGui::IsItemFocused();
    bool isInputActivated = ImGui::IsItemActivated();

    // Then draw the autocomplete dropdown with the correct state
    bool selectionMade = DrawAutocompleteDropdown("ScriptAutocomplete", sTempString, scriptSuggestions, scriptFilter,
        isInputActivated || textActive || isInputFocused);

    // Capture activation for sOrigVal AFTER the dropdown
    if (isInputActivated)
    {
        // Refresh script list every 2 seconds
        if (scriptSuggestions.empty() || currentTime - lastUpdateTime > 2.0)
        {
            scriptSuggestions = AssetManager::Get()->GetAvailableScriptFiles();
            lastUpdateTime = currentTime;
        }
        sOrigVal = sTempString;
    }
    return selectionMade;
}

static void DrawPropertyList(Object* owner, std::vector<Property>& props)
{
    ActionManager* am = ActionManager::Get();
    const float kIndentWidth = 0.0f;
    const float kCategoryIndentWidth = 8.0f;
    const bool ctrlDown = IsControlDown();
    const bool altDown = IsAltDown();
    const bool shiftDown = IsShiftDown();

    const char* curCategory = "";
    bool catOpen = true;
    bool catIndented = false;

    PropertyOwnerType ownerType = PropertyOwnerType::Global;
    if (owner != nullptr)
    {
        if (owner->As<Node>())
        {
            ownerType = PropertyOwnerType::Node;
        }
        else if (owner->As<Asset>())
        {
            ownerType = PropertyOwnerType::Asset;
        }
    }

    for (uint32_t p = 0; p < props.size(); ++p)
    {
        Property& prop = props[p];
        DatumType propType = prop.GetType();

        if (strcmp(curCategory, prop.mCategory) != 0)
        {
            if (catIndented)
            {
                ImGui::Unindent(kCategoryIndentWidth);
                catIndented = false;
            }

            curCategory = prop.mCategory;

            if (strcmp(prop.mCategory, "") != 0)
            {
                catOpen = ImGui::CollapsingHeader(curCategory, ImGuiTreeNodeFlags_DefaultOpen);

                if (catOpen)
                {
                    ImGui::Indent(kCategoryIndentWidth);
                    catIndented = true;
                }
            }
        }

        if (!catOpen)
            continue;

        ImGui::PushID(p);

        // Allow custom property drawing
        bool custom = owner ? owner->DrawCustomProperty(prop) : false;

        // Check addon property drawers (Batch 6)
        if (!custom)
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                // Match by property name - addon registers for specific property names
                custom = hookMgr->DrawPropertyDrawer(
                    prop.mName.c_str(), prop.mName.c_str(),
                    owner, (int32_t)propType);
            }
        }

        if (custom)
        {
            ImGui::PopID();
            continue;
        }

        // Function properties render as editor-only buttons (no data, no label).
        if (propType == DatumType::Function)
        {
            const char* displayText = prop.mDisplayName.empty() ? prop.mName.c_str() : prop.mDisplayName.c_str();
            if (ImGui::Button(displayText))
            {
                Script* script = static_cast<Script*>(prop.mOwner);
                if (script != nullptr && script->IsActive())
                {
                    script->CallFunction(prop.mName.c_str());
                }
            }
            ImGui::PopID();
            continue;
        }

        // Bools handle name on same line after checkbox
        if (propType != DatumType::Bool || prop.GetCount() > 1)
        {
            const char* displayText = prop.mDisplayName.empty() ? prop.mName.c_str() : prop.mDisplayName.c_str();
            ImGui::Text(displayText);

            if (kIndentWidth > 0.0f)
            {
                ImGui::Indent(kIndentWidth);
            }

            if (prop.IsVector())
            {
                ImGui::SameLine();
                if (ImGui::Button("+"))
                {
                    if (prop.IsExternal())
                    {
                        prop.PushBackVector();
                    }
                    else
                    {
                        // Script properties use internal storage
                        prop.SetCount(prop.GetCount() + 1);

                        // HACK: Re-assign values so that we trigger the script prop change handler to propagate changes to script.
                        prop.SetValue(prop.GetValue(0), 0, prop.GetCount());
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("-"))
                {
                    if (prop.GetCount() > 0)
                    {
                        if (prop.IsExternal())
                        {
                            prop.EraseVector(prop.GetCount() - 1);
                        }
                        else
                        {
                            // Script properties use internal storage
                            prop.Erase(prop.GetCount() - 1);

                            // We need to trigger the script prop change handler to propagate changes to script.
                            uint32_t currentCount = prop.GetCount();
                            if (currentCount > 0)
                            {
                                // For non-empty arrays, use the normal approach
                                prop.SetValue(prop.GetValue(0), 0, currentCount);
                            }
                            else
                            {
                                // For empty arrays, we need to ensure the change propagates to the script property
                                // Call the change handler directly to sync the empty state
                                if (prop.mChangeHandler)
                                {
                                    // The change handler expects the property to already be modified, so call it
                                    // with a null pointer to indicate this is a count change, not a value change
                                    prop.mChangeHandler(&prop, 0, nullptr);
                                }
                            }
                        }
                    }
                }
            }
        }

        for (uint32_t i = 0; i < prop.GetCount(); ++i)
        {
            ImGui::PushID(i);

            // Visual separation between array elements (skip first)
            if (prop.IsVector() && i > 0)
            {
                ImGui::Separator();
            }

            // Index label for vector properties
            if (prop.IsVector())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("[%d]", i);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            switch (propType)
            {
            case DatumType::Integer:
            {
                static int32_t sOrigVal = 0;
                int32_t propVal = prop.GetInteger(i);
                int32_t preVal = propVal;

                if (prop.mEnumCount > 0)
                {
                    if (ImGui::Combo("", &propVal, prop.mEnumStrings, prop.mEnumCount))
                    {
                        if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                        {
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                        }
                        else
                        {
                            prop.SetInteger(propVal, i);
                        }
                    }
                }
                else
                {
                    ImGui::DragInt("", &propVal);

                    if (ImGui::IsItemActivated())
                    {
                        sOrigVal = preVal;
                    }

                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                        {
                            prop.SetInteger(sOrigVal, i);
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                        }
                        else
                        {
                            prop.SetInteger(propVal, i);
                        }
                    }
                    else if (propVal != preVal)
                    {
                        prop.SetInteger(propVal, i);
                    }
                }
                break;
            }
            case DatumType::Float:
            {
                static float sOrigVal = 0.0f;
                float propVal = prop.GetFloat(i);
                float preVal = propVal;

                ImGui::DragFloat("", &propVal);

                if (ImGui::IsItemActivated())
                {
                    sOrigVal = preVal;
                }

                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                    {
                        prop.SetFloat(sOrigVal, i);
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                    }
                    else
                    {
                        prop.SetFloat(propVal, i);
                    }
                }
                else if (propVal != preVal)
                {
                    prop.SetFloat(propVal, i);
                }
                break;
            }
            case DatumType::Bool:
            {
                if (prop.mName == "Restart Script")
                {
                    if (ImGui::Button(ICON_IC_TWOTONE_POWER_SETTINGS_NEW "##RestartScript"))
                    {
                        bool val = true;
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, val);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Restart Script");

                    // Draw Reload button on the same line
                    ImGui::SameLine(0.0f, 2.0f);
                    if (ImGui::Button(ICON_IC_SHARP_SETTINGS_BACKUP_RESTORE "##ReloadScriptFile"))
                    {
                        bool val = true;
                        std::string reloadName = "Reload Script File";
                        am->EXE_EditProperty(owner, ownerType, reloadName, 0, val);
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Reload Script File");
                    break;
                }
                else if (prop.mName == "Reload Script File")
                {
                    // Already drawn inline with "Restart Script" above
                    break;
                }

                bool propVal = prop.GetBool(i);
                if (Polyphase::Checkbox("", &propVal))
                {
                    if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                    {
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                    }
                    else
                    {
                        prop.SetBool(propVal, i);
                    }
                }

                ImGui::SameLine();
                const char* displayText = prop.mDisplayName.empty() ? prop.mName.c_str() : prop.mDisplayName.c_str();
                ImGui::Text(displayText);
                break;
            }
            case DatumType::String:
            {
                static std::string sTempString;
                static std::string sOrigVal;
                sTempString = prop.GetString(i);

                if (prop.mName == "Script")
                {
                    // Get available script files
                    // Then draw the autocomplete dropdown with the correct state
                  /*  bool selectionMade = HandleScriptSelection( sTempString,
                        sOrigVal);*/

                    static std::vector<std::string> scriptSuggestions;
                    static double lastUpdateTime = 0.0;
                    double currentTime = ImGui::GetTime();



                    // Case-insensitive filter
                    auto scriptFilter = [](const std::string& suggestion, const std::string& input) {
                        if (input.empty()) return true;
                        std::string lowerSuggestion = suggestion, lowerInput = input;
                        std::transform(lowerSuggestion.begin(), lowerSuggestion.end(), lowerSuggestion.begin(), ::tolower);
                        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
                        return lowerSuggestion.find(lowerInput) != std::string::npos;
                        };

                    // Draw the input field first
                    bool textActive = ImGui::InputText("##ScriptInput", &sTempString);

                    // Drop target for scripts dragged from Scripts panel
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_SCRIPT))
                        {
                            std::string scriptName((const char*)payload->Data, payload->DataSize - 1);
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, scriptName);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Capture input state IMMEDIATELY after InputText
                    bool isInputFocused = ImGui::IsItemFocused();
                    bool isInputActivated = ImGui::IsItemActivated();
                    ImGuiID scriptInputId = ImGui::GetItemID();
                    ImVec2 scriptInputMin = ImGui::GetItemRectMin();
                    ImVec2 scriptInputMax = ImGui::GetItemRectMax();

                    // Clear button
                    ImGui::SameLine(0.0f, 2.0f);
                    if (ImGui::Button(ICON_DASHICONS_NO_ALT "##ScriptClear"))
                    {
                        std::string emptyStr;
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, emptyStr);
                    }

                    // Reveal button - navigate to script in Scripts Panel
                    if (!sTempString.empty())
                    {
                        ImGui::SameLine(0.0f, 2.0f);
                        if (ImGui::Button(ICON_MDI_TARGET "##ScriptReveal"))
                        {
                            GetEditorState()->BrowseToScript(sTempString);
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Reveal in Scripts Panel");
                    }

                    // Then draw the autocomplete dropdown with the correct state
                    // Pass the InputText's saved rect so the dropdown positions below the input, not the clear button
                    bool selectionMade = DrawAutocompleteDropdown("ScriptAutocomplete", sTempString, scriptSuggestions, scriptFilter,
                        isInputActivated || textActive || isInputFocused, scriptInputId, scriptInputMin, scriptInputMax);

                    // Capture activation for sOrigVal AFTER the dropdown
                    if (isInputActivated)
                    {
                        // Refresh script list every 2 seconds
                        if (scriptSuggestions.empty() || currentTime - lastUpdateTime > 2.0)
                        {
                            scriptSuggestions = AssetManager::Get()->GetAvailableScriptFiles();
                            lastUpdateTime = currentTime;
                        }
                        sOrigVal = sTempString;
                    }

                    if (selectionMade || ImGui::IsItemDeactivatedAfterEdit())
                    {
                        if (sTempString != sOrigVal)
                        {
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, sTempString);
                        }
                    }
                }
                else
                {
                    bool isMultiline = (prop.mName == "Options");

                    if (isMultiline)
                    {
                        ImGui::InputTextMultiline("", &sTempString, ImVec2(-1, 80));
                    }
                    else
                    {
                        ImGui::InputText("", &sTempString);
                    }

                    if (ImGui::IsItemActivated())
                    {
                        sOrigVal = sTempString;
                    }

                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        if (sTempString != sOrigVal)
                        {
                            if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                            {
                                am->EXE_EditProperty(owner, ownerType, prop.mName, i, sTempString);
                            }
                            else
                            {
                                prop.SetString(sTempString, i);
                            }
                        }
                    }
                }
                break;
            }
            case DatumType::Vector2D:
            {
                static glm::vec2 sOrigVal = {};
                glm::vec2 propVal = prop.GetVector2D(i);
                glm::vec2 preVal = propVal;

                ImGui::DragFloat2("", &propVal[0], 1.0f, 0.0f, 0.0f, "%.2f");

                if (ImGui::IsItemActivated())
                {
                    sOrigVal = preVal;
                }

                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                    {
                        prop.SetVector2D(sOrigVal, i);
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                    }
                    else
                    {
                        prop.SetVector2D(propVal, i);
                    }
                }
                else if (propVal != preVal)
                {
                    prop.SetVector2D(propVal, i);
                }
                break;
            }
            case DatumType::Vector:
            {
                static glm::vec3 sOrigVal = {};
                glm::vec3 propVal = prop.GetVector(i);
                glm::vec3 preVal = propVal;

                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.85f);
                //ImGui::DragFloat3("", &propVal[0], 1.0f, 0.0f, 0.0f, "%.2f");
                float vMin = 0.0f;
                float vMax = 0.0f;
                ImGui::OctDragScalarN("", ImGuiDataType_Float, &propVal[0], 3, 1.0f, &vMin, &vMax, "%.2f", 0);
                ImGui::PopItemWidth();

                if (ImGui::IsItemActivated())
                {
                    sOrigVal = preVal;
                }

                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                    {
                        prop.SetVector(sOrigVal, i);

                        // Handle edge case where Rotation property is reset so we need to recompute transform to update mRotationEuler.
                        if (owner->As<Node3D>())
                        {
                            owner->As<Node3D>()->UpdateTransform(false);
                        }

                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                    }
                    else
                    {
                        prop.SetVector(propVal, i);
                    }
                }
                else if (propVal != preVal)
                {
                    prop.SetVector(propVal, i);
                }
                break;
            }
            case DatumType::Color:
            {
                static glm::vec4 sOrigVal = {};
                glm::vec4 propVal = prop.GetColor(i);
                glm::vec4 preVal = propVal;

                ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.85f);
                ImGui::OctColorEdit4("", &propVal[0], 0);
                ImGui::PopItemWidth();

                if (ImGui::IsItemActivated())
                {
                    sOrigVal = preVal;
                }

                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                    {
                        prop.SetColor(sOrigVal, i);
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, propVal);
                    }
                    else
                    {
                        prop.SetColor(propVal, i);
                    }
                }
                else if (propVal != preVal)
                {
                    prop.SetColor(propVal, i);
                }
                break;
            }
            case DatumType::Node:
            case DatumType::Node3D:
            case DatumType::Audio3D:
            case DatumType::Widget:
            case DatumType::Text:
            case DatumType::Quad:
            case DatumType::Spline3D:
            case DatumType::SpinBox:
            case DatumType::Window:
            case DatumType::DialogWindow:
            case DatumType::InputField:
            case DatumType::ProgressBar:
            case DatumType::CheckBox:
            case DatumType::ListViewWidget:
            case DatumType::ListViewItemWidget:
            case DatumType::DebugResourcesWidget:
            case DatumType::ArrayWidget:
            case DatumType::Button:
            case DatumType::Slider:
            case DatumType::LineEdit:
            case DatumType::Canvas:
            case DatumType::ComboBox:
            case DatumType::Voxel3D:
            case DatumType::Terrain3D:
            case DatumType::TileMap2D:
            case DatumType::NavMesh3D:
            case DatumType::Camera3D:
            case DatumType::DirectionalLight3D:
            case DatumType::Box3D:
            case DatumType::Particle3D:
            case DatumType::TimelinePlayer:
            case DatumType::NodeGraphPlayer:
            {
                DrawNodeProperty(prop, i, owner, ownerType);
                break;
            }
            case DatumType::Asset:
            case DatumType::Scene:
            case DatumType::Material:
            case DatumType::TileSet:
            case DatumType::TileMap:
            case DatumType::Timeline:
            case DatumType::NodeGraphAsset:
            {
                DrawAssetProperty(prop, i, owner, ownerType);
                break;
            }
            case DatumType::Byte:
            {
                static int32_t sOrigVal = 0;
                int32_t propVal = prop.GetByte(i);
                int32_t preVal = propVal;

                if (prop.mEnumCount > 0)
                {
                    if (ImGui::Combo("", &propVal, prop.mEnumStrings, prop.mEnumCount))
                    {
                        if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                        {
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, (uint8_t)propVal);
                        }
                        else
                        {
                            prop.SetByte((uint8_t)propVal, i);
                        }
                    }
                }
                else if (prop.mExtra &&
                    (prop.mExtra->GetInteger() == int32_t(ByteExtra::FlagWidget) ||
                    prop.mExtra->GetInteger() == int32_t(ByteExtra::ExclusiveFlagWidget))) // Should these be bitwise checks?
                {
                    ImVec2 itemSpacing = ImGui::GetStyle().ItemSpacing;
                    itemSpacing.x = 2.0f;
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);

                    for (uint32_t f = 0; f < 8; ++f)
                    {
                        if (f > 0)
                            ImGui::SameLine();

                        ImGui::PushID(f);

                        int32_t bit = 7 - int32_t(f);
                        bool bitSet = (propVal >> bit) & 1;

                        ImVec4* imColors = ImGui::GetStyle().Colors;

                        if (bitSet)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, kSelectedColor);
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, imColors[ImGuiCol_Button]);
                        }

                        if (ImGui::Button("", ImVec2(16.0f, 16.0f)))
                        {
                            bitSet = !bitSet;
                            uint8_t newBitMask = propVal;
                            if (bitSet)
                            {
                                newBitMask = newBitMask | (1 << bit);
                            }
                            else
                            {
                                newBitMask = newBitMask & (~(1 << bit));
                            }

                            propVal = newBitMask;

                            if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                            {
                                am->EXE_EditProperty(owner, ownerType, prop.mName, i, uint8_t(propVal));
                            }
                            else
                            {
                                prop.SetByte((uint8_t)propVal, i);
                            }
                        }

                        ImGui::PopStyleColor();

                        ImGui::PopID();
                    }

                    ImGui::PopStyleVar();
                }
                else
                {
                    ImGui::DragInt("", &propVal);

                    if (ImGui::IsItemActivated())
                    {
                        sOrigVal = preVal;
                    }

                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        if (ownerType == PropertyOwnerType::Node || ownerType == PropertyOwnerType::Asset)
                        {
                            prop.SetByte((uint8_t)sOrigVal, i);
                            am->EXE_EditProperty(owner, ownerType, prop.mName, i, uint8_t(propVal));
                        }
                        else
                        {
                            prop.SetByte((uint8_t)propVal, i);
                        }
                    }
                    else if (propVal != preVal)
                    {
                        prop.SetByte((uint8_t)propVal, i);
                    }
                }
                break;
            }
            case DatumType::Short:
            {
                static int32_t sOrigVal = 0;
                int32_t propVal = prop.GetShort(i);
                int32_t preVal = propVal;

                if (prop.mEnumCount > 0)
                {
                    if (ImGui::Combo("", &propVal, prop.mEnumStrings, prop.mEnumCount))
                    {
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, (int16_t)propVal);
                    }
                }
                else
                {
                    ImGui::DragInt("", &propVal);

                    if (ImGui::IsItemActivated())
                    {
                        sOrigVal = preVal;
                    }

                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        prop.SetShort((int16_t)sOrigVal, i);
                        am->EXE_EditProperty(owner, ownerType, prop.mName, i, int16_t(propVal));
                    }
                    else if (propVal != preVal)
                    {
                        prop.SetShort((int16_t)propVal, i);
                    }
                }
                break;
            }
            default: break;
            }

            ImGui::PopID();
        }

        if (propType != DatumType::Bool)
        {
            if (kIndentWidth > 0.0f)
            {
                ImGui::Unindent(kIndentWidth);
            }
        }

        AlternatingRowBackground();
        ImGui::PopID();
    }

    if (catIndented)
    {
        ImGui::Unindent(kCategoryIndentWidth);
    }
}

static void SpawnSceneAsNode(const std::string& sceneName, Node* parentNode)
{
    Scene* scene = LoadAsset<Scene>(sceneName);
    if (scene != nullptr)
    {
        Node* spawnedNode = ActionManager::Get()->EXE_SpawnNode(scene);
        if (spawnedNode)
        {
            Node* selNode = GetEditorState()->GetSelectedNode();
            Node* parent = parentNode ? parentNode : (selNode ? selNode : GetWorld(0)->GetRootNode());

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
    }
}

static void BuildSceneCategoryMap()
{
    sScenesByCategory.clear();

    auto& assetMap = AssetManager::Get()->GetAssetMap();
    for (auto it : assetMap)
    {
        if (it.second &&
            it.second->mType == Scene::GetStaticType() &&
            !it.second->mEngineAsset)
        {
            Scene* scene = LoadAsset<Scene>(it.second->mName);
            if (scene)
            {
                const std::string& category = scene->GetMenuOverride();
                std::string key = category.empty() ? "Scene" : category;
                sScenesByCategory[key].push_back(it.second->mName);
            }
        }
    }

    // Sort each category alphabetically
    for (auto& pair : sScenesByCategory)
    {
        std::sort(pair.second.begin(), pair.second.end());
    }
}

void DrawAddNodeMenu(Node* node)
{
    ActionManager* am = ActionManager::Get();

    if (!sNodesDiscovered)
    {
        DiscoverNodeClasses();
        sNodesDiscovered = true;
    }

    if (ImGui::MenuItem("Node"))
    {
        Node* newNode = am->EXE_SpawnNode(Node::GetStaticType());
        if (node)
        {
            node->AddChild(newNode);
        }
        else
        {
            GetWorld(0)->PlaceNewlySpawnedNode(ResolvePtr(newNode), {});
        }
        
        GetEditorState()->SetSelectedNode(newNode);
    }

    // Build scene category map if needed
    if (sScenesByCategory.empty())
    {
        BuildSceneCategoryMap();
    }

    if (ImGui::BeginMenu("3D"))
    {
        for (uint32_t i = 0; i < sNode3dNames.size(); ++i)
        {
            // Terrain3D gets special handling to create companion files
            if (sNode3dNames[i] == "Terrain3D" || sNode3dNames[i] == "Terrain")
            {
                if (ImGui::MenuItem(sNode3dNames[i].c_str()))
                {
                    Node* newNode = am->SpawnBasicNode(BASIC_TERRAIN, node, nullptr, false, {});
                    if (newNode)
                        GetEditorState()->SetSelectedNode(newNode);
                }
                continue;
            }

            if (ImGui::MenuItem(sNode3dNames[i].c_str()))
            {
                const char* nodeName = sNode3dNames[i].c_str();

                Node* newNode = am->EXE_SpawnNode(nodeName);

                if (node)
                    node->AddChild(newNode);
                else
                    GetWorld(0)->PlaceNewlySpawnedNode(ResolvePtr(newNode), {});

                GetEditorState()->SetSelectedNode(newNode);
            }
        }

        ImGui::Separator();
        if (ImGui::BeginMenu("Skybox"))
        {
            if (ImGui::MenuItem(BASIC_SKYBOX_TEXTURED))
                am->SpawnBasicNode(BASIC_SKYBOX_TEXTURED, node, nullptr, false, {});
            if (ImGui::MenuItem(BASIC_SKYBOX_VERTEX_COLOR))
                am->SpawnBasicNode(BASIC_SKYBOX_VERTEX_COLOR, node, nullptr, false, {});
            ImGui::EndMenu();
        }

        // Draw scenes with "3D" menu override
        if (sScenesByCategory.count("3D") > 0)
        {
            ImGui::Separator();
            for (const auto& sceneName : sScenesByCategory["3D"])
            {
                if (ImGui::MenuItem(sceneName.c_str()))
                {
                    SpawnSceneAsNode(sceneName, node);
                }
            }
        }

        // Draw addon node menu items for "3D" category
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->DrawNodeMenuItems("3D", node);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Widget"))
    {
        for (uint32_t i = 0; i < sNodeWidgetNames.size(); ++i)
        {
            if (ImGui::MenuItem(sNodeWidgetNames[i].c_str()))
            {
                const char* nodeName = sNodeWidgetNames[i].c_str();

                Node* newNode = am->EXE_SpawnNode(nodeName);

                if (node)
                    node->AddChild(newNode);
                else
                    GetWorld(0)->PlaceNewlySpawnedNode(ResolvePtr(newNode), {});

                GetEditorState()->SetSelectedNode(newNode);
            }
        }

        // Draw scenes with "Widget" menu override
        if (sScenesByCategory.count("Widget") > 0)
        {
            ImGui::Separator();
            for (const auto& sceneName : sScenesByCategory["Widget"])
            {
                if (ImGui::MenuItem(sceneName.c_str()))
                {
                    SpawnSceneAsNode(sceneName, node);
                }
            }
        }

        // Draw addon node menu items for "Widget" category
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->DrawNodeMenuItems("Widget", node);
        }

        ImGui::EndMenu();
    }

    // Default "Scene" menu - shows scenes with no menu override
    if (sScenesByCategory.count("Scene") > 0 && ImGui::BeginMenu("Scene"))
    {
        for (const auto& sceneName : sScenesByCategory["Scene"])
        {
            if (ImGui::MenuItem(sceneName.c_str()))
            {
                SpawnSceneAsNode(sceneName, node);
            }
        }
        ImGui::EndMenu();
    }

    if (sNodeOtherNames.size() > 0 &&
        ImGui::BeginMenu("Other"))
    {
        for (uint32_t i = 0; i < sNodeOtherNames.size(); ++i)
        {
            if (ImGui::MenuItem(sNodeOtherNames[i].c_str()))
            {
                const char* nodeName = sNodeOtherNames[i].c_str();
                Node* newNode = am->EXE_SpawnNode(nodeName);

                if (node)
                    node->AddChild(newNode);
                else
                    GetWorld(0)->PlaceNewlySpawnedNode(ResolvePtr(newNode), {});

                GetEditorState()->SetSelectedNode(newNode);
            }
        }

        // Draw addon node menu items for "Other" category
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->DrawNodeMenuItems("Other", node);
        }

        ImGui::EndMenu();
    }

    // Draw custom scene category menus (categories other than Scene, 3D, Widget, Other)
    // Supports nested paths like "ShootingGallery/Targets"
    {
        // Build a tree structure from category paths
        struct MenuNode
        {
            std::map<std::string, MenuNode> children;
            std::vector<std::string> scenes;
        };
        MenuNode rootMenu;

        for (const auto& pair : sScenesByCategory)
        {
            if (pair.first == "Scene" || pair.first == "3D" || pair.first == "Widget" || pair.first == "Other")
                continue;

            // Parse path segments (e.g., "ShootingGallery/Targets" -> ["ShootingGallery", "Targets"])
            std::vector<std::string> segments;
            std::string remaining = pair.first;
            size_t pos;
            while ((pos = remaining.find('/')) != std::string::npos)
            {
                std::string segment = remaining.substr(0, pos);
                if (!segment.empty())
                    segments.push_back(segment);
                remaining = remaining.substr(pos + 1);
            }
            if (!remaining.empty())
                segments.push_back(remaining);

            // Navigate/create the tree path
            MenuNode* current = &rootMenu;
            for (const auto& segment : segments)
            {
                current = &current->children[segment];
            }
            // Add scenes at this level
            for (const auto& sceneName : pair.second)
            {
                current->scenes.push_back(sceneName);
            }
        }

        // Recursive lambda to draw menu tree
        std::function<void(MenuNode&)> drawMenuTree = [&](MenuNode& menuNode)
        {
            // Draw child submenus first (sorted)
            for (auto& childPair : menuNode.children)
            {
                if (ImGui::BeginMenu(childPair.first.c_str()))
                {
                    drawMenuTree(childPair.second);
                    ImGui::EndMenu();
                }
            }
            // Draw scenes at this level
            for (const auto& sceneName : menuNode.scenes)
            {
                if (ImGui::MenuItem(sceneName.c_str()))
                {
                    SpawnSceneAsNode(sceneName, node);
                }
            }
        };

        // Draw top-level custom categories
        for (auto& topPair : rootMenu.children)
        {
            if (ImGui::BeginMenu(topPair.first.c_str()))
            {
                drawMenuTree(topPair.second);
                ImGui::EndMenu();
            }
        }
    }

    // Draw addon custom node categories
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->DrawCustomNodeCategories(node);
    }

    // Clear scene category cache at end so it refreshes next time menu opens
    sScenesByCategory.clear();
}

static void DrawImportMenu(Node* node)
{
    ActionManager* am = ActionManager::Get();
    if (ImGui::MenuItem("Camera"))
    {
        am->BeginImportCamera();
    }

    // Draw addon import menu items (Batch 8)
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->DrawImportMenuItems();
    }
}

void DrawSpawnBasic3dMenu(Node* node, bool setFocusPos)
{
    ActionManager* am = ActionManager::Get();
    glm::vec3 spawnPos = EditorGetFocusPosition();
    Asset* selAsset = GetEditorState()->GetSelectedAsset();

    if (ImGui::MenuItem(BASIC_NODE_3D))
        am->SpawnBasicNode(BASIC_NODE_3D, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_STATIC_MESH))
        am->SpawnBasicNode(BASIC_STATIC_MESH, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_SKELETAL_MESH))
        am->SpawnBasicNode(BASIC_SKELETAL_MESH, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_POINT_LIGHT))
        am->SpawnBasicNode(BASIC_POINT_LIGHT, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_DIRECTIONAL_LIGHT))
        am->SpawnBasicNode(BASIC_DIRECTIONAL_LIGHT, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_PARTICLE))
        am->SpawnBasicNode(BASIC_PARTICLE, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_AUDIO))
        am->SpawnBasicNode(BASIC_AUDIO, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_BOX))
        am->SpawnBasicNode(BASIC_BOX, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_SPHERE))
        am->SpawnBasicNode(BASIC_SPHERE, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_CAPSULE))
        am->SpawnBasicNode(BASIC_CAPSULE, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_SCENE))
        am->SpawnBasicNode(BASIC_SCENE, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_CAMERA))
        am->SpawnBasicNode(BASIC_CAMERA, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_TEXT_MESH))
        am->SpawnBasicNode(BASIC_TEXT_MESH, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_INSTANCED_MESH))
        am->SpawnBasicNode(BASIC_INSTANCED_MESH, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_TERRAIN))
        am->SpawnBasicNode(BASIC_TERRAIN, node, selAsset, setFocusPos, spawnPos);

    if (ImGui::BeginMenu("Skybox"))
    {
        if (ImGui::MenuItem(BASIC_SKYBOX_TEXTURED))
            am->SpawnBasicNode(BASIC_SKYBOX_TEXTURED, node, selAsset, setFocusPos, spawnPos);
        if (ImGui::MenuItem(BASIC_SKYBOX_VERTEX_COLOR))
            am->SpawnBasicNode(BASIC_SKYBOX_VERTEX_COLOR, node, selAsset, setFocusPos, spawnPos);
        ImGui::EndMenu();
    }

    // Draw addon spawn basic 3D items
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->DrawSpawnBasic3dItems(node);
    }
    if (ImGui::MenuItem(BASIC_NAV_MESH))
        am->SpawnBasicNode(BASIC_NAV_MESH, node, selAsset, setFocusPos, spawnPos);
    if (ImGui::MenuItem(BASIC_SPLINE))
        am->SpawnBasicNode(BASIC_SPLINE, node, selAsset, setFocusPos, spawnPos);
}
static void DrawSpawnBasicWidgetMenu(Node* node)
{
    ActionManager* am = ActionManager::Get();
    Asset* selAsset = GetEditorState()->GetSelectedAsset();

    const char* widgetTypeName = nullptr;

    if (ImGui::MenuItem("Canvas"))
        widgetTypeName = "Canvas";
    if (ImGui::MenuItem("Widget"))
        widgetTypeName = "Widget";
    if (ImGui::MenuItem("AnimatedWidget"))
        widgetTypeName = "AnimatedWidget";
    if (ImGui::MenuItem("Quad"))
        widgetTypeName = "Quad";
    if (ImGui::MenuItem("Text"))
        widgetTypeName = "Text";
    if (ImGui::MenuItem("Button"))
        widgetTypeName = "Button";
    if (ImGui::MenuItem("Window"))
        widgetTypeName = "Window";
    if (ImGui::MenuItem("DialogWindow"))
        widgetTypeName = "DialogWindow";

    if (widgetTypeName != nullptr)
    {
        Node* newWidget = am->EXE_SpawnNode(widgetTypeName);

        if (node == nullptr)
        {
            GetWorld(0)->PlaceNewlySpawnedNode(ResolvePtr(newWidget), {});
        }
        else
        {
            node->AddChild(newWidget);
        }

        OCT_ASSERT(newWidget);
        if (newWidget)
        {
            GetEditorState()->SetSelectedNode(newWidget);
        }
    }

    // Draw addon spawn basic widget items
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr != nullptr) hookMgr->DrawSpawnBasicWidgetItems(node);
    }
}

static void BuildHierarchyJson(Node* node, std::string& out, int depth)
{
    if (node == nullptr)
        return;

    auto indent = [&out](int d) {
        for (int i = 0; i < d; ++i)
            out += "  ";
    };

    indent(depth);
    out += "{\n";

    // Name
    indent(depth + 1);
    out += "\"name\": \"";
    out += node->GetName();
    out += "\",\n";

    // Type
    indent(depth + 1);
    out += "\"type\": \"";
    out += node->GetTypeName();
    out += "\"";

    // Script (if any)
    Script* script = node->GetScript();
    if (script != nullptr && !script->GetFile().empty())
    {
        out += ",\n";
        indent(depth + 1);
        out += "\"script\": \"";
        out += script->GetFile();
        out += "\"";
    }

    // Scene reference (if linked)
    if (node->IsSceneLinked() && node->GetScene() != nullptr)
    {
        out += ",\n";
        indent(depth + 1);
        out += "\"linkedScene\": \"";
        out += node->GetScene()->GetName();
        out += "\"";
    }

    // Children
    if (node->GetNumChildren() > 0)
    {
        out += ",\n";
        indent(depth + 1);
        out += "\"children\": [\n";

        for (uint32_t i = 0; i < node->GetNumChildren(); ++i)
        {
            BuildHierarchyJson(node->GetChild(i), out, depth + 2);
            if (i < node->GetNumChildren() - 1)
                out += ",";
            out += "\n";
        }

        indent(depth + 1);
        out += "]";
    }

    out += "\n";
    indent(depth);
    out += "}";
}

static void CopyNodeHierarchyToClipboard(Node* node)
{
    if (node == nullptr)
        return;

    std::string json;
    BuildHierarchyJson(node, json, 0);
    json += "\n";

    ImGui::SetClipboardText(json.c_str());
    LogDebug("Copied hierarchy JSON to clipboard (%d characters)", (int)json.size());
}

// Paste the cloned NodeClipboard contents under `parent`. Mirrors the
// duplicate flow in ActionManager::DuplicateNodes: spawn via EXE_SpawnNodes
// (cloned again, so the clipboard snapshots stay reusable) then attach to
// the requested parent. Falls back to the active world's root when parent
// is null. Refuses to paste under scene-linked nodes.
static void PasteNodesFromClipboard(Node* parent)
{
    if (!NodeClipboard::HasContent())
        return;

    const std::vector<NodePtr>& clip = NodeClipboard::GetContents();

    std::vector<Node*> srcNodes;
    srcNodes.reserve(clip.size());
    for (const NodePtr& np : clip)
    {
        if (np != nullptr)
            srcNodes.push_back(np.Get());
    }

    if (srcNodes.empty())
        return;

    if (parent == nullptr)
    {
        World* world = GetWorld(0);
        if (world != nullptr)
            parent = world->GetRootNode();
    }

    if (parent != nullptr && (parent->IsSceneLinked() || parent->IsSceneLinkedChild()))
    {
        LogWarning("Cannot paste nodes into a scene-linked subtree. Unlink the scene first.");
        return;
    }

    ActionManager* am = ActionManager::Get();
    std::vector<Node*> spawned = am->EXE_SpawnNodes(srcNodes);

    if (spawned.size() != srcNodes.size())
        return;

    if (parent != nullptr)
    {
        for (Node* newNode : spawned)
        {
            parent->AddChild(newNode);
        }
    }

    GetEditorState()->SetSelectedNode(nullptr);
    for (Node* newNode : spawned)
    {
        GetEditorState()->AddSelectedNode(newNode, false);
    }
}

// Helpers exposed to EditorMainMenu.cpp so the lifted main-menu construction
// can mutate file-scope state owned by this TU.
void EditorImgui_ResetSaveSceneAsBuffer()
{
    sPopupInputBuffer[0] = '\0';
}

void EditorImgui_RequestDockReset()
{
    sDockResetRequested = true;
}

static int sAltRowIndex = 0;
static ImU32 sAltRowColor = 0;

static void BeginAlternatingRows()
{
    sAltRowIndex = 0;
    const auto& style = ImGui::GetStyle();
    ImVec4 windowBg = style.Colors[ImGuiCol_WindowBg];
    float luminance = windowBg.x * 0.299f + windowBg.y * 0.587f + windowBg.z * 0.114f;
    if (luminance < 0.5f)
        sAltRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.02f));
    else
        sAltRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.03f));
}

static void AlternatingRowBackground()
{
    if (sAltRowIndex % 2 == 1)
    {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
        max.x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        ImGui::GetWindowDrawList()->AddRectFilled(min, max, sAltRowColor);
    }
    sAltRowIndex++;
}

static void DrawScenePanel()
{
    ActionManager* am = ActionManager::Get();

    // Drag-and-drop reparenting/reordering state
    enum class SceneDropZone { None, Above, Into, Below };
    static SceneDropZone sDropZone = SceneDropZone::None;
    static Node* sDropTargetNode = nullptr;
    static ImVec2 sDropLineP1, sDropLineP2;
    static ImVec2 sDropHighlightMin, sDropHighlightMax;
    sDropZone = SceneDropZone::None;
    sDropTargetNode = nullptr;

    static std::string sFilterStrTemp;
    static std::string sFilterStr;
    bool filterCleared = false;

    if (ImGui::InputText(ICON_FILTER, &sFilterStrTemp, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        sFilterStr = sFilterStrTemp;
        if (sFilterStr == "")
        {
            filterCleared = true;
        }
    }

    std::string filterStrUpper = sFilterStr;
    for (uint32_t c = 0; c < filterStrUpper.size(); ++c)
    {
        filterStrUpper[c] = toupper(filterStrUpper[c]);
    }

    if (filterCleared && GetEditorState()->GetSelectedNode() != nullptr)
    {
        GetEditorState()->mTrackSelectedNode = true;
    }

    // Screen filter combo for 3DS dual-screen filtering
    {
        static const char* sScreenFilterNames[] = { "All Screens", "Top Screen", "Bottom Screen" };
        int screenFilterIdx = GetEditorState()->mSceneScreenFilter + 1; // -1->0, 0->1, 1->2
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Combo("##ScreenFilter", &screenFilterIdx, sScreenFilterNames, 3))
        {
            GetEditorState()->mSceneScreenFilter = screenFilterIdx - 1; // 0->-1, 1->0, 2->1
        }
    }

    ImGuiTreeNodeFlags treeNodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_SpanAvailWidth
        | ImGuiTreeNodeFlags_DefaultOpen;

    World* world = GetWorld(0);
    Node* rootNode = world ? world->GetRootNode() : nullptr;
    bool sNodeContextActive = false;
    bool trackingNode = GetEditorState()->mTrackSelectedNode;

    glm::vec4 sceneColor = AssetManager::Get()->GetEditorAssetColor(Scene::GetStaticType());
    ImVec4 sceneColorIm = ImVec4(sceneColor.r, sceneColor.g, sceneColor.b, sceneColor.a);
    ImVec4 subSceneColorIm = ImVec4(0.5f, 0.8f, 0.2f, 1.0f);

    std::function<void(Node*, Scene*)> drawTree = [&](Node* node, Scene* subScene)
    {
        bool nodeSelected = GetEditorState()->IsNodeSelected(node);
        bool nodeSceneLinked = node->IsSceneLinked();
        bool inSubScene = (subScene != nullptr);
        bool nodeHasScene = (node->GetScene() != nullptr);

        bool drawTreeNode = true;

        // Screen filter: hide subtrees targeting a different screen
        if (GetEditorState()->mSceneScreenFilter >= 0 && node->GetParent() == rootNode)
        {
            if (node->GetTargetScreen() != (uint8_t)GetEditorState()->mSceneScreenFilter)
            {
                return; // Skip this subtree entirely in the hierarchy
            }
        }

        if (filterStrUpper != "")
        {
            std::string upperName = node->GetName();
            for (uint32_t c = 0; c < upperName.size(); ++c)
            {
                upperName[c] = toupper(upperName[c]);
            }

            drawTreeNode = (upperName.find(filterStrUpper) != std::string::npos);
        }

        if (drawTreeNode)
        {
            ImGuiTreeNodeFlags nodeFlags = treeNodeFlags;
            if (nodeSelected)
            {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }

            if (node->GetNumChildren() == 0 ||
                node->AreAllChildrenHiddenInTree() ||
                filterStrUpper != "")
            {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            if (inSubScene)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, subSceneColorIm);
            }
            else if (nodeHasScene)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, sceneColorIm);
            }

            const char* nodeIcon = GetNodeIcon(node);
            std::string nodeLabel = std::string(nodeIcon) + " " + node->GetName();
            ImGui::SetNextItemAllowOverlap();
            bool nodeOpen = ImGui::TreeNodeEx(nodeLabel.c_str(), nodeFlags);
            AlternatingRowBackground();
            bool nodeClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
            bool nodeMiddleClicked = ImGui::IsItemClicked(ImGuiMouseButton_Middle);
            ImGui::PushID((void*)node);
            ImGui::OpenPopupOnItemClick("##NodeCtx", ImGuiPopupFlags_MouseButtonRight);
            ImGui::PopID();
            bool expandChildren = trackingNode || (nodeMiddleClicked && IsControlDown());
            bool collapseChildren = !expandChildren && nodeMiddleClicked;

            // Save TreeNode rect for toggle buttons (before drag/drop changes last item)
            ImVec2 treeNodeRectMin = ImGui::GetItemRectMin();
            ImVec2 treeNodeRectMax = ImGui::GetItemRectMax();

            // Drag source for node references - must be right after TreeNodeEx while it's the "last item"
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                Node* dragNode = node;
                ImGui::SetDragDropPayload(DRAGDROP_NODE, &dragNode, sizeof(Node*));
                ImGui::Text("%s", node->GetName().c_str());
                ImGui::EndDragDropSource();
            }

            // Drop target for node reparenting/reordering and asset drag-and-drop
            if (ImGui::BeginDragDropTarget())
            {
                // --- Node reparent / reorder ---
                if (const ImGuiPayload* peekPayload = ImGui::AcceptDragDropPayload(DRAGDROP_NODE, ImGuiDragDropFlags_AcceptPeekOnly))
                {
                    Node* dragNode = *(Node**)peekPayload->Data;

                    // Validate: not self, not dropping onto descendant, not scene-linked target, not world root
                    bool validDrop = (dragNode != nullptr && dragNode != node &&
                                      !node->HasAncestor(dragNode) &&
                                      !node->IsSceneLinkedChild() &&
                                      node != rootNode);

                    if (validDrop)
                    {
                        ImVec2 itemMin = ImGui::GetItemRectMin();
                        ImVec2 itemMax = ImGui::GetItemRectMax();
                        float itemHeight = itemMax.y - itemMin.y;
                        float mouseY = ImGui::GetMousePos().y;
                        float relY = mouseY - itemMin.y;

                        SceneDropZone zone = SceneDropZone::Into;
                        if (relY < itemHeight * 0.25f)
                            zone = SceneDropZone::Above;
                        else if (relY > itemHeight * 0.75f)
                            zone = SceneDropZone::Below;

                        // For Above/Below, the target parent must exist (not root-level reorder with no parent)
                        if ((zone == SceneDropZone::Above || zone == SceneDropZone::Below) && node->GetParent() == nullptr)
                            zone = SceneDropZone::Into;

                        sDropZone = zone;
                        sDropTargetNode = node;

                        if (zone == SceneDropZone::Into)
                        {
                            sDropHighlightMin = itemMin;
                            sDropHighlightMax = itemMax;
                        }
                        else if (zone == SceneDropZone::Above)
                        {
                            sDropLineP1 = ImVec2(itemMin.x, itemMin.y);
                            sDropLineP2 = ImVec2(itemMax.x, itemMin.y);
                        }
                        else // Below
                        {
                            sDropLineP1 = ImVec2(itemMin.x, itemMax.y);
                            sDropLineP2 = ImVec2(itemMax.x, itemMax.y);
                        }

                        // Actual delivery
                        if (const ImGuiPayload* deliverPayload = ImGui::AcceptDragDropPayload(DRAGDROP_NODE))
                        {
                            Node* droppedNode = *(Node**)deliverPayload->Data;

                            if (zone == SceneDropZone::Into)
                            {
                                am->EXE_AttachNode(droppedNode, node, -1, -1);
                            }
                            else
                            {
                                Node* targetParent = node->GetParent();
                                int32_t siblingIndex = targetParent->FindChildIndex(node);
                                int32_t insertIndex = (zone == SceneDropZone::Below) ? siblingIndex + 1 : siblingIndex;

                                // If dragging within the same parent, account for removal shift
                                if (droppedNode->GetParent() == targetParent)
                                {
                                    int32_t draggedIndex = targetParent->FindChildIndex(droppedNode);
                                    if (draggedIndex < insertIndex)
                                        insertIndex--;
                                }

                                am->EXE_AttachNode(droppedNode, targetParent, insertIndex, -1);
                            }

                            sDropZone = SceneDropZone::None;
                            sDropTargetNode = nullptr;
                        }
                    }
                }

                // --- Asset drag-and-drop (StaticMesh → hierarchy node) ---
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
                {
                    AssetStub* droppedStub = *(AssetStub**)payload->Data;
                    if (droppedStub != nullptr && droppedStub->mType == StaticMesh::GetStaticType())
                    {
                        sAssetDropStub = droppedStub;
                        sAssetDropParentNode = node;
                        sAssetDropInViewport = false;
                        sAssetDropPopupPos = ImGui::GetMousePos();
                        sAssetDropPopupPending = true;
                    }
                    else if (droppedStub != nullptr && droppedStub->mType == Scene::GetStaticType())
                    {
                        if (droppedStub->mAsset == nullptr)
                            AssetManager::Get()->LoadAsset(*droppedStub);
                        if (droppedStub->mAsset != nullptr)
                        {
                            ActionManager::Get()->SpawnBasicNode(BASIC_SCENE,
                                node, droppedStub->mAsset, false, glm::vec3(0.0f));
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            // Draw visibility and active toggle buttons on the right side of the row
            {
                const float kToggleButtonSize = 16.0f;
                const float kToggleButtonSpacing = 2.0f;

                // Use saved TreeNode rect (not current last item which may have changed)
                ImVec2 rowMin = treeNodeRectMin;
                ImVec2 rowMax = treeNodeRectMax;
                float rowHeight = rowMax.y - rowMin.y;

                // Calculate button positions (right-aligned)
                float buttonY = rowMin.y + (rowHeight - kToggleButtonSize) * 0.5f;
                float visibilityBtnX = rowMax.x - kToggleButtonSpacing - kToggleButtonSize;
                float activeBtnX = visibilityBtnX - kToggleButtonSpacing - kToggleButtonSize;

                ImGui::PushID((void*)node);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                // --- Active Toggle Button ---
                ImGui::SetCursorScreenPos(ImVec2(activeBtnX, buttonY));
                bool isActive = node->IsActive();

                if (!isActive)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                }
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

                if (ImGui::Button(isActive ? ICON_MATERIAL_SYMBOLS_CHECK_CIRCLE "##active" : ICON_MATERIAL_SYMBOLS_CHECK "##active", ImVec2(kToggleButtonSize, kToggleButtonSize)))
                {
                    am->EXE_EditProperty(node, PropertyOwnerType::Node, "Active", 0, !isActive);
                    nodeClicked = false;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(isActive ? "Active (click to deactivate)" : "Inactive (click to activate)");
                }
                ImGui::PopStyleColor(4);

                // --- Visibility Toggle Button ---
                ImGui::SetCursorScreenPos(ImVec2(visibilityBtnX, buttonY));
                bool isVisible = node->IsVisible();

                if (!isVisible)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                }
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

                if (ImGui::Button(isVisible ? ICON_MDI_EYE "##visible" : ICON_BASIL_EYE_CLOSED_SOLID "##visible", ImVec2(kToggleButtonSize, kToggleButtonSize)))
                {
                    am->EXE_EditProperty(node, PropertyOwnerType::Node, "Visible", 0, !isVisible);
                    nodeClicked = false;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
                }
                ImGui::PopStyleColor(4);

                ImGui::PopStyleVar();
                ImGui::PopID();
            }

            // Hierarchy item GUI overlay (Batch 7)
            {
                EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                if (hookMgr != nullptr)
                {
                    hookMgr->DrawHierarchyItemGUI(node, treeNodeRectMin.x, treeNodeRectMin.y,
                        treeNodeRectMax.x - treeNodeRectMin.x, treeNodeRectMax.y - treeNodeRectMin.y);
                }
            }

            if (inSubScene || nodeHasScene)
            {
                ImGui::PopStyleColor();
            }

            if (nodeSelected && GetEditorState()->mTrackSelectedNode)
            {
                ImGui::SetScrollHereY(0.5f);
                GetEditorState()->mTrackSelectedNode = false;
            }

            ImGui::PushID((void*)node);
            if (ImGui::BeginPopup("##NodeCtx"))
            {
                bool setTextInputFocus = false;
                bool closeContextPopup = false;

                sNodeContextActive = true;

                if (node->IsSceneLinked() && ImGui::Selectable("Open Scene"))
                {
                    GetEditorState()->OpenEditScene(node->GetScene());
                }
                if (node->GetParent() != nullptr &&
                    !inSubScene &&
                    ImGui::BeginMenu("Move"))
                {
                    Node* parent = node->GetParent();
                    int32_t childSlot = parent->FindChildIndex(node);

                    if (ImGui::Selectable("Top"))
                        am->EXE_AttachNode(node, parent, 0, -1);
                    if (ImGui::Selectable("Up"))
                        am->EXE_AttachNode(node, parent, glm::max<int32_t>(childSlot - 1, 0), -1);
                    if (ImGui::Selectable("Down"))
                        am->EXE_AttachNode(node, parent, childSlot + 1, -1);
                    if (ImGui::Selectable("Bottom"))
                        am->EXE_AttachNode(node, parent, -1, -1);

                    ImGui::EndMenu();
                }
                if (ImGui::Selectable("Rename", false, ImGuiSelectableFlags_DontClosePopups))
                {
                    ImGui::OpenPopup("Rename Node");
                    strncpy(sPopupInputBuffer, node->GetName().c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    setTextInputFocus = true;
                }
                if (!inSubScene && ImGui::Selectable("Duplicate", false, ImGuiSelectableFlags_DontClosePopups))
                {
                    ImGui::OpenPopup("Duplicate Node (Context)");
                    std::string defaultName = node->GetName() + "_00";
                    strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    setTextInputFocus = true;
                }
                if (!nodeSceneLinked && !inSubScene && ImGui::Selectable("Attach Selected"))
                {
                    am->AttachSelectedNodes(node, -1);
                }

                // Parent Selected With submenu
                {
                    const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
                    bool canParentSelected = (selNodes.size() >= 1) && !inSubScene;

                    // Check if any nodes are scene-linked
                    for (Node* n : selNodes)
                    {
                        if (n->IsSceneLinkedChild())
                        {
                            canParentSelected = false;
                            break;
                        }
                    }

                    if (canParentSelected && ImGui::BeginMenu("Parent Selected With"))
                    {
                        // Determine what types to show based on selection
                        bool allWidgets = true;
                        bool allNode3D = true;

                        for (Node* n : selNodes)
                        {
                            if (n->IsWidget())
                            {
                                allNode3D = false;
                            }
                            else if (n->IsNode3D())
                            {
                                allWidgets = false;
                            }
                            else
                            {
                                allWidgets = false;
                                allNode3D = false;
                            }
                        }

                        // Widget parent options (only if all selected are widgets)
                        if (allWidgets)
                        {
                            if (ImGui::Selectable("Array Widget (Horizontal)"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    ArrayWidget::GetStaticType(),
                                    ArrayOrientation::Horizontal);
                            }
                            if (ImGui::Selectable("Array Widget (Vertical)"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    ArrayWidget::GetStaticType(),
                                    ArrayOrientation::Vertical);
                            }
                            if (ImGui::Selectable("Canvas"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Canvas::GetStaticType());
                            }
                            if (ImGui::Selectable("Quad"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Quad::GetStaticType());
                            }
                            if (ImGui::Selectable("Widget"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Widget::GetStaticType());
                            }
                            if (ImGui::Selectable("Window"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Window::GetStaticType());
                            }
                            if (ImGui::Selectable("Dialog Window"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    DialogWindow::GetStaticType());
                            }
                        }

                        // Node3D parent options (only if all selected are Node3D)
                        if (allNode3D)
                        {
                            if (ImGui::Selectable("Node 3D"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Node3D::GetStaticType());
                            }
                            if (ImGui::Selectable("Node"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Node::GetStaticType());
                            }
                        }

                        // Mixed or base Node selection - only Node as common base
                        if (!allWidgets && !allNode3D)
                        {
                            if (ImGui::Selectable("Node"))
                            {
                                am->EXE_ParentSelectedWith(
                                    selNodes,
                                    Node::GetStaticType());
                            }
                        }

                        ImGui::EndMenu();
                    }
                }

                if (!nodeSceneLinked && !inSubScene && node->As<SkeletalMesh3D>())
                {
                    if (ImGui::Selectable("Attach Selected To Bone", false, ImGuiSelectableFlags_DontClosePopups))
                    {
                        ImGui::OpenPopup("Attach Selected To Bone");
                        setTextInputFocus = true;
                    }
                }
                if (!node->IsWorldRoot() && !inSubScene && ImGui::Selectable("Set Root Node"))
                {
                    am->EXE_SetRootNode(node);
                }
                if ((nodeSceneLinked || inSubScene) && ImGui::Selectable("Unlink Scene"))
                {
                    am->EXE_UnlinkScene(node->GetSubRoot());
                }
                if ((nodeSceneLinked || inSubScene) && ImGui::Selectable("Reset Scene"))
                {
                    am->EXE_ResetScene(node->GetSubRoot());
                }
                if (!inSubScene && ImGui::Selectable("Delete"))
                {
                    am->EXE_DeleteNode(node);
                }
                if (!inSubScene &&
                    node->As<StaticMesh3D>() &&
                    ImGui::Selectable("Merge"))
                {
                    LogDebug("TODO: Implement Merge for static meshes.");
                }
                if (!inSubScene && ImGui::Selectable("Replace Selected With Asset", false, ImGuiSelectableFlags_DontClosePopups))
                {
                    sReplaceAssetInput = "";
                    sReplaceAssetSuggestions.clear();

                    // Populate suggestions from asset map
                    std::unordered_map<std::string, AssetStub*>& assetMap = AssetManager::Get()->GetAssetMap();
                    for (auto& pair : assetMap)
                    {
                        AssetStub* s = pair.second;
                        if (s && (s->mType == StaticMesh::GetStaticType() ||
                                  s->mType == MaterialLite::GetStaticType() ||
                                  s->mType == MaterialBase::GetStaticType() ||
                                  s->mType == MaterialInstance::GetStaticType() ||
                                  s->mType == Scene::GetStaticType()))
                        {
                            sReplaceAssetSuggestions.push_back(s->mName);
                        }
                    }
                    std::sort(sReplaceAssetSuggestions.begin(), sReplaceAssetSuggestions.end());

                    ImGui::OpenPopup("Replace Selected With Asset");
                }
                {
                    // Check selection flags for instanced/static mesh items
                    const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
                    bool hasSelectedStaticMeshes = false;
                    bool hasSelectedInstancedMeshes = false;
                    for (Node* n : selNodes)
                    {
                        if (n->As<StaticMesh3D>() && !n->As<InstancedMesh3D>()) hasSelectedStaticMeshes = true;
                        if (n->As<InstancedMesh3D>()) hasSelectedInstancedMeshes = true;
                    }

                    if (!inSubScene && hasSelectedStaticMeshes &&
                        ImGui::Selectable("Merge Selected Into Instanced Mesh"))
                    {
                        am->EXE_ReplaceWithInstancedMesh(selNodes, true);
                    }
                    if (!inSubScene && hasSelectedStaticMeshes &&
                        ImGui::Selectable("Convert Selected To Instanced Mesh"))
                    {
                        am->EXE_ReplaceWithInstancedMesh(selNodes, false);
                    }
                    if (!inSubScene && hasSelectedInstancedMeshes &&
                        ImGui::Selectable("Replace Selected With Static Mesh"))
                    {
                        am->EXE_ReplaceWithStaticMesh(selNodes);
                    }
                }

                if (!nodeSceneLinked && !inSubScene && node->GetNumChildren() > 0 && ImGui::Selectable("Create Scene...", false, ImGuiSelectableFlags_DontClosePopups))
                {
                    ImGui::OpenPopup("Create Scene From Node");
                    std::string defaultName = "SC_" + node->GetName();
                    strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    sCreateSceneTargetNode = node;
                    sCreateSceneTargetDir = GetEditorState()->GetAssetDirectory();
                    setTextInputFocus = true;
                }

                // Plugin/addon context menu items
                ImGui::Separator();
                {
                    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                    if (hookMgr)
                    {
                        hookMgr->DrawNodeContextItems();
                    }
                }

                if (!nodeSceneLinked && !inSubScene && ImGui::BeginMenu("Import"))
                {
                    DrawImportMenu(node);
                    ImGui::EndMenu();
                }
                if (!nodeSceneLinked && !inSubScene && ImGui::BeginMenu("Add Node"))
                {
                    DrawAddNodeMenu(node);
                    ImGui::EndMenu();
                }
                if (!nodeSceneLinked && !inSubScene && ImGui::BeginMenu("Add Basic 3D"))
                {
                    DrawSpawnBasic3dMenu(node, false);
                    ImGui::EndMenu();
                }
                if (!nodeSceneLinked && !inSubScene && ImGui::BeginMenu("Add Basic Widget"))
                {
                    DrawSpawnBasicWidgetMenu(node);
                    ImGui::EndMenu();
                }

                // Widget XML export options
                if (node->As<Widget>())
                {
                    ImGui::Separator();
                    if (ImGui::Selectable("Export XML..."))
                    {
                        std::string xml;
                        ExportWidgetTreeToXML(node->As<Widget>(), xml);

                        std::string savePath = SYS_SaveFileDialog();
                        if (!savePath.empty())
                        {
                            // Ensure .xml extension
                            if (savePath.size() < 4 || savePath.substr(savePath.size() - 4) != ".xml")
                                savePath += ".xml";

                            FILE* f = fopen(savePath.c_str(), "w");
                            if (f)
                            {
                                fwrite(xml.c_str(), 1, xml.size(), f);
                                fclose(f);
                                LogDebug("Exported XML to: %s", savePath.c_str());
                            }
                            else
                            {
                                LogError("Failed to write XML file: %s", savePath.c_str());
                            }
                        }
                    }
                    if (ImGui::Selectable("Create UIDocument From Hierarchy"))
                    {
                        std::string xml;
                        ExportWidgetTreeToXML(node->As<Widget>(), xml);

                        AssetDir* currentDir = GetEditorState()->GetAssetDirectory();
                        if (currentDir)
                        {
                            std::string assetName = "UI_" + node->GetName();
                            AssetStub* stub = EditorAddUniqueAsset(assetName.c_str(), currentDir, UIDocument::GetStaticType(), true);
                            if (stub && stub->mAsset)
                            {
                                UIDocument* uiDoc = stub->mAsset->As<UIDocument>();
                                if (uiDoc)
                                {
                                    uiDoc->SetXmlSource(xml);
                                    AssetManager::Get()->SaveAsset(*stub);
                                    LogDebug("Created UIDocument '%s' from widget hierarchy.", assetName.c_str());
                                }
                            }
                        }
                    }
                }

                ImGui::Separator();
                if (!inSubScene && ImGui::Selectable("Copy"))
                {
                    NodeClipboard::Copy(GetEditorState()->GetSelectedNodes());
                }
                if (!inSubScene && ImGui::Selectable("Cut"))
                {
                    const std::vector<Node*>& selForCut = GetEditorState()->GetSelectedNodes();
                    NodeClipboard::Copy(selForCut);
                    am->EXE_DeleteNodes(selForCut);
                    closeContextPopup = true;
                }
                {
                    bool canPaste = NodeClipboard::HasContent() && !inSubScene;
                    if (ImGui::Selectable("Paste", false,
                            canPaste ? 0 : ImGuiSelectableFlags_Disabled))
                    {
                        PasteNodesFromClipboard(node);
                    }
                }
                ImGui::Separator();
                if (ImGui::Selectable("Copy Hierarchy"))
                {
                    CopyNodeHierarchyToClipboard(node);
                }

                //if (ImGui::Selectable("Add Scene..."))
                //{

                //}

                // Sub Popups

                if (ImGui::BeginPopup("Rename Node"))
                {
                    if (setTextInputFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                    }

                    if (ImGui::InputText("Node Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        std::string newName = sPopupInputBuffer;
                        am->EXE_EditProperty(node, PropertyOwnerType::Node, "Name", 0, newName);
                    }

                    ImGui::EndPopup();
                }

                if (!inSubScene && ImGui::BeginPopup("Duplicate Node (Context)"))
                {
                    if (setTextInputFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                    }

                    if (ImGui::InputText("New Name##DupNodeCtx", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        am->DuplicateNodes({ node });

                        const std::vector<Node*>& newSelNodes = GetEditorState()->GetSelectedNodes();
                        if (!newSelNodes.empty())
                        {
                            std::string newName = sPopupInputBuffer;
                            am->EXE_EditProperty(newSelNodes[0], PropertyOwnerType::Node, "Name", 0, newName);
                        }

                        ImGui::CloseCurrentPopup();
                        closeContextPopup = true;
                    }

                    ImGui::EndPopup();
                }

                if (!inSubScene && ImGui::BeginPopup("Attach Selected To Bone"))
                {
                    if (setTextInputFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                    }

                    if (ImGui::InputText("Bone Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        SkeletalMesh3D* skNode = node->As<SkeletalMesh3D>();
                        if (skNode)
                        {
                            int32_t boneIdx = skNode->FindBoneIndex(sPopupInputBuffer);
                            am->AttachSelectedNodes(skNode, boneIdx);
                            ImGui::CloseCurrentPopup();
                            closeContextPopup = true;
                        }
                    }

                    if (ImGui::BeginMenu("Bones"))
                    {
                        SkeletalMesh3D* skNode = node->As<SkeletalMesh3D>();
                        SkeletalMesh* skMesh = skNode ? skNode->GetSkeletalMesh() : nullptr;

                        if (skMesh)
                        {
                            for (uint32_t i = 0; i < skMesh->GetNumBones(); ++i)
                            {
                                ImGui::PushID(i);
                                if (ImGui::MenuItem(skMesh->GetBone(i).mName.c_str()))
                                {
                                    am->AttachSelectedNodes(skNode, (int32_t)i);
                                    ImGui::CloseCurrentPopup();
                                    closeContextPopup = true;
                                }
                                ImGui::PopID();
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopup("Replace Selected With Asset"))
                {
                    ImGui::Text("Enter asset name:");
                    bool inputEntered = ImGui::InputText("##ReplaceAsset", &sReplaceAssetInput, ImGuiInputTextFlags_EnterReturnsTrue);

                    // Autocomplete dropdown
                    auto filterFunc = [](const std::string& suggestion, const std::string& input) -> bool
                    {
                        if (input.empty()) return true;
                        std::string upperSugg = suggestion;
                        std::string upperInput = input;
                        for (char& c : upperSugg) c = (char)toupper(c);
                        for (char& c : upperInput) c = (char)toupper(c);
                        return upperSugg.find(upperInput) != std::string::npos;
                    };

                    bool autocompleteSelected = DrawAutocompleteDropdown(
                        "ReplaceAssetAutocomplete",
                        sReplaceAssetInput,
                        sReplaceAssetSuggestions,
                        filterFunc,
                        !sReplaceAssetInput.empty());

                    if (inputEntered || autocompleteSelected)
                    {
                        Asset* asset = AssetManager::Get()->LoadAsset(sReplaceAssetInput);
                        if (asset)
                        {
                            const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
                            am->EXE_ReplaceSelectedWithAsset(asset, selNodes);
                            ImGui::CloseCurrentPopup();
                            closeContextPopup = true;
                        }
                        else
                        {
                            LogWarning("Could not load asset: %s", sReplaceAssetInput.c_str());
                        }
                    }

                    if (ImGui::Button("Cancel"))
                    {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopup("Create Scene From Node"))
                {
                    if (setTextInputFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                    }

                    ImGui::Text("Scene name:");
                    ImGui::InputText("##CreateSceneName", sPopupInputBuffer, kPopupInputBufferSize);

                    ImGui::Spacing();
                    ImGui::Text("Output directory:");
                    if (sCreateSceneTargetDir != nullptr)
                    {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", sCreateSceneTargetDir->mPath.c_str());
                    }

                    ImGui::Spacing();
                    if (ImGui::TreeNode("Select Directory"))
                    {
                        AssetDir* rootDir = AssetManager::Get()->GetRootDirectory();
                        if (rootDir != nullptr)
                        {
                            ImGui::BeginChild("DirTree", ImVec2(300, 150), true);
                            for (AssetDir* child : rootDir->mChildDirs)
                            {
                                DrawDirPickerTree(child, &sCreateSceneTargetDir);
                            }
                            ImGui::EndChild();
                        }
                        ImGui::TreePop();
                    }

                    ImGui::Spacing();
                    bool canCreate = (sCreateSceneTargetNode != nullptr && strlen(sPopupInputBuffer) > 0 && sCreateSceneTargetDir != nullptr);
                    if (!canCreate)
                        ImGui::BeginDisabled();
                    if (ImGui::Button("Create"))
                    {
                        AssetStub* stub = EditorAddUniqueAsset(sPopupInputBuffer, sCreateSceneTargetDir, Scene::GetStaticType(), true);
                        GetEditorState()->CaptureAndSaveScene(stub, sCreateSceneTargetNode);
                        sCreateSceneTargetNode = nullptr;
                        sCreateSceneTargetDir = nullptr;
                        ImGui::CloseCurrentPopup();
                        closeContextPopup = true;
                    }
                    if (!canCreate)
                        ImGui::EndDisabled();

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                    {
                        sCreateSceneTargetNode = nullptr;
                        sCreateSceneTargetDir = nullptr;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                if (closeContextPopup)
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            ImGui::PopID();

            if (nodeOpen)
            {
                if (filterStrUpper != "")
                {
                    ImGui::TreePop();
                }

                for (uint32_t i = 0; i < node->GetNumChildren(); ++i)
                {
                    Scene* newSubScene = nodeSceneLinked ? node->GetScene() : subScene;

                    Node* child = node->GetChild(i);
                    // Stale-pointer guard: see GetNodeIcon comment. Same root cause.
                    if (child == nullptr || child->IsDestroyed()) continue;
                    if (!child->mHiddenInTree)
                    {
                        if (collapseChildren)
                        {
                            ImGui::SetNextItemOpen(false);
                        }
                        else if (expandChildren)
                        {
                            ImGui::SetNextItemOpen(true);
                        }

                        drawTree(child, newSubScene);
                    }
                }

                if (filterStrUpper == "")
                {
                    ImGui::TreePop();
                }
            }

            if (nodeClicked)
            {
                if (GetEditorState()->mNodePropertySelect)
                {
                    GetEditorState()->AssignNodePropertySelect(node);
                }
                else
                {
                    if (ImGui::GetIO().KeyShift)
                    {
                        // Shift+click: select range from anchor to clicked node
                        GetEditorState()->SelectNodesInRange(rootNode, node);
                    }
                    else if (ImGui::GetIO().KeyCtrl)
                    {
                        // Ctrl+click: toggle selection / add to selection
                        if (nodeSelected)
                        {
                            GetEditorState()->DeselectNode(node);
                        }
                        else
                        {
                            GetEditorState()->AddSelectedNode(node, false);
                        }
                    }
                    else
                    {
                        // Regular click: single select
                        GetEditorState()->SetSelectedNode(node);
                    }
                }
            }
        }
        else if (filterStrUpper != "")
        {
            // If we have a filter string but didnt draw the node,
            // we still need to traverse the tree to see if there are matching children.
            Scene* newSubScene = nodeSceneLinked ? node->GetScene() : subScene;

            for (uint32_t i = 0; i < node->GetNumChildren(); ++i)
            {
                Node* child = node->GetChild(i);
                if (child == nullptr || child->IsDestroyed()) continue;
                drawTree(child, newSubScene);
            }
        }
    };

    BeginAlternatingRows();

    if (rootNode != nullptr)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 6.0f);
        drawTree(rootNode, nullptr);
        ImGui::PopStyleVar();
    }

    // Draw drag-and-drop visual indicators
    if (sDropZone != SceneDropZone::None && sDropTargetNode != nullptr)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 dropColorFill = IM_COL32(60, 120, 220, 40);
        const ImU32 dropColorBorder = IM_COL32(60, 120, 220, 180);
        const ImU32 dropLineColor = IM_COL32(60, 120, 220, 255);

        if (sDropZone == SceneDropZone::Into)
        {
            drawList->AddRectFilled(sDropHighlightMin, sDropHighlightMax, dropColorFill);
            drawList->AddRect(sDropHighlightMin, sDropHighlightMax, dropColorBorder);
        }
        else
        {
            drawList->AddLine(sDropLineP1, sDropLineP2, dropLineColor, 2.0f);
            drawList->AddCircleFilled(sDropLineP1, 3.0f, dropLineColor);
        }
    }

    // Root-level drop target for node unparenting and asset drag-and-drop (empty space below tree)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.y > 0.0f)
    {
        ImGui::InvisibleButton("##SceneAssetDropTarget", ImVec2(avail.x, avail.y));
        if (ImGui::BeginDragDropTarget())
        {
            // Node drop → unparent to root
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_NODE))
            {
                Node* droppedNode = *(Node**)payload->Data;
                if (droppedNode != nullptr &&
                    droppedNode != rootNode &&
                    !droppedNode->IsSceneLinkedChild())
                {
                    am->EXE_AttachNode(droppedNode, rootNode, -1, -1);
                }
            }

            // Asset drop
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
            {
                AssetStub* droppedStub = *(AssetStub**)payload->Data;
                if (droppedStub != nullptr && droppedStub->mType == StaticMesh::GetStaticType())
                {
                    sAssetDropStub = droppedStub;
                    sAssetDropParentNode = nullptr;
                    sAssetDropInViewport = false;
                    sAssetDropPopupPos = ImGui::GetMousePos();
                    sAssetDropPopupPending = true;
                }
                else if (droppedStub != nullptr && droppedStub->mType == Scene::GetStaticType())
                {
                    if (droppedStub->mAsset == nullptr)
                        AssetManager::Get()->LoadAsset(*droppedStub);
                    if (droppedStub->mAsset != nullptr)
                    {
                        ActionManager::Get()->SpawnBasicNode(BASIC_SCENE,
                            nullptr, droppedStub->mAsset, false, glm::vec3(0.0f));
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    bool setKeyboardFocus = false;

    // If no popup is open and we aren't inputting text...
    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) &&
        ImGui::IsWindowHovered() &&
        !ImGui::GetIO().WantTextInput &&
        !sNodeContextActive)
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("Null Node Context");
        }

        const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();

        // Move Up/Down selected node.
        if (selNodes.size() == 1 &&
            selNodes[0]->GetParent() != nullptr)
        {
            Node* node = selNodes[0];
            Node* parent = node->GetParent();
            int32_t childIndex = parent->FindChildIndex(node);

            EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();
            if (hotkeys->IsActionJustTriggered(EditorAction::Hier_ReorderUp))
            {
                am->EXE_AttachNode(node, parent, glm::max<int32_t>(childIndex - 1, 0), -1);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Hier_ReorderDown))
            {
                am->EXE_AttachNode(node, parent, childIndex + 1, -1);
            }
        }

        EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();

        if (selNodes.size() > 0)
        {
            if (hotkeys->IsActionJustTriggered(EditorAction::Edit_DeleteSelected))
            {
                am->EXE_DeleteNodes(selNodes);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Edit_Duplicate))
            {
                if (selNodes.size() == 1)
                {
                    ImGui::OpenPopup("Duplicate Node Name");
                    std::string defaultName = selNodes[0]->GetName() + "_00";
                    strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    sDuplicateNodeFocus = true;
                }
                else
                {
                    am->DuplicateNodes(selNodes);
                }
            }
            else if (hotkeys->IsActionJustTriggeredImGui(EditorAction::Edit_CopyNodes))
            {
                NodeClipboard::Copy(selNodes);
            }
            else if (hotkeys->IsActionJustTriggeredImGui(EditorAction::Edit_CutNodes))
            {
                NodeClipboard::Copy(selNodes);
                am->EXE_DeleteNodes(selNodes);
            }
            else if (hotkeys->IsActionJustTriggered(EditorAction::Hier_Rename))
            {
                ImGui::OpenPopup("Rename Node F2");
                strncpy(sPopupInputBuffer, selNodes[0]->GetName().c_str(), kPopupInputBufferSize - 1);
                sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                setKeyboardFocus = true;
            }
        }

        // Paste works whether or not anything is selected.
        if (NodeClipboard::HasContent() &&
            hotkeys->IsActionJustTriggeredImGui(EditorAction::Edit_PasteNodes))
        {
            Node* parent = (selNodes.size() == 1) ? selNodes[0] : nullptr;
            PasteNodesFromClipboard(parent);
        }
    }

    if (ImGui::BeginPopup("Rename Node F2"))
    {
        if (setKeyboardFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Node Name F2", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
            std::string newName = sPopupInputBuffer;
            am->EXE_EditProperty(selNodes[0], PropertyOwnerType::Node, "Name", 0, newName);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Duplicate Node Name"))
    {
        if (sDuplicateNodeFocus)
        {
            ImGui::SetKeyboardFocusHere();
            sDuplicateNodeFocus = false;
        }

        if (ImGui::InputText("New Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
            if (selNodes.size() == 1)
            {
                am->DuplicateNodes({ selNodes[0] });

                const std::vector<Node*>& newSelNodes = GetEditorState()->GetSelectedNodes();
                if (!newSelNodes.empty())
                {
                    std::string newName = sPopupInputBuffer;
                    am->EXE_EditProperty(newSelNodes[0], PropertyOwnerType::Node, "Name", 0, newName);
                }
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Null Node Context"))
    {
        if (ImGui::BeginMenu("Spawn Node"))
        {
            DrawAddNodeMenu(nullptr);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Spawn Basic 3D"))
        {
            DrawSpawnBasic3dMenu(nullptr, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Spawn Basic Widget"))
        {
            DrawSpawnBasicWidgetMenu(nullptr);
            ImGui::EndMenu();
        }

        // Parent Selected With submenu (for multi-selection)
        {
            ActionManager* am = ActionManager::Get();
            const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
            bool canParentSelected = (selNodes.size() >= 1);

            // Check if any nodes are scene-linked
            for (Node* n : selNodes)
            {
                if (n->IsSceneLinkedChild())
                {
                    canParentSelected = false;
                    break;
                }
            }

            if (canParentSelected && ImGui::BeginMenu("Parent Selected With"))
            {
                // Determine what types to show based on selection
                bool allWidgets = true;
                bool allNode3D = true;

                for (Node* n : selNodes)
                {
                    if (n->IsWidget())
                    {
                        allNode3D = false;
                    }
                    else if (n->IsNode3D())
                    {
                        allWidgets = false;
                    }
                    else
                    {
                        allWidgets = false;
                        allNode3D = false;
                    }
                }

                // Widget parent options (only if all selected are widgets)
                if (allWidgets)
                {
                    if (ImGui::Selectable("Array Widget (Horizontal)"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            ArrayWidget::GetStaticType(),
                            ArrayOrientation::Horizontal);
                    }
                    if (ImGui::Selectable("Array Widget (Vertical)"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            ArrayWidget::GetStaticType(),
                            ArrayOrientation::Vertical);
                    }
                    if (ImGui::Selectable("Canvas"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Canvas::GetStaticType());
                    }
                    if (ImGui::Selectable("Quad"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Quad::GetStaticType());
                    }
                    if (ImGui::Selectable("Widget"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Widget::GetStaticType());
                    }
                    if (ImGui::Selectable("Window"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Window::GetStaticType());
                    }
                    if (ImGui::Selectable("Dialog Window"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            DialogWindow::GetStaticType());
                    }
                }

                // Node3D parent options (only if all selected are Node3D)
                if (allNode3D)
                {
                    if (ImGui::Selectable("Node 3D"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Node3D::GetStaticType());
                    }
                    if (ImGui::Selectable("Node"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Node::GetStaticType());
                    }
                }

                // Mixed or base Node selection - only Node as common base
                if (!allWidgets && !allNode3D)
                {
                    if (ImGui::Selectable("Node"))
                    {
                        am->EXE_ParentSelectedWith(
                            selNodes,
                            Node::GetStaticType());
                    }
                }

                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    }
}

static void DrawAssetsContextPopup(AssetStub* stub, AssetDir* dir)
{
    bool setTextInputFocus = false;
    bool closeContextPopup = false;
    static TypeId sNewAssetType = INVALID_TYPE_ID;
    static bool sNewAssetIsSkybox = false;

    ActionManager* actMan = ActionManager::Get();
    AssetManager* assMan = AssetManager::Get();

    AssetDir* curDir = GetEditorState()->GetAssetDirectory();

    // When right-clicking empty space (no dir/asset specified), default to root directory
    if (dir == nullptr && stub == nullptr)
    {
        if (GetEditorState()->mActiveAssetTab == AssetBrowserTab::Project)
            curDir = AssetManager::Get()->FindProjectDirectory();
        else
            curDir = AssetManager::Get()->FindPackagesDirectory();
    }

    bool readOnly = false;
    if ((stub && stub->mEngineAsset) ||
        (dir && dir->mEngineDir) ||
        (curDir && curDir->mAddonDir))
    {
        readOnly = true;
    }

    bool canInstantiate = false;
    if (stub &&
        (stub->mType == Scene::GetStaticType() ||
            stub->mType == SoundWave::GetStaticType() ||
            stub->mType == StaticMesh::GetStaticType() ||
            stub->mType == SkeletalMesh::GetStaticType() ||
            stub->mType == ParticleSystem::GetStaticType()))
    {
        canInstantiate = true;
    }

    if (stub && ImGui::Selectable("Properties"))
    {
        if (stub->mAsset == nullptr)
            AssetManager::Get()->LoadAsset(*stub);

        GetEditorState()->InspectObject(stub->mAsset);
    }

    if (stub && stub->mType == UIDocument::GetStaticType())
    {
        if (ImGui::Selectable("Edit"))
        {
            if (!stub->mAsset)
                AssetManager::Get()->LoadAsset(*stub);

            UIDocument* doc = stub->mAsset ? stub->mAsset->As<UIDocument>() : nullptr;
            if (doc)
            {
                const std::string& srcPath = doc->GetSourceFilePath();
                if (!srcPath.empty() && SYS_DoesFileExist(srcPath.c_str(), false))
                {
                    PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                    EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
                    if (editors)
                        editors->OpenLuaScript(srcPath);
                }
            }
        }
    }

    if (stub && stub->mType == Scene::GetStaticType())
    {
        if (ImGui::Selectable("Open Scene"))
        {
            if (stub->mAsset == nullptr)
                AssetManager::Get()->LoadAsset(*stub);

            Scene* scene = stub->mAsset ? stub->mAsset->As<Scene>() : nullptr;
            if (scene != nullptr)
            {
                GetEditorState()->OpenEditScene(scene);
            }
            else
            {
                LogError("Failed to load scene asset?");
            }
        }


        if (ImGui::Selectable("Set Default Game Scene"))
        {
            GetMutableEngineConfig()->mDefaultScene = stub->mName;
            WriteEngineConfig();
        }

        if (ImGui::Selectable("Set Default Editor Scene"))
        {
            GetMutableEngineConfig()->mDefaultEditorScene = stub->mName;
            WriteEngineConfig();
        }

        if (ImGui::Selectable("Reimport Scene"))
        {
            actMan->BeginReimportScene(stub);
        }
    }

    if (stub && stub->mType == SkeletalMesh::GetStaticType())
    {
        if (ImGui::Selectable("View Animations"))
        {
            if (stub->mAsset == nullptr)
                AssetManager::Get()->LoadAsset(*stub);

            SkeletalMesh* skelMesh = stub->mAsset ? stub->mAsset->As<SkeletalMesh>() : nullptr;
            if (skelMesh != nullptr)
            {
                GetAnimationBrowser()->Open(skelMesh);
            }
        }
    }

    if (canInstantiate && ImGui::Selectable("Instantiate"))
    {
        if (stub->mAsset == nullptr)
            assMan->LoadAsset(*stub);

        if (stub->mAsset != nullptr)
        {
            Asset* srcAsset = stub->mAsset;
            glm::vec3 spawnPos = EditorGetFocusPosition();
            Node* selNode = GetEditorState()->GetSelectedNode();

            if (stub->mType == Scene::GetStaticType())
                actMan->SpawnBasicNode(BASIC_SCENE, selNode, srcAsset, selNode == nullptr, spawnPos);
            else if (stub->mType == SoundWave::GetStaticType())
                actMan->SpawnBasicNode(BASIC_AUDIO, selNode, srcAsset, selNode == nullptr, spawnPos);
            else if (stub->mType == StaticMesh::GetStaticType())
                actMan->SpawnBasicNode(BASIC_STATIC_MESH, selNode, srcAsset, selNode == nullptr, spawnPos);
            else if (stub->mType == SkeletalMesh::GetStaticType())
                actMan->SpawnBasicNode(BASIC_SKELETAL_MESH, selNode, srcAsset, selNode == nullptr, spawnPos);
            else if (stub->mType == ParticleSystem::GetStaticType())
                actMan->SpawnBasicNode(BASIC_PARTICLE, selNode, srcAsset, selNode == nullptr, spawnPos);
        }
    }

    // Make Zoo — available when multiple StaticMesh assets are selected
    {
        const auto& multiStubs = GetEditorState()->GetSelectedAssetStubs();
        bool hasStaticMeshes = false;
        for (AssetStub* s : multiStubs)
            if (s && s->mType == StaticMesh::GetStaticType()) hasStaticMeshes = true;

        if (hasStaticMeshes && ImGui::Selectable("Make Zoo", false, ImGuiSelectableFlags_DontClosePopups))
        {
            ImGui::OpenPopup("Make Zoo");
            strncpy(sPopupInputBuffer, "SC_Zoo", kPopupInputBufferSize - 1);
            sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
            setTextInputFocus = true;
        }
    }

    // Create Animation Asset From Selected — available when 2+ Texture assets
    // are selected. Frames are added in selection order; default asset name is
    // "Anim_" + first selected texture's base name (T_ prefix stripped if any).
    {
        const auto& multiStubs = GetEditorState()->GetSelectedAssetStubs();
        int textureCount = 0;
        for (AssetStub* s : multiStubs)
        {
            if (s && s->mType == Texture::GetStaticType()) textureCount++;
        }

        if (textureCount >= 2 && ImGui::Selectable("Create Animation Asset From Selected", false, ImGuiSelectableFlags_DontClosePopups))
        {
            std::string defaultName = "Anim_Sprite";
            for (AssetStub* s : multiStubs)
            {
                if (s && s->mType == Texture::GetStaticType())
                {
                    std::string base = s->mName;
                    // Strip the conventional "T_" texture prefix so the default
                    // ends up like "Anim_Walk" rather than "Anim_T_Walk".
                    if (base.length() >= 2 && base[0] == 'T' && base[1] == '_')
                    {
                        base = base.substr(2);
                    }
                    defaultName = "Anim_" + base;
                    break;
                }
            }

            ImGui::OpenPopup("Create Animation From Textures");
            strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
            sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
            setTextInputFocus = true;
        }
    }

    if (!readOnly && (stub || dir))
    {
        if (stub && ImGui::Selectable("Save"))
        {
            if (stub->mAsset == nullptr)
                AssetManager::Get()->LoadAsset(*stub);

            assMan->SaveAsset(*stub);
        }
        if (ImGui::Selectable("Rename", false, ImGuiSelectableFlags_DontClosePopups))
        {
            ImGui::OpenPopup("Rename Asset");
            strncpy(sPopupInputBuffer, stub ? stub->mName.c_str() : dir->mName.c_str(), kPopupInputBufferSize - 1);
            sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
            setTextInputFocus = true;
        }
        if (ImGui::Selectable("Delete"))
        {
            if (stub)
            {
                const auto& selectedStubs = GetEditorState()->GetSelectedAssetStubs();
                if (selectedStubs.size() > 1 && GetEditorState()->IsAssetStubSelected(stub))
                {
                    std::vector<AssetStub*> toDelete = selectedStubs;
                    for (AssetStub* s : toDelete)
                    {
                        actMan->DeleteAsset(s);
                    }
                }
                else
                {
                    actMan->DeleteAsset(stub);
                }
            }
            else if (dir)
            {
                actMan->DeleteAssetDir(dir);
                GetEditorState()->ClearAssetDirHistory();
            }
        }

        if (stub && ImGui::Selectable("Duplicate", false, ImGuiSelectableFlags_DontClosePopups))
        {
            sDuplicateAssetStub = stub;
            ImGui::OpenPopup("Duplicate Asset (Context)");
            std::string defaultName = stub->mName + "_00";
            strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
            sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
            setTextInputFocus = true;
        }
    }

    // Reveal in Explorer — available for both assets and directories regardless of read-only
    if (stub || dir)
    {
        if (ImGui::Selectable("Reveal in Explorer"))
        {
            if (stub)
            {
                // Reveal the asset file, selecting it in the explorer
                std::string absPath = SYS_GetAbsolutePath(stub->mPath);
#if PLATFORM_WINDOWS
                // Replace forward slashes with backslashes for Windows explorer
                for (char& c : absPath) { if (c == '/') c = '\\'; }
                SYS_Exec(("explorer /select,\"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
                // Open the containing directory
                std::string dirPath = absPath.substr(0, absPath.find_last_of('/'));
                SYS_Exec(("xdg-open \"" + dirPath + "\" &").c_str());
#endif
            }
            else if (dir)
            {
                std::string absPath = SYS_GetAbsolutePath(dir->mPath);
#if PLATFORM_WINDOWS
                for (char& c : absPath) { if (c == '/') c = '\\'; }
                SYS_Exec(("explorer \"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
                SYS_Exec(("xdg-open \"" + absPath + "\" &").c_str());
#endif
            }
        }

        if (stub && ImGui::Selectable("Copy Path"))
        {
            // Build the relative path from project root to the asset
            std::string relativePath;
            AssetDir* projDir = AssetManager::Get()->FindProjectDirectory();
            AssetDir* dir = stub->mDirectory;

            // Walk up the directory tree to build the path
            std::vector<std::string> pathParts;
            while (dir != nullptr && dir != projDir)
            {
                pathParts.push_back(dir->mName);
                dir = dir->mParentDir;
            }

            // Build path from root to asset
            for (int i = (int)pathParts.size() - 1; i >= 0; --i)
            {
                relativePath += pathParts[i] + "/";
            }
            relativePath += stub->mName;

            ImGui::SetClipboardText(relativePath.c_str());
        }
    }

    if (curDir && !curDir->mEngineDir && !curDir->mAddonDir)
    {

		// Make into Import submenu with options for different import types (model, scene, etc)

        if (ImGui::BeginMenu("Import Asset"))
        {

        if (ImGui::Selectable("Import"))
        {
            actMan->ImportAsset();
        }

        if (ImGui::Selectable("Import Scene"))
        {
            actMan->BeginImportScene();
        }

        if (ImGui::Selectable("Import TinyLLM Model"))
        {
            actMan->ImportTinyLLMModel();
        }

        if (ImGui::Selectable("Import TinyLLM Tokenizer"))
        {
            actMan->ImportTinyLLMTokenizer();
        }

        ImGui::EndMenu();
		}

        if (ImGui::BeginMenu("Create Asset"))
        {
            bool showPopup = false;

            if (ImGui::Selectable("Material (Lite)", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = MaterialLite::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Material (Base)", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = MaterialBase::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Material (Instance)", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = MaterialInstance::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Skybox Material", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = MaterialLite::GetStaticType();
                sNewAssetIsSkybox = true;
                showPopup = true;
            }
            if (ImGui::Selectable("Particle System", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = ParticleSystem::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Tile Set", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = TileSet::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Tile Map", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = TileMap::GetStaticType();
                showPopup = true;
            }
            bool showScenePopup = false;
            if (ImGui::Selectable("Scene", false, ImGuiSelectableFlags_DontClosePopups))
            {
                showScenePopup = true;
            }
            if (ImGui::Selectable("Timeline", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = Timeline::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("Node Graph", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = NodeGraphAsset::GetStaticType();
                showPopup = true;
            }
            if (ImGui::Selectable("UI Document", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = UIDocument::GetStaticType();
                showPopup = true;
            }
            //if (ImGui::Selectable("Data Asset", false, ImGuiSelectableFlags_DontClosePopups))
            //{
            //    sNewAssetType = DataAsset::GetStaticType();
            //    showPopup = true;
            //}
            if (ImGui::Selectable("Sprite Animation", false, ImGuiSelectableFlags_DontClosePopups))
            {
                sNewAssetType = SpriteAnimation::GetStaticType();
                showPopup = true;
            }

            ImGui::Separator();

            bool showCssPopup = false;
            if (ImGui::Selectable("CSS Stylesheet", false, ImGuiSelectableFlags_DontClosePopups))
            {
                showCssPopup = true;
            }

            // Draw addon create asset items
            {
                EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                if (hookMgr != nullptr) hookMgr->DrawCreateAssetItems();
            }

            ImGui::EndMenu();

            if (showPopup)
            {
                ImGui::OpenPopup("New Asset Name");
                sPopupInputBuffer[0] = '\0';
                setTextInputFocus = true;
            }

            if (showScenePopup)
            {
                ImGui::OpenPopup("New Scene");
                sPopupInputBuffer[0] = '\0';
                sNewSceneType = 1;  // Default to 3D
                sNewSceneCreateCamera = true;
                setTextInputFocus = true;
            }

            if (showCssPopup)
            {
                ImGui::OpenPopup("New CSS Stylesheet");
                sPopupInputBuffer[0] = '\0';
                setTextInputFocus = true;
            }
        }

        if (ImGui::Selectable("New Folder", false, ImGuiSelectableFlags_DontClosePopups))
        {
            ImGui::OpenPopup("New Folder");
            sPopupInputBuffer[0] = '\0';
            setTextInputFocus = true;
        }

        if (ImGui::Selectable("Capture Active Scene", false, ImGuiSelectableFlags_DontClosePopups))
        {
            AssetStub* saveStub = nullptr;
            if (stub && stub->mType == Scene::GetStaticType())
            {
                saveStub = stub;
            }

            if (saveStub == nullptr)
            {
                ImGui::OpenPopup("Capture To New Scene");
                sPopupInputBuffer[0] = '\0';
                setTextInputFocus = true;
            }
            else
            {
                GetEditorState()->CaptureAndSaveScene(saveStub, nullptr);
                closeContextPopup = true;
            }
        }

        const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();
        if (selNodes.size() == 1 && ImGui::Selectable("Capture Selected Node"))
        {
            AssetStub* saveStub = nullptr;
            if (stub && stub->mType == Scene::GetStaticType())
            {
                saveStub = stub;
            }

            GetEditorState()->CaptureAndSaveScene(saveStub, selNodes[0]);
        }
    }

    if (ImGui::BeginPopup("Rename Asset"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (stub)
            {
                Asset* asset = AssetManager::Get()->LoadAsset(*stub);
                AssetManager::Get()->RenameAsset(asset, sPopupInputBuffer);
                AssetManager::Get()->SaveAsset(*stub);
            }
            else if (dir)
            {
                AssetManager::Get()->RenameDirectory(dir, sPopupInputBuffer);
            }

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Duplicate Asset (Context)"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("New Name##DupAssetCtx", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (sDuplicateAssetStub != nullptr)
            {
                GetEditorState()->DuplicateAsset(sDuplicateAssetStub, sPopupInputBuffer);
                sDuplicateAssetStub = nullptr;
            }
            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("New Folder"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Folder Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            const std::string folderName = sPopupInputBuffer;

            if (folderName != "")
            {
                if (SYS_CreateDirectory((curDir->mPath + folderName).c_str()))
                {
                    curDir->CreateSubdirectory(folderName);
                }
                else
                {
                    LogError("Failed to create folder");
                }
            }

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("New CSS Stylesheet"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string cssName = sPopupInputBuffer;
            if (cssName.empty())
                cssName = "style";

            std::string cssPath = curDir->mPath + cssName + ".css";
            FILE* cssFile = fopen(cssPath.c_str(), "w");
            if (cssFile != nullptr)
            {
                fputs("/* Stylesheet */\n\n", cssFile);
                fclose(cssFile);
                LogDebug("Created CSS file: %s", cssPath.c_str());
            }
            else
            {
                LogError("Failed to create CSS file: %s", cssPath.c_str());
            }

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Capture To New Scene"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string sceneName = sPopupInputBuffer;

            if (sceneName == "")
            {
                sceneName = "SC_Scene";
            }

            AssetStub* saveStub = EditorAddUniqueAsset(sceneName.c_str(), curDir, Scene::GetStaticType(), true);
            GetEditorState()->CaptureAndSaveScene(saveStub, nullptr);

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("New Asset Name"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string assetName = sPopupInputBuffer;
            bool userProvidedName = (assetName != "");

            if (assetName == "")
            {
                if (sNewAssetIsSkybox)
                {
                    assetName = "M_Skybox";
                }
                else if (sNewAssetType == MaterialLite::GetStaticType())
                {
                    Asset* selAsset = GetEditorState()->GetSelectedAsset();
                    if (selAsset != nullptr && selAsset->GetType() == Texture::GetStaticType())
                    {
                        std::string texName = selAsset->GetName();
                        if (texName.length() >= 2 && texName[0] == 'T' && texName[1] == '_')
                            texName[0] = 'M';
                        else
                            texName = std::string("M_") + texName;
                        assetName = texName;
                    }
                    else
                    {
                        assetName = "M_Material";
                    }
                }
                else if (sNewAssetType == MaterialBase::GetStaticType())
                    assetName = "MB_Material";
                else if (sNewAssetType == MaterialInstance::GetStaticType())
                    assetName = "MI_Material";
                else if (sNewAssetType == ParticleSystem::GetStaticType())
                    assetName = "P_Particle";
                else if (sNewAssetType == TileSet::GetStaticType())
                    assetName = "TS_TileSet";
                else if (sNewAssetType == TileMap::GetStaticType())
                    assetName = "TM_TileMap";
                else if (sNewAssetType == Timeline::GetStaticType())
                    assetName = "TL_Timeline";
                else if (sNewAssetType == NodeGraphAsset::GetStaticType())
                    assetName = "NG_NodeGraph";
                else if (sNewAssetType == UIDocument::GetStaticType())
                    assetName = "UI_Document";
            }

            if (assetName != "" && sNewAssetType != INVALID_TYPE_ID)
            {
                CreateNewAsset(sNewAssetType, assetName.c_str(), sNewAssetIsSkybox, userProvidedName);
            }

            sNewAssetIsSkybox = false;
            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("New Scene"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize);

        ImGui::RadioButton("2D", &sNewSceneType, 0);
        ImGui::SameLine();
        ImGui::RadioButton("3D", &sNewSceneType, 1);

        // Plugin-registered scene types
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                const auto& sceneTypes = hookMgr->GetSceneTypes();
                for (int i = 0; i < (int)sceneTypes.size(); ++i)
                {
                    ImGui::SameLine();
                    ImGui::RadioButton(sceneTypes[i].mTypeName.c_str(), &sNewSceneType, 2 + i);
                }
            }
        }

        Polyphase::Checkbox("Create Camera", &sNewSceneCreateCamera);
        if (sNewSceneType != 0) {
            Polyphase::Checkbox("Create Skybox", &sNewSceneCreateSkybox);
        }

        if (ImGui::Button("Create"))
        {
            std::string sceneName = sPopupInputBuffer;
            if (sceneName.empty())
                sceneName = "SC_Scene";

            CreateNewScene(sceneName.c_str(), sNewSceneType, sNewSceneCreateCamera, sNewSceneCreateSkybox);

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Make Zoo"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::InputText("Scene Name", sPopupInputBuffer, kPopupInputBufferSize);
        ImGui::DragInt("Columns", &sZooColumns, 1, 1, 100);
        ImGui::DragFloat("Spacing", &sZooSpacing, 0.1f, 0.5f, 100.0f);

        if (ImGui::Button("Create"))
        {
            const auto& multiStubs = GetEditorState()->GetSelectedAssetStubs();
            std::vector<StaticMesh*> meshes;
            for (AssetStub* s : multiStubs)
            {
                if (s && s->mType == StaticMesh::GetStaticType())
                {
                    if (!s->mAsset) AssetManager::Get()->LoadAsset(*s);
                    StaticMesh* m = s->mAsset ? s->mAsset->As<StaticMesh>() : nullptr;
                    if (m) meshes.push_back(m);
                }
            }

            if (!meshes.empty())
            {
                std::string sceneName = sPopupInputBuffer[0] ? sPopupInputBuffer : "SC_Zoo";
                AssetStub* sceneStub = EditorAddUniqueAsset(sceneName.c_str(), curDir, Scene::GetStaticType(), true);
                if (sceneStub)
                {
                    SharedPtr<Node3D> root = Node::Construct<Node3D>();
                    root->SetName("Root");

                    int cols = glm::max(sZooColumns, 1);
                    int rows = ((int)meshes.size() + cols - 1) / cols;

                    for (int i = 0; i < (int)meshes.size(); ++i)
                    {
                        int col = i % cols;
                        int row = i / cols;
                        StaticMesh3D* node = root->CreateChild<StaticMesh3D>();
                        node->SetName(meshes[i]->GetName());
                        node->SetStaticMesh(meshes[i]);
                        node->SetPosition(glm::vec3(col * sZooSpacing, 0.0f, row * sZooSpacing));
                    }

                    float gridWidth = (cols - 1) * sZooSpacing;
                    float gridDepth = (rows - 1) * sZooSpacing;
                    float padding = sZooSpacing;
                    Asset* planeAsset = AssetManager::Get()->GetAsset("SC_Plane");
                    StaticMesh* planeMesh = planeAsset ? planeAsset->As<StaticMesh>() : nullptr;
                    if (planeMesh)
                    {
                        StaticMesh3D* floor = root->CreateChild<StaticMesh3D>("Floor");
                        floor->SetStaticMesh(planeMesh);
                        floor->SetPosition(glm::vec3(gridWidth * 0.5f, -0.05f, gridDepth * 0.5f));
                        float scaleX = (gridWidth + padding * 2) * 0.5f;
                        float scaleZ = (gridDepth + padding * 2) * 0.5f;
                        floor->SetScale(glm::vec3(scaleX, 1.0f, scaleZ));
                    }

                    Scene* scene = (Scene*)sceneStub->mAsset;
                    scene->Capture(root.Get());
                    AssetManager::Get()->SaveAsset(*sceneStub);
                    GetEditorState()->OpenEditScene(scene);
                }
            }

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Create Animation From Textures popup — opened from the multi-Texture
    // context menu entry above. Builds a Discrete-mode SpriteAnimation with
    // the selected Texture assets as frames in selection order.
    if (ImGui::BeginPopup("Create Animation From Textures"))
    {
        if (setTextInputFocus)
        {
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::InputText("Asset Name", sPopupInputBuffer, kPopupInputBufferSize);

        const auto& multiStubs = GetEditorState()->GetSelectedAssetStubs();
        int textureCount = 0;
        for (AssetStub* s : multiStubs)
        {
            if (s && s->mType == Texture::GetStaticType()) textureCount++;
        }
        ImGui::Text("%d frame%s in selection order", textureCount, textureCount == 1 ? "" : "s");

        if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
        {
            std::vector<Texture*> frames;
            for (AssetStub* s : multiStubs)
            {
                if (s && s->mType == Texture::GetStaticType())
                {
                    if (!s->mAsset) AssetManager::Get()->LoadAsset(*s);
                    Texture* tex = s->mAsset ? s->mAsset->As<Texture>() : nullptr;
                    if (tex) frames.push_back(tex);
                }
            }

            if (!frames.empty())
            {
                std::string assetName = sPopupInputBuffer[0] ? sPopupInputBuffer : "Anim_Sprite";
                AssetStub* newStub = EditorAddUniqueAsset(assetName.c_str(), curDir, SpriteAnimation::GetStaticType(), true);
                if (newStub != nullptr && newStub->mAsset != nullptr)
                {
                    SpriteAnimation* anim = newStub->mAsset->As<SpriteAnimation>();
                    if (anim != nullptr)
                    {
                        anim->SetMode(SpriteFrameSourceMode::Discrete);
                        anim->SetAnimationName(assetName);
                        for (Texture* tex : frames)
                        {
                            anim->AddFrame(tex);
                        }
                        AssetManager::Get()->SaveAsset(*newStub);
                    }
                }
            }

            ImGui::CloseCurrentPopup();
            closeContextPopup = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Plugin/addon context menu items
    ImGui::Separator();
    {
        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
        if (hookMgr && stub)
        {
            const char* assetTypeName = Asset::GetNameFromTypeId(stub->mType);
            hookMgr->DrawAssetContextItems(assetTypeName ? assetTypeName : "");
        }
        else if (hookMgr)
        {
            hookMgr->DrawAssetContextItems("*");
        }
    }

    if (closeContextPopup)
    {
        ImGui::CloseCurrentPopup();
    }
}

// Flat directory picker for dialogs (e.g., Save Scene As)
static void DrawDirPickerTree(AssetDir* dir, AssetDir** selectedDir)
{
    if (dir == nullptr || selectedDir == nullptr)
        return;

    // Skip engine/addon directories
    if (dir->mEngineDir || dir->mAddonDir)
        return;

    ImGuiTreeNodeFlags dirFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    // Check if this is the selected directory
    if (*selectedDir == dir)
        dirFlags |= ImGuiTreeNodeFlags_Selected;

    // Check if directory has no subdirectories
    bool hasSubdirs = false;
    for (AssetDir* child : dir->mChildDirs)
    {
        if (!child->mEngineDir && !child->mAddonDir)
        {
            hasSubdirs = true;
            break;
        }
    }
    if (!hasSubdirs)
        dirFlags |= ImGuiTreeNodeFlags_Leaf;

    std::string dirLabel = std::string(ICON_MATERIAL_SYMBOLS_FOLDER_SHARP) + " " + dir->mName;
    ImGui::PushID(dir);
    bool nodeOpen = ImGui::TreeNodeEx(dirLabel.c_str(), dirFlags);

    // Click to select
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        *selectedDir = dir;
    }

    if (nodeOpen)
    {
        for (AssetDir* child : dir->mChildDirs)
        {
            DrawDirPickerTree(child, selectedDir);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void DrawDirPicker()
{
    AssetDir* currentDir = GetEditorState()->GetAssetDirectory();
    if (currentDir == nullptr)
        return;

    ImGui::PushStyleColor(ImGuiCol_Header, kBgInactive);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kBgHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kBgInactive);

    if (currentDir->mParentDir != nullptr)
    {
        if (ImGui::Selectable(ICON_MATERIAL_SYMBOLS_FOLDER_OPEN_SHARP "...", false))
        {
            GetEditorState()->SetAssetDirectory(currentDir->mParentDir, true);
        }
    }

    for (uint32_t i = 0; i < currentDir->mChildDirs.size(); ++i)
    {
        AssetDir* childDir = currentDir->mChildDirs[i];
        std::string dirLabel = std::string(ICON_MATERIAL_SYMBOLS_FOLDER_SHARP) + "  " + childDir->mName;
        if (ImGui::Selectable(dirLabel.c_str(), false))
        {
            GetEditorState()->SetAssetDirectory(childDir, true);
        }
    }

    ImGui::PopStyleColor(3);
}

static bool DirMatchesFilter(AssetDir* dir, const std::string& filterLower)
{
    if (dir == nullptr)
        return false;

    // Check if directory name matches
    {
        std::string nameLower = dir->mName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find(filterLower) != std::string::npos)
            return true;
    }

    // Check if any asset in this directory matches
    for (AssetStub* stub : dir->mAssetStubs)
    {
        std::string nameLower = stub->mName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find(filterLower) != std::string::npos)
            return true;
    }

    // Check if any loose file in this directory matches
    for (const std::string& looseFile : dir->mLooseFiles)
    {
        std::string nameLower = looseFile;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find(filterLower) != std::string::npos)
            return true;
    }

    // Check if any descendant directory matches
    for (AssetDir* child : dir->mChildDirs)
    {
        if (DirMatchesFilter(child, filterLower))
            return true;
    }

    return false;
}

static std::string sSelectedLooseFile; // Full path of selected loose file

static void DrawAssetItems(AssetDir* dir, const std::string& filterLower)
{
    AssetStub* selStub = GetEditorState()->GetSelectedAssetStub();

    for (uint32_t i = 0; i < dir->mAssetStubs.size(); ++i)
    {
        AssetStub* stub = dir->mAssetStubs[i];

        // Filter check
        if (!filterLower.empty())
        {
            std::string nameLower = stub->mName;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos)
                continue;
        }

        bool isSelectedStub = GetEditorState()->IsAssetStubSelected(stub);
        if (isSelectedStub)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSelectedColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, kSelectedColor);
        }

        glm::vec4 assetColor = AssetManager::Get()->GetEditorAssetColor(stub->mType);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(assetColor.r, assetColor.g, assetColor.b, assetColor.a));

        std::string assetDispText = std::string(GetAssetIcon(stub->mType)) + "  " + stub->mName;
        if (stub->mAsset && stub->mAsset->GetDirtyFlag())
        {
            assetDispText = "*" + assetDispText;
        }

        ImGui::PushID(stub);
        bool assetClicked = ImGui::Selectable(assetDispText.c_str(), isSelectedStub, ImGuiSelectableFlags_AllowDoubleClick);
        AlternatingRowBackground();
        if (assetClicked)
        {
            if (IsControlDown())
            {
                // Ctrl+Click: toggle in multi-selection
                if (GetEditorState()->IsAssetStubSelected(stub))
                {
                    GetEditorState()->RemoveSelectedAssetStub(stub);

                    // Update primary to another selected stub, or nullptr
                    const auto& stubs = GetEditorState()->GetSelectedAssetStubs();
                    GetEditorState()->mSelectedAssetStub = stubs.empty() ? nullptr : stubs.back();
                }
                else
                {
                    GetEditorState()->AddSelectedAssetStub(stub);
                    GetEditorState()->mSelectedAssetStub = stub;
                }
                sSelectedLooseFile.clear();

                if (stub != nullptr &&
                    stub->mAsset == nullptr)
                    AssetManager::Get()->LoadAsset(*stub);
                if (stub != nullptr &&
                    stub->mAsset != nullptr)
                    GetEditorState()->InspectObject(stub->mAsset);
            }
            else if (IsShiftDown() && selStub != nullptr)
            {
                // Shift+Click: range selection from primary to clicked item
                GetEditorState()->ClearSelectedAssetStubs();

                bool inRange = false;
                for (uint32_t j = 0; j < dir->mAssetStubs.size(); ++j)
                {
                    AssetStub* s = dir->mAssetStubs[j];

                    // Apply same filter so indices match what's visible
                    if (!filterLower.empty())
                    {
                        std::string nameLower = s->mName;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(filterLower) == std::string::npos)
                            continue;
                    }

                    if (s == selStub || s == stub)
                    {
                        inRange = !inRange;
                        GetEditorState()->AddSelectedAssetStub(s);
                        if (!inRange)
                            break; // We just hit the second endpoint
                    }
                    else if (inRange)
                    {
                        GetEditorState()->AddSelectedAssetStub(s);
                    }
                }

                GetEditorState()->mSelectedAssetStub = stub;
                sSelectedLooseFile.clear();
            }
            else if (selStub != stub)
            {
                GetEditorState()->SetSelectedAssetStub(stub);
                sSelectedLooseFile.clear();

                if (stub != nullptr &&
                    stub->mType == NodeGraphAsset::GetStaticType())
                {
                    if (stub->mAsset == nullptr)
                        AssetManager::Get()->LoadAsset(*stub);
                    if (stub->mAsset != nullptr)
                        GetEditorState()->InspectObject(stub->mAsset);
                }
            }
            else
            {
                GetEditorState()->SetSelectedAssetStub(nullptr);
            }

            if (ImGui::IsMouseDoubleClicked(0))
            {
                EditorUIHookManager::Get()->FireOnAssetOpen(stub->mName.c_str());

                if (stub->mAsset == nullptr)
                    AssetManager::Get()->LoadAsset(*stub);

                if (stub->mType == Scene::GetStaticType())
                {
                    Scene* scene = stub->mAsset ? stub->mAsset->As<Scene>() : nullptr;
                    if (scene)
                        ActionManager::Get()->OpenScene(scene);
                }
                else if (stub->mType == Timeline::GetStaticType())
                {
                    Timeline* timeline = stub->mAsset ? stub->mAsset->As<Timeline>() : nullptr;
                    if (timeline)
                    {
                        OpenTimelineForEditing(timeline);
                    }
                }
                else if (stub->mType == NodeGraphAsset::GetStaticType())
                {
                    NodeGraphAsset* nodeGraph = stub->mAsset ? stub->mAsset->As<NodeGraphAsset>() : nullptr;
                    if (nodeGraph)
                    {
                        OpenNodeGraphForEditing(nodeGraph);
                    }
                }
                else if (stub->mType == UIDocument::GetStaticType())
                {
                    if (stub->mAsset)
                    {
                        UIDocument* doc = stub->mAsset->As<UIDocument>();
                        const std::string& srcPath = doc->GetSourceFilePath();

                        if (!srcPath.empty() && SYS_DoesFileExist(srcPath.c_str(), false))
                        {
                            PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                            EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
                            if (editors)
                            {
                                editors->OpenLuaScript(srcPath);
                            }
                        }
                        else
                        {
                            GetEditorState()->InspectObject(doc);
                        }
                    }
                }
                else if (stub->mAsset && stub->mAsset->Is(Material::ClassRuntimeId()))
                {
                    Material* mat = stub->mAsset->As<Material>();
                    if (mat->HasNodeGraph())
                    {
                        OpenNodeGraphForEditing(mat->GetNodeGraph(), mat);
                    }
                    else
                    {
                        GetEditorState()->InspectObject(mat);
                    }
                }
                else
                {
                    if (stub->mAsset)
                        GetEditorState()->InspectObject(stub->mAsset);
                }

                EditorUIHookManager::Get()->FireOnAssetOpened(stub->mName.c_str());
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            AssetStub* dragStub = stub;
            ImGui::SetDragDropPayload(DRAGDROP_ASSET, &dragStub, sizeof(AssetStub*));
            ImGui::Text("%s", stub->mName.c_str());
            ImGui::EndDragDropSource();
        }

        if (GetEditorState()->mTrackSelectedAsset &&
            stub == GetEditorState()->mSelectedAssetStub)
        {
            ImGui::SetScrollHereY(0.5f);
            GetEditorState()->mTrackSelectedAsset = false;
            GetEditorState()->mRevealAssetExpandDirs.clear();
        }

        ImGui::PopStyleColor(); // Pop asset color

        // Asset browser item GUI overlay
        {
            ImVec2 rowMin = ImGui::GetItemRectMin();
            ImVec2 rowMax = ImGui::GetItemRectMax();
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                const char* typeName = (stub->mAsset != nullptr) ?
                    stub->mAsset->GetTypeName() : "";
                hookMgr->DrawAssetItemGUI(stub->mName.c_str(), typeName,
                    rowMin.x, rowMin.y, rowMax.x - rowMin.x, rowMax.y - rowMin.y);
            }
        }

        if (isSelectedStub)
        {
            ImGui::PopStyleColor(3);
        }

        if (ImGui::BeginPopupContextItem())
        {
            // Set current directory so context popup operations target this asset's directory
            if (stub->mDirectory)
                GetEditorState()->mTabCurrentDir[GetEditorState()->ActiveTab()] = stub->mDirectory;
            DrawAssetsContextPopup(stub, nullptr);
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    // Draw loose files (non-.oct files)
    static const ImVec4 kLooseFileColor = ImVec4(0.65f, 0.65f, 0.70f, 1.0f);

    auto IsTextFileExtension = [](const char* ext) -> bool
    {
        if (ext == nullptr) return false;
        static const char* textExts[] = {
            ".lua", ".txt", ".json", ".xml", ".md", ".css", ".yaml", ".yml",
            ".ini", ".cfg", ".sh", ".bat", ".html", ".js", ".py", ".c", ".cpp",
            ".h", ".hpp", ".cs", ".glsl", ".hlsl", ".vert", ".frag", ".toml", ".log"
        };
        for (const char* textExt : textExts)
        {
            if (strcasecmp(ext, textExt) == 0) return true;
        }
        return false;
    };

    auto GetLooseFileIcon = [](const char* ext) -> const char*
    {
        if (ext == nullptr) return ICON_STREAMLINE_SHARP_NEW_FILE_REMIX;
        if (strcasecmp(ext, ".lua") == 0) return ICON_LUA;
        if (strcasecmp(ext, ".json") == 0) return ICON_JSON;
        if (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return ICON_TDESIGN_IMAGE_FILLED;
        if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0 || strcasecmp(ext, ".ogg") == 0) return ICON_RIVET_ICONS_AUDIO_SOLID;
        if (strcasecmp(ext, ".css") == 0) return ICON_IX_CODE;
        return ICON_STREAMLINE_SHARP_NEW_FILE_REMIX;
    };

    for (uint32_t i = 0; i < dir->mLooseFiles.size(); ++i)
    {
        const std::string& filename = dir->mLooseFiles[i];

        // {asset}.meta sidecars are paired metadata, never their own asset —
        // never list them in the browser. Discovery already excludes them
        // from mLooseFiles, but this is a defensive filter for any future
        // code path that might push them in.
        const char* metaExt = strrchr(filename.c_str(), '.');
        if (metaExt != nullptr && strcmp(metaExt, ".meta") == 0)
        {
            continue;
        }

        // Filter check
        if (!filterLower.empty())
        {
            std::string nameLower = filename;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos)
                continue;
        }

        std::string fullPath = dir->mPath + filename;
        bool isSelected = (sSelectedLooseFile == fullPath);

        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSelectedColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, kSelectedColor);
        }

        ImGui::PushStyleColor(ImGuiCol_Text, kLooseFileColor);

        const char* ext = strrchr(filename.c_str(), '.');
        std::string displayText = std::string(GetLooseFileIcon(ext)) + "  " + filename;

        ImGui::PushID(fullPath.c_str());
        bool looseClicked = ImGui::Selectable(displayText.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick);
        AlternatingRowBackground();
        if (looseClicked)
        {
            if (sSelectedLooseFile != fullPath)
            {
                sSelectedLooseFile = fullPath;
                GetEditorState()->SetSelectedAssetStub(nullptr);
            }
            else if (!IsControlDown())
            {
                sSelectedLooseFile.clear();
            }

            if (ImGui::IsMouseDoubleClicked(0))
            {
                if (IsTextFileExtension(ext))
                {
                    PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                    EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
                    if (editors)
                    {
                        editors->OpenLuaScript(fullPath);
                    }
                }
                else
                {
                    SYS_OpenFileWithDefaultApp(fullPath);
                }
            }
        }

        ImGui::PopStyleColor(); // kLooseFileColor

        if (isSelected)
        {
            ImGui::PopStyleColor(3);
        }

        if (ImGui::BeginPopupContextItem())
        {
            bool isTextFile = IsTextFileExtension(ext);

            if (isTextFile && ImGui::MenuItem("Edit"))
            {
                PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
                if (editors)
                {
                    editors->OpenLuaScript(fullPath);
                }
            }

            if (ImGui::MenuItem("Open with Default App"))
            {
                SYS_OpenFileWithDefaultApp(fullPath);
            }

            if (ImGui::MenuItem("Show in Explorer"))
            {
                std::string dirPath = dir->mPath;
                SYS_ExplorerOpenDirectory(dirPath);
            }

            // Convert submenu for XML files
            if (ext && strcasecmp(ext, ".xml") == 0)
            {
                if (ImGui::BeginMenu("Convert"))
                {
                    if (ImGui::MenuItem("New UIDocument"))
                    {
                        sConvertXmlSourcePath = fullPath;
                        std::string baseName = Asset::GetNameFromPath(fullPath);
                        if (baseName.length() < 3 || baseName.substr(0, 3) != "UI_")
                            baseName = "UI_" + baseName;
                        strncpy(sPopupInputBuffer, baseName.c_str(), kPopupInputBufferSize - 1);
                        sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    }

                    if (ImGui::MenuItem("Refresh UIDocument"))
                    {
                        sConvertXmlSourcePath = fullPath;
                        sShowUIDocumentPicker = true;
                    }

                    ImGui::EndMenu();
                }
            }

            // Packaging submenu — mirrors DrawAssetPackagingSection in the Properties
            // panel. Loose files can't ride the AssetStub "Properties" path, so wire
            // the same {file}.meta read/write directly into the right-click menu.
            if (ImGui::BeginMenu("Packaging"))
            {
                AssetMetaSidecar meta = AssetManager::LoadAssetMeta(fullPath);
                bool changed = false;

                if (ImGui::MenuItem("Embed in shipped exe", nullptr, meta.mEmbed))
                {
                    meta.mEmbed = !meta.mEmbed;
                    changed = true;
                }

                ImGui::Separator();
                ImGui::TextDisabled("Include in builds:");

                static const struct { const char* label; uint32_t bit; } kPkgRows[] = {
                    { "Windows",  PlatformBit_Windows  },
                    { "Linux",    PlatformBit_Linux    },
                    { "Android",  PlatformBit_Android  },
                    { "GameCube", PlatformBit_GameCube },
                    { "Wii",      PlatformBit_Wii      },
                    { "3DS",      PlatformBit_N3DS     },
                };
                for (const auto& row : kPkgRows)
                {
                    bool on = (meta.mPlatformMask & row.bit) != 0;
                    if (ImGui::MenuItem(row.label, nullptr, on))
                    {
                        if (on) meta.mPlatformMask &= ~row.bit;
                        else    meta.mPlatformMask |=  row.bit;
                        changed = true;
                    }
                }

                if (changed)
                {
                    AssetManager::Get()->ApplyAssetMetaFlags(fullPath, meta.mPlatformMask, meta.mEmbed);
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete"))
            {
                SYS_RemoveFile(fullPath.c_str());
                if (sSelectedLooseFile == fullPath)
                    sSelectedLooseFile.clear();
                dir->mLooseFiles.erase(dir->mLooseFiles.begin() + i);
                --i;
                ImGui::EndPopup();
                ImGui::PopID();
                continue;
            }

            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

static void DrawAssetDirTree(AssetDir* dir, const std::string& filterLower, bool isRoot)
{
    if (dir == nullptr)
        return;

    // When filtering, skip directories that don't match
    if (!filterLower.empty() && !DirMatchesFilter(dir, filterLower))
        return;

    ImGuiTreeNodeFlags dirFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!filterLower.empty())
        dirFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (isRoot)
        dirFlags |= ImGuiTreeNodeFlags_DefaultOpen;

    // Check if directory has no children (leaf directory with no subdirs)
    bool hasChildren = !dir->mChildDirs.empty() || !dir->mAssetStubs.empty() || !dir->mLooseFiles.empty();
    if (!hasChildren)
        dirFlags |= ImGuiTreeNodeFlags_Leaf;

    // Force open if this directory is in the reveal path
    EditorState* es = GetEditorState();
    if (es->mRevealAssetExpandDirs.count(dir) > 0)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    std::string dirLabel = std::string(ICON_MATERIAL_SYMBOLS_FOLDER_SHARP) + " " + dir->mName;
    ImGui::PushID(dir);
    bool nodeOpen = ImGui::TreeNodeEx(dirLabel.c_str(), dirFlags);
    AlternatingRowBackground();

    // Drag source for directory (skip engine/addon dirs)
    if (!dir->mEngineDir && !dir->mAddonDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        AssetDir* dragDir = dir;
        ImGui::SetDragDropPayload(DRAGDROP_DIR, &dragDir, sizeof(AssetDir*));
        ImGui::Text("%s", dir->mName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target for assets and directories
    if (!dir->mEngineDir && !dir->mAddonDir && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
        {
            AssetStub* droppedStub = *(AssetStub**)payload->Data;
            MoveAssetToDirectory(droppedStub, dir);
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_DIR))
        {
            AssetDir* droppedDir = *(AssetDir**)payload->Data;
            MoveDirectoryToDirectory(droppedDir, dir);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem())
    {
        // Set current directory so context popup operations target this directory
        GetEditorState()->mTabCurrentDir[GetEditorState()->ActiveTab()] = dir;
        DrawAssetsContextPopup(nullptr, dir);
        ImGui::EndPopup();
    }

    if (nodeOpen)
    {
        // Recurse into child directories
        for (AssetDir* child : dir->mChildDirs)
        {
            DrawAssetDirTree(child, filterLower, false);
        }

        // Draw assets in this directory
        DrawAssetItems(dir, filterLower);

        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void DrawAssetBrowser(AssetDir* rootDir, const std::string& filterLower, bool showRootChildren)
{
    if (rootDir == nullptr)
        return;

    if (showRootChildren)
    {
        // Show root's children directly (don't wrap in a root tree node)
        for (AssetDir* child : rootDir->mChildDirs)
        {
            DrawAssetDirTree(child, filterLower, true);
        }
        // Draw assets directly in the root directory
        DrawAssetItems(rootDir, filterLower);

        // Provide a drop target for the root itself. Without a visible root
        // tree node, users have no way to drop a folder/asset back to the
        // top-level "Assets" directory (e.g. dragging Assets/Folder1/Folder2
        // up to Assets/Folder2). Reserve the remaining vertical space as an
        // invisible drop zone. Skip when the root is engine/addon-owned —
        // those are read-only and MoveDirectoryToDirectory rejects them.
        if (!rootDir->mEngineDir && !rootDir->mAddonDir)
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            // Always reserve a clickable strip even when the panel is full
            // so users have a drop target to aim at.
            if (avail.y < 32.0f) avail.y = 32.0f;
            ImGui::InvisibleButton("##AssetsRootDropZone", avail);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
                {
                    AssetStub* droppedStub = *(AssetStub**)payload->Data;
                    MoveAssetToDirectory(droppedStub, rootDir);
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_DIR))
                {
                    AssetDir* droppedDir = *(AssetDir**)payload->Data;
                    MoveDirectoryToDirectory(droppedDir, rootDir);
                }
                ImGui::EndDragDropTarget();
            }
        }
    }
    else
    {
        // Show the directory itself as a tree node
        DrawAssetDirTree(rootDir, filterLower, true);
    }

    // If no popup is open and we aren't inputting text...
    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) &&
        ImGui::IsWindowHovered() &&
        !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("Null Context");
        }

        AssetDir* currentDir = GetEditorState()->GetAssetDirectory();
        if (currentDir != nullptr && !currentDir->mAddonDir)
        {
            EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();

            if (hotkeys->IsActionJustTriggered(EditorAction::Asset_CreateScene))
            {
                CreateNewAsset(Scene::GetStaticType(), "SC_Scene");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Asset_CreateMaterial))
            {
                CreateNewAsset(MaterialLite::GetStaticType(), "M_Material");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Asset_CreateParticle))
            {
                CreateNewAsset(ParticleSystem::GetStaticType(), "P_Particle");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Edit_Duplicate))
            {
                AssetStub* srcStub = GetEditorState()->GetSelectedAssetStub();

                if (srcStub != nullptr)
                {
                    sDuplicateAssetStub = srcStub;
                    ImGui::OpenPopup("Duplicate Asset Name");
                    std::string defaultName = srcStub->mName + "_00";
                    strncpy(sPopupInputBuffer, defaultName.c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    sDuplicateAssetFocus = true;
                }
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Edit_DeleteSelected))
            {
                const auto& selectedStubs = GetEditorState()->GetSelectedAssetStubs();
                if (!selectedStubs.empty())
                {
                    // Copy the vector since deletion will modify it
                    std::vector<AssetStub*> toDelete = selectedStubs;
                    for (AssetStub* s : toDelete)
                    {
                        ActionManager::Get()->DeleteAsset(s);
                    }
                }
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Asset_Rename))
            {
                AssetStub* selStub = GetEditorState()->GetSelectedAssetStub();
                if (selStub != nullptr && !selStub->mEngineAsset)
                {
                    ImGui::OpenPopup("Rename Asset F2");
                    strncpy(sPopupInputBuffer, selStub->mName.c_str(), kPopupInputBufferSize - 1);
                    sPopupInputBuffer[kPopupInputBufferSize - 1] = '\0';
                    sF2RenameAssetFocus = true;
                }
            }
        }
    }

    if (ImGui::BeginPopup("Rename Asset F2"))
    {
        if (sF2RenameAssetFocus)
        {
            ImGui::SetKeyboardFocusHere();
            sF2RenameAssetFocus = false;
        }

        if (ImGui::InputText("Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            AssetStub* selStub = GetEditorState()->GetSelectedAssetStub();
            if (selStub)
            {
                Asset* asset = AssetManager::Get()->LoadAsset(*selStub);
                AssetManager::Get()->RenameAsset(asset, sPopupInputBuffer);
                AssetManager::Get()->SaveAsset(*selStub);
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Duplicate Asset Name"))
    {
        if (sDuplicateAssetFocus)
        {
            ImGui::SetKeyboardFocusHere();
            sDuplicateAssetFocus = false;
        }

        if (ImGui::InputText("New Name##DupAssetF2", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (sDuplicateAssetStub != nullptr)
            {
                GetEditorState()->DuplicateAsset(sDuplicateAssetStub, sPopupInputBuffer);
                sDuplicateAssetStub = nullptr;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Null Context"))
    {
        DrawAssetsContextPopup(nullptr, nullptr);
        ImGui::EndPopup();
    }
}

static void DrawAssetsPanel()
{
    const float kAssetPanelIndent = 4.0f;

    // Shared search bar above tab bar
    static char sAssetsSearchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##AssetsSearch", "Search assets...", sAssetsSearchBuffer, sizeof(sAssetsSearchBuffer));
    std::string filterLower;
    if (sAssetsSearchBuffer[0] != '\0')
    {
        filterLower = sAssetsSearchBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
    }

    ImGui::Spacing();

    if (ImGui::BeginTabBar("AssetBrowserTabs"))
    {
        if (ImGui::BeginTabItem("Project"))
        {
            if (GetEditorState()->mActiveAssetTab != AssetBrowserTab::Project)
                GetEditorState()->mActiveAssetTab = AssetBrowserTab::Project;

            // Toolbar
            {
                bool hasProject = !GetEngineState()->mProjectDirectory.empty();
                if (!hasProject) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

                if (ImGui::Button(ICON_BXS_ARROW_TO_BOTTOM "##ImportAsset"))
                {
                    if (hasProject)
                        ActionManager::Get()->ImportAsset();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Import Asset");

                ImGui::SameLine();
                if (ImGui::Button(ICON_MATERIAL_SYMBOLS_FOLDER_OPEN_SHARP "##NewFolder"))
                {
                    if (hasProject)
                        ImGui::OpenPopup("NewFolderPopup");
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("New Folder");

                ImGui::SameLine();
                if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE "##OpenProjectExplorer"))
                {
                    if (hasProject)
                    {
                        AssetDir* projDir = AssetManager::Get()->FindProjectDirectory();
                        if (projDir)
                            SYS_ExplorerOpenDirectory(projDir->mPath);
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Open in Explorer");

                ImGui::SameLine();
                if (ImGui::Button(ICON_UNDO "##RefreshAssets"))
                {
                    if (hasProject)
                    {
                        AssetDir* projDir = AssetManager::Get()->FindProjectDirectory();
                        if (projDir)
                            AssetManager::Get()->RefreshDirectory(projDir);
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Refresh");

                if (!hasProject) ImGui::PopStyleVar();

                // New Folder popup
                static char sNewFolderBuffer[256] = "";
                static bool sNewFolderFocus = false;
                if (ImGui::IsPopupOpen("NewFolderPopup"))
                    sNewFolderFocus = true;
                if (ImGui::BeginPopup("NewFolderPopup"))
                {
                    if (sNewFolderFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                        sNewFolderFocus = false;
                    }
                    if (ImGui::InputText("Folder Name", sNewFolderBuffer, sizeof(sNewFolderBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        std::string folderName = sNewFolderBuffer;
                        if (!folderName.empty())
                        {
                            AssetDir* curDir = GetEditorState()->GetAssetDirectory();
                            if (curDir == nullptr)
                                curDir = AssetManager::Get()->FindProjectDirectory();
                            if (curDir)
                            {
                                if (SYS_CreateDirectory((curDir->mPath + folderName).c_str()))
                                    curDir->CreateSubdirectory(folderName);
                                else
                                    LogError("Failed to create folder");
                            }
                        }
                        sNewFolderBuffer[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                ImGui::Spacing();
            }

            BeginAlternatingRows();
            ImGui::Indent(kAssetPanelIndent);
            AssetDir* projectDir = AssetManager::Get()->FindProjectDirectory();
            // Initialize current dir to project root if not set
            if (GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Project] == nullptr && projectDir != nullptr)
                GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Project] = projectDir;
            DrawAssetBrowser(projectDir, filterLower, true);
            ImGui::Unindent(kAssetPanelIndent);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Addons"))
        {
            if (GetEditorState()->mActiveAssetTab != AssetBrowserTab::Addons)
            {
                GetEditorState()->mActiveAssetTab = AssetBrowserTab::Addons;
                if (GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Addons] == nullptr)
                    GetEditorState()->mTabCurrentDir[(int)AssetBrowserTab::Addons] = AssetManager::Get()->FindPackagesDirectory();
            }

            // Toolbar
            {
                if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE "##OpenAddonsExplorer"))
                {
                    AssetDir* pkgDir = AssetManager::Get()->FindPackagesDirectory();
                    if (pkgDir)
                        SYS_ExplorerOpenDirectory(pkgDir->mPath);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Open in Explorer");

                ImGui::SameLine();
                if (ImGui::Button(ICON_UNDO "##RefreshAddons"))
                {
                    AssetDir* pkgDir = AssetManager::Get()->FindPackagesDirectory();
                    if (pkgDir)
                        AssetManager::Get()->RefreshDirectory(pkgDir);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Refresh");

                ImGui::Spacing();
            }

            BeginAlternatingRows();
            ImGui::Indent(kAssetPanelIndent);
            AssetDir* packagesDir = AssetManager::Get()->FindPackagesDirectory();
            DrawAssetBrowser(packagesDir, filterLower, true);
            ImGui::Unindent(kAssetPanelIndent);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void DrawMaterialShaderParams(Material* mat)
{
    std::vector<ShaderParameter>& params = mat->GetParameters();

    if (params.size() == 0)
        return;

    bool catOpen = ImGui::CollapsingHeader("Shader Parameters", ImGuiTreeNodeFlags_DefaultOpen);

    if (!catOpen)
        return;

    ImGui::PushID("ShaderParams");

    for (uint32_t i = 0; i < params.size(); ++i)
    {
        ShaderParameter& param = params[i];

        // This is a hidden parameter needed for ensuring that the material descriptor set is compiled.
        if (param.mName == "ForceMaterialSet")
        {
            continue;
        }

        ImGui::Text(param.mName.c_str());

        ImGui::PushID(i);

        if (param.mType == ShaderParameterType::Scalar)
        {
            ImGui::DragFloat("", &param.mFloatValue[0]);
        }
        else if (param.mType == ShaderParameterType::Vector)
        {
            ImGui::OctDragScalarN("", ImGuiDataType_Float, &param.mFloatValue[0], 3, 1.0f, nullptr, nullptr, "%.2f", 0);
        }
        else if (param.mType == ShaderParameterType::Texture)
        {
            Property prop(DatumType::Asset, "ShaderParam", nullptr, &param.mTextureValue, 1, nullptr, (int32_t)Texture::GetStaticType());
            DrawAssetProperty(prop, 0, nullptr, PropertyOwnerType::Count);
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

static void DrawInstancedMeshExtra(InstancedMesh3D* instMesh)
{
    static int32_t sActiveInstance = 0;

    int32_t selInstance = GetEditorState()->GetSelectedInstance();
    if (selInstance != -1)
    {
        sActiveInstance = selInstance;
    }

    if (ImGui::CollapsingHeader("Instance Data", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID(0);

        int32_t numInstances = (int32_t)instMesh->GetNumInstances();
        char instCountStr[32];
        snprintf(instCountStr, 32, "Instances: %d", numInstances);
        ImGui::Text(instCountStr);

        if (ImGui::Button("-"))
        {
            if (instMesh->GetNumInstances() > 0)
            {
                sActiveInstance = glm::clamp<int32_t>(sActiveInstance, 0, numInstances - 1);
                std::vector<MeshInstanceData> newInstData = instMesh->GetInstanceData();
                newInstData.erase(newInstData.begin() + sActiveInstance);
                ActionManager::Get()->EXE_SetInstanceData(instMesh, -1, newInstData);

                sActiveInstance = int32_t(instMesh->GetNumInstances()) - 1;
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("+"))
        {
            // Not efficient, but lets use the same EXE_SetInstanceData() action
            std::vector<MeshInstanceData> newInstData = instMesh->GetInstanceData();
            newInstData.push_back(MeshInstanceData());
            ActionManager::Get()->EXE_SetInstanceData(instMesh, -1, newInstData);

            sActiveInstance = int32_t(instMesh->GetNumInstances()) - 1;
        }

        numInstances = (int32_t)instMesh->GetNumInstances();

        if (numInstances > 0)
        {
            sActiveInstance = glm::clamp<int32_t>(sActiveInstance, 0, numInstances - 1);
            if (ImGui::SliderInt("Active Instance", &sActiveInstance, 0, numInstances - 1))
            {
                if (selInstance != -1)
                {
                    GetEditorState()->SetSelectedInstance(sActiveInstance);
                }
            }
        }

        if (sActiveInstance >= 0 &&
            sActiveInstance < numInstances)
        {
            MeshInstanceData instData = instMesh->GetInstanceData(sActiveInstance);
            bool positionChanged = false;
            bool rotationChanged = false;
            bool scaleChanged = false;

            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.85f);
            //ImGui::DragFloat3("", &propVal[0], 1.0f, 0.0f, 0.0f, "%.2f");
            float vMin = 0.0f;
            float vMax = 0.0f;

            static MeshInstanceData sOrigVal;
            MeshInstanceData preVal = instData;
            bool itemActivated = false;
            bool itemDeactivated = false;
            
            ImGui::Text("Position");
            positionChanged = ImGui::OctDragScalarN("Position", ImGuiDataType_Float, &instData.mPosition, 3, 1.0f, &vMin, &vMax, "%.2f", 0);
            itemActivated = itemActivated || ImGui::IsItemActivated();
            itemDeactivated = itemDeactivated || ImGui::IsItemDeactivatedAfterEdit();

            ImGui::Text("Rotation");
            rotationChanged = ImGui::OctDragScalarN("Rotation", ImGuiDataType_Float, &instData.mRotation, 3, 1.0f, &vMin, &vMax, "%.2f", 0);
            itemActivated = itemActivated || ImGui::IsItemActivated();
            itemDeactivated = itemDeactivated || ImGui::IsItemDeactivatedAfterEdit();

            ImGui::Text("Scale");
            scaleChanged = ImGui::OctDragScalarN("Scale", ImGuiDataType_Float, &instData.mScale, 3, 1.0f, &vMin, &vMax, "%.2f", 0);
            itemActivated = itemActivated || ImGui::IsItemActivated();
            itemDeactivated = itemDeactivated || ImGui::IsItemDeactivatedAfterEdit();

            if (itemActivated)
            {
                sOrigVal = preVal;
            }

            if (itemDeactivated && (memcmp(&sOrigVal, &instData, sizeof(MeshInstanceData)) != 0))
            {
                instMesh->SetInstanceData(sActiveInstance, sOrigVal);
                ActionManager::Get()->EXE_SetInstanceData(instMesh, sActiveInstance, { instData });
            }
            else if (positionChanged ||
                rotationChanged ||
                scaleChanged)
            {
                instMesh->SetInstanceData(sActiveInstance, instData);
            }

            ImGui::PopItemWidth();
        }

        ImGui::PopID();
    }
}

// Renders the per-asset Packaging section (platform-mask checkboxes + embed
// flag) sourced from {assetPath}.meta. Live-saves on toggle and updates the
// in-memory AssetStub flags so the change takes effect immediately without
// re-running discovery.
//
// `assetAbsolutePath` is the path passed to LoadAssetMeta — it should match
// AssetStub::mPath for cooked .oct assets, or the full disk path for raw files.
// `stub` may be nullptr for raw files (which have no AssetStub).
static void DrawAssetPackagingSection(const std::string& assetAbsolutePath, AssetStub* stub)
{
    if (assetAbsolutePath.empty())
        return;

    // Cache the .meta read keyed on path. Re-read whenever selection changes;
    // re-read after we save so authoritative state == on-disk.
    static std::string sCachedPath;
    static AssetMetaSidecar sCachedMeta;
    if (sCachedPath != assetAbsolutePath)
    {
        sCachedPath = assetAbsolutePath;
        sCachedMeta = AssetManager::LoadAssetMeta(assetAbsolutePath);
    }

    if (!ImGui::CollapsingHeader("Packaging", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // 6 platforms = engine's Platform enum count. Layout in two rows of three.
    struct PlatformRow { const char* label; uint32_t bit; };
    static const PlatformRow kRows[] = {
        { "Windows",  PlatformBit_Windows  },
        { "Linux",    PlatformBit_Linux    },
        { "Android",  PlatformBit_Android  },
        { "GameCube", PlatformBit_GameCube },
        { "Wii",      PlatformBit_Wii      },
        { "3DS",      PlatformBit_N3DS     }, // user-facing label; serialised as "N3DS"
    };

    bool changed = false;
    uint32_t mask = sCachedMeta.mPlatformMask;
    bool embed   = sCachedMeta.mEmbed;

    ImGui::Text("Include in builds:");
    ImGui::Indent();
    for (int i = 0; i < (int)(sizeof(kRows)/sizeof(kRows[0])); ++i)
    {
        if ((i % 3) != 0)
            ImGui::SameLine();

        bool on = (mask & kRows[i].bit) != 0;
        ImGui::PushID(i);
        if (Polyphase::Checkbox(kRows[i].label, &on))
        {
            if (on) mask |=  kRows[i].bit;
            else    mask &= ~kRows[i].bit;
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::Unindent();

    if (Polyphase::Checkbox("Embed in shipped exe", &embed))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Bake this file into the exe via gEmbeddedRawAssets[].\n"
            "Recommended only for files smaller than ~10 MB; the hex-byte\n"
            "format used by the embed pipeline blows up compile times for\n"
            "large files.");
    }

    if (changed)
    {
        // ApplyAssetMetaFlags writes the .meta file AND updates any matching
        // in-memory AssetStub / RawAssetEntry, so a packaging build run right
        // after the toggle picks up the new flags without re-discovery.
        AssetManager::Get()->ApplyAssetMetaFlags(assetAbsolutePath, mask, embed);
        sCachedMeta.mPlatformMask = mask;
        sCachedMeta.mEmbed        = embed;
        sCachedMeta.mExists       = (mask != PlatformBit_All) || embed;
        (void)stub;
    }
}

static void DrawPropertiesPanel()
{
    sObjectTabOpen = false;

    if (ImGui::BeginTabBar("PropertyModeTabs"))
    {
        if (ImGui::BeginTabItem(ICON_FLUENT_MDL2_CUBE_SHAPE "  Object"))
        {
            sObjectTabOpen = true;
            BeginAlternatingRows();
            Object* obj = GetEditorState()->GetInspectedObject();

            if (obj != nullptr)
            {
                bool inspectLocked = GetEditorState()->IsInspectLocked();

                if (obj->As<Asset>())
                {
                    Asset* asset = obj->As<Asset>();

                    if (ImGui::Button("<<"))
                    {
                        GetEditorState()->BrowseToAsset(asset->GetName());
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Save"))
                    {
                        AssetManager::Get()->SaveAsset(asset->GetName());
                    }

                    if (asset->As<UIDocument>())
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Edit"))
                        {
                            UIDocument* doc = asset->As<UIDocument>();
                            const std::string& srcPath = doc->GetSourceFilePath();
                            if (!srcPath.empty() && SYS_DoesFileExist(srcPath.c_str(), false))
                            {
                                PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                                EditorsModule* editors = mod ? static_cast<EditorsModule*>(mod) : nullptr;
                                if (editors)
                                    editors->OpenLuaScript(srcPath);
                            }
                        }
                    }

                    if (asset->GetDirtyFlag())
                    {
                        ImGui::SameLine();
                        ImGui::Text("*");
                    }
                }

                // Display object name (e.g. StaticMesh3D)
                std::string objName = obj->RuntimeName();
                Node* objNode = obj->As<Node>();
                if (objNode)
                    objName = std::string(GetNodeIcon(objNode)) + " " + objName;

                // Calculate width: leave room for lock button on the right
                const char* lockIcon = inspectLocked
                    ? ICON_MATERIAL_SYMBOLS_LOCK_SHARP
                    : ICON_MATERIAL_SYMBOLS_LOCK_OPEN_SHARP;
                float lockButtonWidth = ImGui::CalcTextSize(lockIcon).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
                ImVec2 sz = ImVec2(ImGui::GetContentRegionAvail().x - lockButtonWidth, 0.0f);

                ImVec4 headerColor = kSelectedColor;
                headerColor.w = 0.0f;
                ImVec4 headerTextColor = ImVec4(0.18f, 0.75f, 0.70f, 1.00f);
                ImGui::PushStyleColor(ImGuiCol_Text, headerTextColor);
                ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerColor);
                ImGui::Button(objName.c_str(), sz);
                ImGui::PopStyleColor(4);

                // Lock/Unlock inspector button (right side)
                ImGui::SameLine();
                if (ImGui::Button(lockIcon))
                {
                    GetEditorState()->LockInspect(!inspectLocked);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(inspectLocked ? "Unlock Inspector (L)" : "Lock Inspector (L)");
                }

                Texture* texObj = obj->As<Texture>();
                if (texObj != nullptr &&
                    texObj->GetResource()->mImage != nullptr)
                {
                    // Dealloc prev tex descriptor
                    if (sPrevInspectTexture != texObj)
                    {
                        DeviceWaitIdle();

                        if (sInspectTexId != 0)
                        {
                            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sInspectTexId);
                            sInspectTexId = 0;
                        }

                        sInspectTexId = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
                            texObj->GetResource()->mImage->GetSampler(),
                            texObj->GetResource()->mImage->GetView(),
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                        sPrevInspectTexture = texObj;
                    }

                    if (sInspectTexId != 0)
                    {
                        ImGui::Image(sInspectTexId, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1,1,1,1), ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
                    }

                    ImGui::Text("%d x %d", texObj->GetWidth(), texObj->GetHeight());
                    ImGui::NewLine();
                }

                // Per-asset Packaging section ({asset}.meta sidecar editor) —
                // appears above the regular property rows so it's discoverable
                // when the user is configuring an asset for shipping.
                if (Asset* assetForMeta = obj->As<Asset>())
                {
                    AssetStub* stubForMeta = AssetManager::Get()->GetAssetStub(assetForMeta->GetName());
                    if (stubForMeta != nullptr && !stubForMeta->mPath.empty())
                    {
                        DrawAssetPackagingSection(stubForMeta->mPath, stubForMeta);
                    }
                }

                std::vector<Property> props;
                obj->GatherProperties(props);

                DrawPropertyList(obj, props);

                // Custom imgui drawing
                if (obj->As<Material>())
                {
                    Material* mat = obj->As<Material>();
                    DrawMaterialShaderParams(mat);
                }
                else if (obj->As<InstancedMesh3D>())
                {
                    InstancedMesh3D* instMesh = obj->As<InstancedMesh3D>();
                    DrawInstancedMeshExtra(instMesh);
                }
                else if (obj->As<StaticMesh>())
                {
                    StaticMesh* staticMesh = obj->As<StaticMesh>();
                    bool hasVertColor = staticMesh->HasVertexColor();
                    ImGui::Text("Vertex Color: %s", hasVertColor ? "Yes" : "No");
                }
                else if (obj->As<SkeletalMesh>())
                {
                    SkeletalMesh* skelMesh = obj->As<SkeletalMesh>();
                    ImGui::Text("Animations");
                    ImGui::Indent();
                    const std::vector<Animation>& animations = skelMesh->GetAnimations();
                    for (uint32_t a = 0; a < animations.size(); ++a)
                    {
                        ImGui::Text(animations[a].mName.c_str());
                    }
                    ImGui::Unindent();

                    ImGui::NewLine();
                    if (ImGui::Button("View Animations"))
                    {
                        GetAnimationBrowser()->Open(skelMesh);
                    }
                }
                else if (obj->As<SoundWave>())
                {
                    SoundWave* soundWave = obj->As<SoundWave>();
                    ImGui::Text("Num Channels: %d", soundWave->GetNumChannels());
                    ImGui::Text("Bits Per Sample: %d", soundWave->GetBitsPerSample());
                    ImGui::Text("Sample Rate: %d", soundWave->GetSampleRate());
                }
                else if (obj->As<Voxel3D>())
                {
                    if (GetEditorState()->mPaintMode != PaintMode::Voxel)
                    {
                        ImGui::NewLine();
                        if (ImGui::Button("Open In Voxel Sculpt"))
                        {
                            GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                            GetEditorState()->SetPaintMode(PaintMode::Voxel);
                        }
                    }
                }
                else if (obj->As<Terrain3D>())
                {
                    if (GetEditorState()->mPaintMode != PaintMode::Terrain)
                    {
                        ImGui::NewLine();
                        if (ImGui::Button("Open In Terrain Sculpt"))
                        {
                            GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                            GetEditorState()->SetPaintMode(PaintMode::Terrain);
                        }
                    }
                }
                else if (obj->As<TileMap2D>())
                {
                    if (GetEditorState()->mPaintMode != PaintMode::TilePaint)
                    {
                        ImGui::NewLine();
                        if (ImGui::Button("Open In Tile Paint"))
                        {
                            GetEditorState()->SetEditorMode(EditorMode::Scene3D);
                            GetEditorState()->SetPaintMode(PaintMode::TilePaint);
                        }
                    }
                }
            }
            else if (!sSelectedLooseFile.empty())
            {
                // Raw-file inspector view. Loose files (.mp4, .json, .png, …)
                // aren't Object-derived so they don't go through GatherProperties,
                // but they DO carry a {file}.meta sidecar that drives their
                // packaging behaviour. Show a read-only header + Packaging
                // section so devs can configure platform-mask + embed without
                // having to edit JSON by hand.
                const std::string& filePath = sSelectedLooseFile;
                size_t lastSlash = filePath.find_last_of("/\\");
                std::string fileName = (lastSlash == std::string::npos)
                    ? filePath
                    : filePath.substr(lastSlash + 1);

                ImGui::Text("Raw file: %s", fileName.c_str());
                ImGui::TextDisabled("%s", filePath.c_str());
                ImGui::Separator();

                DrawAssetPackagingSection(filePath, /*stub=*/nullptr);
            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_SCENE "  Scene"))
        {
            BeginAlternatingRows();
            EditScene* editScene = GetEditorState()->GetEditScene();
            Scene* scene = editScene ? editScene->mSceneAsset.Get<Scene>() : nullptr;

            if (scene != nullptr)
            {
                std::vector<Property> sceneProps;
                scene->GatherProperties(sceneProps);

                DrawPropertyList(scene, sceneProps);
            }

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_GLOBE "  Global"))
        {
            BeginAlternatingRows();
            std::vector<Property> globalProps;
            GatherGlobalProperties(globalProps);

            DrawPropertyList(nullptr, globalProps);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) &&
        ImGui::IsWindowHovered() &&
        !ImGui::GetIO().WantTextInput)
    {
        // Hotkey for toggling lock.
        if (EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::Inspector_ToggleLock))
        {
            GetEditorState()->LockInspect(!GetEditorState()->IsInspectLocked());
        }

        // Navigate inspection history.
        if (IsMouseButtonJustDown(MOUSE_X1))
        {
            GetEditorState()->RegressInspectPast();
        }
        else if (IsMouseButtonJustDown(MOUSE_X2))
        {
            GetEditorState()->ProgressInspectFuture();
        }
    }

}

// ===== Scripts Panel =====

struct ScriptFileEntry
{
    std::string mDisplayName; // relative name for display (e.g. "MyDir/Script.lua")
    std::string mFullPath;   // full path for opening
    std::string mOrigin;     // "Engine", "Project", or package name
};

struct CppAddonEntry
{
    std::string mAddonId;
    std::string mAddonName;
    std::string mSourcePath;
    std::string mVcxprojPath;
    std::vector<std::string> mSourceFiles; // full paths
};

static void GatherCppSourceFiles(const std::string& dir, std::vector<std::string>& outFiles)
{
    DirEntry dirEntry;
    SYS_OpenDirectory(dir, dirEntry);
    if (!dirEntry.mValid)
        return;

    std::vector<std::string> subDirs;
    while (dirEntry.mValid)
    {
        if (strcmp(dirEntry.mFilename, ".") != 0 && strcmp(dirEntry.mFilename, "..") != 0)
        {
            std::string path = dir + dirEntry.mFilename;
            if (dirEntry.mDirectory)
            {
                subDirs.push_back(path + "/");
            }
            else
            {
                const char* ext = strrchr(dirEntry.mFilename, '.');
                if (ext != nullptr &&
                    (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".c") == 0 ||
                     strcmp(ext, ".h") == 0 || strcmp(ext, ".hpp") == 0))
                {
                    outFiles.push_back(path);
                }
            }
        }
        SYS_IterateDirectory(dirEntry);
    }
    SYS_CloseDirectory(dirEntry);

    for (const std::string& sub : subDirs)
    {
        GatherCppSourceFiles(sub, outFiles);
    }
}

static void DrawScriptsPanel()
{
    // --- Cached Lua scripts ---
    static std::vector<ScriptFileEntry> sLuaScripts;
    static double sLuaLastUpdate = 0.0;

    // --- Cached C++ addons ---
    static std::vector<CppAddonEntry> sCppAddons;
    static double sCppLastUpdate = 0.0;

    // --- Search filter ---
    static char sSearchBuffer[256] = "";

    // --- Script reveal state ---
    static std::set<std::string> sRevealExpandPaths;
    static std::string sSelectedScript = "";

    double currentTime = ImGui::GetTime();

    // Check for pending script reveal and build expand paths
    EditorState* es = GetEditorState();
    if (!es->mRevealScriptName.empty())
    {
        sRevealExpandPaths.clear();
        sSelectedScript = es->mRevealScriptName;  // Set selection to revealed script
        // Will be populated when we find the script in the tree build loop below
    }

    // Search bar at top
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##ScriptsSearch", "Search scripts...", sSearchBuffer, sizeof(sSearchBuffer));
    std::string filterLower;
    if (sSearchBuffer[0] != '\0')
    {
        filterLower = sSearchBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
    }

    ImGui::Spacing();

    if (ImGui::BeginTabBar("ScriptBrowserTabs"))
    {
        // ===== Lua Scripts Tab =====
        if (ImGui::BeginTabItem("Lua Scripts"))
        {
            // Toolbar buttons
            {
                bool hasProject = !GetEngineState()->mProjectDirectory.empty();
                PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                EditorsModule* editors = static_cast<EditorsModule*>(mod);

                if (!hasProject) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::Button(ICON_STREAMLINE_SHARP_NEW_FILE_REMIX "##NewScript"))
                {
                    if (hasProject)
                        OpenCreateScriptDialog();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("New Script");

                ImGui::SameLine();
                bool editorConfigured = editors && editors->IsLuaEditorConfigured();
                if (!editorConfigured) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::Button(ICON_IX_CODE "##OpenLuaEditor"))
                {
                    if (hasProject && editorConfigured) {
                        editors->OpenLuaScript(GetEngineState()->mProjectDirectory);
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    if (editorConfigured)
                    {
                        std::string editorName = editors->mLuaEditorPath;
                        size_t lastSlash = editorName.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                            editorName = editorName.substr(lastSlash + 1);
                        ImGui::SetTooltip("Open in %s", editorName.c_str());
                    }
                    else
                    {
                        ImGui::SetTooltip("Open in External Editor (not configured)");
                    }
                }
                if (!editorConfigured) ImGui::PopStyleVar();

                ImGui::SameLine();
                if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE "##OpenLuaExplorer"))
                {
                    if (hasProject)
                    {
                        std::string scriptsDir = GetEngineState()->mProjectDirectory + "Scripts/";
                        SYS_CreateDirectory(scriptsDir.c_str());
#if PLATFORM_WINDOWS
                        SYS_Exec(("start \"\" \"" + scriptsDir + "\"").c_str());
#elif PLATFORM_LINUX
                        SYS_Exec(("xdg-open \"" + scriptsDir + "\" &").c_str());
#endif
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Open in Explorer");

                ImGui::SameLine();
                if (ImGui::Button(ICON_UNDO "##RefreshLuaScripts"))
                {
                    sLuaLastUpdate = 0.0;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Refresh");

                if (!hasProject) ImGui::PopStyleVar();

                ImGui::Spacing();
            }

            // Cache the engine root directory on first use so relative paths survive cwd changes
            static std::string sEngineRoot;
            if (sEngineRoot.empty())
            {
                sEngineRoot = AssetManager::Get()->GetPolyphaseDirectory();
            }

            // Refresh every 2 seconds
            static bool sLuaInitialized = false;
            if (!sLuaInitialized || currentTime - sLuaLastUpdate > 2.0)
            {
                std::vector<ScriptFileEntry> newScripts;
                const std::string& projectDir = GetEngineState()->mProjectDirectory;

                // Engine scripts (use cached absolute path)
                {
                    std::string engineScriptsDir = sEngineRoot + "Engine/Scripts/";
                    std::vector<std::string> files;
                    AssetManager::Get()->GatherScriptFiles(engineScriptsDir, files);
                    for (const std::string& f : files)
                    {
                        ScriptFileEntry entry;
                        entry.mFullPath = f;
                        entry.mOrigin = "Engine";
                        entry.mDisplayName = (f.length() > engineScriptsDir.length() && f.substr(0, engineScriptsDir.length()) == engineScriptsDir)
                            ? f.substr(engineScriptsDir.length()) : f;
                        newScripts.push_back(entry);
                    }
                }

                // Project scripts
                if (!projectDir.empty())
                {
                    std::string projScriptsDir = projectDir + "Scripts/";
                    std::vector<std::string> files;
                    AssetManager::Get()->GatherScriptFiles(projScriptsDir, files);
                    for (const std::string& f : files)
                    {
                        ScriptFileEntry entry;
                        entry.mFullPath = f;
                        entry.mOrigin = "Project";
                        entry.mDisplayName = (f.length() > projScriptsDir.length() && f.substr(0, projScriptsDir.length()) == projScriptsDir)
                            ? f.substr(projScriptsDir.length()) : f;
                        newScripts.push_back(entry);
                    }

                    // Package scripts
                    std::string packagesDir = projectDir + "Packages/";
                    DirEntry pkgDirEntry;
                    SYS_OpenDirectory(packagesDir, pkgDirEntry);
                    while (pkgDirEntry.mValid)
                    {
                        if (pkgDirEntry.mDirectory &&
                            strcmp(pkgDirEntry.mFilename, ".") != 0 &&
                            strcmp(pkgDirEntry.mFilename, "..") != 0)
                        {
                            std::string pkgName = pkgDirEntry.mFilename;
                            std::string pkgScriptsDir = packagesDir + pkgName + "/Scripts/";
                            std::vector<std::string> pkgFiles;
                            AssetManager::Get()->GatherScriptFiles(pkgScriptsDir, pkgFiles);
                            for (const std::string& f : pkgFiles)
                            {
                                ScriptFileEntry entry;
                                entry.mFullPath = f;
                                entry.mOrigin = pkgName;
                                entry.mDisplayName = (f.length() > pkgScriptsDir.length() && f.substr(0, pkgScriptsDir.length()) == pkgScriptsDir)
                                    ? "Packages/" + pkgName + "/" + f.substr(pkgScriptsDir.length()) : f;
                                newScripts.push_back(entry);
                            }
                        }
                        SYS_IterateDirectory(pkgDirEntry);
                    }
                    SYS_CloseDirectory(pkgDirEntry);
                }

                sLuaScripts = std::move(newScripts);
                sLuaLastUpdate = currentTime;
                sLuaInitialized = true;
            }

            // Build directory tree
            // Map: directory path -> list of entries
            struct TreeNode
            {
                std::string name;
                std::map<std::string, TreeNode> children;
                std::vector<const ScriptFileEntry*> files;
            };

            TreeNode root;
            bool hasFilter = !filterLower.empty();

            for (const ScriptFileEntry& entry : sLuaScripts)
            {
                // Filter check
                if (hasFilter)
                {
                    std::string nameLower = entry.mDisplayName;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(filterLower) == std::string::npos)
                        continue;
                }

                // Build reveal expand paths if this is the target script
                bool isRevealTarget = (!es->mRevealScriptName.empty() && entry.mDisplayName == es->mRevealScriptName);
                if (isRevealTarget)
                {
                    // Add origin as first path to expand
                    sRevealExpandPaths.insert(entry.mOrigin);

                    // Add each directory level to expand
                    std::string expandPath = entry.mOrigin;
                    std::string displayPath = entry.mDisplayName;
                    size_t p = 0;
                    size_t s;
                    while ((s = displayPath.find('/', p)) != std::string::npos)
                    {
                        std::string d = displayPath.substr(p, s - p);
                        if (!d.empty())
                        {
                            expandPath += "/" + d;
                            sRevealExpandPaths.insert(expandPath);
                        }
                        p = s + 1;
                    }
                }

                // Group by origin (Engine, Project, package name) at the top level
                TreeNode* current = &root.children[entry.mOrigin];
                current->name = entry.mOrigin;

                // Split display path into subdirectory components
                std::string path = entry.mDisplayName;
                size_t pos = 0;
                size_t slash;
                while ((slash = path.find('/', pos)) != std::string::npos)
                {
                    std::string dir = path.substr(pos, slash - pos);
                    if (!dir.empty())
                    {
                        current = &current->children[dir];
                        current->name = dir;
                    }
                    pos = slash + 1;
                }
                // Remaining part is the file name
                current->files.push_back(&entry);
            }

            // Recursive draw function
            std::function<void(const TreeNode&, const std::string&)> drawTree;
            drawTree = [&](const TreeNode& node, const std::string& currentPath)
            {
                // Draw subdirectories
                for (auto& pair : node.children)
                {
                    std::string childPath = currentPath.empty() ? pair.first : (currentPath + "/" + pair.first);

                    ImGuiTreeNodeFlags dirFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (hasFilter)
                        dirFlags |= ImGuiTreeNodeFlags_DefaultOpen;

                    // Force open if this directory is in the reveal path
                    if (sRevealExpandPaths.count(childPath) > 0)
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    }

                    std::string dirLabel = std::string(ICON_MATERIAL_SYMBOLS_FOLDER_SHARP) + " " + pair.first;
                    bool dirOpen = ImGui::TreeNodeEx(dirLabel.c_str(), dirFlags);
                    AlternatingRowBackground();

                    // Context menu for script directory
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::Selectable("Reveal in Explorer"))
                        {
                            // Find the first file in this subtree to derive the directory path
                            std::function<const ScriptFileEntry*(const TreeNode&)> findFirst;
                            findFirst = [&](const TreeNode& n) -> const ScriptFileEntry* {
                                if (!n.files.empty()) return n.files[0];
                                for (auto& child : n.children) {
                                    const ScriptFileEntry* f = findFirst(child.second);
                                    if (f) return f;
                                }
                                return nullptr;
                            };
                            const ScriptFileEntry* firstFile = findFirst(pair.second);
                            if (firstFile)
                            {
                                std::string dirPath = firstFile->mFullPath;
                                size_t lastSlash = dirPath.find_last_of("/\\");
                                if (lastSlash != std::string::npos)
                                    dirPath = dirPath.substr(0, lastSlash);
                                std::string absPath = SYS_GetAbsolutePath(dirPath);
#if PLATFORM_WINDOWS
                                for (char& c : absPath) { if (c == '/') c = '\\'; }
                                SYS_Exec(("explorer \"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
                                SYS_Exec(("xdg-open \"" + absPath + "\" &").c_str());
#endif
                            }
                        }
                        if (ImGui::Selectable("Copy Path"))
                        {
                            // Copy the directory path (relative to Scripts folder)
                            // Find the first file and extract the directory portion of its display name
                            std::function<const ScriptFileEntry*(const TreeNode&)> findFirst;
                            findFirst = [&](const TreeNode& n) -> const ScriptFileEntry* {
                                if (!n.files.empty()) return n.files[0];
                                for (auto& child : n.children) {
                                    const ScriptFileEntry* f = findFirst(child.second);
                                    if (f) return f;
                                }
                                return nullptr;
                            };
                            const ScriptFileEntry* firstFile = findFirst(pair.second);
                            if (firstFile)
                            {
                                std::string dirPath = firstFile->mDisplayName;
                                size_t lastSlash = dirPath.find_last_of('/');
                                if (lastSlash != std::string::npos)
                                    dirPath = dirPath.substr(0, lastSlash);
                                ImGui::SetClipboardText(dirPath.c_str());
                            }
                        }
                        ImGui::EndPopup();
                    }

                    if (dirOpen)
                    {
                        drawTree(pair.second, childPath);
                        ImGui::TreePop();
                    }
                }

                // Draw files
                for (const ScriptFileEntry* entry : node.files)
                {
                    // Extract just the filename
                    std::string fileName = entry->mDisplayName;
                    size_t lastSlash = fileName.find_last_of('/');
                    if (lastSlash != std::string::npos)
                        fileName = fileName.substr(lastSlash + 1);

                    // Check if this script is selected
                    bool isSelected = (entry->mDisplayName == sSelectedScript);
                    bool isRevealTarget = (!es->mRevealScriptName.empty() && entry->mDisplayName == es->mRevealScriptName);

                    ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (isSelected)
                        leafFlags |= ImGuiTreeNodeFlags_Selected;

                    std::string labelWithIcon = std::string(ICON_LUA) + " " + fileName;
                    ImGui::TreeNodeEx(labelWithIcon.c_str(), leafFlags);
                    AlternatingRowBackground();

                    // Handle click to select
                    if (ImGui::IsItemClicked(0))
                    {
                        sSelectedScript = entry->mDisplayName;
                    }

                    // Scroll to revealed script and clear reveal state
                    if (isRevealTarget)
                    {
                        ImGui::SetScrollHereY(0.5f);
                        es->mRevealScriptName.clear();
                        sRevealExpandPaths.clear();
                    }

                    // Drag source for script files
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::SetDragDropPayload(DRAGDROP_SCRIPT, entry->mDisplayName.c_str(), entry->mDisplayName.size() + 1);
                        ImGui::Text("%s", fileName.c_str());
                        ImGui::EndDragDropSource();
                    }

                    // Context menu for script file
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::Selectable("Reveal in Explorer"))
                        {
                            std::string absPath = SYS_GetAbsolutePath(entry->mFullPath);
#if PLATFORM_WINDOWS
                            for (char& c : absPath) { if (c == '/') c = '\\'; }
                            SYS_Exec(("explorer /select,\"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
                            std::string dirPath = absPath.substr(0, absPath.find_last_of('/'));
                            SYS_Exec(("xdg-open \"" + dirPath + "\" &").c_str());
#endif
                        }
                        if (ImGui::Selectable("Copy Path"))
                        {
                            // Copy the script reference path (relative to Scripts folder)
                            ImGui::SetClipboardText(entry->mDisplayName.c_str());
                        }
                        ImGui::EndPopup();
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s\n(%s)", entry->mDisplayName.c_str(), entry->mOrigin.c_str());
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    {
                        PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                        EditorsModule* editors = static_cast<EditorsModule*>(mod);
                        if (editors)
                        {
                            editors->OpenLuaScript(entry->mFullPath);
                        }
                    }
                }
            };

            ImGui::BeginChild("##LuaScriptsList", ImVec2(0, 0), false);
            BeginAlternatingRows();
            drawTree(root, "");
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        // ===== C++ Addons Tab =====
        if (ImGui::BeginTabItem("C++ Addons"))
        {
            NativeAddonManager* nam = NativeAddonManager::Get();

            // Toolbar buttons
            {
                bool hasProject = !GetEngineState()->mProjectDirectory.empty();
                bool hasAddons = nam && !nam->GetDiscoveredAddonIds().empty();
                PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                EditorsModule* editors = static_cast<EditorsModule*>(mod);

                if (!hasProject) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::Button(ICON_IC_SHARP_EXTENSION "##NewNativeAddon"))
                {
                    if (hasProject)
                        OpenCreateNativeAddonDialog();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("New Native Addon");

                ImGui::SameLine();
                if (!hasAddons) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::Button(ICON_STREAMLINE_SHARP_NEW_FILE_REMIX "##NewCppFile"))
                {
                    if (hasProject && hasAddons)
                        OpenCreateCppFileDialog();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("New C++ File");
                if (!hasAddons) ImGui::PopStyleVar();

                ImGui::SameLine();
                bool cppConfigured = editors && editors->IsCppEditorConfigured();
                if (!cppConfigured || !hasAddons) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                if (ImGui::Button(ICON_IX_CODE "##OpenCppEditor"))
                {
                    if (hasProject && hasAddons && cppConfigured)
                    {
                        // Open first addon's vcxproj in IDE
                        std::vector<std::string> addonIds = nam->GetDiscoveredAddonIds();
                        for (const std::string& id : addonIds)
                        {
                            const NativeAddonState* state = nam->GetState(id);
                            if (state && state->mNativeMetadata.mHasNative)
                            {
                                std::string binaryName = state->mNativeMetadata.mBinaryName.empty() ? id : state->mNativeMetadata.mBinaryName;
                                std::string vcxproj = state->mSourcePath + binaryName + ".vcxproj";
                                editors->OpenCppFile(vcxproj, vcxproj);
                                break;
                            }
                        }
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    if (cppConfigured)
                    {
                        std::string editorName = editors->mCppEditorPath;
                        size_t lastSlash = editorName.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                            editorName = editorName.substr(lastSlash + 1);
                        ImGui::SetTooltip("Open in %s", editorName.c_str());
                    }
                    else
                    {
                        ImGui::SetTooltip("Open in External Editor (not configured)");
                    }
                }
                if (!cppConfigured || !hasAddons) ImGui::PopStyleVar();

                ImGui::SameLine();
                if (ImGui::Button(ICON_MDI_FOLDER_SEARCH_OUTLINE "##OpenCppExplorer"))
                {
                    if (hasProject)
                    {
                        std::string packagesDir = GetEngineState()->mProjectDirectory + "Packages/";
#if PLATFORM_WINDOWS
                        SYS_Exec(("start \"\" \"" + packagesDir + "\"").c_str());
#elif PLATFORM_LINUX
                        SYS_Exec(("xdg-open \"" + packagesDir + "\" &").c_str());
#endif
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Open in Explorer");

                ImGui::SameLine();
                if (ImGui::Button(ICON_UNDO "##RefreshCppAddons"))
                {
                    sCppLastUpdate = 0.0;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Refresh");

                if (!hasProject) ImGui::PopStyleVar();

                ImGui::Spacing();
            }

            // Refresh every 2 seconds
            if (nam && (sCppAddons.empty() || currentTime - sCppLastUpdate > 2.0))
            {
                sCppAddons.clear();
                std::vector<std::string> addonIds = nam->GetDiscoveredAddonIds();

                for (const std::string& id : addonIds)
                {
                    const NativeAddonState* state = nam->GetState(id);
                    if (!state || !state->mNativeMetadata.mHasNative)
                        continue;

                    CppAddonEntry addon;
                    addon.mAddonId = id;
                    addon.mAddonName = state->mNativeMetadata.mBinaryName.empty() ? id : state->mNativeMetadata.mBinaryName;
                    addon.mSourcePath = state->mSourcePath;
                    addon.mVcxprojPath = state->mSourcePath + addon.mAddonName + ".vcxproj";

                    // Gather source files
                    std::string sourceDir = state->mSourcePath + state->mNativeMetadata.mSourceDir + "/";
                    GatherCppSourceFiles(sourceDir, addon.mSourceFiles);
                    std::sort(addon.mSourceFiles.begin(), addon.mSourceFiles.end());

                    sCppAddons.push_back(addon);
                }

                sCppLastUpdate = currentTime;
            }

            ImGui::BeginChild("##CppAddonsList", ImVec2(0, 0), false);
            BeginAlternatingRows();

            if (!nam)
            {
                ImGui::TextDisabled("Native addon manager not available.");
            }
            else if (sCppAddons.empty())
            {
                ImGui::TextDisabled("No native addons discovered.");
            }
            else
            {
                for (const CppAddonEntry& addon : sCppAddons)
                {
                    ImGuiTreeNodeFlags addonFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

                    bool addonVisible = true;
                    if (!filterLower.empty())
                    {
                        // Check if addon name or any source file matches
                        addonVisible = false;
                        std::string nameLower = addon.mAddonName;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(filterLower) != std::string::npos)
                        {
                            addonVisible = true;
                        }
                        else
                        {
                            for (const std::string& sf : addon.mSourceFiles)
                            {
                                std::string sfLower = sf;
                                std::transform(sfLower.begin(), sfLower.end(), sfLower.begin(), ::tolower);
                                if (sfLower.find(filterLower) != std::string::npos)
                                {
                                    addonVisible = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!addonVisible)
                        continue;

                    if (!filterLower.empty())
                        addonFlags |= ImGuiTreeNodeFlags_DefaultOpen;

                    bool addonOpen = ImGui::TreeNodeEx(addon.mAddonId.c_str(), addonFlags, "%s %s", ICON_MATERIAL_SYMBOLS_FOLDER_SHARP, addon.mAddonName.c_str());
                    AlternatingRowBackground();
                    if (addonOpen)
                    {
                        for (const std::string& srcFile : addon.mSourceFiles)
                        {
                            // Extract just the filename
                            std::string fileName = srcFile;
                            size_t lastSlash = fileName.find_last_of("/\\");
                            if (lastSlash != std::string::npos)
                                fileName = fileName.substr(lastSlash + 1);

                            // Filter individual files
                            if (!filterLower.empty())
                            {
                                std::string fileLower = fileName;
                                std::transform(fileLower.begin(), fileLower.end(), fileLower.begin(), ::tolower);
                                if (fileLower.find(filterLower) == std::string::npos)
                                    continue;
                            }

                            ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            const char* fileIcon = ICON_MINGCUTE_PAPER_FILL ;
                            {
                                size_t dot = fileName.rfind('.');
                                if (dot != std::string::npos)
                                {
                                    std::string ext = fileName.substr(dot);
                                    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
                                        fileIcon = ICON_CPP;
                                    else if (ext == ".lua")
                                        fileIcon = ICON_LUA;
                                    else if (ext == ".json")
                                        fileIcon = ICON_JSON;
                                    else if (ext == ".cs")
                                        fileIcon = ICON_CS;
                                }
                            }
                            ImGui::TreeNodeEx(srcFile.c_str(), leafFlags, "%s %s", fileIcon, fileName.c_str());
                            AlternatingRowBackground();

                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("%s", srcFile.c_str());
                            }

                            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                            {
                                PreferencesModule* mod = PreferencesManager::Get()->FindModule("External/Editors");
                                EditorsModule* editors = static_cast<EditorsModule*>(mod);
                                if (editors)
                                {
                                    editors->OpenCppFile(srcFile, addon.mVcxprojPath);
                                }
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

static void DrawMainMenuBar()
{
    ActionManager* am = ActionManager::Get();

    bool openSaveSceneAsModal = false;

    float menuPadding = GetMenuBarPadding();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, menuPadding));

    if (ImGui::BeginMainMenuBar())
    {
        DrawMainMenuBarMenus(openSaveSceneAsModal);

        // -- Toolbar items (merged into menu bar) --
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 20.0f);

        // Editor mode combo
        int curMode = (int)GetEditorState()->mMode;
        PaintMode paintMode = (GetEditorState()->mMode == EditorMode::Scene3D) ? GetEditorState()->mPaintMode : PaintMode::None;

        if (paintMode != PaintMode::None && paintMode != PaintMode::Count)
        {
            curMode = int(EditorMode::Count) + int(paintMode) - 1;
        }

        const char* modeStrings[] = { "Scene", "2D", "3D", "Paint Colors", "Paint Instances", "Voxel Sculpt", "Terrain Sculpt", "Tile Paint" };
        ImGui::SetNextItemWidth(80);
        ImGui::Combo("##EditorMode", &curMode, modeStrings, 8);

        if (curMode == 3)
        {
            curMode = (int)EditorMode::Scene3D;
            paintMode = PaintMode::Color;
        }
        else if (curMode == 4)
        {
            curMode = (int)EditorMode::Scene3D;
            paintMode = PaintMode::Instance;
        }
        else if (curMode == 5)
        {
            curMode = (int)EditorMode::Scene3D;
            paintMode = PaintMode::Voxel;
        }
        else if (curMode == 6)
        {
            curMode = (int)EditorMode::Scene3D;
            paintMode = PaintMode::Terrain;
        }
        else if (curMode == 7)
        {
            curMode = (int)EditorMode::Scene3D;
            paintMode = PaintMode::TilePaint;
        }
        else
        {
            paintMode = PaintMode::None;
        }

        {
            EditorMode prevMode = GetEditorState()->mMode;
            GetEditorState()->SetEditorMode((EditorMode)curMode);
            GetEditorState()->SetPaintMode(paintMode);

            // Fire editor mode changed hook (Batch 10)
            if ((int)prevMode != curMode)
            {
                EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                if (hookMgr != nullptr) hookMgr->FireOnEditorModeChanged(curMode);
            }
        }

        // Gizmo Operation Buttons (Translate/Rotate/Scale)
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        EditorState* edState = GetEditorState();
        ImGuizmo::OPERATION& gizmoOp = edState->mGizmoOperation;
        ImGuizmo::MODE& gizmoMode = edState->mGizmoMode;

        // Translate button
        bool isTranslate = (gizmoOp == ImGuizmo::TRANSLATE);
        if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, kSelectedColor);
        if (ImGui::Button(ICON_BX_MOVE))
            gizmoOp = ImGuizmo::TRANSLATE;
        if (isTranslate) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate (Space+G)");

        // Rotate button
        ImGui::SameLine();
        bool isRotate = (gizmoOp == ImGuizmo::ROTATE);
        if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, kSelectedColor);
        if (ImGui::Button(ICON_LUCIDE_ROTATE_3D))
            gizmoOp = ImGuizmo::ROTATE;
        if (isRotate) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (Space+R)");

        // Scale button
        ImGui::SameLine();
        bool isScale = (gizmoOp == ImGuizmo::SCALE);
        if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, kSelectedColor);
        if (ImGui::Button(ICON_MAGE_SCALE_UP))
            gizmoOp = ImGuizmo::SCALE;
        if (isScale) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (Space+S)");

        // Draw addon gizmo tools (Batch 10)
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                Node* selNode = GetEditorState()->GetSelectedNode();
                hookMgr->DrawGizmoTools(selNode);
            }
        }

        // Local/World mode toggle
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        bool isLocal = (gizmoMode == ImGuizmo::LOCAL);
        if (isLocal) ImGui::PushStyleColor(ImGuiCol_Button, kToggledColor);
        if (ImGui::Button(isLocal ? ICON_MDI_AXIS_ARROW "  Local" : ICON_GLOBE "  World"))
            gizmoMode = isLocal ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        if (isLocal) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Local/World (Ctrl+T)");

        // Scene tabs
        static int32_t sPrevActiveSceneIdx = 0;
        std::vector<EditScene>& scenes = GetEditorState()->mEditScenes;
        int32_t activeSceneIdx = GetEditorState()->mEditSceneIndex;
        bool sceneJustChanged = sPrevActiveSceneIdx != activeSceneIdx;

        const ImGuiTabBarFlags kSceneTabBarFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll;
        ImGui::SameLine(0.0f, 20.0f);

        if (scenes.size() > 0 &&
            ImGui::BeginTabBar("SceneTabBar", kSceneTabBarFlags))
        {
            int32_t openedTab = activeSceneIdx;

            for (int32_t n = 0; n < (int32_t)scenes.size(); n++)
            {
                ImGui::PushID(n);

                const EditScene& scene = scenes[n];

                bool opened = true;
                std::string sceneName = "*New";

                if (scene.mSceneAsset != nullptr)
                {
                    Scene* sceneAsset = scene.mSceneAsset.Get<Scene>();
                    sceneName = sceneAsset->GetName();
                    if (sceneAsset->GetDirtyFlag())
                    {
                        sceneName = "*" + sceneName;
                    }
                }

                static char sTabId[128];
                snprintf(sTabId, 127, "%s###%d", sceneName.c_str(), n);

                ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
                if (sceneJustChanged && n == activeSceneIdx)
                {
                    tabFlags = ImGuiTabItemFlags_SetSelected;
                }

                if (ImGui::BeginTabItem(sTabId, &opened, tabFlags))
                {
                    if (n != activeSceneIdx)
                    {
                        openedTab = n;
                    }

                    ImGui::EndTabItem();
                }

                // Scene tab context menu (Batch 8)
                if (ImGui::BeginPopupContextItem())
                {
                    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                    if (hookMgr != nullptr) hookMgr->DrawSceneTabContextItems();
                    ImGui::EndPopup();
                }

                if (!opened)
                {
                    GetEditorState()->CloseEditScene(n);
                }

                ImGui::PopID();
            }

            if (!sceneJustChanged &&
                openedTab != activeSceneIdx)
            {
                GetEditorState()->OpenEditScene(openedTab);
            }

            ImGui::EndTabBar();
        }

        sPrevActiveSceneIdx = activeSceneIdx;

        // -- Right-aligned buttons: [Addon Toolbar Items] [Hammer] [Play ▾] --
        {
            enum class PlayTarget { PlayInEditor, PlayFullScreen, Dolphin, Azahar, Standalone, Send3dsLink, Count };
            int32_t& playTargetRef = GetEditorState()->mPlayTarget;
            PlayTarget currentPlayTarget = (PlayTarget)playTargetRef;

            const char* playTargetLabels[] = {
                "Play In Editor",
                "Play Full Screen",
                "Play on Dolphin",
                "Play on Azahar",
                "Play Standalone",
                "Send 3dsLink",
            };

            const ImGuiStyle& style = ImGui::GetStyle();
            float buttonSize = ImGui::GetFrameHeight();
            float spacing = style.ItemSpacing.x;
            bool inPie = IsPlayingInEditor();

            // Build the play button label: "icon TargetName"
            const char* targetLabel = playTargetLabels[(int)currentPlayTarget];
            char playBtnLabel[128];
            snprintf(playBtnLabel, sizeof(playBtnLabel), "%s %s", inPie ? ICON_IC_BASELINE_STOP : ICON_MDI_PLAY, inPie ? "Stop" : targetLabel);

            float playBtnWidth = ImGui::CalcTextSize(playBtnLabel).x + style.FramePadding.x * 2.0f;
            float arrowBtnWidth = ImGui::CalcTextSize(ICON_DASHICONS_ARROW_DOWN).x + style.FramePadding.x * 2.0f;
            float playGroupWidth = inPie ? playBtnWidth : (playBtnWidth + arrowBtnWidth);
            float rightGroupWidth = buttonSize + spacing + playGroupWidth + 20.0f;
            float targetX = ImGui::GetWindowWidth() - rightGroupWidth;
            float cursorX = ImGui::GetCursorPosX();
            if (targetX > cursorX)
                ImGui::SameLine(targetX);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

            // Hammer button
            if (ImGui::Button(ICON_ION_HAMMER_SHARP))
            {
                ReloadAllScripts();
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

                    nam->ReloadAllNativeAddons();
                    LogDebug("Native addon dependencies regenerated and addons reloaded.");
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Refresh Scripts (Ctrl+R)");
            }

            // Play/Stop button with target label
            ImGui::SameLine();
            if (ImGui::Button(playBtnLabel))
            {
                if (inPie)
                {
                    GetEditorState()->EndPlayInEditor();
                }
                else
                {
                    switch (currentPlayTarget)
                    {
                    case PlayTarget::PlayInEditor:
                        GetEditorState()->mPlayInGameWindow = true;
                        GetEditorState()->BeginPlayInEditor();
                        break;
                    case PlayTarget::PlayFullScreen:
                        GetEditorState()->BeginPlayInEditor();
                        break;
                    case PlayTarget::Dolphin:
                        GetPackagingWindow()->BuildAndRunWithProfile(Platform::GameCube, true, false);
                        break;
                    case PlayTarget::Azahar:
                        GetPackagingWindow()->BuildAndRunWithProfile(Platform::N3DS, true, false);
                        break;
                    case PlayTarget::Standalone:
                        GetPackagingWindow()->BuildAndRunWithProfile(Platform::Windows, false, false);
                        break;
                    case PlayTarget::Send3dsLink:
                        GetPackagingWindow()->BuildAndRunWithProfile(Platform::N3DS, true, true);
                        break;
                    default:
                        break;
                    }
                }
            }

            // Dropdown arrow button (hidden during PIE since stop is the only action)
            if (!inPie)
            {
                ImGui::SameLine(0, 0);
                if (ImGui::Button(ICON_DASHICONS_ARROW_DOWN "##PlayDropdown", ImVec2(arrowBtnWidth, 0)))
                {
                    ImGui::OpenPopup("PlayTargetPopup");
                }
            }

            ImGui::PopStyleColor();

            // Play target dropdown popup
            if (ImGui::BeginPopup("PlayTargetPopup"))
            {
                LaunchersModule* launchers = nullptr;
                PreferencesManager* prefsMgr = PreferencesManager::Get();
                if (prefsMgr != nullptr)
                {
                    launchers = static_cast<LaunchersModule*>(prefsMgr->FindModule("External/Launchers"));
                }

                bool dolphinOk = launchers && launchers->IsEmulatorConfigured(Platform::GameCube);
                bool azaharOk = launchers && launchers->IsEmulatorConfigured(Platform::N3DS);
                bool threeDsLinkOk = launchers && launchers->Is3dsLinkConfigured();

                struct PlayTargetInfo
                {
                    PlayTarget target;
                    const char* label;
                    bool enabled;
                    const char* disabledTooltip;
                };

                PlayTargetInfo items[] = {
                    { PlayTarget::PlayInEditor, "Play In Editor",  true,          nullptr },
                    { PlayTarget::PlayFullScreen, "Play Full Screen", true,       nullptr },
                    { PlayTarget::Dolphin,      "Play on Dolphin", dolphinOk,     "Configure Dolphin path in Preferences > External > Launchers" },
                    { PlayTarget::Azahar,       "Play on Azahar",  azaharOk,      "Configure Azahar path in Preferences > External > Launchers" },
                    { PlayTarget::Standalone,   "Play Standalone",  true,          nullptr },
                    { PlayTarget::Send3dsLink,  "Send 3dsLink",    threeDsLinkOk, "Configure 3dslink in Preferences > External > Launchers" },
                };

                for (const PlayTargetInfo& item : items)
                {
                    bool isSelected = (currentPlayTarget == item.target);

                    if (!item.enabled)
                    {
                        ImGui::BeginDisabled();
                    }

                    if (ImGui::Selectable(item.label, isSelected))
                    {
                        playTargetRef = (int32_t)item.target;
                    }

                    if (!item.enabled)
                    {
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && item.disabledTooltip)
                        {
                            ImGui::SetTooltip("%s", item.disabledTooltip);
                        }
                    }
                }

                // Draw addon play targets (Batch 8)
                {
                    EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                    if (hookMgr != nullptr && hookMgr->HasPlayTargets())
                    {
                        ImGui::Separator();
                        hookMgr->DrawPlayTargets();
                    }
                }

                ImGui::EndPopup();
            }
        }

        // Draw addon toolbar items
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr)
            {
                hookMgr->DrawToolbarItems();
            }
        }

        // Hotkey Menus
        if (GetEditorState()->GetViewport3D()->ShouldHandleInput())
        {
            EditorHotkeyMap* hotkeys = EditorHotkeyMap::Get();

            if (hotkeys->IsActionJustTriggered(EditorAction::Spawn_Basic3DMenu))
            {
                ImGui::OpenPopup("Spawn Basic 3D");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Spawn_BasicWidgetMenu))
            {
                ImGui::OpenPopup("Spawn Basic Widget");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Spawn_NodeMenu))
            {
                ImGui::OpenPopup("Spawn Node");
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Asset_CreateScene))
            {
                GetEditorState()->OpenEditScene(nullptr);
            }

            if (hotkeys->IsActionJustTriggered(EditorAction::Edit_ReloadScripts))
            {
                ReloadAllScripts();
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

                    nam->ReloadAllNativeAddons();
                    LogDebug("Native addon dependencies regenerated and addons reloaded.");
                }

                // Refresh asset directories
                AssetDir* projDir = AssetManager::Get()->FindProjectDirectory();
                if (projDir)
                    AssetManager::Get()->RefreshDirectory(projDir);
                AssetDir* pkgDir = AssetManager::Get()->FindPackagesDirectory();
                if (pkgDir)
                    AssetManager::Get()->RefreshDirectory(pkgDir);

                // Sync UIDocument .xml <-> .oct files
                RefreshUIDocuments();
            }
        }

        // Process addon keyboard shortcuts (Batch 5)
        {
            EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
            if (hookMgr != nullptr) hookMgr->ProcessShortcuts();
        }

        if (ImGui::BeginPopup("Spawn Basic 3D"))
        {
            DrawSpawnBasic3dMenu(nullptr, true);
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Spawn Basic Widget"))
        {
            DrawSpawnBasicWidgetMenu(GetEditorState()->GetSelectedWidget());
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Spawn Node"))
        {
            DrawAddNodeMenu(nullptr);
            ImGui::EndPopup();
        }

        // Asset drop mesh type selection popup
        if (sAssetDropPopupPending)
        {
            ImGui::SetNextWindowPos(sAssetDropPopupPos);
            ImGui::OpenPopup("AssetDropMeshType");
            sAssetDropPopupPending = false;
        }

        if (ImGui::BeginPopup("AssetDropMeshType"))
        {
            // Determine spawn position: raycast for viewport drops, focal point otherwise
            auto getAssetDropSpawnPos = [&]() -> glm::vec3
            {
                if (sAssetDropInViewport)
                {
                    Camera3D* camera = GetEditorState()->GetEditorCamera();
                    if (camera != nullptr)
                    {
                        RayTestResult rayResult;
                        camera->TraceScreenToWorld(sAssetDropScreenX, sAssetDropScreenY, ColGroupAll, rayResult);
                        if (rayResult.mHitNode != nullptr)
                        {
                            return rayResult.mHitPosition;
                        }

                        Node3D* hitNode = Renderer::Get()->ProcessHitCheck(GetWorld(0), sAssetDropScreenX, sAssetDropScreenY);
                        if (hitNode != nullptr)
                        {
                            Primitive3D* prim = hitNode->As<Primitive3D>();
                            if (prim != nullptr)
                            {
                                Bounds bounds = prim->GetBounds();
                                glm::vec3 rayOrigin = camera->GetWorldPosition();
                                glm::vec3 nearPoint = camera->ScreenToWorldPosition(sAssetDropScreenX, sAssetDropScreenY);
                                glm::vec3 rayDir = Maths::SafeNormalize(nearPoint - rayOrigin);

                                glm::vec3 oc = rayOrigin - bounds.mCenter;
                                float b = 2.0f * glm::dot(oc, rayDir);
                                float c = glm::dot(oc, oc) - bounds.mRadius * bounds.mRadius;
                                float discriminant = b * b - 4.0f * c;

                                if (discriminant >= 0.0f)
                                {
                                    float t = (-b - sqrtf(discriminant)) / 2.0f;
                                    return rayOrigin + rayDir * t;
                                }

                                float t = -glm::dot(oc, rayDir);
                                return rayOrigin + rayDir * glm::max(t, 0.0f);
                            }
                        }
                    }
                }
                return EditorGetFocusPosition();
            };

            if (ImGui::Selectable(BASIC_STATIC_MESH))
            {
                if (sAssetDropStub != nullptr)
                {
                    if (sAssetDropStub->mAsset == nullptr)
                        AssetManager::Get()->LoadAsset(*sAssetDropStub);
                    if (sAssetDropStub->mAsset != nullptr)
                    {
                        glm::vec3 spawnPos = getAssetDropSpawnPos();
                        ActionManager::Get()->SpawnBasicNode(BASIC_STATIC_MESH,
                            sAssetDropParentNode, sAssetDropStub->mAsset, sAssetDropInViewport, spawnPos);

                        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                        if (hookMgr)
                        {
                            if (sAssetDropInViewport)
                                hookMgr->FireOnAssetDropViewport(sAssetDropStub->mName.c_str());
                            else
                                hookMgr->FireOnAssetDropHierarchy(sAssetDropStub->mName.c_str());
                        }
                    }
                }
                sAssetDropStub = nullptr;
            }
            if (ImGui::Selectable(BASIC_INSTANCED_MESH))
            {
                if (sAssetDropStub != nullptr)
                {
                    if (sAssetDropStub->mAsset == nullptr)
                        AssetManager::Get()->LoadAsset(*sAssetDropStub);
                    if (sAssetDropStub->mAsset != nullptr)
                    {
                        glm::vec3 spawnPos = getAssetDropSpawnPos();
                        ActionManager::Get()->SpawnBasicNode(BASIC_INSTANCED_MESH,
                            sAssetDropParentNode, sAssetDropStub->mAsset, sAssetDropInViewport, spawnPos);

                        EditorUIHookManager* hookMgr = EditorUIHookManager::Get();
                        if (hookMgr)
                        {
                            if (sAssetDropInViewport)
                                hookMgr->FireOnAssetDropViewport(sAssetDropStub->mName.c_str());
                            else
                                hookMgr->FireOnAssetDropHierarchy(sAssetDropStub->mName.c_str());
                        }
                    }
                }
                sAssetDropStub = nullptr;
            }
            ImGui::EndPopup();
        }
        else if (sAssetDropStub != nullptr && !sAssetDropPopupPending)
        {
            sAssetDropStub = nullptr;
            sAssetDropParentNode = nullptr;
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleVar();

    // Set up ImGuizmo rect for the viewport area
    {
        EditorState* edState = GetEditorState();
        float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
        if (interfaceScale == 0.0f)
        {
            interfaceScale = 1.0f;
        }
        float invInterfaceScale = 1.0f / interfaceScale;

        ImGuizmo::SetRect(
            (float)edState->mViewportX * invInterfaceScale,
            (float)edState->mViewportY * invInterfaceScale,
            (float)edState->mViewportWidth * invInterfaceScale,
            (float)edState->mViewportHeight * invInterfaceScale
        );

        ImGuizmo::SetAlternativeWindow(sViewportDockWindow);
    }

    // Modal dialogs - drawn outside menu bar scope
    if (GetEditorState()->mRequestSaveSceneAs)
    {
        openSaveSceneAsModal = true;
    }

    if (openSaveSceneAsModal)
    {
        ImGui::OpenPopup("Save Scene As");
    }

    // Center the "Save Scene As" modal
    if (ImGui::IsPopupOpen("Save Scene As"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        GetEditorState()->mRequestSaveSceneAs = false;

        AssetDir* curDir = GetEditorState()->GetAssetDirectory();

        if (curDir == nullptr || curDir->mEngineDir || curDir == AssetManager::Get()->GetRootDirectory())
        {
            GetEditorState()->SetAssetDirectory(AssetManager::Get()->FindProjectDirectory(), true);
        }

        {
            ImGuiWindowFlags childFlags = ImGuiWindowFlags_None;
            ImGui::BeginChild("Dir Browser", ImVec2(250, 250), true, childFlags);
            DrawDirPicker();

            ImGui::EndChild();
        }


        if (curDir != nullptr && !curDir->mEngineDir)
        {
            AssetDir* dir = curDir;
            std::string dirString = dir->mName + "/";
            dir = dir->mParentDir;

            while (dir != nullptr)
            {
                if (dir->mParentDir == nullptr)
                    dirString = "/" + dirString;
                else
                    dirString = dir->mName + "/" + dirString;

                dir = dir->mParentDir;
            }

            ImGui::Text("Save scene to directory...");
            ImGui::Indent(10);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.7f, 1.0f));
            ImGui::Text(dirString.c_str());
            ImGui::PopStyleColor();
            ImGui::Unindent(10);

            bool save = false;
            if (ImGui::InputText("Scene Name", sPopupInputBuffer, kPopupInputBufferSize, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                save = true;
            }

            if (ImGui::Button("Save"))
            {
                save = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            if (save)
            {
                std::string sceneName = sPopupInputBuffer;

                if (sceneName == "")
                {
                    sceneName = "SC_Scene";
                }

                AssetStub* stub = EditorAddUniqueAsset(sceneName.c_str(), curDir, Scene::GetStaticType(), true);
                OCT_ASSERT(stub != nullptr);

                GetEditorState()->CaptureAndSaveScene(stub, nullptr);

                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::Text("Invalid asset directory. Please navigate to a project directory.");
            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }


    if (!ImGui::IsPopupOpen("Import Scene") && GetEditorState()->mPendingSceneImportPath != "")
    {
        std::string sceneName = GetFileNameFromPath(GetEditorState()->mPendingSceneImportPath);
        sSceneImportOptions.mSceneName = sceneName;
        ImGui::OpenPopup("Import Scene");
    }

    if (ImGui::IsPopupOpen("Import Scene"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Import Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        sSceneImportOptions.mFilePath = GetEditorState()->mPendingSceneImportPath;
        ImGui::Text(GetEditorState()->mPendingSceneImportPath.c_str());
        ImGui::InputText("Scene Name", &sSceneImportOptions.mSceneName);
        ImGui::InputText("Prefix", &sSceneImportOptions.mPrefix);
        Polyphase::Checkbox("Import Meshes", &sSceneImportOptions.mImportMeshes);
        Polyphase::Checkbox("Import Materials", &sSceneImportOptions.mImportMaterials);
        Polyphase::Checkbox("Import Textures", &sSceneImportOptions.mImportTextures);
        Polyphase::Checkbox("Import Lights", &sSceneImportOptions.mImportLights);
        Polyphase::Checkbox("Import Cameras", &sSceneImportOptions.mImportCameras);
        Polyphase::Checkbox("Enable Collision", &sSceneImportOptions.mEnableCollision);
        Polyphase::Checkbox("Apply glTF Extras", &sSceneImportOptions.mApplyGltfExtras);

        int32_t shadingModelCount = int32_t(ShadingModel::Count);
        ImGui::Combo("Shading Model", (int*)&(sSceneImportOptions.mDefaultShadingModel), gShadingModelStrings, shadingModelCount);

        int32_t vertColorMode = int32_t(VertexColorMode::Count);
        ImGui::Combo("Vertex Color Mode", (int*)&(sSceneImportOptions.mDefaultVertexColorMode), gVertexColorModeStrings, vertColorMode);

        if (ImGui::Button("Import"))
        {
            am->ImportScene(sSceneImportOptions);
            GetEditorState()->mPendingSceneImportPath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            GetEditorState()->mPendingSceneImportPath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }


    // Reimport Scene popup
    if (!ImGui::IsPopupOpen("Reimport Scene") &&
        GetEditorState()->mPendingReimportSceneStub != nullptr &&
        GetEditorState()->mPendingReimportScenePath != "")
    {
        sReimportSceneOptions = SceneImportOptions();
        sReimportSceneOptions.mFilePath = GetEditorState()->mPendingReimportScenePath;
        ImGui::OpenPopup("Reimport Scene");
    }

    if (ImGui::IsPopupOpen("Reimport Scene"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Reimport Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        AssetStub* reimportStub = GetEditorState()->mPendingReimportSceneStub;

        ImGui::Text("File: %s", GetEditorState()->mPendingReimportScenePath.c_str());
        ImGui::Text("Scene: %s", reimportStub ? reimportStub->mName.c_str() : "");
        ImGui::Separator();

        ImGui::InputText("Prefix", &sReimportSceneOptions.mPrefix);
        Polyphase::Checkbox("Import Meshes", &sReimportSceneOptions.mImportMeshes);
        Polyphase::Checkbox("Import Materials", &sReimportSceneOptions.mImportMaterials);
        Polyphase::Checkbox("Import Textures", &sReimportSceneOptions.mImportTextures);
        Polyphase::Checkbox("Import Lights", &sReimportSceneOptions.mImportLights);
        Polyphase::Checkbox("Import Cameras", &sReimportSceneOptions.mImportCameras);
        Polyphase::Checkbox("Enable Collision", &sReimportSceneOptions.mEnableCollision);
        Polyphase::Checkbox("Textures Updated", &sReimportSceneOptions.mReimportTextures);

        int32_t shadingModelCount = int32_t(ShadingModel::Count);
        ImGui::Combo("Shading Model", (int*)&(sReimportSceneOptions.mDefaultShadingModel), gShadingModelStrings, shadingModelCount);

        int32_t vertColorMode = int32_t(VertexColorMode::Count);
        ImGui::Combo("Vertex Color Mode", (int*)&(sReimportSceneOptions.mDefaultVertexColorMode), gVertexColorModeStrings, vertColorMode);

        if (ImGui::Button("Reimport"))
        {
            if (reimportStub != nullptr)
            {
                std::string sceneName = reimportStub->mName;
                if (sceneName.size() > 3 && sceneName.substr(0, 3) == "SC_")
                {
                    sceneName = sceneName.substr(3);
                }
                sReimportSceneOptions.mSceneName = sceneName;
                sReimportSceneOptions.mReimportSceneStub = reimportStub;

                am->ImportScene(sReimportSceneOptions);
            }

            GetEditorState()->mPendingReimportSceneStub = nullptr;
            GetEditorState()->mPendingReimportScenePath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            GetEditorState()->mPendingReimportSceneStub = nullptr;
            GetEditorState()->mPendingReimportScenePath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen("Import Camera") && GetEditorState()->mIOAssetPath != "")
    {
        ImGui::OpenPopup("Import Camera");
    }

    if (ImGui::IsPopupOpen("Import Camera"))
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Import Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        sCameraImportOptions.mFilePath = GetEditorState()->mIOAssetPath;
        ImGui::Text(GetEditorState()->mPendingSceneImportPath.c_str());
        Polyphase::Checkbox("Set As Main Camera", &sCameraImportOptions.mIsMainCamera);
        Polyphase::Checkbox("Override Camera Name", &sCameraImportOptions.mOverrideCameraName);
        if (sCameraImportOptions.mOverrideCameraName) {
            ImGui::InputText("Camera Name", &sCameraImportOptions.mCameraName);
        }

        if (ImGui::Button("Import"))
        {
            am->ImportCamera(sCameraImportOptions);
            GetEditorState()->mIOAssetPath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            GetEditorState()->mIOAssetPath = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Convert XML to New UIDocument popup
    if (!sConvertXmlSourcePath.empty() && !sShowUIDocumentPicker)
    {
        ImGui::OpenPopup("Convert to UIDocument");
    }

    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Convert to UIDocument", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Create UIDocument from XML:");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", sConvertXmlSourcePath.c_str());
        ImGui::Separator();

        bool create = ImGui::InputText("Asset Name", sPopupInputBuffer, kPopupInputBufferSize,
                                        ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button("Create"))
            create = true;

        ImGui::SameLine();
        if (ImGui::Button("Cancel##ConvertCancel"))
        {
            sConvertXmlSourcePath.clear();
            ImGui::CloseCurrentPopup();
        }

        if (create && sPopupInputBuffer[0] != '\0')
        {
            std::string assetName = sPopupInputBuffer;
            AssetDir* currentDir = GetEditorState()->GetAssetDirectory();

            if (currentDir != nullptr)
            {
                AssetStub* stub = EditorAddUniqueAsset(assetName.c_str(), currentDir,
                                                        UIDocument::GetStaticType(), false);
                if (stub != nullptr && stub->mAsset != nullptr)
                {
                    UIDocument* doc = stub->mAsset->As<UIDocument>();
                    if (doc != nullptr && doc->Import(sConvertXmlSourcePath, nullptr))
                    {
                        AssetManager::Get()->SaveAsset(*stub);
                        LogDebug("Created UIDocument '%s' from: %s", assetName.c_str(),
                                 sConvertXmlSourcePath.c_str());
                    }
                    else
                    {
                        LogError("Failed to import XML: %s", sConvertXmlSourcePath.c_str());
                    }
                }
            }

            sConvertXmlSourcePath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Refresh UIDocument picker popup
    if (sShowUIDocumentPicker)
    {
        ImGui::OpenPopup("Refresh UIDocument");
        sShowUIDocumentPicker = false;
    }

    {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Refresh UIDocument", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Select UIDocument to refresh with XML:");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", sConvertXmlSourcePath.c_str());
        ImGui::Separator();

        static char filterBuffer[128] = {};
        ImGui::InputText("Search", filterBuffer, sizeof(filterBuffer));

        std::string filterLower = filterBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        if (ImGui::BeginListBox("##UIDocList", ImVec2(350, 250)))
        {
            TypeId uiDocType = UIDocument::GetStaticType();
            const auto& assetMap = AssetManager::Get()->GetAssetMap();

            for (const auto& pair : assetMap)
            {
                AssetStub* stub = pair.second;
                if (stub == nullptr || stub->mType != uiDocType)
                    continue;

                // Filter by search
                if (!filterLower.empty())
                {
                    std::string nameLower = stub->mName;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(filterLower) == std::string::npos)
                        continue;
                }

                if (ImGui::Selectable(stub->mName.c_str()))
                {
                    // Load if needed
                    if (stub->mAsset == nullptr)
                        AssetManager::Get()->LoadAsset(*stub);

                    if (stub->mAsset != nullptr)
                    {
                        UIDocument* doc = stub->mAsset->As<UIDocument>();
                        if (doc != nullptr && doc->Import(sConvertXmlSourcePath, nullptr))
                        {
                            AssetManager::Get()->SaveAsset(*stub);
                            LogDebug("Refreshed UIDocument '%s' from: %s", stub->mName.c_str(),
                                     sConvertXmlSourcePath.c_str());
                        }
                        else
                        {
                            LogError("Failed to refresh UIDocument: %s", stub->mName.c_str());
                        }
                    }

                    filterBuffer[0] = '\0';
                    sConvertXmlSourcePath.clear();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndListBox();
        }

        if (ImGui::Button("Cancel##RefreshCancel"))
        {
            filterBuffer[0] = '\0';
            sConvertXmlSourcePath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void DrawPaintColorsPanel()
{
    const float dispWidth = ImGui::GetIO().DisplaySize.x;
    const float dispHeight = ImGui::GetIO().DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(210.0f, GetTopBarHeight()));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 170.0f));

    ImGui::Begin("Paint Colors", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    PaintManager* pm = GetEditorState()->mPaintManager;
    ImGui::DragFloat("Radius", &pm->mRadius, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Opacity", &pm->mOpacity, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Spacing", &pm->mSpacing, 0.5f, 0.0f, 500.0f);
    ImGui::OctColorEdit4("Color", &pm->mColorOptions.mColor[0], 0);
    
    const char* blendModeStrings[] = {"Mix", "Add", "Subtract", "Multiply", "+Alpha", "-Alpha"};
    int32_t blendModeCount = OCT_ARRAY_SIZE(blendModeStrings);
    ImGui::Combo("Blend Mode", (int*)&(pm->mColorOptions.mBlendMode), blendModeStrings, blendModeCount);

    Polyphase::Checkbox("Only Facing Normals", &pm->mColorOptions.mOnlyFacingNormals);
    Polyphase::Checkbox("Only Render Selected", &pm->mOnlyRenderSelected);

    ImGui::End();
}

static void DrawPaintInstancesPanel()
{
    const float dispWidth = ImGui::GetIO().DisplaySize.x;
    const float dispHeight = ImGui::GetIO().DisplaySize.y;

    ImGui::SetNextWindowPos(ImVec2(210.0f, GetTopBarHeight()));
    ImGui::SetNextWindowSize(ImVec2(250.0f, 210.0f));

    ImGui::Begin("Paint Instances", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::PushItemWidth(120.0f);

    PaintManager* pm = GetEditorState()->mPaintManager;

    Property prop(DatumType::Asset, "Instance Mesh", nullptr, &(pm->mInstanceOptions.mMesh), 1, nullptr, (int32_t)StaticMesh::GetStaticType());
    DrawAssetProperty(prop, 0, nullptr, PropertyOwnerType::Count);
    ImGui::SameLine();
    ImGui::Text("Mesh");

    ImGui::DragFloat("Radius", &pm->mRadius, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Spacing", &pm->mSpacing, 0.5f, 0.0f, 500.0f);
    ImGui::DragFloat("Density", &pm->mInstanceOptions.mDensity, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Min Separation", &pm->mInstanceOptions.mMinSeparation, 0.05f, 0.0f, 100.0f);
    ImGui::OctDragScalarN("Min Position", ImGuiDataType_Float, &pm->mInstanceOptions.mMinPosition[0], 3, 1.0f, nullptr, nullptr, "%.2f", 0);
    ImGui::OctDragScalarN("Max Position", ImGuiDataType_Float, &pm->mInstanceOptions.mMaxPosition[0], 3, 1.0f, nullptr, nullptr, "%.2f", 0);
    ImGui::OctDragScalarN("Min Rotation", ImGuiDataType_Float, &pm->mInstanceOptions.mMinRotation[0], 3, 1.0f, nullptr, nullptr, "%.2f", 0);
    ImGui::OctDragScalarN("Max Rotation", ImGuiDataType_Float, &pm->mInstanceOptions.mMaxRotation[0], 3, 1.0f, nullptr, nullptr, "%.2f", 0);
    ImGui::DragFloat("Min Scale", &pm->mInstanceOptions.mMinScale, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Max Scale", &pm->mInstanceOptions.mMaxScale, 0.05f, 0.0f, 100.0f);
    Polyphase::Checkbox("Align With Normal", &pm->mInstanceOptions.mAlignWithNormal);
    Polyphase::Checkbox("Only Render Selected", &pm->mOnlyRenderSelected);
    Polyphase::Checkbox("Erase", &pm->mInstanceOptions.mErase);

    ImGui::PopItemWidth();

    ImGui::End();
}

// Editor-only: draw an orange border around each Canvas widget's rect in the
// Scene2D viewport so the user can see where their UI canvases sit. Replaces
// the previous engine-config-based design bounds rectangle.
static void DrawDesignBounds()
{
    if (GetFeatureFlagsEditor().mShow2DBorder == false)
    {
        return;
    }

    World* world = GetWorld(0);
    if (world == nullptr)
        return;

    std::vector<Canvas*> canvases;
    world->FindNodes(canvases);
    if (canvases.empty())
        return;

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    glm::vec4 vp = Renderer::Get()->GetViewport(0);

    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }
    float invInterfaceScale = 1.0f / interfaceScale;

    Rect viewportRect;
    viewportRect.mX = 0.0f;
    viewportRect.mY = 0.0f;
    viewportRect.mWidth = vp.z;
    viewportRect.mHeight = vp.w;

    ImColor boundsColor(1.0f, 0.6f, 0.0f, 0.8f);  // Orange color
    float thickness = 2.0f;

    for (Canvas* canvas : canvases)
    {
        if (canvas == nullptr || !canvas->IsVisible())
            continue;

        Rect rect = canvas->GetRect();

        if (!rect.OverlapsRect(viewportRect))
            continue;

        rect.Clamp(viewportRect);

        float x = invInterfaceScale * (rect.mX + vp.x);
        float y = invInterfaceScale * (rect.mY + vp.y);
        float w = invInterfaceScale * rect.mWidth;
        float h = invInterfaceScale * rect.mHeight;

        draw_list->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), boundsColor, 0.0f, ImDrawFlags_None, thickness);
    }
}

static void Draw2dSelections()
{
    const std::vector<Node*>& selNodes = GetEditorState()->GetSelectedNodes();

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImColor mutliSelColor(0.7f, 1.0f, 0.0f, 1.0f);
    ImColor selColor(0.0f, 1.0f, 0.0f, 1.0f);
    ImColor hoverColor(0.0f, 1.0f, 1.0f, 1.0f);
    float thickness = 3.0f;
    glm::vec4 vp = Renderer::Get()->GetViewport(0);

    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }
    float invInterfaceScale = 1.0f / interfaceScale;

    Rect boundsRect;
    boundsRect.mX = 0.0f;
    boundsRect.mY = 0.0f;
    boundsRect.mWidth = vp.z;
    boundsRect.mHeight = vp.w;

    // Draw multiselected widget rects.
    for (int32_t i = 0; i < int32_t(selNodes.size()) - 1; ++i)
    {
        Widget* widget = selNodes[i]->As<Widget>();

        if (widget)
        {
            Rect rect = widget->GetRect();

            if (rect.OverlapsRect(boundsRect))
            {
                rect.Clamp(boundsRect);

                float x = invInterfaceScale * (rect.mX + vp.x);
                float y = invInterfaceScale * (rect.mY + vp.y);
                float w = invInterfaceScale * rect.mWidth;
                float h = invInterfaceScale * rect.mHeight;
                draw_list->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), mutliSelColor, 0.0f, ImDrawFlags_None, thickness);
            }
        }
    }

    // Draw the most recent selection differently
    if (selNodes.size() > 0)
    {
        Widget* widget = selNodes.back()->As<Widget>();

        if (widget)
        {
            Rect rect = widget->GetRect();

            if (rect.OverlapsRect(boundsRect))
            {
                rect.Clamp(boundsRect);

                float x = invInterfaceScale * (rect.mX + vp.x);
                float y = invInterfaceScale * (rect.mY + vp.y);
                float w = invInterfaceScale * (rect.mWidth);
                float h = invInterfaceScale * (rect.mHeight);
                draw_list->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), selColor, 0.0f, ImDrawFlags_None, thickness);
            }
        }
    }

    // Draw the hovered rect
    Widget* hoveredWidget = GetEditorState()->GetViewport2D()->GetHoveredWidget();
    if (hoveredWidget != nullptr)
    {
        Rect rect = hoveredWidget->GetRect();

        if (rect.OverlapsRect(boundsRect))
        {
            rect.Clamp(boundsRect);

            float x = invInterfaceScale * (rect.mX + vp.x);
            float y = invInterfaceScale * (rect.mY + vp.y);
            float w = invInterfaceScale * (rect.mWidth);
            float h = invInterfaceScale * (rect.mHeight);
            draw_list->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), hoverColor, 0.0f, ImDrawFlags_None, thickness);
        }
    }
}

static void DrawNodePropertySelectOverlay()
{
    int32_t mX;
    int32_t mY;
    GetMousePosition(mX, mY);

    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }
    float invInterfaceScale = 1.0f / interfaceScale;

    float rot = GetEngineState()->mRealElapsedTime * 3.0f;

    const float lineLength = 40.0f;
    const float lineWidth = 3.0f;
    glm::vec2 point1a = glm::vec2(-lineLength * 0.5f, 0.0f);
    glm::vec2 point1b = glm::vec2( lineLength * 0.5f, 0.0f);
    glm::vec2 point2a = glm::vec2(0.0f, -lineLength * 0.5f);
    glm::vec2 point2b = glm::vec2(0.0f,  lineLength * 0.5f);

    point1a = glm::rotate(point1a, rot);
    point1b = glm::rotate(point1b, rot);
    point2a = glm::rotate(point2a, rot);
    point2b = glm::rotate(point2b, rot);

    glm::vec2 offset = glm::vec2((float)mX * invInterfaceScale, (float)mY * invInterfaceScale);

    point1a += offset;
    point1b += offset;
    point2a += offset;
    point2b += offset;

    ImGui::GetForegroundDrawList()->AddLine({ point1a.x, point1a.y }, { point1b.x, point1b.y }, IM_COL32(255, 0, 0, 200), lineWidth);
    ImGui::GetForegroundDrawList()->AddLine({ point2a.x, point2a.y }, { point2b.x, point2b.y }, IM_COL32(255, 0, 0, 200), lineWidth);
}

static void DrawImGuizmo()
{
    EditorState* edState = GetEditorState();

    // Clear gizmo block when mouse is released
    if (edState->mGizmoBlockedBySelection && !IsMouseButtonDown(MouseCode::MOUSE_LEFT))
    {
        edState->mGizmoBlockedBySelection = false;
    }

    // If gizmo is blocked by recent selection, disable interaction
    if (edState->mGizmoBlockedBySelection)
    {
        ImGuizmo::Enable(false);
    }
    else
    {
        ImGuizmo::Enable(true);
    }

    // Only draw gizmos in 3D mode when not playing in editor
    if (edState->GetEditorMode() != EditorMode::Scene3D &&
        edState->GetEditorMode() != EditorMode::Scene)
    {
        return;
    }

    if (IsPlayingInEditor() && !edState->mEjected)
    {
        return;
    }

    // Get the selected node
    Node* selectedNode = edState->GetSelectedNode();
    if (selectedNode == nullptr || !selectedNode->IsNode3D())
    {
        return;
    }

    Node3D* node3d = static_cast<Node3D*>(selectedNode);

    // Get the editor camera
    Camera3D* camera = edState->GetEditorCamera();
    if (camera == nullptr)
    {
        return;
    }

    // Get view and projection matrices
    glm::mat4 viewMatrix = camera->GetViewMatrix();
    glm::mat4 projMatrix = camera->GetProjectionMatrix();

    // The engine's projection matrix has Y flipped for Vulkan (projMatrix[1][1] is negated).
    // ImGuizmo expects OpenGL-style projection, so we need to un-flip Y.
    glm::mat4 imguizmoProjMatrix = projMatrix;
    imguizmoProjMatrix[1][1] *= -1.0f;

    // Get the node's world transform
    glm::mat4 modelMatrix = node3d->GetTransform();

    // Set orthographic mode if needed
    ImGuizmo::SetOrthographic(camera->GetProjectionMode() == ProjectionMode::ORTHOGRAPHIC);

    // Gather all selected Node3D nodes
    const std::vector<Node*>& selectedNodes = edState->GetSelectedNodes();
    std::vector<Node3D*> selectedNode3Ds;
    for (Node* n : selectedNodes)
    {
        if (n->IsNode3D())
            selectedNode3Ds.push_back(static_cast<Node3D*>(n));
    }

    // Track undo state: cache the original transforms when manipulation starts
    static bool wasUsing = false;
    static std::vector<glm::mat4> originalMatrices;
    static std::vector<Node3D*> lastManipulatedNodes;

    bool isUsing = ImGuizmo::IsUsing();

    // Gizmo manipulation started - cache the original transforms for all selected nodes
    if (isUsing && !wasUsing)
    {
        originalMatrices.clear();
        lastManipulatedNodes.clear();
        for (Node3D* n : selectedNode3Ds)
        {
            originalMatrices.push_back(n->GetTransform());
            lastManipulatedNodes.push_back(n);
        }
    }

    // Store the delta matrix so we can apply incremental changes
    glm::mat4 deltaMatrix = glm::mat4(1.0f);

    // Call Manipulate with delta matrix
    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(imguizmoProjMatrix),
        edState->mGizmoOperation,
        edState->mGizmoMode,
        glm::value_ptr(modelMatrix),
        glm::value_ptr(deltaMatrix),
        nullptr   // snap
    );

    // If the gizmo was manipulated, apply the delta to all selected nodes
    if (manipulated)
    {
        // Apply the primary node's new transform
        node3d->SetTransform(modelMatrix);

        // Apply the delta to all other selected nodes
        for (Node3D* n : selectedNode3Ds)
        {
            if (n != node3d)
            {
                glm::mat4 currentTransform = n->GetTransform();
                glm::mat4 newTransform = deltaMatrix * currentTransform;
                n->SetTransform(newTransform);
            }
        }
    }

    // Gizmo manipulation ended - commit to undo system
    if (!isUsing && wasUsing && !lastManipulatedNodes.empty())
    {
        // Collect new transforms
        std::vector<glm::mat4> newMatrices;
        for (Node3D* n : lastManipulatedNodes)
        {
            newMatrices.push_back(n->GetTransform());
        }

        // Check if any transform changed
        bool anyChanged = false;
        for (size_t i = 0; i < lastManipulatedNodes.size(); ++i)
        {
            if (originalMatrices[i] != newMatrices[i])
            {
                anyChanged = true;
                break;
            }
        }

        if (anyChanged)
        {
            // Restore original transforms
            for (size_t i = 0; i < lastManipulatedNodes.size(); ++i)
            {
                lastManipulatedNodes[i]->SetTransform(originalMatrices[i]);
                lastManipulatedNodes[i]->UpdateTransform(false);
            }

            // Commit all changes through ActionManager for undo support
            ActionManager::Get()->EXE_EditTransforms(lastManipulatedNodes, newMatrices);
        }

        lastManipulatedNodes.clear();
        originalMatrices.clear();
    }

    wasUsing = isUsing;
}

static void DrawImGuizmo2D()
{
    EditorState* edState = GetEditorState();

    // Clear gizmo block when mouse is released
    if (edState->mGizmoBlockedBySelection && !IsMouseButtonDown(MouseCode::MOUSE_LEFT))
    {
        edState->mGizmoBlockedBySelection = false;
    }

    // If gizmo is blocked by recent selection, disable interaction
    if (edState->mGizmoBlockedBySelection)
    {
        ImGuizmo::Enable(false);
    }
    else
    {
        ImGuizmo::Enable(true);
    }

    // Only draw gizmos in 2D mode when not playing in editor
    if (edState->GetEditorMode() != EditorMode::Scene2D)
    {
        return;
    }

    if (IsPlayingInEditor() && !edState->mEjected)
    {
        return;
    }

    // Get the selected widget
    Widget* widget = edState->GetSelectedWidget();
    if (widget == nullptr)
    {
        return;
    }

    // Get viewport info
    Viewport2D* vp2d = edState->GetViewport2D();
    float zoom = vp2d->GetZoom();

    // Account for interface scale - ImGuizmo operates in scaled ImGui coordinates
    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }
    float invInterfaceScale = 1.0f / interfaceScale;

    // Viewport dimensions in scaled coordinates (matching ImGuizmo::SetRect)
    float vpWidth = (float)edState->mViewportWidth * invInterfaceScale;
    float vpHeight = (float)edState->mViewportHeight * invInterfaceScale;

    // Build orthographic projection matrix (Y down for screen coords)
    glm::mat4 projMatrix = glm::ortho(0.0f, vpWidth, vpHeight, 0.0f, -1.0f, 1.0f);

    // View matrix is identity since GetRect() already returns screen-space coordinates
    // (zoom and rootOffset are already applied via the wrapper widget)
    glm::mat4 viewMatrix = glm::mat4(1.0f);

    // Get widget's rect (already in screen space after zoom/offset transform)
    // Scale to match ImGuizmo's coordinate space
    Rect rect = widget->GetRect();
    rect.mX *= invInterfaceScale;
    rect.mY *= invInterfaceScale;
    rect.mWidth *= invInterfaceScale;
    rect.mHeight *= invInterfaceScale;
    glm::vec2 pivot = widget->GetPivot();

    // Calculate pivot point in screen space (already scaled)
    float pivotX = rect.mX + rect.mWidth * pivot.x;
    float pivotY = rect.mY + rect.mHeight * pivot.y;

    // For scale mode, include current size in the model matrix so ImGuizmo can scale it
    // For translate/rotate, just use position and rotation
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(pivotX, pivotY, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(widget->GetRotation()), glm::vec3(0.0f, 0.0f, 1.0f));

    // For scale, encode the widget's screen-space size into the matrix
    if (edState->mGizmoOperation == ImGuizmo::SCALE)
    {
        modelMatrix = glm::scale(modelMatrix, glm::vec3(rect.mWidth, rect.mHeight, 1.0f));
    }

    // Set orthographic mode
    ImGuizmo::SetOrthographic(true);

    // Gather all selected widgets
    const std::vector<Node*>& selectedNodes = edState->GetSelectedNodes();
    std::vector<Widget*> selectedWidgets;
    for (Node* n : selectedNodes)
    {
        if (n->IsWidget())
            selectedWidgets.push_back(static_cast<Widget*>(n));
    }

    // Track undo state: cache the original transforms when manipulation starts
    struct WidgetTransformState
    {
        Widget* widget;
        glm::vec2 offset;
        glm::vec2 size;
        float rotation;
    };
    static bool wasUsing2D = false;
    static std::vector<WidgetTransformState> originalStates;

    bool isUsing = ImGuizmo::IsUsing();

    // Gizmo manipulation started - cache the original transforms for all selected widgets
    if (isUsing && !wasUsing2D)
    {
        originalStates.clear();
        for (Widget* w : selectedWidgets)
        {
            WidgetTransformState state;
            state.widget = w;
            state.offset = w->GetOffset();
            state.size = w->GetSize();
            state.rotation = w->GetRotation();
            originalStates.push_back(state);
        }
    }

    // Store the delta matrix
    glm::mat4 deltaMatrix = glm::mat4(1.0f);

    // Call Manipulate
    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projMatrix),
        edState->mGizmoOperation,
        ImGuizmo::LOCAL,  // Use local mode for 2D
        glm::value_ptr(modelMatrix),
        glm::value_ptr(deltaMatrix),
        nullptr   // snap
    );

    // Apply transform changes to all selected widgets
    if (manipulated)
    {
        if (edState->mGizmoOperation == ImGuizmo::TRANSLATE)
        {
            // Extract translation delta from deltaMatrix (in scaled coordinates)
            glm::vec3 deltaTrans(deltaMatrix[3]);

            // Convert from scaled screen coordinates back to render target pixels,
            // then to widget offset units (account for zoom)
            glm::vec2 offsetDelta = glm::vec2(deltaTrans.x, deltaTrans.y) * interfaceScale / zoom;

            for (Widget* w : selectedWidgets)
            {
                glm::vec2 adjustedDelta = offsetDelta;

                // Handle stretch mode - convert to ratio if needed
                if (w->StretchX())
                {
                    adjustedDelta.x *= 0.002f;
                }
                if (w->StretchY())
                {
                    adjustedDelta.y *= 0.002f;
                }

                glm::vec2 newOffset = w->GetOffset() + adjustedDelta;
                w->SetOffset(newOffset.x, newOffset.y);
            }
        }
        else if (edState->mGizmoOperation == ImGuizmo::ROTATE)
        {
            // Extract rotation delta from deltaMatrix
            // For 2D, we only care about Z rotation
            glm::vec3 deltaScale, deltaTrans, deltaSkew;
            glm::vec4 deltaPerspective;
            glm::quat deltaRot;
            glm::decompose(deltaMatrix, deltaScale, deltaRot, deltaTrans, deltaSkew, deltaPerspective);

            // Convert quaternion to euler and extract Z rotation
            // Negate because screen space Y is down, inverting rotation direction
            glm::vec3 eulerDelta = glm::eulerAngles(deltaRot);
            float deltaRotDeg = -glm::degrees(eulerDelta.z);

            for (Widget* w : selectedWidgets)
            {
                float newRotation = w->GetRotation() + deltaRotDeg;
                w->SetRotation(newRotation);
            }
        }
        else if (edState->mGizmoOperation == ImGuizmo::SCALE)
        {
            // Extract the scale delta from deltaMatrix
            glm::vec3 deltaScale, deltaTrans, deltaSkew;
            glm::vec4 deltaPerspective;
            glm::quat deltaRot;
            glm::decompose(deltaMatrix, deltaScale, deltaRot, deltaTrans, deltaSkew, deltaPerspective);

            // Apply scale delta to all selected widgets
            for (Widget* w : selectedWidgets)
            {
                glm::vec2 currentSize = w->GetSize();
                glm::vec2 scaledSize = currentSize * glm::vec2(deltaScale.x, deltaScale.y);

                // Handle stretch mode
                if (w->StretchX())
                {
                    float pixelChange = (deltaScale.x - 1.0f) * currentSize.x;
                    scaledSize.x = currentSize.x + pixelChange * 0.00002f;
                }
                if (w->StretchY())
                {
                    float pixelChange = (deltaScale.y - 1.0f) * currentSize.y;
                    scaledSize.y = currentSize.y + pixelChange * 0.00002f;
                }

                w->SetSize(scaledSize.x, scaledSize.y);
            }
        }
    }

    // Gizmo manipulation ended - commit to undo system
    if (!isUsing && wasUsing2D && !originalStates.empty())
    {
        // Collect new transforms
        std::vector<WidgetTransformState> newStates;
        for (const WidgetTransformState& orig : originalStates)
        {
            WidgetTransformState state;
            state.widget = orig.widget;
            state.offset = orig.widget->GetOffset();
            state.size = orig.widget->GetSize();
            state.rotation = orig.widget->GetRotation();
            newStates.push_back(state);
        }

        // Check if any transform actually changed
        bool anyChanged = false;
        for (size_t i = 0; i < originalStates.size(); ++i)
        {
            if (originalStates[i].offset != newStates[i].offset ||
                originalStates[i].size != newStates[i].size ||
                originalStates[i].rotation != newStates[i].rotation)
            {
                anyChanged = true;
                break;
            }
        }

        if (anyChanged)
        {
            // Restore original transforms
            for (const WidgetTransformState& orig : originalStates)
            {
                orig.widget->SetOffset(orig.offset.x, orig.offset.y);
                orig.widget->SetSize(orig.size.x, orig.size.y);
                orig.widget->SetRotation(orig.rotation);
            }

            // Build widget and transform lists for batched undo
            std::vector<Widget*> widgets;
            std::vector<WidgetTransformData> transforms;
            for (const WidgetTransformState& newState : newStates)
            {
                widgets.push_back(newState.widget);
                WidgetTransformData data;
                data.mOffset = newState.offset;
                data.mSize = newState.size;
                data.mRotation = newState.rotation;
                transforms.push_back(data);
            }

            // Commit all changes as a single undo action
            ActionManager::Get()->EXE_EditWidgetTransforms(widgets, transforms);
        }

        originalStates.clear();
    }

    wasUsing2D = isUsing;
}

static std::string GetDefaultEditorFontPath()
{

    if (!SYS_DoesFileExist("Engine/Assets/Fonts/F_InterRegular18.ttf", false)) {
        return "";
    }

    return SYS_GetAbsolutePath("Engine/Assets/Fonts/F_InterRegular18.ttf");
}

static std::string ResolveEditorFontName()
{
    const EngineConfig* config = GetEngineConfig();
    if (config != nullptr && !config->mCurrentFont.empty())
    {
        return config->mCurrentFont;
    }

    std::string savedFont = ThemeModule::LoadSavedFontPreference();
    if (savedFont.empty())
    {
        savedFont = "Default";
    }

    GetMutableEngineConfig()->mCurrentFont = savedFont;
    return savedFont;
}

static std::string ResolveEditorFontPath()
{
    const std::string defaultPath = GetDefaultEditorFontPath();
    const std::string desiredFontName = ResolveEditorFontName();

    if (desiredFontName.empty() || desiredFontName == "Default")
    {
        return defaultPath;
    }

    const std::string relativePath = "Engine/Assets/Fonts/" + desiredFontName;
    const std::string absolutePath = SYS_GetAbsolutePath(relativePath.c_str());

    if (!SYS_DoesFileExist(absolutePath.c_str(), false))
    {
        LogWarning("Editor font '%s' not found at %s. Falling back to default font.", desiredFontName.c_str(), absolutePath.c_str());
        GetMutableEngineConfig()->mCurrentFont = "Default";
        return defaultPath;
    }

    return absolutePath;
}

static std::string ResolveEditorIconFontPath()
{
    const std::string relativePath = "Engine/Assets/Fonts/PolyphaseEngineIcons.ttf";
    const std::string absolutePath = SYS_GetAbsolutePath(relativePath.c_str());

    if (!SYS_DoesFileExist(absolutePath.c_str(), false))
    {
        LogWarning("Editor Icon font '%s' not found at %s.", relativePath.c_str(), absolutePath.c_str());
        return "";
    }

    LogDebug("Using editor icon font at %s", absolutePath.c_str());
    return absolutePath;
}

void EditorImguiInit()
{
    if (IsHeadless())
    {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Disabling keyboard controls because it interferes with Alt hotkeys.
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui default style
    ImGui::StyleColorsDark();

    if (GetFeatureFlagsEditor().mShowTheming == true) {
       
        std::string fontPath = ResolveEditorFontPath();
        ImFont* myFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 15.0f);
        if (myFont == nullptr)
        {
            const std::string defaultFontPath = GetDefaultEditorFontPath();
            myFont = io.Fonts->AddFontFromFileTTF(defaultFontPath.c_str(), 15.0f);
            if (myFont == nullptr)
            {
                LogError("Failed to load editor font from %s", defaultFontPath.c_str());
            }
        }
        }


    std::string iconFontPath = ResolveEditorIconFontPath();
    if (!iconFontPath.empty())
    {
        MergePolyphaseIcons(io.Fonts, 14.0f, iconFontPath.c_str());
    }

    // Terminal panel font: load Roboto Mono with an extended glyph range
    // covering the Unicode blocks Ink-rendered TUI apps (claude, etc.) use
    // for layout. Loaded as a separate ImFont so it only applies inside the
    // terminal panel via PushFont/PopFont.
    {
        const std::string termFontPath =
            SYS_GetAbsolutePath("Engine/Assets/Fonts/F_RobotoMono16.ttf");
        if (SYS_DoesFileExist(termFontPath.c_str(), false))
        {
            // Build the glyph range. Static so the lifetime extends past
            // AddFontFromFileTTF, which only stores a pointer to it.
            static const ImWchar kTerminalRanges[] = {
                0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
                0x2010, 0x205F, // General Punctuation (dashes, quotes, ellipsis)
                0x2190, 0x21FF, // Arrows
                0x2200, 0x22FF, // Mathematical Operators
                0x2300, 0x23FF, // Miscellaneous Technical
                0x2500, 0x257F, // Box Drawing
                0x2580, 0x259F, // Block Elements (incl. quadrants)
                0x25A0, 0x25FF, // Geometric Shapes
                0x2600, 0x26FF, // Miscellaneous Symbols
                0x2700, 0x27BF, // Dingbats (spinners ✶✷✸✹✺✻ live here)
                0,
            };

            ImFontConfig cfg;
            cfg.OversampleH = 1;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = true;

            sTerminalFont = io.Fonts->AddFontFromFileTTF(
                termFontPath.c_str(), 15.0f, &cfg, kTerminalRanges);
            if (sTerminalFont == nullptr)
            {
                LogWarning("[CLI] Failed to load terminal font from %s",
                           termFontPath.c_str());
            }
            else
            {
                LogDebug("[CLI] Terminal font loaded from %s", termFontPath.c_str());
            }
        }
        else
        {
            LogWarning("[CLI] Terminal font %s not found; terminal panel will "
                       "use the editor default font and may render some glyphs "
                       "as '?'.", termFontPath.c_str());
        }
    }

    //ImGui::StyleColorsLight();

    // Override theme
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Tab] = kBgInactive;
    colors[ImGuiCol_TabHovered] = kBgHover;
    colors[ImGuiCol_TabActive] = kSelectedColor;



    // Set unactive window title bg equal to active title.
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_TitleBgActive];

    RegisterLogCallback(DebugLogWindow::LogCallback);

    ControllerServer::Create();

    GetScriptEditorWindow()->Init();
    GetLuaDebuggerPanel()->Init();

    ImGui::InitDock();

    // Check imgui.ini for stale dock layout BEFORE first NewFrame() loads it.
    // If dock panel names changed (e.g. icons added), the saved layout won't
    // match and causes crashes.  Delete the file so ImGui starts fresh.
    ValidateDockLayoutIni();
}

ImFont* GetEditorTerminalFont()
{
    return sTerminalFont;
}

// Open the host OS file manager and select / reveal the given path.
//   Windows : explorer /select,"<path>"        — selects file, opens parent
//   Linux   : xdg-open "<parentDir>"            — open the containing folder
//             (no portable "select file" verb across desktop environments)
static void RevealPathInOSFileManager(const std::string& path)
{
    if (path.empty()) return;

    std::string absPath = SYS_GetAbsolutePath(path);

#if PLATFORM_WINDOWS
    for (char& c : absPath) { if (c == '/') c = '\\'; }
    SYS_Exec(("explorer /select,\"" + absPath + "\"").c_str());
#elif PLATFORM_LINUX
    std::string dirPath = absPath;
    size_t lastSlash = absPath.find_last_of('/');
    if (lastSlash != std::string::npos) dirPath = absPath.substr(0, lastSlash);
    SYS_Exec(("xdg-open \"" + dirPath + "\" &").c_str());
#endif
}

// Build the "manual nuke" command the user can run from a privileged shell.
// Includes path-separator fix-ups and per-OS shell selection.
static std::string MakeIntermediateNukeCommand(const std::string& intermediateDir,
                                               const char*& outShellLabel)
{
    std::string dir = intermediateDir;

    // Strip trailing slash for nicer command output, but keep the leading
    // drive letter (Windows) or root slash (POSIX).
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();

#if PLATFORM_WINDOWS
    for (char& c : dir) { if (c == '/') c = '\\'; }
    outShellLabel = "cmd.exe (Run as administrator)";
    return "rmdir /s /q \"" + dir + "\"";
#elif PLATFORM_LINUX
    outShellLabel = "Terminal";
    return "sudo rm -rf \"" + dir + "\"";
#else
    outShellLabel = "shell";
    return "rm -rf \"" + dir + "\"";
#endif
}

static void DrawNativeAddonBuildBlockedModal()
{
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr) return;

    const bool blocked = nam->IsBuildBlocked();
    const char* kPopupName = "Native Addon Build Blocked##NativeAddonBlockedModal";

    if (blocked && !ImGui::IsPopupOpen(kPopupName))
    {
        ImGui::OpenPopup(kPopupName);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(820, 480), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal(kPopupName, nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!blocked)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const auto& info = nam->GetBuildBlocked();

        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f),
                           "Build of '%s' is paused — intermediate files are locked.",
                           info.mAddonId.c_str());
        ImGui::Separator();

        ImGui::TextWrapped(
            "The compiler/linker can't overwrite these files because another "
            "process is holding them open. Even an Administrator 'rmdir' will "
            "report \"Access is denied\" — the lock is on a file handle, not a "
            "permission. The two usual suspects:");
        ImGui::Bullet();
        ImGui::TextWrapped(
            "Visual Studio debugger attached to Polyphase.exe. VS auto-loads "
            "the addon's .pdb for source-level debugging and keeps it open "
            "until you Stop Debugging (Shift+F5) or detach.");
        ImGui::Bullet();
        ImGui::TextWrapped(
            "mspdbsrv.exe — the PDB server the linker spawns. It hangs around "
            "between builds and can hold a stale handle on the old .pdb. "
            "Killing it is safe; it respawns when next needed.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "The simplest fix is to delete the addon's entire Intermediate "
            "folder once the holder is stopped — the build system recreates it. "
            "Use the helpers below, or run the command in a privileged shell, "
            "then click Retry.");

        ImGui::Spacing();
        ImGui::Separator();

        // ---- "Manual nuke" command box ----
        const char* shellLabel = "shell";
        const std::string nukeCmd = MakeIntermediateNukeCommand(
            info.mIntermediateDir, shellLabel);

        ImGui::TextDisabled("Run in %s:", shellLabel);
#if PLATFORM_WINDOWS
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                           " (must be Administrator)");
#elif PLATFORM_LINUX
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                           " (sudo prompts for password)");
#endif

        // Render the command in a read-only multiline input so it's easy to
        // visually inspect and select. Uses a sized buffer copy because
        // InputTextMultiline takes a non-const buffer.
        {
            static thread_local std::string sCmdBuf;
            sCmdBuf = nukeCmd;
            // ReadOnly + AutoSelectAll on focus makes drag-select natural.
            ImGui::InputTextMultiline(
                "##NABlockedNukeCmd",
                sCmdBuf.data(), sCmdBuf.size() + 1,
                ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2.5f),
                ImGuiInputTextFlags_ReadOnly);
        }

        if (ImGui::Button("Copy command", ImVec2(160, 0)))
        {
            ImGui::SetClipboardText(nukeCmd.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Intermediate folder", ImVec2(220, 0)))
        {
            RevealPathInOSFileManager(info.mIntermediateDir);
        }
#if PLATFORM_WINDOWS
        ImGui::SameLine();
        // mspdbsrv.exe is the most common silent culprit on Windows. Killing it
        // is safe — the linker respawns it on next invocation. Run with /F so
        // it gets terminated even if it's busy on something else.
        if (ImGui::Button("Kill mspdbsrv.exe", ImVec2(180, 0)))
        {
            SYS_Exec("taskkill /F /IM mspdbsrv.exe");
            LogDebug("Sent taskkill to mspdbsrv.exe — click Retry to re-sweep.");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(
                "Kills the Microsoft PDB server process if it's running.\n"
                "Safe — it respawns when the next link needs it.\n"
                "Click Retry afterwards to re-sweep.");
        }
#endif

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextDisabled("Locked files (%d) — click a row to reveal in the OS file manager:",
                            (int)info.mLockedFiles.size());

        // Reserve room at the bottom for the action-button row.
        const float buttonRowHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        ImGui::BeginChild("##NABlockedFiles",
                          ImVec2(0, -buttonRowHeight),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        for (size_t i = 0; i < info.mLockedFiles.size(); ++i)
        {
            const std::string& path = info.mLockedFiles[i];
            ImGui::PushID((int)i);
            // Selectable spans the full row width; clicking reveals the file
            // in Explorer / Finder / xdg-open.
            if (ImGui::Selectable(path.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                RevealPathInOSFileManager(path);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Click to reveal in OS file manager");
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        if (ImGui::Button("Retry", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            // Defer the call until after EndPopup so the modal isn't redrawn
            // mid-state-change. RetryBlockedBuild may set mBlocked again if
            // files are still locked, which will reopen the popup next frame.
            nam->RetryBlockedBuild();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            nam->CancelBlockedBuild();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void DrawNativeAddonBuildModal()
{
    NativeAddonManager* nam = NativeAddonManager::Get();
    if (nam == nullptr) return;

    const bool building = nam->IsBuildingAsync();
    const char* kPopupName = "Building Native Addons##NativeAddonBuildModal";

    if (building && !ImGui::IsPopupOpen(kPopupName))
    {
        ImGui::OpenPopup(kPopupName);
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 360), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal(kPopupName, nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!building)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        std::string addonId = nam->GetAsyncBuildAddonId();
        int idx   = nam->GetAsyncBuildIndex();
        int total = nam->GetAsyncBuildTotal();

        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f),
                           "Building '%s'  (%d / %d)",
                           addonId.empty() ? "(starting...)" : addonId.c_str(),
                           idx, total);

        // Indeterminate progress bar — the build script is opaque and we
        // don't get intermediate progress out of cl.exe, so just animate.
        float pct = (total > 0) ? (float)(idx - 1) / (float)total : 0.0f;
        if (idx > 0 && idx <= total)
        {
            // Visually, while addon N is building, the bar sits at (N-1)/total
            // on the "completed" side; bumps to N/total once that addon finishes.
            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%d / %d", idx - 1, total);
            ImGui::ProgressBar(pct, ImVec2(-FLT_MIN, 0), overlay);
        }
        else
        {
            ImGui::ProgressBar(0.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Build output:");

        std::string output = nam->GetAsyncBuildOutput();
        ImGui::BeginChild("##BuildOutput", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4.0f),
                          true, ImGuiWindowFlags_HorizontalScrollbar);
        if (output.empty())
        {
            ImGui::TextDisabled("(waiting for output...)");
        }
        else
        {
            ImGui::TextUnformatted(output.c_str());
            // Auto-scroll to bottom while building.
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("This window closes automatically when all addons finish.");

        ImGui::EndPopup();
    }
}

void EditorImguiDraw()
{
    EngineState* engState = GetEngineState();

    ImGuiIO& io = ImGui::GetIO();
    float interfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (interfaceScale == 0.0f)
    {
        interfaceScale = 1.0f;
    }

    io.PolyphaseInterfaceScale = interfaceScale;
    io.DisplaySize = ImVec2(engState->mWindowWidth / interfaceScale, engState->mWindowHeight / interfaceScale);
    io.DisplayFramebufferScale = ImVec2(interfaceScale, interfaceScale);

    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    if (EditorIsInterfaceVisible())
    {
        DrawMainMenuBar();
        DrawDockspace();

        // Native-addon build progress (only renders while a build queue
        // is active; closes itself when the queue drains).
        DrawNativeAddonBuildModal();

        // Surface locked-intermediate-file blockers (LNK1201 etc.) with a
        // Retry/Cancel prompt so the user can release the lock and resume.
        DrawNativeAddonBuildBlockedModal();

        if (GetEditorState()->mShowTimelinePanel)
        {
            DrawTimelinePanel();
        }


        // Draw ImGuizmo gizmos for selected 3D nodes
        DrawImGuizmo();
        // Draw ImGuizmo gizmos for selected 2D widgets
        DrawImGuizmo2D();

        PaintMode paintMode = GetEditorState()->GetPaintMode();
        if (paintMode == PaintMode::Color)
        {
            DrawPaintColorsPanel();
        }
        else if (paintMode == PaintMode::Instance)
        {
            DrawPaintInstancesPanel();
        }
        else if (paintMode == PaintMode::Voxel)
        {
            // Voxel sculpt panel
            ImGui::SetNextWindowPos(ImVec2(210.0f, GetTopBarHeight()));
            ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f));
            ImGui::Begin("Voxel Sculpt", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

            VoxelSculptManager* mgr = GetEditorState()->mVoxelSculptManager;

            Node* sel = GetEditorState()->GetSelectedNode();
            if (sel != nullptr && sel->GetType() == Voxel3D::GetStaticType())
            {
                Voxel3D* voxel = static_cast<Voxel3D*>(sel);

                // Get atlas ImTextureID for tile previews
                static ImTextureID sAtlasTexId = 0;
                static Texture* sLastAtlasTex = nullptr;
                Texture* atlasTex = voxel->GetAtlasTexture();
                if (atlasTex != sLastAtlasTex)
                {
                    if (sAtlasTexId != 0)
                    {
                        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sAtlasTexId);
                        sAtlasTexId = 0;
                    }
                    if (atlasTex != nullptr)
                    {
                        TextureResource* res = atlasTex->GetResource();
                        if (res != nullptr && res->mImage != nullptr)
                        {
                            sAtlasTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                                res->mImage->GetSampler(),
                                res->mImage->GetView(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        }
                    }
                    sLastAtlasTex = atlasTex;
                }

                uint32_t tilesX = voxel->mAtlasTilesX;
                uint32_t tilesY = voxel->mAtlasTilesY;

                // Lambda: draw a tile preview button, returns true if clicked
                auto DrawTileButton = [&](const char* id, int32_t tileIdx, float size) -> bool
                {
                    bool clicked = false;
                    if (sAtlasTexId != 0 && tilesX > 0 && tilesY > 0 && tileIdx >= 0)
                    {
                        int col = tileIdx % tilesX;
                        int row = tileIdx / tilesX;
                        float u0 = float(col) / float(tilesX);
                        float v0 = float(row) / float(tilesY);
                        float u1 = float(col + 1) / float(tilesX);
                        float v1 = float(row + 1) / float(tilesY);

                        ImGui::PushID(id);
                        clicked = ImGui::ImageButton(id, sAtlasTexId, ImVec2(size, size), ImVec2(u0, v0), ImVec2(u1, v1));
                        ImGui::PopID();

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("Tile %d (Row %d, Col %d)", tileIdx, row, col);
                            ImGui::Image(sAtlasTexId, ImVec2(64, 64), ImVec2(u0, v0), ImVec2(u1, v1));
                            ImGui::EndTooltip();
                        }
                    }
                    else
                    {
                        ImGui::PushID(id);
                        clicked = ImGui::Button("?", ImVec2(size + 8, size + 8));
                        ImGui::PopID();
                    }
                    return clicked;
                };

                // Lambda: tile picker popup - shows atlas grid, returns selected tile
                auto DrawTilePickerPopup = [&](const char* popupId, int32_t& tileIdx)
                {
                    if (ImGui::BeginPopup(popupId))
                    {
                        ImGui::Text("Click a tile:");
                        if (sAtlasTexId != 0 && tilesX > 0 && tilesY > 0)
                        {
                            float tileDrawSize = 24.0f;
                            float gridW = tilesX * tileDrawSize;
                            float gridH = tilesY * tileDrawSize;
                            float maxW = 400.0f;
                            float maxH = 300.0f;

                            ImGui::BeginChild("TileGrid", ImVec2(glm::min(gridW + 16.0f, maxW), glm::min(gridH + 16.0f, maxH)), true, ImGuiWindowFlags_HorizontalScrollbar);

                            ImVec2 origin = ImGui::GetCursorScreenPos();
                            ImGui::Image(sAtlasTexId, ImVec2(gridW, gridH));
                            bool imageHovered = ImGui::IsItemHovered();

                            // Grid overlay
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            for (uint32_t gx = 0; gx <= tilesX; ++gx)
                                dl->AddLine(ImVec2(origin.x + gx * tileDrawSize, origin.y), ImVec2(origin.x + gx * tileDrawSize, origin.y + gridH), IM_COL32(255,255,255,30));
                            for (uint32_t gy = 0; gy <= tilesY; ++gy)
                                dl->AddLine(ImVec2(origin.x, origin.y + gy * tileDrawSize), ImVec2(origin.x + gridW, origin.y + gy * tileDrawSize), IM_COL32(255,255,255,30));

                            // Highlight current
                            if (tileIdx >= 0 && (uint32_t)tileIdx < tilesX * tilesY)
                            {
                                int sc = tileIdx % tilesX;
                                int sr = tileIdx / tilesX;
                                dl->AddRect(ImVec2(origin.x + sc * tileDrawSize, origin.y + sr * tileDrawSize),
                                            ImVec2(origin.x + (sc+1) * tileDrawSize, origin.y + (sr+1) * tileDrawSize),
                                            IM_COL32(0, 255, 0, 255), 0, 0, 2.0f);
                            }

                            // Click detection
                            ImVec2 mp = ImGui::GetMousePos();
                            if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            {
                                int clickCol = int((mp.x - origin.x) / tileDrawSize);
                                int clickRow = int((mp.y - origin.y) / tileDrawSize);
                                tileIdx = clickRow * tilesX + clickCol;
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::EndChild();
                        }
                        else
                        {
                            ImGui::Text("No atlas texture set.");
                        }
                        ImGui::EndPopup();
                    }
                };

                // Mode selector
                int mode = (int)mgr->mOptions.mMode;
                ImGui::RadioButton("Add", &mode, 0); ImGui::SameLine();
                ImGui::RadioButton("Remove", &mode, 1); ImGui::SameLine();
                ImGui::RadioButton("Paint", &mode, 2);
                mgr->mOptions.mMode = (VoxelSculptMode)mode;

                // Brush radius
                ImGui::SliderInt("Radius", &mgr->mOptions.mBrushRadius, 1, 8);

                // Material ID with step buttons (for Add and Paint modes)
                if (mgr->mOptions.mMode != VoxelSculptMode::Remove)
                {
                    int matId = (int)mgr->mOptions.mMaterialId;

                    ImGui::Text("Material:");
                    ImGui::SameLine();
                    if (ImGui::Button("<##MatPrev")) { matId--; if (matId < 1) matId = 255; }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("##MatId", &matId, 0, 0);
                    ImGui::SameLine();
                    if (ImGui::Button(">##MatNext")) { matId++; if (matId > 255) matId = 1; }

                    if (matId < 1) matId = 1;
                    if (matId > 255) matId = 255;
                    mgr->mOptions.mMaterialId = (uint8_t)matId;

                    // Show current material's tile preview
                    const VoxelMaterialInfo& curInfo = voxel->GetMaterialInfo(mgr->mOptions.mMaterialId);
                    if (curInfo.mUseTexture && sAtlasTexId != 0)
                    {
                        ImGui::SameLine();
                        DrawTileButton("##CurMatPreview", curInfo.mAtlasTile[2], 20.0f);
                    }
                }

                // Hover info
                if (mgr->mHoverValid)
                {
                    ImGui::Separator();
                    ImGui::Text("Voxel: %d, %d, %d", mgr->mHoverVoxel.x, mgr->mHoverVoxel.y, mgr->mHoverVoxel.z);
                }

                // Buttons
                ImGui::Separator();
                if (ImGui::Button("Rebuild Mesh"))
                {
                    voxel->MarkDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Mesh"))
                {
                    voxel->Fill(0);
                    voxel->MarkDirty();
                }

                // Material palette editor
                if (ImGui::CollapsingHeader("Material Palette", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int numConfigured = 0;
                    for (int i = 1; i < 256; ++i)
                    {
                        if (voxel->GetMaterialInfo(static_cast<VoxelType>(i)).mUseTexture)
                            numConfigured++;
                    }
                    ImGui::Text("Configured: %d materials", numConfigured);

                    for (int i = 1; i <= 16; ++i)
                    {
                        ImGui::PushID(i);
                        const VoxelMaterialInfo& info = voxel->GetMaterialInfo(static_cast<VoxelType>(i));

                        char label[32];
                        snprintf(label, sizeof(label), "Mat %d", i);

                        bool isOpen = ImGui::TreeNode(label);

                        // Show tile thumbnails on the same line as the tree node
                        if (info.mUseTexture && sAtlasTexId != 0)
                        {
                            ImGui::SameLine();
                            DrawTileButton("##TopPrev", info.mAtlasTile[0], 16.0f);
                            ImGui::SameLine();
                            DrawTileButton("##BotPrev", info.mAtlasTile[1], 16.0f);
                            ImGui::SameLine();
                            DrawTileButton("##SidePrev", info.mAtlasTile[2], 16.0f);
                        }

                        if (isOpen)
                        {
                            bool enabled = info.mUseTexture;
                            if (Polyphase::Checkbox("Enabled", &enabled))
                            {
                                if (enabled)
                                    voxel->SetMaterialTexture(static_cast<VoxelType>(i), info.mAtlasTile[0], info.mAtlasTile[1], info.mAtlasTile[2]);
                                else
                                    voxel->DisableMaterialTexture(static_cast<VoxelType>(i));
                            }

                            if (enabled)
                            {
                                int top = info.mAtlasTile[0];
                                int bottom = info.mAtlasTile[1];
                                int side = info.mAtlasTile[2];

                                // Top tile
                                ImGui::Text("Top:");
                                ImGui::SameLine();
                                if (DrawTileButton("##TopBtn", top, 28.0f))
                                    ImGui::OpenPopup("##PickTop");
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(50.0f);
                                ImGui::InputInt("##TopIdx", &top, 0, 0);
                                DrawTilePickerPopup("##PickTop", top);

                                // Bottom tile
                                ImGui::Text("Bot:");
                                ImGui::SameLine();
                                if (DrawTileButton("##BotBtn", bottom, 28.0f))
                                    ImGui::OpenPopup("##PickBot");
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(50.0f);
                                ImGui::InputInt("##BotIdx", &bottom, 0, 0);
                                DrawTilePickerPopup("##PickBot", bottom);

                                // Side tile
                                ImGui::Text("Side:");
                                ImGui::SameLine();
                                if (DrawTileButton("##SideBtn", side, 28.0f))
                                    ImGui::OpenPopup("##PickSide");
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(50.0f);
                                ImGui::InputInt("##SideIdx", &side, 0, 0);
                                DrawTilePickerPopup("##PickSide", side);

                                if (top != info.mAtlasTile[0] || bottom != info.mAtlasTile[1] || side != info.mAtlasTile[2])
                                {
                                    voxel->SetMaterialTexture(static_cast<VoxelType>(i), top, bottom, side);
                                }
                            }

                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }
                }
            }
            else
            {
                ImGui::TextWrapped("Select a Voxel3D node to begin sculpting.");
            }

            ImGui::End();
        }
        else if (paintMode == PaintMode::Terrain)
        {
            // Terrain sculpt panel
            ImGui::SetNextWindowPos(ImVec2(210.0f, GetTopBarHeight()), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(340.0f, 500.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 200.0f), ImVec2(600.0f, 1200.0f));
            ImGui::Begin("Terrain Sculpt", nullptr, 0);

            TerrainSculptManager* mgr = GetEditorState()->mTerrainSculptManager;

            Node* sel = GetEditorState()->GetSelectedNode();
            if (sel != nullptr && sel->GetType() == Terrain3D::GetStaticType())
            {
                Terrain3D* terrain = static_cast<Terrain3D*>(sel);

                // Get atlas ImTextureID for tile previews
                static ImTextureID sTerrainAtlasTexId = 0;
                static Texture* sLastTerrainAtlasTex = nullptr;
                Texture* atlasTex = terrain->mAtlasTexture.Get<Texture>();
                if (atlasTex != sLastTerrainAtlasTex)
                {
                    if (sTerrainAtlasTexId != 0)
                    {
                        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sTerrainAtlasTexId);
                        sTerrainAtlasTexId = 0;
                    }
                    if (atlasTex != nullptr)
                    {
                        TextureResource* res = atlasTex->GetResource();
                        if (res != nullptr && res->mImage != nullptr)
                        {
                            sTerrainAtlasTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                                res->mImage->GetSampler(),
                                res->mImage->GetView(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        }
                    }
                    sLastTerrainAtlasTex = atlasTex;
                }

                uint32_t tilesX = terrain->mAtlasTilesX;
                uint32_t tilesY = terrain->mAtlasTilesY;

                // Lambda: draw a tile preview image for a given tile index
                auto DrawSlotTilePreview = [&](int32_t tileIdx, float size)
                {
                    if (sTerrainAtlasTexId != 0 && tilesX > 0 && tilesY > 0 && tileIdx >= 0)
                    {
                        int col = tileIdx % tilesX;
                        int row = tileIdx / tilesX;
                        float u0 = float(col) / float(tilesX);
                        float v0 = float(row) / float(tilesY);
                        float u1 = float(col + 1) / float(tilesX);
                        float v1 = float(row + 1) / float(tilesY);
                        ImGui::Image(sTerrainAtlasTexId, ImVec2(size, size), ImVec2(u0, v0), ImVec2(u1, v1));
                    }
                    else
                    {
                        ImGui::Dummy(ImVec2(size, size));
                    }
                };

                // Mode selection
                const char* brushTypeNames[] =
                {
                    ICON_TERRAIN_RAISE " Raise",
                    ICON_TERRAIN_LOWER " Lower",
                    ICON_TERRAIN_FLATTEN " Flatten",
                    ICON_TERRAIN_SMOOTH " Smooth",
                    ICON_HEROICONS_PAINT_BRUSH_20_SOLID " Paint Material"
                };

                int mode = (int)mgr->mOptions.mMode;
                if (mode < 0 || mode >= (int)TerrainSculptMode::Count)
                {
                    mode = (int)TerrainSculptMode::Raise;
                }

                ImGui::Text(ICON_HEROICONS_PAINT_BRUSH_20_SOLID " Brush");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::Combo("##BrushType", &mode, brushTypeNames, IM_ARRAYSIZE(brushTypeNames));
                mgr->mOptions.mMode = (TerrainSculptMode)mode;

                ImGui::Separator();

                // Brush settings
                ImGui::SliderFloat("Radius", &mgr->mOptions.mBrushRadius, 0.5f, 50.0f);
                ImGui::SliderFloat("Strength", &mgr->mOptions.mBrushStrength, 0.01f, 1.0f);
                ImGui::SliderFloat("Falloff", &mgr->mOptions.mBrushFalloff, 0.0f, 1.0f);

                if (mgr->mOptions.mMode == TerrainSculptMode::Flatten)
                {
                    ImGui::SliderFloat("Target Height", &mgr->mOptions.mFlattenHeight, -10.0f, 10.0f);
                }

                // Brush mask
                ImGui::Separator();
                ImGui::Text("Brush Mask");
                Texture* maskTex = mgr->mOptions.mBrushMask.Get<Texture>();
                if (maskTex != nullptr)
                {
                    // Show preview of the mask texture
                    static ImTextureID sBrushMaskTexId = 0;
                    static Texture* sLastBrushMaskTex = nullptr;
                    if (maskTex != sLastBrushMaskTex)
                    {
                        if (sBrushMaskTexId != 0)
                        {
                            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sBrushMaskTexId);
                            sBrushMaskTexId = 0;
                        }
                        TextureResource* res = maskTex->GetResource();
                        if (res != nullptr && res->mImage != nullptr)
                        {
                            sBrushMaskTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                                res->mImage->GetSampler(),
                                res->mImage->GetView(),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        }
                        sLastBrushMaskTex = maskTex;
                    }

                    if (sBrushMaskTexId != 0)
                    {
                        ImGui::Image(sBrushMaskTexId, ImVec2(48, 48));
                        ImGui::SameLine();
                    }
                    ImGui::BeginGroup();
                    ImGui::Text("%s", maskTex->GetName().c_str());
                    if (ImGui::Button("Clear##BrushMask"))
                    {
                        mgr->mOptions.mBrushMask = nullptr;
                        if (sBrushMaskTexId != 0)
                        {
                            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sBrushMaskTexId);
                            sBrushMaskTexId = 0;
                        }
                        sLastBrushMaskTex = nullptr;
                    }
                    ImGui::EndGroup();
                }
                else
                {
                    ImGui::Text("(circular falloff)");
                }

                // Drop target: drag a Texture asset from the Assets panel onto this area
                ImGui::Button("Drop Brush Mask Here##BrushMaskDrop", ImVec2(-1, 0));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
                    {
                        AssetStub* droppedStub = *(AssetStub**)payload->Data;
                        if (droppedStub != nullptr && droppedStub->mType == Texture::GetStaticType())
                        {
                            Texture* droppedTex = static_cast<Texture*>(LoadAsset(droppedStub->mName));
                            if (droppedTex != nullptr)
                            {
                                mgr->mOptions.mBrushMask = droppedTex;
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // Stamp brush mask onto entire terrain as heightmap
                if (maskTex != nullptr)
                {
                    if (ImGui::Button("Stamp as Height"))
                    {
                        terrain->SetHeightFromTexture(maskTex);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Height"))
                    {
                        // Additively blend the mask texture onto existing heights
                        if (!maskTex->GetPixels().empty())
                        {
                            uint32_t texW = maskTex->GetWidth();
                            uint32_t texH = maskTex->GetHeight();
                            float strength = mgr->mOptions.mBrushStrength;
                            for (int32_t iz = 0; iz < terrain->mResolutionZ; ++iz)
                            {
                                for (int32_t ix = 0; ix < terrain->mResolutionX; ++ix)
                                {
                                    float u = (float)ix / (terrain->mResolutionX - 1);
                                    float v = (float)iz / (terrain->mResolutionZ - 1);
                                    uint32_t px = std::min((uint32_t)(u * (texW - 1)), texW - 1);
                                    uint32_t py = std::min((uint32_t)(v * (texH - 1)), texH - 1);
                                    uint32_t idx = (py * texW + px) * 4;
                                    if (idx < maskTex->GetPixels().size())
                                    {
                                        float texVal = maskTex->GetPixels()[idx] / 255.0f;
                                        float curH = terrain->GetHeight(ix, iz);
                                        terrain->SetHeight(ix, iz, curH + texVal * strength);
                                    }
                                }
                            }
                            terrain->MarkDirty();
                        }
                    }
                    if (terrain->mUseMaterialSlots)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Stamp as Splat"))
                        {
                            // Stamp the mask as splatmap weight for the active paint slot
                            if (!maskTex->GetPixels().empty())
                            {
                                uint32_t texW = maskTex->GetWidth();
                                uint32_t texH = maskTex->GetHeight();
                                int32_t slot = mgr->mOptions.mPaintSlot;
                                for (int32_t iz = 0; iz < terrain->mResolutionZ; ++iz)
                                {
                                    for (int32_t ix = 0; ix < terrain->mResolutionX; ++ix)
                                    {
                                        float u = (float)ix / (terrain->mResolutionX - 1);
                                        float v = (float)iz / (terrain->mResolutionZ - 1);
                                        uint32_t px = std::min((uint32_t)(u * (texW - 1)), texW - 1);
                                        uint32_t py = std::min((uint32_t)(v * (texH - 1)), texH - 1);
                                        uint32_t idx = (py * texW + px) * 4;
                                        if (idx < maskTex->GetPixels().size())
                                        {
                                            float texVal = maskTex->GetPixels()[idx] / 255.0f;
                                            if (texVal > 0.01f)
                                            {
                                                terrain->SetMaterialWeight(ix, iz, slot, texVal);
                                                // Normalize other slots
                                                float totalOther = 0.0f;
                                                for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                                                    if (s != slot) totalOther += terrain->GetMaterialWeight(ix, iz, s);
                                                if (totalOther > 0.0f)
                                                {
                                                    float scale = (1.0f - texVal) / totalOther;
                                                    for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                                                        if (s != slot)
                                                            terrain->SetMaterialWeight(ix, iz, s, terrain->GetMaterialWeight(ix, iz, s) * scale);
                                                }
                                            }
                                        }
                                    }
                                }
                                terrain->MarkDirty();
                                terrain->BakeAndSaveMap();
                            }
                        }
                    }
                }

                // Material painting options
                if (mgr->mOptions.mMode == TerrainSculptMode::PaintMaterial)
                {
                    ImGui::Separator();
                    ImGui::Text("Material Slot");
                    for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                    {
                        ImGui::PushID(s);
                        DrawSlotTilePreview(terrain->mSlotAtlasTile[s], 24.0f);
                        ImGui::SameLine();
                        char label[16];
                        snprintf(label, sizeof(label), "Slot %d", s);
                        ImGui::RadioButton(label, &mgr->mOptions.mPaintSlot, s);
                        if (s < Terrain3D::MAX_MATERIAL_SLOTS - 1) ImGui::SameLine();
                        ImGui::PopID();
                    }
                }

                // Slot tile assignment
                if (terrain->mUseMaterialSlots && terrain->mEnableAtlasTexturing)
                {
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Material Slot Tiles", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                        {
                            ImGui::PushID(s + 100);
                            DrawSlotTilePreview(terrain->mSlotAtlasTile[s], 32.0f);
                            ImGui::SameLine();
                            char label[32];
                            snprintf(label, sizeof(label), "Slot %d Tile", s);
                            ImGui::SetNextItemWidth(80.0f);
                            if (ImGui::InputInt(label, &terrain->mSlotAtlasTile[s], 1, 1))
                            {
                                int32_t maxTile = (int32_t)(tilesX * tilesY) - 1;
                                if (terrain->mSlotAtlasTile[s] < 0) terrain->mSlotAtlasTile[s] = 0;
                                if (terrain->mSlotAtlasTile[s] > maxTile) terrain->mSlotAtlasTile[s] = maxTile;
                                terrain->MarkDirty();
                            }
                            ImGui::PopID();
                        }
                    }
                }

                // Auto-generate splatmap from elevation/slope rules
                if (terrain->mUseMaterialSlots)
                {
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Auto-Generate Splatmap"))
                    {
                        const char* slotLabels[] = { "Slot 0", "Slot 1", "Slot 2", "Slot 3" };
                        for (int32_t s = 0; s < Terrain3D::MAX_MATERIAL_SLOTS; ++s)
                        {
                            ImGui::PushID(s);
                            DrawSlotTilePreview(terrain->mSlotAtlasTile[s], 20.0f);
                            ImGui::SameLine();
                            if (ImGui::TreeNode(slotLabels[s]))
                            {
                                Terrain3D::SlotRule& rule = terrain->mSlotRules[s];
                                ImGui::SliderFloat("Height Min", &rule.mHeightMin, 0.0f, 1.0f);
                                ImGui::SliderFloat("Height Max", &rule.mHeightMax, 0.0f, 1.0f);
                                ImGui::SliderFloat("Slope Min", &rule.mSlopeMin, 0.0f, 90.0f, "%.0f deg");
                                ImGui::SliderFloat("Slope Max", &rule.mSlopeMax, 0.0f, 90.0f, "%.0f deg");
                                ImGui::SliderFloat("Blend", &rule.mBlend, 0.0f, 0.5f);
                                ImGui::SliderFloat("Strength", &rule.mStrength, 0.0f, 2.0f);
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }

                        if (ImGui::Button("Generate Splatmap"))
                        {
                            terrain->GenerateSplatmapFromRules();
                            terrain->BakeAndSaveMap();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Generate (Preview)"))
                        {
                            terrain->GenerateSplatmapFromRules();
                            if (terrain->mBakeSplatmap)
                                terrain->BakeSplatmapTexture();
                        }

                        ImGui::TextWrapped("Height: 0=lowest, 1=highest. Slope: 0=flat, 90=cliff.");
                    }
                }

                // Mountain preset generator
                if (terrain->mUseMaterialSlots)
                {
                    if (ImGui::CollapsingHeader("Mountain Generate Splatmap"))
                    {
                        static int32_t mtSlotGround = 0;
                        static int32_t mtSlotMountainSide = 1;
                        static int32_t mtSlotMountainTop = 2;
                        static int32_t mtSlotPeak = 3;

                        static float mtGroundMax = 0.35f;
                        static float mtSideMinSlope = 25.0f;
                        static float mtTopMin = 0.55f;
                        static float mtPeakMin = 0.8f;
                        static float mtBlend = 0.12f;

                        // Ground
                        DrawSlotTilePreview(terrain->mSlotAtlasTile[mtSlotGround], 20.0f);
                        ImGui::SameLine();
                        if (ImGui::TreeNode("Ground (flat lowlands)"))
                        {
                            ImGui::SliderInt("Slot##Ground", &mtSlotGround, 0, 3);
                            ImGui::SliderFloat("Max Height##Ground", &mtGroundMax, 0.0f, 1.0f);
                            ImGui::TreePop();
                        }

                        // Mountain Side
                        DrawSlotTilePreview(terrain->mSlotAtlasTile[mtSlotMountainSide], 20.0f);
                        ImGui::SameLine();
                        if (ImGui::TreeNode("Mountain Side (steep slopes)"))
                        {
                            ImGui::SliderInt("Slot##Side", &mtSlotMountainSide, 0, 3);
                            ImGui::SliderFloat("Min Slope##Side", &mtSideMinSlope, 5.0f, 60.0f, "%.0f deg");
                            ImGui::TreePop();
                        }

                        // Mountain Top
                        DrawSlotTilePreview(terrain->mSlotAtlasTile[mtSlotMountainTop], 20.0f);
                        ImGui::SameLine();
                        if (ImGui::TreeNode("Mountain Top (high flat areas)"))
                        {
                            ImGui::SliderInt("Slot##Top", &mtSlotMountainTop, 0, 3);
                            ImGui::SliderFloat("Min Height##Top", &mtTopMin, 0.2f, 1.0f);
                            ImGui::TreePop();
                        }

                        // Peak
                        DrawSlotTilePreview(terrain->mSlotAtlasTile[mtSlotPeak], 20.0f);
                        ImGui::SameLine();
                        if (ImGui::TreeNode("Peak (highest points)"))
                        {
                            ImGui::SliderInt("Slot##Peak", &mtSlotPeak, 0, 3);
                            ImGui::SliderFloat("Min Height##Peak", &mtPeakMin, 0.5f, 1.0f);
                            ImGui::TreePop();
                        }

                        ImGui::SliderFloat("Blend##Mt", &mtBlend, 0.0f, 0.3f);

                        if (ImGui::Button("Generate Mountain"))
                        {
                            // Ground: low elevation, flat
                            terrain->mSlotRules[mtSlotGround] = { 0.0f, mtGroundMax, 0.0f, mtSideMinSlope, mtBlend, 1.0f };
                            // Mountain Side: steep slopes at any elevation
                            terrain->mSlotRules[mtSlotMountainSide] = { 0.0f, 1.0f, mtSideMinSlope, 90.0f, mtBlend, 1.2f };
                            // Mountain Top: high elevation, moderate slopes
                            terrain->mSlotRules[mtSlotMountainTop] = { mtTopMin, mtPeakMin, 0.0f, mtSideMinSlope + 10.0f, mtBlend, 1.0f };
                            // Peak: highest points, any slope
                            terrain->mSlotRules[mtSlotPeak] = { mtPeakMin, 1.0f, 0.0f, 90.0f, mtBlend, 1.5f };

                            terrain->GenerateSplatmapFromRules();
                            terrain->BakeAndSaveMap();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Preview Mountain"))
                        {
                            terrain->mSlotRules[mtSlotGround] = { 0.0f, mtGroundMax, 0.0f, mtSideMinSlope, mtBlend, 1.0f };
                            terrain->mSlotRules[mtSlotMountainSide] = { 0.0f, 1.0f, mtSideMinSlope, 90.0f, mtBlend, 1.2f };
                            terrain->mSlotRules[mtSlotMountainTop] = { mtTopMin, mtPeakMin, 0.0f, mtSideMinSlope + 10.0f, mtBlend, 1.0f };
                            terrain->mSlotRules[mtSlotPeak] = { mtPeakMin, 1.0f, 0.0f, 90.0f, mtBlend, 1.5f };

                            terrain->GenerateSplatmapFromRules();
                            if (terrain->mBakeSplatmap)
                                terrain->BakeSplatmapTexture();
                        }
                    }
                }

                // Terrain config
                ImGui::Separator();
                ImGui::Text("Terrain Config");
                ImGui::Text("Resolution: %d x %d", terrain->mResolutionX, terrain->mResolutionZ);
                ImGui::Text("World Size: %.1f x %.1f", terrain->mWorldWidth, terrain->mWorldDepth);
                ImGui::Text("Height Scale: %.2f", terrain->mHeightScale);

                // Actions
                ImGui::Separator();
                if (ImGui::Button("Flatten All"))
                {
                    terrain->FlattenAll(0.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Rebuild Mesh"))
                {
                    terrain->RebuildMesh();
                }
                if (terrain->mEnableAtlasTexturing && terrain->mBakeSplatmap)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Bake Splatmap"))
                    {
                        terrain->BakeSplatmapTexture();
                    }
                }

                if (terrain->mEnableAtlasTexturing && terrain->mUseMaterialSlots)
                {
                    if (ImGui::Button("Bake & Save Map"))
                    {
                        terrain->BakeAndSaveMap();
                    }
                    ImGui::SameLine();
                }
                if (ImGui::Button("Save Heightmap"))
                {
                    terrain->BakeAndSaveHeightmap();
                }

                // Warn if no baked map is saved
                if (terrain->mEnableAtlasTexturing && terrain->mUseMaterialSlots &&
                    terrain->mBakedMapTexture.Get<Texture>() == nullptr)
                {
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    ImGui::TextWrapped("No Baked Map saved. The terrain material won't display in PIE or runtime builds. Click \"Bake & Save Map\" to export.");
                    ImGui::PopStyleColor();
                }

                // Debug splatmap toggle
                ImGui::Separator();
                if (Polyphase::Checkbox("Debug Splatmap", &terrain->mDebugSplatmap))
                {
                    terrain->MarkDirty();
                }
                if (terrain->mDebugSplatmap)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(R=Slot0 G=Slot1 B=Slot2 A=Slot3)");
                }

                // Hover info
                if (mgr->mHoverValid)
                {
                    ImGui::Separator();
                    ImGui::Text("Grid: [%d, %d]", mgr->mHoverGridX, mgr->mHoverGridZ);
                    ImGui::Text("Height: %.3f", terrain->GetHeight(mgr->mHoverGridX, mgr->mHoverGridZ));
                    if (terrain->mUseMaterialSlots)
                    {
                        ImGui::Text("Weights: %.0f%% %.0f%% %.0f%% %.0f%%",
                            terrain->GetMaterialWeight(mgr->mHoverGridX, mgr->mHoverGridZ, 0) * 100.0f,
                            terrain->GetMaterialWeight(mgr->mHoverGridX, mgr->mHoverGridZ, 1) * 100.0f,
                            terrain->GetMaterialWeight(mgr->mHoverGridX, mgr->mHoverGridZ, 2) * 100.0f,
                            terrain->GetMaterialWeight(mgr->mHoverGridX, mgr->mHoverGridZ, 3) * 100.0f);
                    }
                }
            }
            else
            {
                ImGui::TextWrapped("Select a Terrain3D node to begin sculpting.");
            }

            ImGui::End();
        }
        else if (paintMode == PaintMode::TilePaint)
        {
            // Tile paint panel — Phase 2: tile palette grid, layer panel, fill tools, tag editing.
            ImGui::SetNextWindowPos(ImVec2(210.0f, GetTopBarHeight()), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(360.0f, 720.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f, 400.0f), ImVec2(720.0f, 1600.0f));
            ImGui::Begin("Tile Paint", nullptr, 0);

            TilePaintManager* mgr = GetEditorState()->mTilePaintManager;
            Node* sel = GetEditorState()->GetSelectedNode();
            if (sel != nullptr && sel->GetType() == TileMap2D::GetStaticType())
            {
                TileMap2D* tileMapNode = static_cast<TileMap2D*>(sel);
                TileMap* tileMap = tileMapNode->GetTileMap();
                TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

                // Tool selection
                ImGui::TextUnformatted("Tool:");
                struct TileToolEntry { TileSculptMode mode; const char* icon; const char* name; };
                static const TileToolEntry kTileTools[] = {
                    { TileSculptMode::Pencil,    ICON_MDI_PENCIL,                  "Pencil"    },
                    { TileSculptMode::Eraser,    ICON_MDI_ERASER,                  "Eraser"    },
                    { TileSculptMode::Picker,    ICON_BOXICONS_EYEDROPPER_FILLED,  "Picker"    },
                    { TileSculptMode::RectFill,  ICON_RECTFILL,                    "Rect Fill" },
                    { TileSculptMode::FloodFill, ICON_RI_PAINT_FILL,               "Flood Fill"},
                    { TileSculptMode::Line,      ICON_LINEICONS_VECTOR_NODES_7,    "Line"      },
                    { TileSculptMode::NineBox,   ICON_9POINT,                      "9 Box"     },
                    { TileSculptMode::Select,    ICON_RI_CURSOR_FILL,              "Select"    },
                    { TileSculptMode::Autotile,  ICON_FLUENT_MDL2_FIVE_TILE_GRID,  "Autotile"  },
                };

                const TileToolEntry* current = &kTileTools[0];
                for (const TileToolEntry& e : kTileTools)
                {
                    if (e.mode == mgr->mOptions.mMode) { current = &e; break; }
                }

                char previewLabel[64];
                snprintf(previewLabel, sizeof(previewLabel), "%s %s", current->icon, current->name);
                if (ImGui::BeginCombo("##TileTool", previewLabel))
                {
                    for (const TileToolEntry& e : kTileTools)
                    {
                        char itemLabel[64];
                        snprintf(itemLabel, sizeof(itemLabel), "%s %s", e.icon, e.name);
                        bool selected = (e.mode == mgr->mOptions.mMode);
                        if (ImGui::Selectable(itemLabel, selected))
                        {
                            mgr->mOptions.mMode = e.mode;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Cache the atlas descriptor via the shared TilePicker helper
                Texture* atlasTex = (tileSet != nullptr) ? tileSet->GetTexture() : nullptr;
                void* atlasImId = TilePicker::GetOrCreateImTextureID("TilePaintPanel", atlasTex);
                int32_t tilesX = (tileSet != nullptr) ? tileSet->GetAtlasGridSize().x : 0;
                int32_t tilesY = (tileSet != nullptr) ? tileSet->GetAtlasGridSize().y : 0;

                // Selected tile preview
                ImGui::Separator();
                ImGui::Text("Selected Tile: %d", mgr->mOptions.mSelectedTileIndex);
                ImGui::SameLine();
                TilePicker::DrawTileButton("##SelTilePreview", atlasImId,
                    mgr->mOptions.mSelectedTileIndex, tilesX, tilesY, 32.0f);

                // Tile palette grid
                if (ImGui::CollapsingHeader(ICON_FLUENT_MDL2_PICTURE_TILE " Tile Palette", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int32_t pickedIdx = mgr->mOptions.mSelectedTileIndex;
                    if (TilePicker::DrawInlineTileGrid("##TilePalette", atlasImId,
                            tilesX, tilesY, pickedIdx, 22.0f, 720.0f, 240.0f))
                    {
                        mgr->mOptions.mSelectedTileIndex = pickedIdx;
                    }
                }

                // Layer panel
                if (tileMap != nullptr && ImGui::CollapsingHeader(ICON_LAYERS " Layers", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int32_t numLayers = tileMap->GetNumLayers();
                    if (mgr->mOptions.mActiveLayer >= numLayers)
                        mgr->mOptions.mActiveLayer = numLayers - 1;
                    if (mgr->mOptions.mActiveLayer < 0)
                        mgr->mOptions.mActiveLayer = 0;

                    for (int32_t li = 0; li < numLayers; ++li)
                    {
                        TileMapLayer* layer = tileMap->GetLayer(li);
                        if (layer == nullptr) continue;

                        ImGui::PushID(li);

                        bool isActive = (mgr->mOptions.mActiveLayer == li);
                        if (ImGui::RadioButton("##Active", isActive))
                        {
                            mgr->mOptions.mActiveLayer = li;
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active layer");
                        ImGui::SameLine();

                        bool visible = layer->mVisible;
                        if (Polyphase::Checkbox("##Vis", &visible))
                        {
                            layer->mVisible = visible;
                            tileMapNode->MarkDirty();
                            tileMap->SetDirtyFlag();
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Visible");
                        ImGui::SameLine();

                        bool locked = layer->mLocked;
                        if (Polyphase::Checkbox("##Lock", &locked))
                        {
                            layer->mLocked = locked;
                            tileMap->SetDirtyFlag();
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Locked");
                        ImGui::SameLine();

                        char nameBuf[64];
                        strncpy(nameBuf, layer->mName.c_str(), sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        ImGui::SetNextItemWidth(140.0f);
                        if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf)))
                        {
                            layer->mName = nameBuf;
                            tileMap->SetDirtyFlag();
                        }
                        ImGui::SameLine();

                        if (ImGui::SmallButton("X") && numLayers > 1)
                        {
                            // We need a public way to remove layers from the asset.
                            // For now, just hide it; full delete lands when layer
                            // remove API is added to the asset.
                            layer->mVisible = false;
                        }

                        ImGui::PopID();
                    }

                    if (ImGui::Button(ICON_MDI_ANIMATION_PLUS " Layer"))
                    {
                        // Cheap version: append by setting a far-future cell index
                        // through the EnsureLayer pathway. Replace with a public
                        // AddLayer() API once metadata authoring lands.
                        TileCell empty;
                        tileMap->SetCell(0, 0, empty, numLayers);
                        tileMapNode->MarkDirty();
                        tileMap->SetDirtyFlag();
                    }
                }

                // 9-Box Brushes (Phase 3)
                if (tileSet != nullptr && ImGui::CollapsingHeader( ICON_9POINT " 9-Box Brushes"))
                {
                    auto& brushes = tileSet->GetNineBoxBrushesMutable();

                    if (ImGui::Button("+ New Brush"))
                    {
                        int32_t newIdx = tileSet->AddNineBoxBrush(ICON_HEROICONS_PAINT_BRUSH_20_SOLID);
                        mgr->mOptions.mActiveNineBoxIndex = newIdx;
                        tileSet->SetDirtyFlag();
                    }

                    ImGui::SameLine();
                    ImGui::Text("(%d defined)", int(brushes.size()));

                    int32_t removeBrushIdx = -1;
                    for (size_t bi = 0; bi < brushes.size(); ++bi)
                    {
                        NineBoxBrushDef& brush = brushes[bi];
                        ImGui::PushID(int(bi));

                        bool active = (mgr->mOptions.mActiveNineBoxIndex == int32_t(bi));
                        if (ImGui::RadioButton("##ActiveBrush", active))
                        {
                            mgr->mOptions.mActiveNineBoxIndex = int32_t(bi);
                        }
                        ImGui::SameLine();

                        char nameBuf[64];
                        strncpy(nameBuf, brush.mName.c_str(), sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        ImGui::SetNextItemWidth(140.0f);
                        if (ImGui::InputText("##BrushName", nameBuf, sizeof(nameBuf)))
                        {
                            brush.mName = nameBuf;
                            tileSet->SetDirtyFlag();
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) removeBrushIdx = int32_t(bi);

                        // 3x3 grid of slot pickers
                        int32_t* slots[9] = {
                            &brush.mTopLeft, &brush.mTop, &brush.mTopRight,
                            &brush.mLeft,    &brush.mCenter, &brush.mRight,
                            &brush.mBottomLeft, &brush.mBottom, &brush.mBottomRight
                        };
                        const char* slotPopupIds[9] = {
                            "##PickTL", "##PickTM", "##PickTR",
                            "##PickML", "##PickMM", "##PickMR",
                            "##PickBL", "##PickBM", "##PickBR"
                        };

                        for (int32_t row = 0; row < 3; ++row)
                        {
                            for (int32_t col = 0; col < 3; ++col)
                            {
                                int32_t slotIdx = row * 3 + col;
                                ImGui::PushID(slotIdx);
                                if (TilePicker::DrawTileButton("##slot", atlasImId,
                                        *slots[slotIdx], tilesX, tilesY, 28.0f))
                                {
                                    ImGui::OpenPopup(slotPopupIds[slotIdx]);
                                }
                                int32_t pickedSlot = *slots[slotIdx];
                                TilePicker::DrawTilePickerPopup(slotPopupIds[slotIdx],
                                    atlasImId, tilesX, tilesY, pickedSlot);
                                if (pickedSlot != *slots[slotIdx])
                                {
                                    *slots[slotIdx] = pickedSlot;
                                    tileSet->SetDirtyFlag();
                                }
                                ImGui::PopID();
                                if (col < 2) ImGui::SameLine();
                            }
                        }

                        ImGui::PopID();
                        ImGui::Separator();
                    }

                    if (removeBrushIdx >= 0)
                    {
                        tileSet->RemoveNineBoxBrush(removeBrushIdx);
                        if (mgr->mOptions.mActiveNineBoxIndex >= int32_t(brushes.size()))
                            mgr->mOptions.mActiveNineBoxIndex = int32_t(brushes.size()) - 1;
                        tileSet->SetDirtyFlag();
                    }

                }

                // Autotile Brushes (Phase 4)
                if (tileSet != nullptr && ImGui::CollapsingHeader(ICON_FLUENT_MDL2_FIVE_TILE_GRID " Autotile Brushes"))
                {
                    auto& sets = tileSet->GetAutotileSetsMutable();
                    if (ImGui::Button("+ New Autotile"))
                    {
                        int32_t newIdx = tileSet->AddAutotileSet("Autotile");
                        mgr->mOptions.mActiveAutotileIndex = newIdx;
                        tileSet->SetDirtyFlag();
                    }
                    ImGui::SameLine();
                    ImGui::Text("(%d defined)", int(sets.size()));

                    int32_t removeAutoIdx = -1;
                    for (size_t ai = 0; ai < sets.size(); ++ai)
                    {
                        AutotileSet& set = sets[ai];
                        ImGui::PushID(int(ai));

                        bool active = (mgr->mOptions.mActiveAutotileIndex == int32_t(ai));
                        if (ImGui::RadioButton("##ActiveAuto", active))
                            mgr->mOptions.mActiveAutotileIndex = int32_t(ai);
                        ImGui::SameLine();

                        char nameBuf[64];
                        strncpy(nameBuf, set.mName.c_str(), sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        ImGui::SetNextItemWidth(140.0f);
                        if (ImGui::InputText("##AutoName", nameBuf, sizeof(nameBuf)))
                        {
                            set.mName = nameBuf;
                            tileSet->SetDirtyFlag();
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) removeAutoIdx = int32_t(ai);

                        // Member tags
                        ImGui::TextUnformatted( "Member Tags:");
                        int32_t removeMemberTag = -1;
                        for (size_t t = 0; t < set.mMemberTags.size(); ++t)
                        {
                            ImGui::PushID(int(t) + 1000);
                            char tagBuf[64];
                            strncpy(tagBuf, set.mMemberTags[t].c_str(), sizeof(tagBuf) - 1);
                            tagBuf[sizeof(tagBuf) - 1] = '\0';
                            ImGui::SetNextItemWidth(160.0f);
                            if (ImGui::InputText("##MemTag", tagBuf, sizeof(tagBuf)))
                            {
                                set.mMemberTags[t] = tagBuf;
                                tileSet->SetDirtyFlag();
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("X")) removeMemberTag = int32_t(t);
                            ImGui::PopID();
                        }
                        if (removeMemberTag >= 0)
                        {
                            set.mMemberTags.erase(set.mMemberTags.begin() + removeMemberTag);
                            tileSet->SetDirtyFlag();
                        }
                        if (ImGui::Button("+ Add Member Tag"))
                        {
                            set.mMemberTags.push_back("");
                            tileSet->SetDirtyFlag();
                        }

                        // Rules
                        ImGui::TextUnformatted(ICON_MAGE_SCALE_UP " Rules:");
                        int32_t removeRuleIdx = -1;
                        for (size_t ri = 0; ri < set.mRules.size(); ++ri)
                        {
                            AutotileRule& rule = set.mRules[ri];
                            ImGui::PushID(int(ri) + 2000);

                            // 3x3 tri-state grid. Slot indices in the layout
                            // skip the center cell entirely.
                            //   slot[0]=NW slot[1]=N slot[2]=NE
                            //   slot[3]=W   center  slot[4]=E
                            //   slot[5]=SW slot[6]=S slot[7]=SE
                            const int32_t kSlotForGrid[9] = { 0, 1, 2, 3, -1, 4, 5, 6, 7 };
                            const char* kStateLabel[3] = { "?", "1", "0" }; // DontCare, MustBe, MustNot

                            for (int32_t row = 0; row < 3; ++row)
                            {
                                for (int32_t col = 0; col < 3; ++col)
                                {
                                    int32_t gridIdx = row * 3 + col;
                                    int32_t slot = kSlotForGrid[gridIdx];
                                    ImGui::PushID(gridIdx);
                                    if (slot < 0)
                                    {
                                        ImGui::Button("*", ImVec2(28, 28));
                                    }
                                    else
                                    {
                                        AutotileNeighborState st = rule.mNeighbors[slot];
                                        if (ImGui::Button(kStateLabel[int(st)], ImVec2(28, 28)))
                                        {
                                            int next = (int(st) + 1) % int(AutotileNeighborState::Count);
                                            rule.mNeighbors[slot] = AutotileNeighborState(next);
                                            tileSet->SetDirtyFlag();
                                        }
                                        if (ImGui::IsItemHovered())
                                        {
                                            const char* tip = (st == AutotileNeighborState::DontCare) ? "Don't care"
                                                              : (st == AutotileNeighborState::MustBeSelf) ? "Must be self"
                                                              : "Must NOT be self";
                                            ImGui::SetTooltip("%s", tip);
                                        }
                                    }
                                    ImGui::PopID();
                                    if (col < 2) ImGui::SameLine();
                                }
                            }

                            // Result tile
                            int32_t resultTile = rule.mResultTiles.empty() ? -1 : rule.mResultTiles[0];
                            ImGui::Text("Result:");
                            ImGui::SameLine();
                            if (TilePicker::DrawTileButton("##RuleResult", atlasImId,
                                    resultTile, tilesX, tilesY, 32.0f))
                            {
                                ImGui::OpenPopup("##PickRuleResult");
                            }
                            int32_t pickedResult = resultTile;
                            TilePicker::DrawTilePickerPopup("##PickRuleResult", atlasImId,
                                tilesX, tilesY, pickedResult);
                            if (pickedResult != resultTile)
                            {
                                if (rule.mResultTiles.empty())
                                    rule.mResultTiles.push_back(pickedResult);
                                else
                                    rule.mResultTiles[0] = pickedResult;
                                tileSet->SetDirtyFlag();
                            }

                            ImGui::SameLine();
                            if (ImGui::SmallButton("X##RemoveRule")) removeRuleIdx = int32_t(ri);

                            ImGui::PopID();
                            ImGui::Separator();
                        }
                        if (removeRuleIdx >= 0)
                        {
                            set.mRules.erase(set.mRules.begin() + removeRuleIdx);
                            tileSet->SetDirtyFlag();
                        }
                        if (ImGui::Button("+ Add Rule"))
                        {
                            set.mRules.push_back(AutotileRule{});
                            tileSet->SetDirtyFlag();
                        }

                        ImGui::PopID();
                        ImGui::Separator();
                    }
                    if (removeAutoIdx >= 0)
                    {
                        tileSet->RemoveAutotileSet(removeAutoIdx);
                        if (mgr->mOptions.mActiveAutotileIndex >= int32_t(sets.size()))
                            mgr->mOptions.mActiveAutotileIndex = int32_t(sets.size()) - 1;
                        tileSet->SetDirtyFlag();
                    }
                }

                // Selection / Clipboard / Transforms (Phase 3)
                if (ImGui::CollapsingHeader(ICON_UIWIDGET " Selection & Clipboard"))
                {
                    if (mgr->HasSelection())
                    {
                        glm::ivec2 mn = mgr->GetSelectionMin();
                        glm::ivec2 mx = mgr->GetSelectionMax();
                        ImGui::Text("Selection: (%d,%d) -> (%d,%d)  (%d cells)",
                            mn.x, mn.y, mx.x, mx.y, int(mgr->mSelectedCells.size()));
                    }
                    else
                    {
                        ImGui::TextDisabled("No selection. Use Select tool to drag a rect.");
                    }
                    ImGui::TextDisabled("Shift+drag = add to selection. Ctrl+drag = remove.");

                    if (ImGui::Button(ICON_CLAPPERBOARD_OPEN))     mgr->DoCopy();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_MINGCUTE_SCISSORS_FILL))      mgr->DoCut();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cut");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_MDI_CONTENT_PASTE))    mgr->DoPasteAtCursor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paste");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_ZONDICONS_TRASH))    mgr->ClearSelection();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear");

                    bool hasClip = mgr->HasClipboard();
                    if (!hasClip) ImGui::BeginDisabled();
                    if (ImGui::Button(ICON_OCTICON_MIRROR_Y))   mgr->DoFlipClipboardX();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flip X");
                    ImGui::SameLine();                

                    if (ImGui::Button(ICON_OCTICON_MIRROR_Y ))   mgr->DoFlipClipboardY();

                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flip Y");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_BOXICONS_SHAPE_ROTATE_CW)) mgr->DoRotateClipboard90();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate 90");
                    if (!hasClip) ImGui::EndDisabled();

                    ImGui::TextDisabled(hasClip ? "Clipboard ready (paste at cursor)." : "Clipboard empty.");

                    // 9-Box Fill — apply the active 9-box brush to the
                    // currently-selected rectangle. Requires both an active
                    // selection AND an active 9-box brush in the panel.
                    ImGui::Spacing();
                    bool canApply9Box = mgr->HasSelection() &&
                                        tileSet != nullptr &&
                                        mgr->mOptions.mActiveNineBoxIndex >= 0 &&
                                        mgr->mOptions.mActiveNineBoxIndex < int32_t(tileSet->GetNineBoxBrushes().size());
                    if (!canApply9Box) ImGui::BeginDisabled();
                    if (ImGui::Button(ICON_9POINT " Fill Selection With 9-Box Brush"))
                    {
                        mgr->DoApply9BoxToSelection();
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Replace every cell in the (possibly freeform) selection with the appropriate corner/edge/center tile from the active 9-box brush.\nWorks on rectangles, L-shapes, paths, and other irregular shapes.");
                    if (!canApply9Box) ImGui::EndDisabled();
                    if (!canApply9Box)
                    {
                        if (!mgr->HasSelection())
                            ImGui::TextDisabled("Need a marquee selection (use Select tool).");
                        else
                            ImGui::TextDisabled("Need an active 9-box brush (in 9-Box Brushes panel).");
                    }
                }

                // View overlays (Phase 3 + Phase 4 cell grid)
                if (ImGui::CollapsingHeader(ICON_MDI_EYE " View"))
                {
                    Polyphase::Checkbox(ICON_BXS_GRID " Cell Grid",         &mgr->mOptions.mShowCellGrid);
                    Polyphase::Checkbox( ICON_FLUENT_MDL2_CUBE_SHAPE " Collision Overlay", &mgr->mOptions.mShowCollisionOverlay);
                    Polyphase::Checkbox(ICON_FLUENT_TAG_24_FILLED " Tag Overlay",       &mgr->mOptions.mShowTagOverlay);
                    if (mgr->mOptions.mShowTagOverlay)
                    {
                        char tagBuf[64];
                        strncpy(tagBuf, mgr->mOptions.mTagOverlayName.c_str(), sizeof(tagBuf) - 1);
                        tagBuf[sizeof(tagBuf) - 1] = '\0';
                        if (ImGui::InputText("Highlight Tag", tagBuf, sizeof(tagBuf)))
                        {
                            mgr->mOptions.mTagOverlayName = tagBuf;
                        }
                    }
                }

                // Status / hover info
                ImGui::Separator();
                if (mgr->mHoverValid)
                {
                    ImGui::Text("Cell: %d, %d", mgr->mHoverCell.x, mgr->mHoverCell.y);
                    int32_t cx = TileMap::FloorDivChunk(mgr->mHoverCell.x);
                    int32_t cy = TileMap::FloorDivChunk(mgr->mHoverCell.y);
                    ImGui::Text("Chunk: %d, %d", cx, cy);
                    if (tileMap != nullptr)
                    {
                        int32_t hoverTile = tileMap->GetTile(mgr->mHoverCell.x, mgr->mHoverCell.y, mgr->mOptions.mActiveLayer);
                        ImGui::Text("Tile under cursor: %d", hoverTile);
                    }
                }
                else
                {
                    ImGui::TextDisabled("Hover over the tile map to begin painting.");
                }

                // Selected-tile metadata editor (Phase 2: tags + collision flag)
                if (tileSet != nullptr && mgr->mOptions.mSelectedTileIndex >= 0 &&
                    mgr->mOptions.mSelectedTileIndex < tileSet->GetNumTiles() &&
                    ImGui::CollapsingHeader("Tile Metadata"))
                {
                    TileDefinition* def = tileSet->GetTileDefMutable(mgr->mOptions.mSelectedTileIndex);
                    if (def != nullptr)
                    {
                        // Find All Uses (Phase 3)
                        if (ImGui::Button("Find All Uses"))
                        {
                            if (tileMap != nullptr)
                            {
                                int32_t count = tileMap->CountTileUses(
                                    mgr->mOptions.mSelectedTileIndex,
                                    mgr->mOptions.mActiveLayer);
                                LogDebug("Tile %d is used by %d cells on layer %d",
                                    mgr->mOptions.mSelectedTileIndex,
                                    count,
                                    mgr->mOptions.mActiveLayer);
                            }
                        }

                        char nameBuf[64];
                        strncpy(nameBuf, def->mName.c_str(), sizeof(nameBuf) - 1);
                        nameBuf[sizeof(nameBuf) - 1] = '\0';
                        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                        {
                            def->mName = nameBuf;
                            tileSet->SetDirtyFlag();
                        }

                        bool hasCol = def->mHasCollision;
                        if (Polyphase::Checkbox("Has Collision", &hasCol))
                        {
                            def->mHasCollision = hasCol;
                            if (hasCol && def->mCollisionType == TileCollisionType::None)
                                def->mCollisionType = TileCollisionType::FullSolid;
                            tileSet->SetDirtyFlag();
                        }

                        if (def->mHasCollision)
                        {
                            const char* colTypeNames[] = { "None", "Full Solid", "Box", "Boxes", "Slope", "Polygon" };
                            int colType = int(def->mCollisionType);
                            if (ImGui::Combo("Type", &colType, colTypeNames, IM_ARRAYSIZE(colTypeNames)))
                            {
                                def->mCollisionType = TileCollisionType(colType);
                                tileSet->SetDirtyFlag();
                            }
                        }

                        ImGui::TextUnformatted("Tags:");
                        int32_t removeIdx = -1;
                        for (size_t t = 0; t < def->mTags.size(); ++t)
                        {
                            ImGui::PushID(int(t));
                            char tagBuf[64];
                            strncpy(tagBuf, def->mTags[t].c_str(), sizeof(tagBuf) - 1);
                            tagBuf[sizeof(tagBuf) - 1] = '\0';
                            ImGui::SetNextItemWidth(180.0f);
                            if (ImGui::InputText("##Tag", tagBuf, sizeof(tagBuf)))
                            {
                                def->mTags[t] = tagBuf;
                                tileSet->SetDirtyFlag();
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("X"))
                            {
                                removeIdx = int32_t(t);
                            }
                            ImGui::PopID();
                        }
                        if (removeIdx >= 0)
                        {
                            def->mTags.erase(def->mTags.begin() + removeIdx);
                            tileSet->SetDirtyFlag();
                        }
                        if (ImGui::Button("+ Add Tag"))
                        {
                            def->mTags.push_back("");
                            tileSet->SetDirtyFlag();
                        }
                    }
                }

                if (tileSet == nullptr)
                {
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
                    ImGui::TextWrapped("This TileMap2D has no TileSet bound. Assign a TileMap asset whose TileSet has a Texture.");
                    ImGui::PopStyleColor();
                }

                // ----- Tile preview overlay (Phase 4) ----------------------
                // Draw the selected tile as a semi-transparent ImGui sprite
                // at every cell that would be painted, so the user can see
                // exactly what their stroke will produce before committing.
                if (mgr->mHoverValid && tileSet != nullptr && atlasImId != 0)
                {
                    Camera3D* cam = GetWorld(0)->GetActiveCamera();
                    if (cam != nullptr)
                    {
                        glm::vec2 uv0, uv1;
                        bool haveUVs = tileSet->GetTileUVs(mgr->mOptions.mSelectedTileIndex, uv0, uv1);

                        // Project the 4 corners of a cell to screen space and
                        // call ImDrawList::AddImageQuad. Returns whether all 4
                        // corners are in front of the camera.
                        auto drawCellSprite = [&](int32_t cx, int32_t cy, ImU32 tint)
                        {
                            if (!haveUVs) return;

                            glm::vec2 bl = tileMapNode->CellToWorld({ cx,     cy     });
                            glm::vec2 tr = tileMapNode->CellToWorld({ cx + 1, cy + 1 });
                            float z = tileMapNode->GetWorldPosition().z;

                            glm::vec3 sBL = cam->WorldToScreenPosition(glm::vec3(bl.x, bl.y, z));
                            glm::vec3 sBR = cam->WorldToScreenPosition(glm::vec3(tr.x, bl.y, z));
                            glm::vec3 sTR = cam->WorldToScreenPosition(glm::vec3(tr.x, tr.y, z));
                            glm::vec3 sTL = cam->WorldToScreenPosition(glm::vec3(bl.x, tr.y, z));

                            // Reject corners behind the camera (z component <= 0).
                            if (sBL.z <= 0.0f || sBR.z <= 0.0f || sTR.z <= 0.0f || sTL.z <= 0.0f)
                                return;

                            ImDrawList* dl = ImGui::GetForegroundDrawList();
                            dl->AddImageQuad(
                                (ImTextureID)atlasImId,
                                ImVec2(sTL.x, sTL.y),
                                ImVec2(sTR.x, sTR.y),
                                ImVec2(sBR.x, sBR.y),
                                ImVec2(sBL.x, sBL.y),
                                ImVec2(uv0.x, uv0.y),
                                ImVec2(uv1.x, uv0.y),
                                ImVec2(uv1.x, uv1.y),
                                ImVec2(uv0.x, uv1.y),
                                tint);
                        };

                        ImU32 ghostTint = IM_COL32(255, 255, 255, 170);
                        TileSculptMode m = mgr->mOptions.mMode;

                        // Single-cell preview at the hover for any "draw a tile" tool.
                        if (m == TileSculptMode::Pencil ||
                            m == TileSculptMode::Eraser ||
                            m == TileSculptMode::FloodFill ||
                            m == TileSculptMode::RectFill ||
                            m == TileSculptMode::Line ||
                            m == TileSculptMode::NineBox ||
                            m == TileSculptMode::Autotile)
                        {
                            // Eraser shows a faint red instead of the tile sprite.
                            if (m == TileSculptMode::Eraser)
                            {
                                ImDrawList* dl = ImGui::GetForegroundDrawList();
                                glm::vec2 bl = tileMapNode->CellToWorld({ mgr->mHoverCell.x,     mgr->mHoverCell.y     });
                                glm::vec2 tr = tileMapNode->CellToWorld({ mgr->mHoverCell.x + 1, mgr->mHoverCell.y + 1 });
                                float z = tileMapNode->GetWorldPosition().z;
                                glm::vec3 sBL = cam->WorldToScreenPosition(glm::vec3(bl.x, bl.y, z));
                                glm::vec3 sTR = cam->WorldToScreenPosition(glm::vec3(tr.x, tr.y, z));
                                if (sBL.z > 0.0f && sTR.z > 0.0f)
                                {
                                    dl->AddRectFilled(
                                        ImVec2(std::min(sBL.x, sTR.x), std::min(sBL.y, sTR.y)),
                                        ImVec2(std::max(sBL.x, sTR.x), std::max(sBL.y, sTR.y)),
                                        IM_COL32(255, 80, 80, 90));
                                }
                            }
                            else
                            {
                                drawCellSprite(mgr->mHoverCell.x, mgr->mHoverCell.y, ghostTint);
                            }
                        }

                        // Drag-preview for RectFill: ghost-fill the entire rect.
                        if (mgr->mPreviewActive && m == TileSculptMode::RectFill)
                        {
                            int32_t minX = std::min(mgr->mDragStartCell.x, mgr->mHoverCell.x);
                            int32_t maxX = std::max(mgr->mDragStartCell.x, mgr->mHoverCell.x);
                            int32_t minY = std::min(mgr->mDragStartCell.y, mgr->mHoverCell.y);
                            int32_t maxY = std::max(mgr->mDragStartCell.y, mgr->mHoverCell.y);
                            int32_t cellCount = (maxX - minX + 1) * (maxY - minY + 1);
                            if (cellCount <= 1024)  // bound the preview cost
                            {
                                for (int32_t cy = minY; cy <= maxY; ++cy)
                                    for (int32_t cx = minX; cx <= maxX; ++cx)
                                        drawCellSprite(cx, cy, ghostTint);
                            }
                        }

                        // Drag-preview for Line: Bresenham trace.
                        if (mgr->mPreviewActive && m == TileSculptMode::Line)
                        {
                            int32_t x0 = mgr->mDragStartCell.x;
                            int32_t y0 = mgr->mDragStartCell.y;
                            int32_t x1 = mgr->mHoverCell.x;
                            int32_t y1 = mgr->mHoverCell.y;
                            int32_t dx = std::abs(x1 - x0);
                            int32_t dy = -std::abs(y1 - y0);
                            int32_t sx = (x0 < x1) ? 1 : -1;
                            int32_t sy = (y0 < y1) ? 1 : -1;
                            int32_t err = dx + dy;
                            int32_t safety = 0;
                            while (safety++ < 1024)
                            {
                                drawCellSprite(x0, y0, ghostTint);
                                if (x0 == x1 && y0 == y1) break;
                                int32_t e2 = 2 * err;
                                if (e2 >= dy) { err += dy; x0 += sx; }
                                if (e2 <= dx) { err += dx; y0 += sy; }
                            }
                        }

                        // Pencil/Eraser drag-paint: ghost the interpolated
                        // path from the previous frame's cell to the current
                        // hover cell. The actual paint is committed by
                        // TilePaintManager via the same Bresenham trace, so
                        // the preview overlays the cells that will be
                        // painted on this frame for clear visual feedback.
                        if (mgr->mStrokeActive && mgr->mLastStrokeCellValid &&
                            (m == TileSculptMode::Pencil || m == TileSculptMode::Eraser))
                        {
                            int32_t x0 = mgr->mLastStrokeCell.x;
                            int32_t y0 = mgr->mLastStrokeCell.y;
                            int32_t x1 = mgr->mHoverCell.x;
                            int32_t y1 = mgr->mHoverCell.y;
                            int32_t dx = std::abs(x1 - x0);
                            int32_t dy = -std::abs(y1 - y0);
                            int32_t sx = (x0 < x1) ? 1 : -1;
                            int32_t sy = (y0 < y1) ? 1 : -1;
                            int32_t err = dx + dy;
                            int32_t safety = 0;
                            ImU32 dragTint = (m == TileSculptMode::Eraser)
                                ? IM_COL32(255, 80, 80, 140)
                                : IM_COL32(255, 255, 255, 200);
                            while (safety++ < 256)
                            {
                                if (m == TileSculptMode::Eraser)
                                {
                                    ImDrawList* dl = ImGui::GetForegroundDrawList();
                                    glm::vec2 bl = tileMapNode->CellToWorld({ x0,     y0     });
                                    glm::vec2 tr = tileMapNode->CellToWorld({ x0 + 1, y0 + 1 });
                                    float z = tileMapNode->GetWorldPosition().z;
                                    glm::vec3 sBL = cam->WorldToScreenPosition(glm::vec3(bl.x, bl.y, z));
                                    glm::vec3 sTR = cam->WorldToScreenPosition(glm::vec3(tr.x, tr.y, z));
                                    if (sBL.z > 0.0f && sTR.z > 0.0f)
                                    {
                                        dl->AddRectFilled(
                                            ImVec2(std::min(sBL.x, sTR.x), std::min(sBL.y, sTR.y)),
                                            ImVec2(std::max(sBL.x, sTR.x), std::max(sBL.y, sTR.y)),
                                            dragTint);
                                    }
                                }
                                else
                                {
                                    drawCellSprite(x0, y0, dragTint);
                                }
                                if (x0 == x1 && y0 == y1) break;
                                int32_t e2 = 2 * err;
                                if (e2 >= dy) { err += dy; x0 += sx; }
                                if (e2 <= dx) { err += dx; y0 += sy; }
                            }
                        }
                    }
                }
            }
            else
            {
                ImGui::TextWrapped("Select a TileMap2D node to begin painting.");
            }

            ImGui::End();
        }

        if (GetEditorState()->GetEditorMode() == EditorMode::Scene2D)
        {
            DrawDesignBounds();
            Draw2dSelections();
        }

        if (GetEditorState()->mNodePropertySelect)
        {
            DrawNodePropertySelectOverlay();
        }

        DrawFileBrowser();

        DrawUnsavedCheck();
        DrawProjectUpgradeModal();
        DrawAddonsDialogs();
        DrawScriptCreatorDialogs();

        GetPreferencesWindow()->Draw();
        GetPackagingWindow()->Draw();
        GetAppSettingsWindow()->Draw();
        GetBuildDependencyWindow()->Draw();
        GetInputMapWindow()->Draw();
        GetEditorHotkeysWindow()->Draw();
        GetGitWorkspaceWindow()->Draw();
        GetGitCloneDialog()->Draw();
        GetGitOpenDialog()->Draw();
        GetGitInitDialog()->Draw();
        GetGitCreateBranchDialog()->Draw();
        GetGitCheckoutConfirmDialog()->Draw();
        GetGitCreateTagDialog()->Draw();
        GetGitDeleteConfirmDialog()->Draw();
        GetGitPushDialog()->Draw();
        GetGitPullDialog()->Draw();
        GetGitFetchDialog()->Draw();
        GetGitRemoteEditDialog()->Draw();
        GetGitMergeDialog()->Draw();
        GetGitCliStatusDialog()->Draw();
        GetGitOperationProgressDialog()->Draw();
        GetGitSyncBranchDialog()->Draw();
        GetPlayerInputEditor()->Draw();
        GetPlayerInputDebugger()->Draw();
        ActionManager::Get()->DrawBuildModal();
        GetProjectSelectWindow()->Draw();
        GetAddonsWindow()->Draw();
        GetThemeEditorWindow()->Draw();
        GetAutoUpdaterWindow()->Draw();

        if (GetThemeEditorWindow()->IsInspectModeActive())
        {
            GetThemeEditorWindow()->DrawInspectOverlay();
        }
    }

    if (ControllerServer::Get())
    {
        ControllerServer::Get()->Tick();
    }

    ImGui::Render();
}

void EditorImguiShutdown()
{
    if (IsHeadless())
    {
        return;
    }

    ImGui::DestroyContext();
}

void EditorImguiPreShutdown()
{
    if (IsHeadless())
    {
        return;
    }

    // Save INI before clearing dock data, then disable auto-save so DestroyContext
    // doesn't overwrite our save with empty dock state.
    if (ImGui::GetIO().IniFilename)
    {
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
        StampDockLayoutVersion();
        ImGui::GetIO().IniFilename = nullptr;
    }
    GetScriptEditorWindow()->Shutdown();
    GetTerminalPanel()->Shutdown();
    GetLuaDebuggerPanel()->Shutdown();
    ImGui::ShutdownDock();
    UnregisterLogCallback(DebugLogWindow::LogCallback);
    ControllerServer::Destroy();

    if (sInspectTexId != 0)
    {
        DeviceWaitIdle();
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)sInspectTexId);
        sInspectTexId = 0;
    }
}

void EditorImguiGetViewport(uint32_t& x, uint32_t& y, uint32_t& width, uint32_t& height)
{
    if (EditorIsInterfaceVisible())
    {
        float scale = ImGui::GetIO().PolyphaseInterfaceScale;
        if (scale == 0.0f)
        {
            scale = 1.0f;
        }

        x = uint32_t(sViewportDockPos.x * scale + 0.5f);
        y = uint32_t(sViewportDockPos.y * scale + 0.5f);
        int32_t iW = int32_t(sViewportDockSize.x * scale + 0.5f);
        int32_t iH = int32_t(sViewportDockSize.y * scale + 0.5f);
        iW = glm::clamp<int32_t>(iW, 100, int32_t(ImGui::GetIO().DisplaySize.x + 0.5f));
        iH = glm::clamp<int32_t>(iH, 100, int32_t(ImGui::GetIO().DisplaySize.y + 0.5f));
        width = uint32_t(iW);
        height = uint32_t(iH);
    }
    else
    {
        x = 0;
        y = 0;
        width = uint32_t(ImGui::GetIO().DisplaySize.x + 0.5f);
        height = uint32_t(ImGui::GetIO().DisplaySize.y + 0.5f);
    }
}

bool EditorImguiIsViewportHovered()
{
    if (!sViewportDockActive)
        return false;

    ImVec2 mp = ImGui::GetIO().MousePos;
    bool inRect = (mp.x >= sViewportDockPos.x &&
                   mp.x <  sViewportDockPos.x + sViewportDockSize.x &&
                   mp.y >= sViewportDockPos.y &&
                   mp.y <  sViewportDockPos.y + sViewportDockSize.y);

    if (!inRect)
        return false;

    // If any ImGui window other than the main viewport dock is hovered (a floating
    // popup, OR another docked editor panel that overlaps the viewport rect — e.g.
    // Animation Browser tabbed inside the viewport node), the main viewport should
    // not consume input. Only the actual "Viewport##EditorDock" window owns input.
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx != nullptr && ctx->HoveredWindow != nullptr)
    {
        const char* name = ctx->HoveredWindow->Name;
        if (name == nullptr || strstr(name, "Viewport##EditorDock") == nullptr)
        {
            return false;
        }
    }

    return true;
}

bool EditorIsInterfaceVisible()
{
    EditorState* es = GetEditorState();
    return es->mShowInterface && (!IsPlaying() || es->mEjected || es->mPlayInGameWindow);
}

#endif


