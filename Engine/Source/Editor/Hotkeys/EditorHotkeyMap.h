#pragma once

#if EDITOR

#include "EditorAction.h"

#include "document.h"

#include <stdint.h>
#include <string>
#include <vector>

// Central registry for editor hotkey bindings.
//
// Use IsActionJustTriggered for one-shot actions (menus, toggles, gizmo
// switches) and IsActionDown for continuous actions (camera movement). Both
// queries gate themselves on AreEditorHotkeysActive() so they automatically
// stop firing when Play-In-Editor has captured the cursor in the game window.
class EditorHotkeyMap
{
public:

    static void Create();
    static void Destroy();
    static EditorHotkeyMap* Get();

    // ----- Query API used by editor call sites -----

    // True for one frame when the binding's key+modifier combo just edged
    // down. Returns false during captured PIE so game code owns those keys.
    bool IsActionJustTriggered(EditorAction action) const;

    // True every frame the binding's key+modifier combo is held. Use for
    // camera movement / continuous tools. Same PIE gate as above.
    bool IsActionDown(EditorAction action) const;

    // True for one frame when the binding's key just released.
    bool IsActionJustReleased(EditorAction action) const;

    // ----- Binding management -----

    KeyBinding GetBinding(EditorAction action) const;
    void SetBinding(EditorAction action, const KeyBinding& binding);
    void ClearBinding(EditorAction action);
    void ResetActionToDefault(EditorAction action);
    void ResetAllToDefaults();

    // Return every action whose binding exactly matches the supplied combo,
    // optionally excluding one (used to exclude the action being remapped).
    std::vector<EditorAction> FindConflicts(const KeyBinding& binding,
                                            EditorAction excluding = EditorAction::Count) const;

    // Explicit consume helper. Some sites (Ctrl+S, Ctrl+P) need to clear the
    // key after handling so a follow-up popup or text field doesn't see it as
    // still-held. Mirrors the existing INP_ClearKey calls in InputManager.cpp.
    void ConsumeBindingKey(EditorAction action) const;

    // Display string ("Ctrl+Shift+S", "Space+G", or "—" if unbound).
    static std::string BindingToDisplayString(const KeyBinding& binding);

    // True iff editor hotkeys should be processed this frame. Editor code can
    // call this directly when it needs to gate something other than a binding
    // (e.g. mouse-button hotkeys).
    static bool AreEditorHotkeysActive();

    // ----- Preset management -----
    //
    // Presets live as JSON files in
    //   Windows: %APPDATA%/PolyphaseEditor/HotkeyPresets/
    //   Linux:   ~/.config/PolyphaseEditor/HotkeyPresets/
    //
    // Users share presets by exporting to a chosen file and importing
    // someone else's file.

    bool SavePreset(const std::string& name);
    bool LoadPreset(const std::string& name);
    bool DeletePreset(const std::string& name);
    std::vector<std::string> GetPresetNames() const;
    static std::string GetPresetsDirectory();

    bool ExportToFile(const std::string& absPath);
    bool ImportFromFile(const std::string& absPath);

    // Writes a "Default.json" preset to the presets folder if one doesn't
    // already exist. Called from Create() during editor startup, before any
    // user customizations are loaded by the PreferencesManager, so the saved
    // file always reflects the canonical defaults from the metadata table.
    // Users can then load "Default" from the dropdown like any other preset,
    // and the file is regenerated next launch if they delete it.
    void EnsureDefaultPresetExists();
    static const char* GetDefaultPresetName() { return "Default"; }

    // ----- Preferences-module serialization -----

    void SerializeToJson(rapidjson::Document& doc) const;
    void DeserializeFromJson(const rapidjson::Document& doc);

private:

    static EditorHotkeyMap* sInstance;

    EditorHotkeyMap();

    bool MatchesBinding(const KeyBinding& binding) const;
    void WriteBindingToJson(const KeyBinding& binding,
                            rapidjson::Value& outObj,
                            rapidjson::Document::AllocatorType& alloc) const;
    bool ReadBindingFromJson(const rapidjson::Value& obj, KeyBinding& outBinding) const;

    KeyBinding mBindings[(int32_t)EditorAction::Count];
};

#endif
