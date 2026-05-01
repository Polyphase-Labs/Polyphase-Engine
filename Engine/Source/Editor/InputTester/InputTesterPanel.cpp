#if EDITOR

#include "InputTesterPanel.h"
#include "EditorWidgets.h"

#include "Engine.h"
#include "Input/Input.h"
#include "Input/InputTypes.h"
#include "Input/InputConstants.h"

#include "imgui.h"

#include <stdint.h>
#include <string.h>

#if PLATFORM_WINDOWS
#include <hidsdi.h>
#endif

static InputTesterPanel sInputTesterPanel;

InputTesterPanel* GetInputTesterPanel()
{
    return &sInputTesterPanel;
}

static const char* GamepadTypeName(GamepadType type)
{
    switch (type)
    {
    case GamepadType::Standard:    return "Standard";
    case GamepadType::GameCube:    return "GameCube";
    case GamepadType::Wiimote:     return "Wiimote";
    case GamepadType::WiiClassic:  return "WiiClassic";
    case GamepadType::DualShock4:  return "DualShock4";
    case GamepadType::DualSense:   return "DualSense";
    default:                       return "Unknown";
    }
}

static const char* GamepadButtonShortName(int btn)
{
    switch (btn)
    {
    case GAMEPAD_A:        return "A";
    case GAMEPAD_B:        return "B";
    case GAMEPAD_C:        return "C";
    case GAMEPAD_X:        return "X";
    case GAMEPAD_Y:        return "Y";
    case GAMEPAD_Z:        return "Z";
    case GAMEPAD_L1:       return "L1";
    case GAMEPAD_R1:       return "R1";
    case GAMEPAD_L2:       return "L2";
    case GAMEPAD_R2:       return "R2";
    case GAMEPAD_THUMBL:   return "L3";
    case GAMEPAD_THUMBR:   return "R3";
    case GAMEPAD_START:    return "Start";
    case GAMEPAD_SELECT:   return "Sel";
    case GAMEPAD_LEFT:     return "D<";
    case GAMEPAD_RIGHT:    return "D>";
    case GAMEPAD_UP:       return "D^";
    case GAMEPAD_DOWN:     return "Dv";
    case GAMEPAD_L_LEFT:   return "L<";
    case GAMEPAD_L_RIGHT:  return "L>";
    case GAMEPAD_L_UP:     return "L^";
    case GAMEPAD_L_DOWN:   return "Lv";
    case GAMEPAD_R_LEFT:   return "R<";
    case GAMEPAD_R_RIGHT:  return "R>";
    case GAMEPAD_R_UP:     return "R^";
    case GAMEPAD_R_DOWN:   return "Rv";
    case GAMEPAD_HOME:     return "Home";
    default:               return "?";
    }
}

static const char* GamepadAxisName(int axis)
{
    switch (axis)
    {
    case GAMEPAD_AXIS_LTRIGGER: return "L2 Trigger";
    case GAMEPAD_AXIS_RTRIGGER: return "R2 Trigger";
    case GAMEPAD_AXIS_LTHUMB_X: return "L Stick X";
    case GAMEPAD_AXIS_LTHUMB_Y: return "L Stick Y";
    case GAMEPAD_AXIS_RTHUMB_X: return "R Stick X";
    case GAMEPAD_AXIS_RTHUMB_Y: return "R Stick Y";
    default:                    return "?";
    }
}

static const char* SlotBackendName(int slot)
{
#if PLATFORM_WINDOWS
    const InputState& input = GetEngineState()->mInput;
    if (slot < 0 || slot >= INPUT_MAX_GAMEPADS) return "-";
    if (input.mHidSlotUsed[slot])     return "Sony HID";
    if (input.mDInputSlotUsed[slot])  return "DirectInput";
    if (input.mGamepads[slot].mConnected) return "XInput";
    return "-";
#else
    const InputState& input = GetEngineState()->mInput;
    if (slot < 0 || slot >= INPUT_MAX_GAMEPADS) return "-";
    return input.mGamepads[slot].mConnected ? "Platform" : "-";
#endif
}

