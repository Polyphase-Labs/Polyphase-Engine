#pragma once

#if EDITOR

#include <string>
#include <set>
#include <unordered_map>
#include <vector>
#include "Maths.h"
#include "AssetRef.h"
#include "SmartPointer.h"
#include "Property.h"
#include "Nodes/Widgets/Text.h"
#include "imgui.h"
#include "./ImGuizmo/ImGuizmo.h"

class Node;
class Scene;
class Timeline;
class TimelineInstance;
class Widget;
class Text;
class Asset;
struct AssetStub;
class AssetDir;
class ActionList;
class Canvas;
class Camera3D;
class Viewport3D;
class Viewport2D;
class PaintManager;
class VoxelSculptManager;
class TilePaintManager;
struct SubSceneOverride;

enum class ControlMode
{
    Default,
    Pilot,
    Translate,
    Rotate,
    Scale,
    Pan,
    Orbit
};

enum class TransformLock
{
    None,
    AxisX,
    AxisY,
    AxisZ,
    PlaneYZ,
    PlaneXZ,
    PlaneXY,
    Count
};

enum class EditorMode
{
    Scene,
    Scene2D,
    Scene3D,

    Count
};

enum class PaintMode
{
    None,
    Color,
    Instance,
    Voxel,
    Terrain,
    TilePaint,

    Count
};

enum class AssetBrowserTab
{
    Project = 0,
    Addons = 1,
    Count
};

struct LinkedSceneProps
{
    Node* mNode = nullptr;
    std::vector<Property> mProps;
    std::vector<SubSceneOverride> mSubSceneOverrides;
};

struct EditScene
{
    SceneRef mSceneAsset;
    NodePtr mRootNode;
    glm::mat4 mCameraTransform;
    std::vector<LinkedSceneProps> mLinkedSceneProps;
};

struct RecentScene
{
    std::string mSceneName;
    uint64_t mTimestamp = 0;
};

// Per-asset state captured at BeginPlayInEditor and re-applied at
// EndPlayInEditor so script mutations during play don't leak into the editor.
struct PieAssetSnapshot
{
    std::vector<uint8_t> mBytes;
    bool mWasDirty = false;
};

struct EditorState
{
    // Data
    EditorMode mMode;
    std::vector<Node*> mSelectedNodes;
    Node* mShiftSelectAnchor = nullptr;
    int32_t mSelectedInstance = -1;
    std::vector<EditScene> mEditScenes;
    AssetStub* mSelectedAssetStub = nullptr;
    std::vector<AssetStub*> mSelectedAssetStubs;
    ControlMode mControlMode = ControlMode::Default;
    TransformLock mTransformLock = TransformLock::None;
    SharedPtr<Camera3D> mEditorCamera;
    float mPerspectiveNearZ = 0.25f;
    float mPerspectiveFarZ = 4096.0f;
    float mPerspectiveFov = 70.0f;
    float mOrthoNearZ = -2048.0f;
    float mOrthoFarZ = 2048.0f;
    float mOrthoWidth = 12.8f;
    bool mMouseNeedsRecenter = false;
    bool mUiEnabled = true;
    bool mPlayInEditor = false;
    bool mPlayInGameWindow = false;
    bool mEjected = false;
    bool mPaused = false;
    bool mHasEjectedOnce = false;
    bool mSavedGridEnabled = false;
    glm::vec4 mSavedEditorClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    int32_t mSavedWindowRect[4] = {}; // x, y, w, h — saved before Play Full Screen resize
    int32_t mEditSceneIndex = -1;
    int32_t mPieEditSceneIdx = -1;
    std::unordered_map<Asset*, PieAssetSnapshot> mPieAssetSnapshots;
    // Lazy cache of instantiated default trees for sub-scene source assets,
    // used by the inspector to detect overridden properties and revert them.
    // Keyed by source Scene*. Invalidated when an EditScene closes, on
    // editor shutdown, and whenever a source Scene is reimported/saved.
    std::unordered_map<Scene*, NodePtr> mInspectorDefaultCache;
    // Deferred PIE start/stop: the click handler sets the *AtEndOfFrame flag
    // so the loading modal can render for a couple of frames before
    // BeginPlayInEditor / EndPlayInEditor blocks the main thread on asset
    // snapshot / scene clone / restore work. mPieLoadingFramesRemaining ticks
    // down each frame; the actual work runs when it reaches zero. A small
    // non-zero delay guarantees the modal is presented (and any auto-sizing
    // has settled) before we block.
    bool mBeginPieAtEndOfFrame = false;
    bool mShowPieLoadingModal = false;
    int32_t mPieLoadingFramesRemaining = 0;
    std::string mPieLoadingMessage;
    AssetBrowserTab mActiveAssetTab = AssetBrowserTab::Project;
    AssetDir* mTabCurrentDir[(int)AssetBrowserTab::Count] = {};
    std::vector<AssetDir*> mTabDirPast[(int)AssetBrowserTab::Count];
    std::vector<AssetDir*> mTabDirFuture[(int)AssetBrowserTab::Count];
    std::string mTabFilterStr[(int)AssetBrowserTab::Count];
    std::vector<AssetStub*> mTabFilteredStubs[(int)AssetBrowserTab::Count];
    std::vector<Object*> mInspectPast;
    std::vector<Object*> mInspectFuture;
    Object* mInspectedObject;
    Object* mPrevInspectedObject;
    AssetRef mInspectedAsset;
    bool mInspectLocked = false;
    Viewport3D* mViewport3D = nullptr;
    Viewport2D* mViewport2D = nullptr;
    std::string mIOAssetPath;
    bool mRequestSaveSceneAs = false;
    bool mTrackSelectedAsset = false;
    bool mTrackSelectedNode = false;
    std::string mRevealScriptName = "";
    std::set<AssetDir*> mRevealAssetExpandDirs;
    uint32_t mViewportX = 0;
    uint32_t mViewportY = 0;
    uint32_t mViewportWidth = 100;
    uint32_t mViewportHeight = 100;
    glm::uvec4 mPrevViewport = {};
    bool mShowLeftPane = true;
    bool mShowRightPane = true;
    bool mShowInterface = true;
    bool mPreviewLighting = true;
    SharedPtr<Text> mOverlayText = nullptr;
    std::vector<std::string> mFavoritedDirs;
    std::vector<std::string> mRecentProjects;
    std::vector<RecentScene> mRecentScenes;
    PaintMode mPaintMode = PaintMode::None;
    PaintManager* mPaintManager = nullptr;
    VoxelSculptManager* mVoxelSculptManager = nullptr;
    class TerrainSculptManager* mTerrainSculptManager = nullptr;
    TilePaintManager* mTilePaintManager = nullptr;

