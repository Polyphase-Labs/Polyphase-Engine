#pragma once

#if EDITOR

#include <stdint.h>

// Single source of truth for every rebindable editor action.
//
// Add new actions to this enum, then add a matching row to sActionMetadata
// in EditorAction.cpp. Do NOT use INP_IsKeyJustDown / IsKeyJustDown directly
// in editor code -- always go through EditorHotkeyMap so users can remap.
//
// Keys that intentionally remain hardcoded (NOT in this enum):
//   - PIE safety: Escape, F8, Alt+P (during PIE), F10, Ctrl+Alt+P
//   - Transform axis-lock: X / Y / Z while inside an active rotate/scale mode
//   - Dialog confirm/cancel: Enter / Escape inside modals
enum class EditorAction : int32_t
{
    // ----- File -----
    File_OpenProject,
    File_NewProject,
    File_SaveScene,
    File_SaveAllAssets,
    File_SaveSelectedAsset,
    File_OpenScene,
    File_ImportAsset,

    // ----- Edit -----
    Edit_Undo,
    Edit_Redo,
    Edit_ReloadScripts,
    Edit_SelectAll,
    Edit_DeselectAll,
    Edit_Duplicate,
    Edit_DeleteSelected,
    Edit_CopyNodes,
    Edit_PasteNodes,
    Edit_CutNodes,

    // ----- Scene Mode -----
    Mode_Scene,
    Mode_Scene2D,
    Mode_Scene3D,
    Mode_PaintColor,
    Mode_PaintInstance,

    // ----- Play-In-Editor -----
    PIE_Toggle,

    // ----- Camera (Pilot Mode) -----
    Camera_Forward,
    Camera_Back,
    Camera_Left,
    Camera_Right,
    Camera_Up,
    Camera_Down,

    // ----- View Navigation -----
    View_Front,
    View_Back,
    View_Right,
    View_Left,
    View_Top,
    View_Bottom,
    View_PerspToggle,
    View_FocusSelection,
    View_PositionAtCamera,

    // ----- Gizmo / Transform -----
    Gizmo_Translate,
    Gizmo_Rotate,
    Gizmo_Scale,
    Gizmo_TranslateImGuizmo,
    Gizmo_RotateImGuizmo,
    Gizmo_ScaleImGuizmo,
    Gizmo_TransformLocalToggle,
    Gizmo_GridToggle,

    // ----- Debug -----
    Debug_Wireframe,
    Debug_Collision,
    Debug_Proxy,
    Debug_PathTracing,
    Debug_BoundsCycle,

    // ----- UI Panes -----
    UI_ToggleLeftPane,
    UI_ToggleRightPane,
    UI_ToggleAll,

    // ----- Quick Spawn (Alt+Number) -----
    Spawn_StaticMesh,
    Spawn_PointLight,
    Spawn_Node3D,
    Spawn_DirLight,
    Spawn_SkeletalMesh,
    Spawn_Box,
    Spawn_Sphere,
    Spawn_Particle,
    Spawn_Audio,
    Spawn_Scene,

    // ----- Spawn Menus -----
    Spawn_Basic3DMenu,
    Spawn_BasicWidgetMenu,
    Spawn_NodeMenu,

    // ----- Hierarchy / Asset Browser -----
    Hier_ReorderUp,
    Hier_ReorderDown,
    Hier_Rename,
    Asset_CreateScene,
    Asset_CreateMaterial,
    Asset_CreateParticle,
    Asset_Rename,

    // ----- Inspector -----
    Inspector_ToggleLock,

    // ----- Tool / Placement -----
    Tool_DropActorToGround,
    Tool_DropActorWithRotation,

    // ----- Paint -----
    Paint_ToggleErase,
    Paint_BrushAdjust,

    // ----- Version Control -----
    Git_OpenPanel,
    Git_Commit,
    Git_RefreshStatus,
    Git_Fetch,
    Git_Pull,
    Git_Push,
    Git_QuickSwitchBranch,
    Git_SearchHistory,

    // ----- Lua Debugger -----
    Debug_LuaToggleBreakpoint, // F9 -- toggle bp at cursor in Script Editor
    Debug_LuaContinue,         // F5 -- only fires while debugger is paused

    Count
};

// A binding is a key + an exact-match modifier signature. The "RequireSpace"
// flag covers the small handful of Space+G/R/S gizmo gestures.
struct KeyBinding
{
    int32_t mKeyCode      = -1;     // -1 = unbound
    bool    mCtrl         = false;
    bool    mShift        = false;
    bool    mAlt          = false;
    bool    mRequireSpace = false;

    bool IsValid() const { return mKeyCode >= 0; }

    bool operator==(const KeyBinding& other) const
    {
        return mKeyCode      == other.mKeyCode
            && mCtrl         == other.mCtrl
            && mShift        == other.mShift
            && mAlt          == other.mAlt
            && mRequireSpace == other.mRequireSpace;
    }
};

// Metadata for one EditorAction. Filled in by sActionMetadata in EditorAction.cpp.
struct EditorActionInfo
{
    const char*  mName;         // Display name shown in the remap window
    const char*  mCategory;     // Category header ("File", "Camera", "Debug", etc.)
    const char*  mDescription;  // Tooltip
    const char*  mSerializeKey; // Stable identifier used in JSON (e.g. "Camera_Forward")
    KeyBinding   mDefault;      // Default binding
};

const EditorActionInfo& GetEditorActionInfo(EditorAction action);

// Lookup an action by its serialize key (used by preset import). Returns
// EditorAction::Count if no match.
EditorAction FindEditorActionByKey(const char* serializeKey);

// True for Ctrl/Shift/Alt — used by the capture overlay to skip modifier presses.
bool EditorActionKeyCodeIsModifier(int32_t keyCode);

// Convert a key code to a stable symbolic name ("A", "F11", "Numpad 5") and
// vice-versa. JSON presets store keys by name so they're portable across
// platforms (POLYPHASE_KEY_Z = 90 on Windows but 52 on Linux).
const char* EditorActionKeyCodeToSymbol(int32_t keyCode);
int32_t     EditorActionSymbolToKeyCode(const char* symbol);

#endif