static void GetSlotVidPid(int slot, uint32_t& vid, uint32_t& pid, char* productName, size_t productNameSize)
{
    vid = 0;
    pid = 0;
    if (productName && productNameSize > 0) productName[0] = '\0';

#if PLATFORM_WINDOWS
    InputState& input = GetEngineState()->mInput;

    if (input.mHidSlotUsed[slot] && input.mHidDevices[slot] != INVALID_HANDLE_VALUE)
    {
        HIDD_ATTRIBUTES attrs;
        attrs.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(input.mHidDevices[slot], &attrs))
        {
            vid = attrs.VendorID;
            pid = attrs.ProductID;
        }
        if (productName && productNameSize > 0)
        {
            wchar_t wname[128] = {};
            if (HidD_GetProductString(input.mHidDevices[slot], wname, sizeof(wname)))
            {
                WideCharToMultiByte(CP_UTF8, 0, wname, -1, productName, (int)productNameSize, nullptr, nullptr);
            }
        }
    }
    else if (input.mDInputSlotUsed[slot] && input.mDInputDevices[slot] != nullptr)
    {
        DIDEVICEINSTANCE inst = {};
        inst.dwSize = sizeof(DIDEVICEINSTANCE);
        if (SUCCEEDED(input.mDInputDevices[slot]->GetDeviceInfo(&inst)))
        {
            vid = inst.guidProduct.Data1 & 0xFFFF;
            pid = (inst.guidProduct.Data1 >> 16) & 0xFFFF;
            if (productName && productNameSize > 0)
            {
#ifdef UNICODE
                WideCharToMultiByte(CP_UTF8, 0, inst.tszProductName, -1, productName, (int)productNameSize, nullptr, nullptr);
#else
                strncpy(productName, inst.tszProductName, productNameSize - 1);
                productName[productNameSize - 1] = '\0';
#endif
            }
        }
    }
#endif
}

static void DrawButtonCell(const char* label, bool down, bool justDown, bool justUp)
{
    ImVec4 color;
    if (down)
        color = ImVec4(0.20f, 0.75f, 0.25f, 1.0f);
    else if (justUp)
        color = ImVec4(0.75f, 0.40f, 0.20f, 1.0f);
    else
        color = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::Button(label, ImVec2(42.0f, 0.0f));
    ImGui::PopStyleColor(3);
    if (justDown)
    {
        ImVec2 p0 = ImGui::GetItemRectMin();
        ImVec2 p1 = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(p0, p1, IM_COL32(255, 230, 80, 255), 2.0f, 0, 2.0f);
    }
}

void InputTesterPanel::DrawGamepadSlot(int slot)
{
    InputState& input = GetEngineState()->mInput;
    const GamepadState& pad = input.mGamepads[slot];
    const GamepadState& prev = input.mPrevGamepads[slot];

    bool connected = pad.mConnected;

    ImVec4 dotColor = connected ? ImVec4(0.25f, 0.80f, 0.25f, 1.0f) : ImVec4(0.55f, 0.55f, 0.55f, 1.0f);

    char header[128];
    snprintf(header, sizeof(header), "Gamepad %d  [%s]  %s###slot%d",
             slot, SlotBackendName(slot), connected ? "connected" : "empty", slot);

    ImGui::PushStyleColor(ImGuiCol_Text, dotColor);
    ImGui::TextUnformatted(connected ? "\xE2\x97\x8F" : "\xE2\x97\x8B"); // filled/hollow circle
    ImGui::PopStyleColor();
    ImGui::SameLine();

    if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    uint32_t vid = 0, pid = 0;
    char productName[128] = {};
    GetSlotVidPid(slot, vid, pid, productName, sizeof(productName));

    ImGui::Text("Type:    %s", GamepadTypeName(pad.mType));
    if (vid != 0 || pid != 0)
    {
        ImGui::Text("VID/PID: 0x%04X / 0x%04X", vid, pid);
    }
    else
    {
        ImGui::Text("VID/PID: n/a");
    }
    if (productName[0] != '\0')
    {
        ImGui::Text("Name:    %s", productName);
    }

    if (!connected)
    {
        ImGui::Unindent();
        return;
    }

    ImGui::Spacing();
    ImGui::Text("Buttons:");

    const int kButtonsPerRow = 9;
    for (int b = 0; b < GAMEPAD_BUTTON_COUNT; ++b)
    {
        bool down     = pad.mButtons[b] != 0;
        bool prevDown = prev.mButtons[b] != 0;
        bool justDown = down && !prevDown;
        bool justUp   = !down && prevDown;

        char label[16];
        snprintf(label, sizeof(label), "%s##b%d_%d", GamepadButtonShortName(b), slot, b);
        DrawButtonCell(label, down, justDown, justUp);
        if (((b + 1) % kButtonsPerRow) != 0 && b != GAMEPAD_BUTTON_COUNT - 1)
            ImGui::SameLine();
    }

    ImGui::Spacing();
    ImGui::Text("Axes:");
    for (int a = 0; a < GAMEPAD_AXIS_COUNT; ++a)
    {
        float v = pad.mAxes[a];
        ImGui::Text("%-12s %+6.3f", GamepadAxisName(a), v);
        ImGui::SameLine(240.0f);
        float norm = (v + 1.0f) * 0.5f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        ImGui::ProgressBar(norm, ImVec2(260.0f, 0.0f), "");
    }

#if PLATFORM_WINDOWS
    if (mShowRawDInput && input.mDInputSlotUsed[slot])
    {
        ImGui::Spacing();
        ImGui::Text("Raw DIJOYSTATE2:");
        const DIJOYSTATE2& js = input.mDInputStates[slot];

        ImGui::Text("rgbButtons[0..31]:");
        for (int b = 0; b < 32; ++b)
        {
            bool down = (js.rgbButtons[b] & 0x80) != 0;
            ImVec4 c = down ? ImVec4(0.20f, 0.75f, 0.25f, 1.0f) : ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, c);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%d##rb%d_%d", b, slot, b);
            ImGui::Button(lbl, ImVec2(28.0f, 0.0f));
            ImGui::PopStyleColor(3);
            if (((b + 1) % 16) != 0 && b != 31) ImGui::SameLine();
        }

        ImGui::Spacing();
        ImGui::Text("POV[0]: %u%s  |  lX=%ld lY=%ld lZ=%ld  |  lRx=%ld lRy=%ld lRz=%ld",
                    (unsigned)js.rgdwPOV[0],
                    (LOWORD(js.rgdwPOV[0]) == 0xFFFF) ? " (centered)" : "",
                    js.lX, js.lY, js.lZ, js.lRx, js.lRy, js.lRz);
    }
