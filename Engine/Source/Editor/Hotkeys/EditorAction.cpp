#if EDITOR

#include "EditorAction.h"

#include "Input/InputTypes.h"

#include <string.h>

// Compact constructors so the metadata table reads cleanly below.
static KeyBinding KB(int32_t key)
{
    KeyBinding b;
    b.mKeyCode = key;
    return b;
}

static KeyBinding KB(int32_t key, bool ctrl, bool shift, bool alt)
{
    KeyBinding b;
    b.mKeyCode = key;
    b.mCtrl    = ctrl;
    b.mShift   = shift;
    b.mAlt     = alt;
    return b;
}

static KeyBinding KB_Space(int32_t key)
{
    KeyBinding b;
    b.mKeyCode      = key;
    b.mRequireSpace = true;
    return b;
}

// Metadata, indexed by EditorAction. Order MUST match the enum.
static const EditorActionInfo sActionMetadata[(int32_t)EditorAction::Count] =
{
    // ----- File -----
    { "Open Project",         "File",   "Open an existing project",            "File_OpenProject",       KB(POLYPHASE_KEY_P, true, false, false) },
    { "New Project",          "File",   "Create a new project",                "File_NewProject",        KB(POLYPHASE_KEY_P, true, true,  false) },
    { "Save Scene",           "File",   "Save the active scene",               "File_SaveScene",         KB(POLYPHASE_KEY_S, true, false, false) },
    { "Save All Assets",      "File",   "Save every dirty asset",              "File_SaveAllAssets",     KB(POLYPHASE_KEY_S, true, true,  false) },
    { "Save Selected Asset",  "File",   "Save the asset selected in the browser","File_SaveSelectedAsset", KB(POLYPHASE_KEY_S, false, true, false) },
    { "Open Scene",           "File",   "Open a scene asset",                  "File_OpenScene",         KB(POLYPHASE_KEY_O, true, false, false) },
    { "Import Asset",         "File",   "Import an external asset file",       "File_ImportAsset",       KB(POLYPHASE_KEY_I, true, false, false) },

    // ----- Edit -----
    { "Undo",                 "Edit",   "Undo the last action",                "Edit_Undo",              KB(POLYPHASE_KEY_Z, true, false, false) },
    { "Redo",                 "Edit",   "Redo the last undone action",         "Edit_Redo",              KB(POLYPHASE_KEY_Z, true, true,  false) },
    { "Reload Scripts",       "Edit",   "Reload all Lua scripts",              "Edit_ReloadScripts",     KB(POLYPHASE_KEY_R, true, false, false) },
    { "Select All",           "Edit",   "Select every node in the scene",      "Edit_SelectAll",         KB(POLYPHASE_KEY_A, true, false, false) },
    { "Deselect All",         "Edit",   "Clear the current selection",         "Edit_DeselectAll",       KB(POLYPHASE_KEY_A, false, false, true) },
    { "Duplicate",            "Edit",   "Duplicate the selected nodes/assets", "Edit_Duplicate",         KB(POLYPHASE_KEY_D, true, false, false) },
    { "Delete Selected",      "Edit",   "Delete the current selection",        "Edit_DeleteSelected",    KB(POLYPHASE_KEY_DELETE) },

    // ----- Scene Mode -----
    { "Mode: Scene",          "Mode",   "Switch to the mixed Scene editor",    "Mode_Scene",             KB(POLYPHASE_KEY_1, true, false, false) },
    { "Mode: 2D",             "Mode",   "Switch to the 2D Widget editor",      "Mode_Scene2D",           KB(POLYPHASE_KEY_2, true, false, false) },
    { "Mode: 3D",             "Mode",   "Switch to the 3D Scene editor",      "Mode_Scene3D",           KB(POLYPHASE_KEY_3, true, false, false) },
    { "Mode: Paint Color",    "Mode",   "Enter the color paint mode",          "Mode_PaintColor",        KB(POLYPHASE_KEY_4, true, false, false) },
    { "Mode: Paint Instance", "Mode",   "Enter the instance paint mode",       "Mode_PaintInstance",     KB(POLYPHASE_KEY_5, true, false, false) },

    // ----- Play-In-Editor -----
    { "Toggle Play in Editor","PIE",    "Start Play-In-Editor (PIE safety keys F8/Esc/F10/Alt+P stay fixed during PIE)", "PIE_Toggle", KB(POLYPHASE_KEY_P, false, false, true) },

    // ----- Camera (Pilot Mode) -----
    { "Camera: Forward",      "Camera", "Move the editor camera forward (pilot mode)", "Camera_Forward", KB(POLYPHASE_KEY_W) },
    { "Camera: Back",         "Camera", "Move the editor camera back (pilot mode)",    "Camera_Back",    KB(POLYPHASE_KEY_S) },
    { "Camera: Left",         "Camera", "Strafe the editor camera left (pilot mode)",  "Camera_Left",    KB(POLYPHASE_KEY_A) },
    { "Camera: Right",        "Camera", "Strafe the editor camera right (pilot mode)", "Camera_Right",   KB(POLYPHASE_KEY_D) },
    { "Camera: Up",           "Camera", "Move the editor camera up (pilot mode)",      "Camera_Up",      KB(POLYPHASE_KEY_E) },
    { "Camera: Down",         "Camera", "Move the editor camera down (pilot mode)",    "Camera_Down",    KB(POLYPHASE_KEY_Q) },

    // ----- View Navigation -----
    { "View: Front",          "View",   "Snap to front orthographic view",     "View_Front",             KB(POLYPHASE_KEY_NUMPAD1) },
    { "View: Back",           "View",   "Snap to back orthographic view",      "View_Back",              KB(POLYPHASE_KEY_NUMPAD1, true, false, false) },
    { "View: Right",          "View",   "Snap to right orthographic view",     "View_Right",             KB(POLYPHASE_KEY_NUMPAD3) },
    { "View: Left",           "View",   "Snap to left orthographic view",      "View_Left",              KB(POLYPHASE_KEY_NUMPAD3, true, false, false) },
    { "View: Top",            "View",   "Snap to top orthographic view",       "View_Top",               KB(POLYPHASE_KEY_NUMPAD7) },
    { "View: Bottom",         "View",   "Snap to bottom orthographic view",    "View_Bottom",            KB(POLYPHASE_KEY_NUMPAD7, true, false, false) },
    { "View: Toggle Perspective","View","Toggle between perspective and orthographic","View_PerspToggle", KB(POLYPHASE_KEY_NUMPAD5) },
    { "View: Focus Selection","View",   "Center the camera on the selection",  "View_FocusSelection",    KB(POLYPHASE_KEY_F) },
    { "View: Position At Camera","View","Snap selected node to the camera",    "View_PositionAtCamera",  KB(POLYPHASE_KEY_NUMPAD0) },

    // ----- Gizmo / Transform -----
    { "Gizmo: Translate",            "Gizmo", "Enter cursor-locked translate mode",     "Gizmo_Translate",            KB(POLYPHASE_KEY_G) },
    { "Gizmo: Rotate",               "Gizmo", "Enter cursor-locked rotate mode",        "Gizmo_Rotate",               KB(POLYPHASE_KEY_R) },
    { "Gizmo: Scale",                "Gizmo", "Enter cursor-locked scale mode",         "Gizmo_Scale",                KB(POLYPHASE_KEY_S) },
    { "Gizmo: Translate (ImGuizmo)", "Gizmo", "Switch ImGuizmo to translate (Space+G)", "Gizmo_TranslateImGuizmo",    KB_Space(POLYPHASE_KEY_G) },
    { "Gizmo: Rotate (ImGuizmo)",    "Gizmo", "Switch ImGuizmo to rotate (Space+R)",    "Gizmo_RotateImGuizmo",       KB_Space(POLYPHASE_KEY_R) },
    { "Gizmo: Scale (ImGuizmo)",     "Gizmo", "Switch ImGuizmo to scale (Space+S)",     "Gizmo_ScaleImGuizmo",        KB_Space(POLYPHASE_KEY_S) },
    { "Gizmo: Toggle Local/World",   "Gizmo", "Toggle between local and world transform","Gizmo_TransformLocalToggle", KB(POLYPHASE_KEY_T, true, false, false) },
    { "Gizmo: Toggle Grid",          "Gizmo", "Show or hide the grid",                  "Gizmo_GridToggle",           KB(POLYPHASE_KEY_G, true, false, false) },

    // ----- Debug -----
    { "Debug: Wireframe",     "Debug",  "Toggle wireframe rendering",          "Debug_Wireframe",        KB(POLYPHASE_KEY_Z) },
    { "Debug: Collision",     "Debug",  "Toggle collision-bounds rendering",   "Debug_Collision",        KB(POLYPHASE_KEY_K) },
    { "Debug: Proxy",         "Debug",  "Toggle proxy gizmo rendering",        "Debug_Proxy",            KB(POLYPHASE_KEY_P) },
    { "Debug: Path Tracing",  "Debug",  "Toggle path tracing",                 "Debug_PathTracing",      KB(POLYPHASE_KEY_L, false, false, true) },
    { "Debug: Cycle Bounds",  "Debug",  "Cycle through bounds visualizations", "Debug_BoundsCycle",      KB(POLYPHASE_KEY_B) },

    // ----- UI Panes -----
    { "UI: Toggle Left Pane", "UI",     "Show or hide the hierarchy pane",     "UI_ToggleLeftPane",      KB(POLYPHASE_KEY_T) },
    { "UI: Toggle Right Pane","UI",     "Show or hide the inspector pane",     "UI_ToggleRightPane",     KB(POLYPHASE_KEY_N) },
    { "UI: Toggle Interface", "UI",     "Show or hide the entire editor UI",   "UI_ToggleAll",           KB(POLYPHASE_KEY_Z, false, false, true) },

    // ----- Quick Spawn (Alt+Number) -----
    { "Spawn: Static Mesh",   "Spawn",  "Spawn a Static Mesh node",            "Spawn_StaticMesh",       KB(POLYPHASE_KEY_1, false, false, true) },
    { "Spawn: Point Light",   "Spawn",  "Spawn a Point Light node",            "Spawn_PointLight",       KB(POLYPHASE_KEY_2, false, false, true) },
    { "Spawn: Node3D",        "Spawn",  "Spawn an empty Node3D",               "Spawn_Node3D",           KB(POLYPHASE_KEY_3, false, false, true) },
    { "Spawn: Directional Light","Spawn","Spawn a Directional Light node",     "Spawn_DirLight",         KB(POLYPHASE_KEY_4, false, false, true) },
    { "Spawn: Skeletal Mesh", "Spawn",  "Spawn a Skeletal Mesh node",          "Spawn_SkeletalMesh",     KB(POLYPHASE_KEY_5, false, false, true) },
    { "Spawn: Box",           "Spawn",  "Spawn a Box primitive",               "Spawn_Box",              KB(POLYPHASE_KEY_6, false, false, true) },
    { "Spawn: Sphere",        "Spawn",  "Spawn a Sphere primitive",            "Spawn_Sphere",           KB(POLYPHASE_KEY_7, false, false, true) },
    { "Spawn: Particle",      "Spawn",  "Spawn a Particle node",               "Spawn_Particle",         KB(POLYPHASE_KEY_8, false, false, true) },
    { "Spawn: Audio",         "Spawn",  "Spawn an Audio node",                 "Spawn_Audio",            KB(POLYPHASE_KEY_9, false, false, true) },
    { "Spawn: Scene Instance","Spawn",  "Spawn a sub-scene instance",          "Spawn_Scene",            KB(POLYPHASE_KEY_0, false, false, true) },

    // ----- Spawn Menus -----
    { "Spawn Menu: Basic 3D", "Spawn",  "Open the basic 3D spawn menu",        "Spawn_Basic3DMenu",      KB(POLYPHASE_KEY_Q, false, true,  false) },
    { "Spawn Menu: Widget",   "Spawn",  "Open the basic widget spawn menu",    "Spawn_BasicWidgetMenu",  KB(POLYPHASE_KEY_W, false, true,  false) },
    { "Spawn Menu: Node",     "Spawn",  "Open the full node spawn menu",       "Spawn_NodeMenu",         KB(POLYPHASE_KEY_A, false, true,  false) },

    // ----- Hierarchy / Asset Browser -----
    { "Hierarchy: Reorder Up",    "Hierarchy", "Move the selected node up in its parent",   "Hier_ReorderUp",       KB(POLYPHASE_KEY_MINUS) },
    { "Hierarchy: Reorder Down",  "Hierarchy", "Move the selected node down in its parent", "Hier_ReorderDown",     KB(POLYPHASE_KEY_PLUS) },
    { "Hierarchy: Rename",        "Hierarchy", "Rename the selected node",                  "Hier_Rename",          KB(POLYPHASE_KEY_F2) },
    { "Asset: New Scene",         "Asset",     "Create a new Scene asset",                  "Asset_CreateScene",    KB(POLYPHASE_KEY_N, true, false, false) },
    { "Asset: New Material",      "Asset",     "Create a new Material asset",               "Asset_CreateMaterial", KB(POLYPHASE_KEY_M, true, false, false) },
    { "Asset: New Particle",      "Asset",     "Create a new Particle asset",               "Asset_CreateParticle", KB(POLYPHASE_KEY_P, true, false, false) },
    { "Asset: Rename",            "Asset",     "Rename the selected asset",                 "Asset_Rename",         KB(POLYPHASE_KEY_F2) },

    // ----- Inspector -----
    { "Inspector: Toggle Lock",   "Inspector", "Lock the inspector to the current target",  "Inspector_ToggleLock", KB(POLYPHASE_KEY_L) },

    // ----- Tool / Placement -----
    { "Tool: Drop to Ground",     "Tool",  "Drop the selected node onto the surface below it",                  "Tool_DropActorToGround",     KB(POLYPHASE_KEY_END) },
    { "Tool: Drop and Align",     "Tool",  "Drop and align selected node to the surface normal",                "Tool_DropActorWithRotation", KB(POLYPHASE_KEY_INSERT) },

    // ----- Paint -----
    { "Paint: Toggle Erase",      "Paint", "Toggle paint erase mode",                                            "Paint_ToggleErase",          KB(POLYPHASE_KEY_E) },
    { "Paint: Adjust Brush",      "Paint", "Hold and drag to adjust brush radius/opacity",                       "Paint_BrushAdjust",          KB(POLYPHASE_KEY_F) },

    // ----- Version Control -----
    { "Git: Open Panel",          "Version Control", "Open the Git workspace panel",                            "Git_OpenPanel",              KB(0) },
    { "Git: Commit",              "Version Control", "Commit staged changes",                                   "Git_Commit",                 KB(0) },
    { "Git: Refresh Status",      "Version Control", "Refresh Git repository status",                           "Git_RefreshStatus",          KB(POLYPHASE_KEY_F5) },
    { "Git: Fetch",               "Version Control", "Fetch from remote",                                       "Git_Fetch",                  KB(0) },
    { "Git: Pull",                "Version Control", "Pull from remote",                                        "Git_Pull",                   KB(0) },
    { "Git: Push",                "Version Control", "Push to remote",                                          "Git_Push",                   KB(0) },
    { "Git: Quick Switch Branch", "Version Control", "Open quick branch switcher",                              "Git_QuickSwitchBranch",      KB(0) },
    { "Git: Search History",      "Version Control", "Search commit history",                                   "Git_SearchHistory",          KB(0) },

    // ----- Lua Debugger -----
    { "Debug: Toggle Lua Breakpoint", "Lua Debugger", "Toggle a Lua breakpoint at the cursor's line in the Script Editor", "Debug_LuaToggleBreakpoint", KB(POLYPHASE_KEY_F9) },
    { "Debug: Continue Lua",          "Lua Debugger", "Resume Lua execution after a breakpoint pause",                    "Debug_LuaContinue",         KB(POLYPHASE_KEY_F5) },
};