    // Tile-paint mode stashes the editor camera's previous projection AND
    // transform so it can switch to a top-down orthographic view of the
    // currently-selected TileMap2D and restore the original camera on exit.
    bool mTilePaintProjectionStashed = false;
    bool mTilePaintPrevWasPerspective = true;
    bool mTilePaintTransformStashed = false;
    glm::vec3 mTilePaintPrevCameraPosition = { 0.0f, 0.0f, 0.0f };
    glm::quat mTilePaintPrevCameraRotation = { 1.0f, 0.0f, 0.0f, 0.0f };
    bool mNodePropertySelect = false;
    int32_t mNodePropertySelectIndex = 0;
    std::string mNodePropertySelectName = "";
    std::string mPendingSceneImportPath = "";
    AssetStub* mPendingReimportSceneStub = nullptr;
    std::string mPendingReimportScenePath = "";
    bool mShutdownUnsavedCheck = false;
    bool mDevMode = false;
    bool mShowBottomPane = true;
    float mBottomPaneHeight = 180.0f;
    bool mEndPieAtEndOfFrame = false;
    bool mShowProjectUpgradeModal = false;
    bool mProjectUpgradeInProgress = false;
    std::vector<AssetStub*> mAssetsNeedingUpgrade;

    // 3DS Preview Panel state
    bool mShow3DSPreview = false;
    int32_t mSceneScreenFilter = -1;  // -1 = All Screens, 0 = Top Screen, 1 = Bottom Screen

    // Game Preview Panel state
    bool mShowGamePreview = false;
    bool mGamePreviewCaptured = false;

    // Play target (shared between viewport toolbar and Game Preview)
    int32_t mPlayTarget = 0;  // 0=PlayInEditor, 1=PlayFullScreen, 2=Dolphin, 3=Azahar, 4=Standalone, 5=Send3dsLink

    // Node Graph Panel state
    bool mShowNodeGraphPanel = false;

    // Timeline Panel state
    bool mShowTimelinePanel = false;

    // Profiling Panel state
    bool mShowProfilingPanel = false;

    // Input Tester Panel state
    bool mShowInputTesterPanel = false;

    // Texture Atlas Viewer state
    bool mShowTextureAtlasViewer = false;

    // Git Panel state
    bool mShowGitPanel = false;

    // Animation Browser Panel state
    bool mShowAnimationBrowser = false;
    TimelineRef mEditedTimelineRef;
    TimelineInstance* mTimelinePreviewInstance = nullptr;
    float mTimelinePlayheadTime = 0.0f;
    bool mTimelinePreviewing = false;
    float mTimelineZoom = 100.0f;
    float mTimelineScrollX = 0.0f;
    float mTimelineSnapInterval = 0.1f;
    int32_t mTimelineSelectedTrack = -1;
    int32_t mTimelineSelectedClip = -1;
    int32_t mTimelineSelectedKeyframe = -1;

    // ImGuizmo state
    ImGuizmo::OPERATION mGizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mGizmoMode = ImGuizmo::WORLD;
    bool mGizmoBlockedBySelection = false;

    // Methods
    void Init();
    void Shutdown();
    void Update(float deltaTime);

