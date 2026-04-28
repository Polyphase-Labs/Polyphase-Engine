#if EDITOR

#include "InputMapWindow.h"
#include "Input/InputMap.h"
#include "Input/Input.h"
#include "Preferences/PreferencesManager.h"
#include "Preferences/PreferencesModule.h"

#include "imgui.h"

static void MarkInputMapDirty()
{
    PreferencesModule* mod = PreferencesManager::Get()->FindModule("Input");
    if (mod != nullptr)
    {
        mod->SetDirty(true);
        PreferencesManager::Get()->SaveModule(mod);
    }
}

static InputMapWindow sInputMapWindow;

InputMapWindow* GetInputMapWindow()
{
    return &sInputMapWindow;
}

void InputMapWindow::Open()
{
    mIsOpen = true;
    RefreshPresetList();
}

void InputMapWindow::RefreshPresetList()
{
    InputMap* inputMap = InputMap::Get();
    if (inputMap != nullptr)
    {
        mPresetNames = inputMap->GetPresetNames();
    }
}

bool InputMapWindow::IsOpen() const
{
    return mIsOpen;
}

bool InputMapWindow::IsCapturing() const
{
    return mCapturing;
}

bool InputMapWindow::PollKeyPress(int32_t& outKeyCode)
{
    for (int32_t key = 0; key < INPUT_MAX_KEYS; ++key)
    {
        // Skip modifier keys (they shouldn't be standalone bindings for gamepad)
        if (key == POLYPHASE_KEY_SHIFT_L || key == POLYPHASE_KEY_SHIFT_R ||
            key == POLYPHASE_KEY_CONTROL_L || key == POLYPHASE_KEY_CONTROL_R ||
            key == POLYPHASE_KEY_ALT_L || key == POLYPHASE_KEY_ALT_R)
        {
            continue;
        }

        if (INP_IsKeyJustDown(key))
        {
            outKeyCode = key;
            return true;
        }
    }
    return false;
}