const EditorActionInfo& GetEditorActionInfo(EditorAction action)
{
    return sActionMetadata[(int32_t)action];
}

EditorAction FindEditorActionByKey(const char* serializeKey)
{
    if (serializeKey == nullptr)
        return EditorAction::Count;

    for (int32_t i = 0; i < (int32_t)EditorAction::Count; ++i)
    {
        if (strcmp(sActionMetadata[i].mSerializeKey, serializeKey) == 0)
            return (EditorAction)i;
    }
    return EditorAction::Count;
}

bool EditorActionKeyCodeIsModifier(int32_t keyCode)
{
    return keyCode == POLYPHASE_KEY_SHIFT_L
        || keyCode == POLYPHASE_KEY_SHIFT_R
        || keyCode == POLYPHASE_KEY_CONTROL_L
        || keyCode == POLYPHASE_KEY_CONTROL_R
        || keyCode == POLYPHASE_KEY_ALT_L
        || keyCode == POLYPHASE_KEY_ALT_R;
}

// ---------- Symbolic key name table ----------
//
// Presets serialize keys by symbolic name (e.g. "Z", "F11", "Numpad 5") rather
// than numeric code, because POLYPHASE_KEY_* values differ across platforms
// (Z = 90 on Windows, 52 on Linux). Symbolic names make presets portable.