#endif

    ImGui::Unindent();
    ImGui::Spacing();
}

void InputTesterPanel::DrawKeyboardSection()
{
    InputState& input = GetEngineState()->mInput;

    if (!ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    int firstDown = -1;
    int downCount = 0;
    for (int k = 0; k < INPUT_MAX_KEYS; ++k)
    {
        if (input.mKeys[k])
        {
            if (firstDown < 0) firstDown = k;
            downCount++;
        }
    }

    ImGui::Text("Keys held: %d", downCount);

    ImGui::BeginChild("KeysHeld", ImVec2(0, 80), true);
    for (int k = 0; k < INPUT_MAX_KEYS; ++k)
    {
        if (input.mKeys[k])
        {
            char c = INP_ConvertKeyCodeToChar(k);
            if (c != 0)
                ImGui::Text("  0x%02X ('%c')", k, c);
            else
                ImGui::Text("  0x%02X", k);
        }
    }
    ImGui::EndChild();

    ImGui::Unindent();
}

void InputTesterPanel::DrawMouseSection()
{
    InputState& input = GetEngineState()->mInput;

    if (!ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Indent();

    int32_t mx = 0, my = 0;
    INP_GetMousePosition(mx, my);
    ImGui::Text("Position: (%d, %d)", mx, my);

    int32_t dx = 0, dy = 0;
    INP_GetMouseDelta(dx, dy);
    ImGui::Text("Delta:    (%d, %d)", dx, dy);

    ImGui::Text("Wheel:    %d", INP_GetScrollWheelDelta());

    static const char* kMouseNames[MOUSE_BUTTON_COUNT] = { "Left", "Right", "Middle", "X1", "X2" };
    for (int b = 0; b < MOUSE_BUTTON_COUNT; ++b)
    {
        bool down = input.mMouseButtons[b];
        ImVec4 c = down ? ImVec4(0.20f, 0.75f, 0.25f, 1.0f) : ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, c);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
        ImGui::Button(kMouseNames[b], ImVec2(60.0f, 0.0f));
        ImGui::PopStyleColor(3);
        if (b != MOUSE_BUTTON_COUNT - 1) ImGui::SameLine();
    }

    ImGui::Unindent();
}

void InputTesterPanel::Draw()
{
    DrawContent();
}

void InputTesterPanel::DrawContent()
{
    Polyphase::Checkbox("Raw DInput", &mShowRawDInput);
    ImGui::SameLine();
    Polyphase::Checkbox("Keyboard",   &mShowKeyboard);
    ImGui::SameLine();
    Polyphase::Checkbox("Mouse",      &mShowMouse);

    ImGui::Separator();

    ImGui::BeginChild("InputTesterContent", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        DrawGamepadSlot(i);
    }

    if (mShowKeyboard)
    {
        DrawKeyboardSection();
    }

    if (mShowMouse)
    {
        DrawMouseSection();
    }

    ImGui::EndChild();
}

#endif