void InputMapWindow::Draw()
{
    if (!mIsOpen)
        return;

    InputMap* inputMap = InputMap::Get();
    if (inputMap == nullptr)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(550.0f, 650.0f);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, (io.DisplaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Input Map", &mIsOpen, windowFlags))
    {
        ImGui::TextWrapped("Map keyboard keys to gamepad buttons and axes for testing in Play-in-Editor without a physical controller.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawPresets();
        ImGui::Spacing();

        // Handle capture mode
        if (mCapturing)
        {
            DrawCaptureOverlay();
        }

        DrawGamepadButtons();
        ImGui::Spacing();
        DrawGamepadAxes();
        ImGui::Spacing();
        DrawMouseAxes();
        ImGui::Spacing();
        DrawMousePointerSection();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Reset All to Defaults"))
        {
            inputMap->ResetToDefaults();
            MarkInputMapDirty();
        }
    }
    ImGui::End();
}

void InputMapWindow::DrawPresets()
{
    InputMap* inputMap = InputMap::Get();

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Preset selector
        if (mPresetNames.empty())
        {
            ImGui::TextDisabled("No saved presets.");
        }
        else
        {
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("##preset_select", mSelectedPreset >= 0 && mSelectedPreset < (int)mPresetNames.size()
                ? mPresetNames[mSelectedPreset].c_str() : "Select preset..."))
            {
                for (int i = 0; i < (int)mPresetNames.size(); ++i)
                {
                    bool selected = (mSelectedPreset == i);
                    if (ImGui::Selectable(mPresetNames[i].c_str(), selected))
                    {
                        mSelectedPreset = i;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            bool hasSelection = mSelectedPreset >= 0 && mSelectedPreset < (int)mPresetNames.size();

            if (hasSelection)
            {
                if (ImGui::Button("Load"))
                {
                    inputMap->LoadPreset(mPresetNames[mSelectedPreset]);
                    MarkInputMapDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                {
                    inputMap->DeletePreset(mPresetNames[mSelectedPreset]);
                    mSelectedPreset = -1;
                    RefreshPresetList();
                }
            }
        }

        // Save new preset
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("##preset_name", mNewPresetName, sizeof(mNewPresetName));
        ImGui::SameLine();
        bool validName = mNewPresetName[0] != '\0';
        if (!validName) ImGui::BeginDisabled();
        if (ImGui::Button("Save Preset"))
        {
            inputMap->SavePreset(mNewPresetName);
            mNewPresetName[0] = '\0';
            RefreshPresetList();
        }
        if (!validName) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !validName)
            ImGui::SetTooltip("Enter a name for the preset.");
    }
}

void InputMapWindow::DrawGamepadButtons()
{
    InputMap* inputMap = InputMap::Get();

    if (ImGui::CollapsingHeader("Gamepad Buttons", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Columns(4, "##gamepad_buttons", true);
        ImGui::SetColumnWidth(0, 150.0f);
        ImGui::SetColumnWidth(1, 150.0f);
        ImGui::SetColumnWidth(2, 70.0f);
        ImGui::SetColumnWidth(3, 70.0f);

        ImGui::Text("Button");
        ImGui::NextColumn();
        ImGui::Text("Mapped Key");
        ImGui::NextColumn();
        ImGui::Text("");
        ImGui::NextColumn();
        ImGui::Text("");
        ImGui::NextColumn();
        ImGui::Separator();

        for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
        {
            GamepadButtonCode button = (GamepadButtonCode)i;
            const char* buttonName = InputMap::GetGamepadButtonName(button);
            int32_t mappedKey = inputMap->GetButtonMapping(button);
            const char* keyName = (mappedKey >= 0) ? InputMap::GetKeyCodeName(mappedKey) : "\xe2\x80\x94";

            ImGui::PushID(i);

            // Highlight row if currently capturing this button
            bool isCapturing = mCapturing && mCaptureType == CaptureButton && mCaptureIndex == i;
            if (isCapturing)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }

            ImGui::Text("%s", buttonName);
            ImGui::NextColumn();
            ImGui::Text("%s", isCapturing ? "Press a key..." : keyName);
            ImGui::NextColumn();

            if (isCapturing)
            {
                ImGui::PopStyleColor();
            }

            if (!mCapturing)
            {
                if (ImGui::SmallButton("Remap"))
                {
                    mCapturing = true;
                    mCaptureType = CaptureButton;
                    mCaptureIndex = i;
                    mCaptureTimer = 5.0f;
                }
            }
            else
            {
                ImGui::TextDisabled("Remap");
            }
            ImGui::NextColumn();

            if (!mCapturing && mappedKey >= 0)
            {
                if (ImGui::SmallButton("Clear"))
                {
                    inputMap->SetButtonMapping(button, -1);
                    MarkInputMapDirty();
                }
            }
            ImGui::NextColumn();

            ImGui::PopID();
        }

        ImGui::Columns(1);
    }
}

void InputMapWindow::DrawGamepadAxes()
{
    InputMap* inputMap = InputMap::Get();

    if (ImGui::CollapsingHeader("Gamepad Axes — Keyboard", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Columns(5, "##gamepad_axes", true);
        ImGui::SetColumnWidth(0, 130.0f);
        ImGui::SetColumnWidth(1, 100.0f);
        ImGui::SetColumnWidth(2, 100.0f);
        ImGui::SetColumnWidth(3, 70.0f);
        ImGui::SetColumnWidth(4, 70.0f);

        ImGui::Text("Axis");
        ImGui::NextColumn();
        ImGui::Text("Key (-)");
        ImGui::NextColumn();
        ImGui::Text("Key (+)");
        ImGui::NextColumn();
        ImGui::Text("");
        ImGui::NextColumn();
        ImGui::Text("");
        ImGui::NextColumn();
        ImGui::Separator();

        for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
        {
            GamepadAxisCode axis = (GamepadAxisCode)i;
            const char* axisName = InputMap::GetGamepadAxisName(axis);
            int32_t posKey = -1;
            int32_t negKey = -1;
            inputMap->GetAxisMapping(axis, posKey, negKey);

            const char* posKeyName = (posKey >= 0) ? InputMap::GetKeyCodeName(posKey) : "\xe2\x80\x94";
            const char* negKeyName = (negKey >= 0) ? InputMap::GetKeyCodeName(negKey) : "\xe2\x80\x94";

            ImGui::PushID(100 + i);

            // Check if capturing this axis
            bool isCapNeg = mCapturing && mCaptureType == CaptureAxisNegative && mCaptureIndex == i;
            bool isCapPos = mCapturing && mCaptureType == CaptureAxisPositive && mCaptureIndex == i;

            ImGui::Text("%s", axisName);
            ImGui::NextColumn();

            if (isCapNeg)
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press a key...");
            }
            else
            {
                ImGui::Text("%s", negKeyName);
            }
            ImGui::NextColumn();

            if (isCapPos)
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press a key...");
            }
            else
            {
                ImGui::Text("%s", posKeyName);
            }
            ImGui::NextColumn();

            if (!mCapturing)
            {
                if (ImGui::SmallButton("(-) Key"))
                {
                    mCapturing = true;
                    mCaptureType = CaptureAxisNegative;
                    mCaptureIndex = i;
                    mCaptureTimer = 5.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("(+) Key"))
                {
                    mCapturing = true;
                    mCaptureType = CaptureAxisPositive;
                    mCaptureIndex = i;
                    mCaptureTimer = 5.0f;
                }
            }
            else
            {
                ImGui::TextDisabled("...");
            }
            ImGui::NextColumn();

            if (!mCapturing)
            {
                if (ImGui::SmallButton("Clear"))
                {
                    inputMap->SetAxisMapping(axis, -1, -1);
                    MarkInputMapDirty();
                }
            }
            ImGui::NextColumn();

            ImGui::PopID();
        }

        ImGui::Columns(1);
    }
}

void InputMapWindow::DrawMouseAxes()
{
    InputMap* inputMap = InputMap::Get();

    if (ImGui::CollapsingHeader("Gamepad Axes — Mouse Source", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("Drive gamepad axes from mouse movement. Useful for camera look without a right stick.");
        ImGui::Spacing();

        for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
        {
            GamepadAxisCode axis = (GamepadAxisCode)i;
            const char* axisName = InputMap::GetGamepadAxisName(axis);

            ImGui::PushID(200 + i);

            bool useMouseX = inputMap->GetAxisUsesMouseX(axis);
            bool useMouseY = inputMap->GetAxisUsesMouseY(axis);
            float sensitivity = inputMap->GetAxisMouseSensitivity(axis);

            bool changed = false;

            ImGui::Text("%s", axisName);
            ImGui::SameLine(140.0f);

            if (ImGui::Checkbox("Mouse X", &useMouseX))
                changed = true;

            ImGui::SameLine(260.0f);
            if (ImGui::Checkbox("Mouse Y", &useMouseY))
                changed = true;

            ImGui::SameLine(380.0f);
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::SliderFloat("##sens", &sensitivity, 0.1f, 5.0f, "%.1f"))
                changed = true;

            if (changed)
            {
                inputMap->SetAxisMouseMapping(axis, useMouseX, useMouseY, sensitivity);
                MarkInputMapDirty();
            }

            ImGui::PopID();
        }
    }
}

void InputMapWindow::DrawMousePointerSection()
{
    InputMap* inputMap = InputMap::Get();

    if (ImGui::CollapsingHeader("Mouse / Pointer", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool mouseEnabled = inputMap->IsMouseEnabled();
        if (ImGui::Checkbox("Mouse Input Enabled", &mouseEnabled))
        {
            inputMap->SetMouseEnabled(mouseEnabled);
            MarkInputMapDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("When disabled, all mouse queries (buttons, delta, position, scroll) return zero for game code.\nUse this during PIE to let gamepad right stick take over camera look.");

        bool pointerEnabled = inputMap->IsPointerEnabled();
        if (ImGui::Checkbox("Pointer Input Enabled", &pointerEnabled))
        {
            inputMap->SetPointerEnabled(pointerEnabled);
            MarkInputMapDirty();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("When disabled, all pointer queries (IsPointerDown, position) return false/zero for game code.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Mouse Buttons (reference):");
        ImGui::BulletText("Mouse Left   = MOUSE_LEFT (also Pointer 0)");
        ImGui::BulletText("Mouse Right  = MOUSE_RIGHT");
        ImGui::BulletText("Mouse Middle = MOUSE_MIDDLE");
        ImGui::BulletText("Mouse X1     = MOUSE_X1");
        ImGui::BulletText("Mouse X2     = MOUSE_X2");
        ImGui::BulletText("Scroll Wheel = GetScrollWheelDelta()");
        ImGui::BulletText("Pointer      = Unified mouse / touch input");
    }
}

void InputMapWindow::DrawCaptureOverlay()
{
    InputMap* inputMap = InputMap::Get();

    mCaptureTimer -= ImGui::GetIO().DeltaTime;

    // Timeout only — no escape cancel so Escape can be bound
    //
    if (mCaptureTimer <= 0.0f)
    {
        mCapturing = false;
        return;
    }

    // Poll for key press
    int32_t pressedKey = -1;
    if (PollKeyPress(pressedKey))
    {
        if (mCaptureType == CaptureButton)
        {
            inputMap->SetButtonMapping((GamepadButtonCode)mCaptureIndex, pressedKey);
        }
        else if (mCaptureType == CaptureAxisPositive)
        {
            int32_t posKey = -1;
            int32_t negKey = -1;
            inputMap->GetAxisMapping((GamepadAxisCode)mCaptureIndex, posKey, negKey);
            inputMap->SetAxisMapping((GamepadAxisCode)mCaptureIndex, pressedKey, negKey);
        }
        else if (mCaptureType == CaptureAxisNegative)
        {
            int32_t posKey = -1;
            int32_t negKey = -1;
            inputMap->GetAxisMapping((GamepadAxisCode)mCaptureIndex, posKey, negKey);
            inputMap->SetAxisMapping((GamepadAxisCode)mCaptureIndex, posKey, pressedKey);
        }

        MarkInputMapDirty();
        mCapturing = false;
        return;
    }

    // Display capture status at top of window
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    ImGui::Text("Listening for input... (%.0fs)", mCaptureTimer);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

#endif