struct KeySymbolEntry
{
    int32_t mKey;
    const char* mSymbol;
};

static const KeySymbolEntry sKeySymbols[] =
{
    // Letters
    { POLYPHASE_KEY_A, "A" }, { POLYPHASE_KEY_B, "B" }, { POLYPHASE_KEY_C, "C" },
    { POLYPHASE_KEY_D, "D" }, { POLYPHASE_KEY_E, "E" }, { POLYPHASE_KEY_F, "F" },
    { POLYPHASE_KEY_G, "G" }, { POLYPHASE_KEY_H, "H" }, { POLYPHASE_KEY_I, "I" },
    { POLYPHASE_KEY_J, "J" }, { POLYPHASE_KEY_K, "K" }, { POLYPHASE_KEY_L, "L" },
    { POLYPHASE_KEY_M, "M" }, { POLYPHASE_KEY_N, "N" }, { POLYPHASE_KEY_O, "O" },
    { POLYPHASE_KEY_P, "P" }, { POLYPHASE_KEY_Q, "Q" }, { POLYPHASE_KEY_R, "R" },
    { POLYPHASE_KEY_S, "S" }, { POLYPHASE_KEY_T, "T" }, { POLYPHASE_KEY_U, "U" },
    { POLYPHASE_KEY_V, "V" }, { POLYPHASE_KEY_W, "W" }, { POLYPHASE_KEY_X, "X" },
    { POLYPHASE_KEY_Y, "Y" }, { POLYPHASE_KEY_Z, "Z" },

    // Digits
    { POLYPHASE_KEY_0, "0" }, { POLYPHASE_KEY_1, "1" }, { POLYPHASE_KEY_2, "2" },
    { POLYPHASE_KEY_3, "3" }, { POLYPHASE_KEY_4, "4" }, { POLYPHASE_KEY_5, "5" },
    { POLYPHASE_KEY_6, "6" }, { POLYPHASE_KEY_7, "7" }, { POLYPHASE_KEY_8, "8" },
    { POLYPHASE_KEY_9, "9" },

    // Function keys
    { POLYPHASE_KEY_F1,  "F1" },  { POLYPHASE_KEY_F2,  "F2" },  { POLYPHASE_KEY_F3,  "F3" },
    { POLYPHASE_KEY_F4,  "F4" },  { POLYPHASE_KEY_F5,  "F5" },  { POLYPHASE_KEY_F6,  "F6" },
    { POLYPHASE_KEY_F7,  "F7" },  { POLYPHASE_KEY_F8,  "F8" },  { POLYPHASE_KEY_F9,  "F9" },
    { POLYPHASE_KEY_F10, "F10" }, { POLYPHASE_KEY_F11, "F11" }, { POLYPHASE_KEY_F12, "F12" },

    // Numpad
    { POLYPHASE_KEY_NUMPAD0, "Numpad 0" }, { POLYPHASE_KEY_NUMPAD1, "Numpad 1" },
    { POLYPHASE_KEY_NUMPAD2, "Numpad 2" }, { POLYPHASE_KEY_NUMPAD3, "Numpad 3" },
    { POLYPHASE_KEY_NUMPAD4, "Numpad 4" }, { POLYPHASE_KEY_NUMPAD5, "Numpad 5" },
    { POLYPHASE_KEY_NUMPAD6, "Numpad 6" }, { POLYPHASE_KEY_NUMPAD7, "Numpad 7" },
    { POLYPHASE_KEY_NUMPAD8, "Numpad 8" }, { POLYPHASE_KEY_NUMPAD9, "Numpad 9" },
    { POLYPHASE_KEY_DECIMAL, "Numpad ." },

    // Specials
    { POLYPHASE_KEY_SPACE,     "Space" },
    { POLYPHASE_KEY_ENTER,     "Enter" },
    { POLYPHASE_KEY_BACKSPACE, "Backspace" },
    { POLYPHASE_KEY_TAB,       "Tab" },
    { POLYPHASE_KEY_ESCAPE,    "Escape" },
    { POLYPHASE_KEY_INSERT,    "Insert" },
    { POLYPHASE_KEY_DELETE,    "Delete" },
    { POLYPHASE_KEY_HOME,      "Home" },
    { POLYPHASE_KEY_END,       "End" },
    { POLYPHASE_KEY_PAGE_UP,   "Page Up" },
    { POLYPHASE_KEY_PAGE_DOWN, "Page Down" },
    { POLYPHASE_KEY_UP,        "Up" },
    { POLYPHASE_KEY_DOWN,      "Down" },
    { POLYPHASE_KEY_LEFT,      "Left" },
    { POLYPHASE_KEY_RIGHT,     "Right" },

    // Punctuation
    { POLYPHASE_KEY_PERIOD,        "Period" },
    { POLYPHASE_KEY_COMMA,         "Comma" },
    { POLYPHASE_KEY_PLUS,          "Plus" },
    { POLYPHASE_KEY_MINUS,         "Minus" },
    { POLYPHASE_KEY_COLON,         "Semicolon" },
    { POLYPHASE_KEY_QUESTION,      "Slash" },
    { POLYPHASE_KEY_SQUIGGLE,      "Tilde" },
    { POLYPHASE_KEY_LEFT_BRACKET,  "Left Bracket" },
    { POLYPHASE_KEY_BACK_SLASH,    "Backslash" },
    { POLYPHASE_KEY_RIGHT_BRACKET, "Right Bracket" },
    { POLYPHASE_KEY_QUOTE,         "Quote" },
};

static const int32_t sKeySymbolCount = (int32_t)(sizeof(sKeySymbols) / sizeof(sKeySymbols[0]));

const char* EditorActionKeyCodeToSymbol(int32_t keyCode)
{
    if (keyCode < 0)
        return "";

    for (int32_t i = 0; i < sKeySymbolCount; ++i)
    {
        if (sKeySymbols[i].mKey == keyCode)
            return sKeySymbols[i].mSymbol;
    }
    return "";
}

int32_t EditorActionSymbolToKeyCode(const char* symbol)
{
    if (symbol == nullptr || symbol[0] == '\0')
        return -1;

    for (int32_t i = 0; i < sKeySymbolCount; ++i)
    {
        if (strcmp(sKeySymbols[i].mSymbol, symbol) == 0)
            return sKeySymbols[i].mKey;
    }
    return -1;
}

#endif
