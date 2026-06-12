#pragma once

#if EDITOR

#include "InputCaptureModal.h"
#include "Input/PlayerInputSystem.h"

#include <string>

class InputPromptMap;

// Standalone editor window for InputPromptMap. Opened on asset double-click via
// OpenInputPromptMapForEditing(); also exposed in the asset Inspector through a
// small "Open Editor..." button so single-click flow still works.
//
// Lifecycle mirrors SpriteAnimationAtlasEditor: singleton, persistent across
// asset switches (Open() retargets), and self-draws when DrawWindow() is
// called once per editor frame.
class InputPromptMapInspector
{
public:

    static InputPromptMapInspector* Get();

    // Retarget the editor window at `map` and ensure it's visible. Safe to call
    // even if the window is already open with a different asset.
    void Open(InputPromptMap* map);

    // Close the window without changing the assigned target (next Open() reuses
    // existing UI state — filter, selection, etc.).
    void Close();

    bool IsOpen() const { return mIsOpen; }

    // Per-frame entry point. Call once from EditorImguiDraw().
    void DrawWindow();

    // Small Inspector stub (still called from the EditorImgui.cpp type-dispatch
    // chain). Renders "Open Editor..." button so single-click users can launch
    // the full window without going through double-click.
    void DrawInspectorButton(InputPromptMap* map);

private:

    InputPromptMapInspector() = default;

    void DrawToolbar(InputPromptMap* map);
    void DrawFilterRow();
    void DrawEntryTable(InputPromptMap* map);
    void DrawValidationStrip(InputPromptMap* map);

    void AddEntry(InputPromptMap* map);
    void DuplicateEntry(InputPromptMap* map, int32_t idx);
    void DeleteEntry(InputPromptMap* map, int32_t idx);
    void MoveEntry(InputPromptMap* map, int32_t idx, int32_t delta);
    void MarkDirty(InputPromptMap* map);

    static InputPromptMapInspector* sInstance;

    InputPromptMap* mTarget = nullptr;
    bool mIsOpen = false;

    int32_t mSelectedEntry = -1;
    int32_t mCaptureRowIndex = -1;
    InputCaptureModal mCapture;

    int32_t mFilterPlatform = -1;     // -1 == any
    int32_t mFilterGamepad  = -1;     // -1 == any
    std::string mFilterPath;

    int32_t mTestDeviceKind = -1;
    int32_t mTestGamepadType = (int)GamepadType::Standard;
};

// Convenience hook used from the asset-browser double-click dispatch in
// EditorImgui.cpp. Wraps Get()->Open() so the call site doesn't need to
// include this header.
void OpenInputPromptMapForEditing(InputPromptMap* map);

#endif // EDITOR
