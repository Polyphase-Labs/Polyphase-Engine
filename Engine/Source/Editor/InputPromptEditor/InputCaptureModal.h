#pragma once

#if EDITOR

#include "Input/PlayerInputSystem.h"

#include <functional>
#include <string>

// Modal "press any input" capture used by InputPromptMapInspector to fill in
// an entry's input path and device. Mirrors the pattern in
// Editor/Hotkeys/EditorHotkeysWindow.cpp:412 (DrawCaptureOverlay).
//
// While the modal is open it mutes PlayerInputSystem action evaluation via
// SetActionEvaluationEnabled(false) so a captured key doesn't also fire a
// registered game action.
class InputCaptureModal
{
public:

    struct Result
    {
        InputActionBinding binding;     // synthesized binding
        std::string path;               // MakeInputPath(binding)
        bool        gamepadDetected = false;
        int32_t     gamepadIndex = -1;
        GamepadType gamepadType = GamepadType::Standard;
    };

    using Callback = std::function<void(const Result&)>;

    // Begin capture. The modal opens on the next Draw() call.
    void Start(Callback onCaptured);

    // Call once per editor frame from the parent inspector. Returns true while
    // the modal is open (so the parent can early-out from other input).
    bool Draw();

    bool IsCapturing() const { return mCapturing; }

private:

    bool     mCapturing = false;
    bool     mJustStarted = false;
    float    mTimer = 0.0f;
    Callback mOnCaptured;
};

#endif // EDITOR