    void GatherProperties(std::vector<Property>& props);

    void SetEditorMode(EditorMode mode);
    EditorMode GetEditorMode();

    void SetPaintMode(PaintMode paintMode);
    PaintMode GetPaintMode();

    void ReadEditorSave();
    void WriteEditorSave();

    void ReadEditorProjectSave();
    void WriteEditorProjectSave();

    void HandleNodeDestroy(Node* node);

    void SetSelectedNode(Node* newNode);
    void AddSelectedNode(Node* node, bool addAllChildren);
    void SelectNodesInRange(Node* rootNode, Node* endNode);
    void RemoveSelectedNode(Node* node);
    void SetSelectedAssetStub(AssetStub* newStub);
    void AddSelectedAssetStub(AssetStub* stub);
    void RemoveSelectedAssetStub(AssetStub* stub);
    void ClearSelectedAssetStubs();
    const std::vector<AssetStub*>& GetSelectedAssetStubs();
    bool IsAssetStubSelected(AssetStub* stub);
    void SetControlMode(ControlMode newMode);

    void BeginPlayInEditor();
    void EndPlayInEditor();
    // Defer the start/stop by one frame so the loading modal can render
    // before the (potentially slow) snapshot/clone or restore work runs.
    void RequestBeginPlayInEditor();
    void RequestEndPlayInEditor();
    void EjectPlayInEditor();
    void InjectPlayInEditor();
    void SnapshotAssetsForPie();
    void RestoreAssetsFromPie();

    // Sub-scene override inspector helpers. The cache holds an instantiated
    // copy of each source Scene asset that's currently being inspected; the
    // inspector compares the live node's property values against the
    // corresponding default node in the cached tree to detect overrides.
    // Pass nullptr to InvalidateSubSceneDefaultCache to clear everything.
    Node* GetSubSceneDefaultTree(Scene* src);
    void InvalidateSubSceneDefaultCache(Scene* src = nullptr);
    bool IsPropertyOverridden(Node* node, const std::string& propName);
    void RevertPropertyToSource(Node* node, const std::string& propName);
    void SetPlayInEditorPaused(bool paused);
    bool IsPlayInEditorPaused();

    void SetNodePropertySelect(bool enable, int32_t index, const std::string& propName);
    void ClearNodePropertySelect();
    void AssignNodePropertySelect(Node* targetNode);

    Camera3D* GetEditorCamera();
    void ToggleEditorCameraProjection();
    void ApplyEditorCameraSettings();

    void LoadStartupScene();

    Node* GetSelectedNode();
    Widget* GetSelectedWidget();
    const std::vector<Node*>& GetSelectedNodes();
    bool IsNodeSelected(Node* node);
    void DeselectNode(Node* node);
    int32_t GetSelectedInstance();
    void SetSelectedInstance(int32_t instance);
    //void ShowTextPrompt(const char* title, TextFieldHandlerFP confirmHandler, const char* defaultText = nullptr);

    void OpenEditScene(Scene* scene);
    void OpenEditScene(int32_t idx);
    void CloseEditScene(int32_t idx);
    void ShelveEditScene();
    EditScene* GetEditScene(int32_t idx = -1);
    void CloseAllEditScenes();
    void EnsureActiveScene();

    void ShowEditorUi(bool show);

    Asset* GetSelectedAsset();
    AssetStub* GetSelectedAssetStub();
    ControlMode GetControlMode();
    glm::vec3 GetTransformLockVector(TransformLock lock);
    void SetTransformLock(TransformLock lock);

    Object* GetInspectedObject();
    Node* GetInspectedNode();
    Asset* GetInspectedAsset();
    void InspectObject(Object* obj, bool force = false, bool recordHistory = true);
    void LockInspect(bool lock);
    bool IsInspectLocked();
    void RecordInspectHistory();
    void ClearInspectHistory();
    void RemoveFromInspectHistory(Object* obj);
    void ProgressInspectFuture();
    void RegressInspectPast();
    void ClearAssetDirHistory();
    void SetAssetDirectory(AssetDir* assetDir, bool recordHistory);
    AssetDir* GetAssetDirectory();
    void BrowseToAsset(const std::string& name);
    void BrowseToScript(const std::string& scriptName);

    void CaptureAndSaveScene(AssetStub* stub, Node* rootNode);
    void DuplicateAsset(AssetStub* srcStub, const char* overrideName = nullptr);

    void ProgressDirFuture();
    void RegressDirPast();

    void RemoveFilteredAssetStub(AssetStub* stub);
    int ActiveTab() const { return (int)mActiveAssetTab; }

    Viewport3D* GetViewport3D();
    Viewport2D* GetViewport2D();

    bool IsDirFavorited(const std::string& dirPath);
    void AddFavoriteDir(const std::string& dirPath);
    void RemoveFavoriteDir(const std::string& dirPath);

    void AddRecentProject(const std::string& projPath);
    void AddRecentScene(const std::string& sceneName);
};

EditorState* GetEditorState();

#endif
