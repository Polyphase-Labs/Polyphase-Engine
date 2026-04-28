#pragma once

#if EDITOR

#include "Input/InputTypes.h"

#include <stdint.h>
#include <string>
#include <vector>

class InputMapWindow
{
public:
    void Open();
    void Draw();
    bool IsOpen() const;
    bool IsCapturing() const;

private:

    bool mIsOpen = false;

    // Preset state
    std::vector<std::string> mPresetNames;
    int mSelectedPreset = -1;
    char mNewPresetName[64] = {};
    bool mShowSavePopup = false;

    // Capture state for remapping
    bool mCapturing = false;
    enum CaptureTarget
    {
        CaptureButton,
        CaptureAxisPositive,
        CaptureAxisNegative
    };
    CaptureTarget mCaptureType = CaptureButton;
    int32_t mCaptureIndex = -1;
    float mCaptureTimer = 0.0f;

    void DrawPresets();
    void DrawGamepadButtons();
    void DrawGamepadAxes();
    void DrawMouseAxes();
    void DrawMousePointerSection();
    void DrawCaptureOverlay();
    bool PollKeyPress(int32_t& outKeyCode);
    void RefreshPresetList();
};

InputMapWindow* GetInputMapWindow();

#endif
