#if PLATFORM_WINDOWS

#include "Input/Input.h"
#include "Input/InputUtils.h"

#include "Engine.h"
#include "Log.h"
#include "Profiler.h"

#include <atomic>

#include <set>
#include <vector>
#include <string>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#include <hidsdi.h>
#include <setupapi.h>

// ============================================================================
// Sony VID/PID constants
// ============================================================================
static const DWORD SONY_VID = 0x054C;
static const DWORD DS4_PID_V1 = 0x05C4;
static const DWORD DS4_PID_V2 = 0x09CC;
static const DWORD DUALSENSE_PID = 0x0CE6;
static const DWORD DUALSENSE_EDGE_PID = 0x0DF2;

static bool IsSonyPid(DWORD pid)
{
    return pid == DS4_PID_V1 || pid == DS4_PID_V2 ||
           pid == DUALSENSE_PID || pid == DUALSENSE_EDGE_PID;
}

static bool IsDualSensePid(DWORD pid)
{
    return pid == DUALSENSE_PID || pid == DUALSENSE_EDGE_PID;
}

// ============================================================================
// Parse DualSense/DS4 HID report into GamepadState
// ============================================================================
static void ParseDualSenseReport(const uint8_t* data, int size, bool bluetooth, GamepadState& pad)
{
    // USB: data starts at offset 0 (report ID already consumed or not present)
    // Bluetooth report 0x31: data starts at offset 1
    int off = bluetooth ? 1 : 0;

    if (size < off + 10) return;

    // Sticks: 0-255, center=128
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_X] = ((float)data[off + 0] - 128.0f) / 128.0f;
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_Y] = -((float)data[off + 1] - 128.0f) / 128.0f; // invert Y
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_X] = ((float)data[off + 2] - 128.0f) / 128.0f;
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_Y] = -((float)data[off + 3] - 128.0f) / 128.0f; // invert Y

    // Triggers: 0-255
    pad.mAxes[GAMEPAD_AXIS_LTRIGGER] = (float)data[off + 4] / 255.0f;
    pad.mAxes[GAMEPAD_AXIS_RTRIGGER] = (float)data[off + 5] / 255.0f;
    pad.mButtons[GAMEPAD_L2] = pad.mAxes[GAMEPAD_AXIS_LTRIGGER] > 0.2f;
    pad.mButtons[GAMEPAD_R2] = pad.mAxes[GAMEPAD_AXIS_RTRIGGER] > 0.2f;

    // Buttons byte 1 (offset+7 for USB, offset+7 for BT)
    // Skip counter at off+6
    uint8_t btn1 = data[off + 7];
    uint8_t dpad = btn1 & 0x0F;
    pad.mButtons[GAMEPAD_X] = (btn1 & 0x10) != 0;      // Square
    pad.mButtons[GAMEPAD_A] = (btn1 & 0x20) != 0;      // Cross
    pad.mButtons[GAMEPAD_B] = (btn1 & 0x40) != 0;      // Circle
    pad.mButtons[GAMEPAD_Y] = (btn1 & 0x80) != 0;      // Triangle

    // D-pad from 4-bit value (0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW, 8=released)
    pad.mButtons[GAMEPAD_UP]    = (dpad == 0 || dpad == 1 || dpad == 7);
    pad.mButtons[GAMEPAD_RIGHT] = (dpad == 1 || dpad == 2 || dpad == 3);
    pad.mButtons[GAMEPAD_DOWN]  = (dpad == 3 || dpad == 4 || dpad == 5);
    pad.mButtons[GAMEPAD_LEFT]  = (dpad == 5 || dpad == 6 || dpad == 7);

    // Buttons byte 2
    uint8_t btn2 = data[off + 8];
    pad.mButtons[GAMEPAD_L1]     = (btn2 & 0x01) != 0;
    pad.mButtons[GAMEPAD_R1]     = (btn2 & 0x02) != 0;
    // btn2 & 0x04 = L2 digital (already handled via axis)
    // btn2 & 0x08 = R2 digital (already handled via axis)
    pad.mButtons[GAMEPAD_SELECT] = (btn2 & 0x10) != 0;  // Create/Share
    pad.mButtons[GAMEPAD_START]  = (btn2 & 0x20) != 0;  // Options
    pad.mButtons[GAMEPAD_THUMBL] = (btn2 & 0x40) != 0;  // L3
    pad.mButtons[GAMEPAD_THUMBR] = (btn2 & 0x80) != 0;  // R3

    // Buttons byte 3
    uint8_t btn3 = data[off + 9];
    pad.mButtons[GAMEPAD_HOME] = (btn3 & 0x01) != 0;    // PS button
    // btn3 & 0x02 = touchpad click
    // btn3 & 0x04 = mute button
}

static void ParseDS4Report(const uint8_t* data, int size, bool bluetooth, GamepadState& pad)
{
    // DS4 USB: report ID 0x01, data after ID
    // DS4 BT:  report ID 0x11, data at offset 2
    int off = bluetooth ? 2 : 0;

    if (size < off + 10) return;

    // Sticks: 0-255, center=128
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_X] = ((float)data[off + 0] - 128.0f) / 128.0f;
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_Y] = -((float)data[off + 1] - 128.0f) / 128.0f;
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_X] = ((float)data[off + 2] - 128.0f) / 128.0f;
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_Y] = -((float)data[off + 3] - 128.0f) / 128.0f;

    // Buttons byte 1 (offset 4 for DS4)
    uint8_t btn1 = data[off + 4];
    uint8_t dpad = btn1 & 0x0F;
    pad.mButtons[GAMEPAD_X] = (btn1 & 0x10) != 0;      // Square
    pad.mButtons[GAMEPAD_A] = (btn1 & 0x20) != 0;      // Cross
    pad.mButtons[GAMEPAD_B] = (btn1 & 0x40) != 0;      // Circle
    pad.mButtons[GAMEPAD_Y] = (btn1 & 0x80) != 0;      // Triangle

    pad.mButtons[GAMEPAD_UP]    = (dpad == 0 || dpad == 1 || dpad == 7);
    pad.mButtons[GAMEPAD_RIGHT] = (dpad == 1 || dpad == 2 || dpad == 3);
    pad.mButtons[GAMEPAD_DOWN]  = (dpad == 3 || dpad == 4 || dpad == 5);
    pad.mButtons[GAMEPAD_LEFT]  = (dpad == 5 || dpad == 6 || dpad == 7);

    // Buttons byte 2
    uint8_t btn2 = data[off + 5];
    pad.mButtons[GAMEPAD_L1]     = (btn2 & 0x01) != 0;
    pad.mButtons[GAMEPAD_R1]     = (btn2 & 0x02) != 0;
    pad.mButtons[GAMEPAD_SELECT] = (btn2 & 0x10) != 0;
    pad.mButtons[GAMEPAD_START]  = (btn2 & 0x20) != 0;
    pad.mButtons[GAMEPAD_THUMBL] = (btn2 & 0x40) != 0;
    pad.mButtons[GAMEPAD_THUMBR] = (btn2 & 0x80) != 0;

    // Buttons byte 3
    uint8_t btn3 = data[off + 6];
    pad.mButtons[GAMEPAD_HOME] = (btn3 & 0x01) != 0;

    // Triggers (offset 7, 8 for DS4)
    pad.mAxes[GAMEPAD_AXIS_LTRIGGER] = (float)data[off + 7] / 255.0f;
    pad.mAxes[GAMEPAD_AXIS_RTRIGGER] = (float)data[off + 8] / 255.0f;
    pad.mButtons[GAMEPAD_L2] = pad.mAxes[GAMEPAD_AXIS_LTRIGGER] > 0.2f;
    pad.mButtons[GAMEPAD_R2] = pad.mAxes[GAMEPAD_AXIS_RTRIGGER] > 0.2f;
}

// ============================================================================
// Sony HID device enumeration and management
// ============================================================================
static ULONGLONG sLastHidEnumTime = 0;

// Device-topology dirty flag. WndProc (System_Windows.cpp) sets this on
// WM_DEVICECHANGE; the two enumeration functions below consume it. We also
// keep a very long fallback timer (60s) as a safety net in case any device
// change event is missed. This replaces the old 2s periodic enumeration,
// which was the direct cause of the recurring INP frame spike.
static std::atomic<bool> sDeviceChangeDirty{ true };  // initial enum forced

extern "C" void INP_NotifyDeviceChange()
{
    sDeviceChangeDirty.store(true, std::memory_order_relaxed);
}

static void EnumerateSonyHidDevices(InputState& input)
{
    ULONGLONG now = GetTickCount64();
    bool dirty = sDeviceChangeDirty.load(std::memory_order_relaxed);
    if (!dirty && (now - sLastHidEnumTime < 60000)) return;
    sLastHidEnumTime = now;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) return;

    SP_DEVICE_INTERFACE_DATA ifaceData;
    ifaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, idx, &ifaceData); ++idx)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfo, &ifaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) continue;

        std::vector<uint8_t> detailBuf(requiredSize);
        SP_DEVICE_INTERFACE_DETAIL_DATA_A* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)detailBuf.data();
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(devInfo, &ifaceData, detail, requiredSize, nullptr, nullptr))
            continue;

        // Open device to check VID/PID
        HANDLE hDevice = CreateFileA(detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

        if (hDevice == INVALID_HANDLE_VALUE)
        {
            // Try read-only (some devices don't allow write)
            hDevice = CreateFileA(detail->DevicePath,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        }

        if (hDevice == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrs;
        attrs.Size = sizeof(HIDD_ATTRIBUTES);
        if (!HidD_GetAttributes(hDevice, &attrs))
        {
            CloseHandle(hDevice);
            continue;
        }

        // Only care about Sony controllers
        if (attrs.VendorID != SONY_VID || !IsSonyPid(attrs.ProductID))
        {
            CloseHandle(hDevice);
            continue;
        }

        // Check if already tracked
        bool alreadyTracked = false;
        for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
        {
            if (input.mHidSlotUsed[i] && input.mHidDevices[i] == hDevice)
            {
                alreadyTracked = true;
                break;
            }
        }

        // Also check by trying to avoid duplicates — compare device path via handle validity
        // Since handles are unique per open, check if we already have this device open
        // by checking device paths. For simplicity, check if any HID slot has a valid handle.
        if (!alreadyTracked)
        {
            // More robust check: see if we already have a slot with same VID/PID that's still connected
            for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
            {
                if (input.mHidSlotUsed[i] && input.mHidDevices[i] != INVALID_HANDLE_VALUE)
                {
                    HIDD_ATTRIBUTES existingAttrs;
                    existingAttrs.Size = sizeof(HIDD_ATTRIBUTES);
                    if (HidD_GetAttributes(input.mHidDevices[i], &existingAttrs) &&
                        existingAttrs.VendorID == attrs.VendorID &&
                        existingAttrs.ProductID == attrs.ProductID)
                    {
                        // Same device type already tracked — check if it's this exact device
                        // by comparing device path string
                        // For now, allow multiple same-type controllers
                    }
                }
            }
        }

        // Find a free slot
        int slot = -1;
        for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
        {
            if (!input.mGamepads[i].mConnected && !input.mDInputSlotUsed[i] && !input.mHidSlotUsed[i])
            {
                slot = i;
                break;
            }
        }

        if (slot < 0)
        {
            CloseHandle(hDevice);
            continue;
        }

        // Check if we already have this exact device path open
        // Simple approach: check if any existing HID handle matches this path
        bool duplicate = false;
        for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
        {
            if (input.mHidSlotUsed[i] && input.mHidDevices[i] != INVALID_HANDLE_VALUE)
            {
                // Can't easily compare handles to paths, so skip for now
                // The re-enum will just fail to open an already-open device
                duplicate = true;
                break;
            }
        }

        if (duplicate)
        {
            CloseHandle(hDevice);
            continue;
        }

        // Set up the slot
        input.mHidDevices[slot] = hDevice;
        input.mHidSlotUsed[slot] = true;
        input.mHidReadPending[slot] = false;
        memset(&input.mHidOverlapped[slot], 0, sizeof(OVERLAPPED));
        input.mHidOverlapped[slot].hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        input.mGamepads[slot].mConnected = true;
        if (IsDualSensePid(attrs.ProductID))
            input.mGamepads[slot].mType = GamepadType::DualSense;
        else
            input.mGamepads[slot].mType = GamepadType::DualShock4;

        LogDebug("HID: Acquired Sony controller in slot %d (VID=0x%04X PID=0x%04X)",
                 slot, attrs.VendorID, attrs.ProductID);

        // Only claim one Sony device per enumeration pass to avoid duplicates
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfo);
}

static void PollSonyHidDevices(InputState& input)
{
    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (!input.mHidSlotUsed[i] || input.mHidDevices[i] == INVALID_HANDLE_VALUE)
            continue;

        // Start an async read if none is pending
        if (!input.mHidReadPending[i])
        {
            ResetEvent(input.mHidOverlapped[i].hEvent);
            memset(input.mHidReportBuf[i], 0, sizeof(input.mHidReportBuf[i]));

            BOOL result = ReadFile(input.mHidDevices[i],
                input.mHidReportBuf[i], sizeof(input.mHidReportBuf[i]),
                nullptr, &input.mHidOverlapped[i]);

            if (result || GetLastError() == ERROR_IO_PENDING)
            {
                input.mHidReadPending[i] = true;
            }
            else
            {
                // Device disconnected
                CloseHandle(input.mHidOverlapped[i].hEvent);
                CloseHandle(input.mHidDevices[i]);
                input.mHidDevices[i] = INVALID_HANDLE_VALUE;
                input.mHidSlotUsed[i] = false;
                input.mHidReadPending[i] = false;
                input.mGamepads[i].mConnected = false;
                memset(&input.mGamepads[i], 0, sizeof(GamepadState));
                continue;
            }
        }

        // Check if read completed
        DWORD bytesRead = 0;
        BOOL completed = GetOverlappedResult(input.mHidDevices[i],
            &input.mHidOverlapped[i], &bytesRead, FALSE);

        if (completed && bytesRead > 0)
        {
            input.mHidReadPending[i] = false;
            input.mGamepads[i].mConnected = true;

            uint8_t* buf = input.mHidReportBuf[i];
            uint8_t reportId = buf[0];
            GamepadType type = input.mGamepads[i].mType;

            if (type == GamepadType::DualSense)
            {
                if (reportId == 0x01)
                {
                    // USB mode: data starts at buf[1]
                    ParseDualSenseReport(buf + 1, (int)bytesRead - 1, false, input.mGamepads[i]);
                }
                else if (reportId == 0x31)
                {
                    // Bluetooth mode: data starts at buf[1], but report format has extra header
                    ParseDualSenseReport(buf + 1, (int)bytesRead - 1, true, input.mGamepads[i]);
                }
            }
            else if (type == GamepadType::DualShock4)
            {
                if (reportId == 0x01)
                {
                    ParseDS4Report(buf + 1, (int)bytesRead - 1, false, input.mGamepads[i]);
                }
                else if (reportId == 0x11)
                {
                    ParseDS4Report(buf + 1, (int)bytesRead - 1, true, input.mGamepads[i]);
                }
            }
        }
        else if (!completed && GetLastError() == ERROR_IO_INCOMPLETE)
        {
            // Still pending — keep waiting
        }
        else
        {
            // Error — device likely disconnected
            input.mHidReadPending[i] = false;
            CancelIo(input.mHidDevices[i]);
            CloseHandle(input.mHidOverlapped[i].hEvent);
            CloseHandle(input.mHidDevices[i]);
            input.mHidDevices[i] = INVALID_HANDLE_VALUE;
            input.mHidSlotUsed[i] = false;
            input.mGamepads[i].mConnected = false;
            memset(&input.mGamepads[i], 0, sizeof(GamepadState));
        }
    }
}

// ============================================================================
// XInput device detection via RawInput
// ============================================================================
static std::set<std::pair<DWORD, DWORD>> sXInputVidPids;

static void BuildXInputDeviceList()
{
    sXInputVidPids.clear();

    UINT numDevices = 0;
    GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST));
    if (numDevices == 0) return;

    std::vector<RAWINPUTDEVICELIST> devices(numDevices);
    if (GetRawInputDeviceList(devices.data(), &numDevices, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
        return;

    for (UINT i = 0; i < numDevices; ++i)
    {
        if (devices[i].dwType != RIM_TYPEHID) continue;

        UINT nameLen = 0;
        GetRawInputDeviceInfoA(devices[i].hDevice, RIDI_DEVICENAME, nullptr, &nameLen);
        if (nameLen == 0) continue;

        std::string name(nameLen, '\0');
        GetRawInputDeviceInfoA(devices[i].hDevice, RIDI_DEVICENAME, &name[0], &nameLen);

        // XInput devices have "&IG_" in their device path
        if (name.find("&IG_") != std::string::npos || name.find("&ig_") != std::string::npos)
        {
            DWORD vid = 0, pid = 0;
            size_t vidPos = name.find("VID_");
            if (vidPos == std::string::npos) vidPos = name.find("vid_");
            size_t pidPos = name.find("PID_");
            if (pidPos == std::string::npos) pidPos = name.find("pid_");

            if (vidPos != std::string::npos)
                vid = strtoul(name.c_str() + vidPos + 4, nullptr, 16);
            if (pidPos != std::string::npos)
                pid = strtoul(name.c_str() + pidPos + 4, nullptr, 16);

            if (vid != 0 && pid != 0)
            {
                sXInputVidPids.insert({ vid, pid });
            }
        }
    }
}

static bool IsXInputDevice(const GUID& productGuid)
{
    DWORD vid = productGuid.Data1 & 0xFFFF;
    DWORD pid = (productGuid.Data1 >> 16) & 0xFFFF;
    return sXInputVidPids.count({ vid, pid }) > 0;
}

// ============================================================================
// DirectInput D-Pad mapping from POV hat
// ============================================================================
static void MapDInputDPad(DWORD pov, GamepadState& pad)
{
    pad.mButtons[GAMEPAD_UP] = false;
    pad.mButtons[GAMEPAD_DOWN] = false;
    pad.mButtons[GAMEPAD_LEFT] = false;
    pad.mButtons[GAMEPAD_RIGHT] = false;

    if (LOWORD(pov) == 0xFFFF) return; // centered

    if (pov >= 31500 || pov <= 4500)  pad.mButtons[GAMEPAD_UP] = true;
    if (pov >= 4500 && pov <= 13500)  pad.mButtons[GAMEPAD_RIGHT] = true;
    if (pov >= 13500 && pov <= 22500) pad.mButtons[GAMEPAD_DOWN] = true;
    if (pov >= 22500 && pov <= 31500) pad.mButtons[GAMEPAD_LEFT] = true;
}

// ============================================================================
// Read generic DirectInput controller state (best-guess mapping)
// ============================================================================
static void ReadDInputGenericState(const DIJOYSTATE2& js, GamepadState& pad)
{
    // Sticks
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_X] = (float)js.lX / 32767.0f;
    pad.mAxes[GAMEPAD_AXIS_LTHUMB_Y] = -(float)js.lY / 32767.0f;
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_X] = (float)js.lZ / 32767.0f;
    pad.mAxes[GAMEPAD_AXIS_RTHUMB_Y] = -(float)js.lRz / 32767.0f;

    // Triggers (generic: try sliders first, fall back to Rx/Ry)
    pad.mAxes[GAMEPAD_AXIS_LTRIGGER] = ((float)js.lRx + 32767.0f) / 65534.0f;
    pad.mAxes[GAMEPAD_AXIS_RTRIGGER] = ((float)js.lRy + 32767.0f) / 65534.0f;
    pad.mButtons[GAMEPAD_L2] = pad.mAxes[GAMEPAD_AXIS_LTRIGGER] > 0.2f;
    pad.mButtons[GAMEPAD_R2] = pad.mAxes[GAMEPAD_AXIS_RTRIGGER] > 0.2f;

    // Generic button mapping (0=A, 1=B, 2=X, 3=Y, ...)
    if (js.rgbButtons[0] & 0x80) pad.mButtons[GAMEPAD_A] = true;
    if (js.rgbButtons[1] & 0x80) pad.mButtons[GAMEPAD_B] = true;
    if (js.rgbButtons[2] & 0x80) pad.mButtons[GAMEPAD_X] = true;
    if (js.rgbButtons[3] & 0x80) pad.mButtons[GAMEPAD_Y] = true;
    if (js.rgbButtons[4] & 0x80) pad.mButtons[GAMEPAD_L1] = true;
    if (js.rgbButtons[5] & 0x80) pad.mButtons[GAMEPAD_R1] = true;
    if (js.rgbButtons[6] & 0x80) pad.mButtons[GAMEPAD_SELECT] = true;
    if (js.rgbButtons[7] & 0x80) pad.mButtons[GAMEPAD_START] = true;
    if (js.rgbButtons[8] & 0x80) pad.mButtons[GAMEPAD_THUMBL] = true;
    if (js.rgbButtons[9] & 0x80) pad.mButtons[GAMEPAD_THUMBR] = true;
    if (js.rgbButtons[10] & 0x80) pad.mButtons[GAMEPAD_HOME] = true;

    MapDInputDPad(js.rgdwPOV[0], pad);
}

// ============================================================================
// DirectInput device enumeration callback
// ============================================================================
struct DInputEnumContext
{
    InputState* input;
    int nextSlot;
};

static BOOL CALLBACK DInputEnumDevicesCallback(const DIDEVICEINSTANCE* inst, VOID* context)
{
    DInputEnumContext* ctx = (DInputEnumContext*)context;
    InputState& input = *ctx->input;

    if (ctx->nextSlot >= INPUT_MAX_GAMEPADS)
        return DIENUM_STOP;

    // Skip XInput devices
    if (IsXInputDevice(inst->guidProduct))
        return DIENUM_CONTINUE;

    // Skip Sony devices (handled via raw HID instead)
    {
        DWORD vid = inst->guidProduct.Data1 & 0xFFFF;
        DWORD pid = (inst->guidProduct.Data1 >> 16) & 0xFFFF;
        if (vid == SONY_VID && IsSonyPid(pid))
            return DIENUM_CONTINUE;
    }

    // Check if this device is already acquired in a slot
    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (input.mDInputSlotUsed[i] &&
            IsEqualGUID(input.mDInputDeviceGUIDs[i], inst->guidInstance))
        {
            return DIENUM_CONTINUE;
        }
    }

    // Find next available slot (not used by XInput, DInput, or HID)
    while (ctx->nextSlot < INPUT_MAX_GAMEPADS &&
           (input.mGamepads[ctx->nextSlot].mConnected || input.mDInputSlotUsed[ctx->nextSlot] || input.mHidSlotUsed[ctx->nextSlot]))
    {
        ctx->nextSlot++;
    }
    if (ctx->nextSlot >= INPUT_MAX_GAMEPADS)
        return DIENUM_STOP;

    int slot = ctx->nextSlot;

    // Create and configure the device
    IDirectInputDevice8* device = nullptr;
    HRESULT hr = input.mDirectInput->CreateDevice(inst->guidInstance, &device, nullptr);
    if (FAILED(hr)) return DIENUM_CONTINUE;

    hr = device->SetDataFormat(&c_dfDIJoystick2);
    if (FAILED(hr)) { device->Release(); return DIENUM_CONTINUE; }

    // Background + nonexclusive so it works even without focus
    HWND hwnd = GetEngineState()->mSystem.mWindow;
    hr = device->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) { device->Release(); return DIENUM_CONTINUE; }

    // Set axis range per-axis to -32767..32767 to match XInput normalization
    {
        DIPROPRANGE range;
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwHow = DIPH_BYOFFSET;
        range.lMin = -32767;
        range.lMax = 32767;

        DWORD axisOffsets[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ };
        for (int a = 0; a < 6; ++a)
        {
            range.diph.dwObj = axisOffsets[a];
            device->SetProperty(DIPROP_RANGE, &range.diph);
        }
    }

    hr = device->Acquire();
    if (FAILED(hr)) { device->Release(); return DIENUM_CONTINUE; }

    // Store in slot
    input.mDInputDevices[slot] = device;
    input.mDInputDeviceGUIDs[slot] = inst->guidInstance;
    input.mDInputSlotUsed[slot] = true;

    // Determine gamepad type from VID/PID
    DWORD vid = inst->guidProduct.Data1 & 0xFFFF;
    DWORD pid = (inst->guidProduct.Data1 >> 16) & 0xFFFF;
    if (vid == SONY_VID)
    {
        if (pid == DUALSENSE_PID || pid == DUALSENSE_EDGE_PID)
            input.mGamepads[slot].mType = GamepadType::DualSense;
        else if (pid == DS4_PID_V1 || pid == DS4_PID_V2)
            input.mGamepads[slot].mType = GamepadType::DualShock4;
        else
            input.mGamepads[slot].mType = GamepadType::Standard;
    }
    else
    {
        input.mGamepads[slot].mType = GamepadType::Standard;
    }

    input.mGamepads[slot].mConnected = true;

    LogDebug("DirectInput: Acquired device '%s' in slot %d (VID=0x%04X PID=0x%04X)",
             inst->tszProductName, slot, vid, pid);

    ctx->nextSlot++;
    return DIENUM_CONTINUE;
}

// ============================================================================
// DirectInput polling for all acquired DInput devices
// ============================================================================
static void PollDirectInputDevices(InputState& input)
{
    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (!input.mDInputSlotUsed[i] || input.mDInputDevices[i] == nullptr)
            continue;

        HRESULT hr = input.mDInputDevices[i]->Poll();
        if (FAILED(hr))
        {
            hr = input.mDInputDevices[i]->Acquire();
            if (FAILED(hr))
            {
                input.mDInputDevices[i]->Release();
                input.mDInputDevices[i] = nullptr;
                input.mDInputSlotUsed[i] = false;
                input.mGamepads[i].mConnected = false;
                memset(&input.mGamepads[i], 0, sizeof(GamepadState));
                continue;
            }
            input.mDInputDevices[i]->Poll();
        }

        memset(&input.mDInputStates[i], 0, sizeof(DIJOYSTATE2));
        hr = input.mDInputDevices[i]->GetDeviceState(sizeof(DIJOYSTATE2), &input.mDInputStates[i]);
        if (SUCCEEDED(hr))
        {
            input.mGamepads[i].mConnected = true;
            ReadDInputGenericState(input.mDInputStates[i], input.mGamepads[i]);
        }
        else
        {
            input.mGamepads[i].mConnected = false;
        }
    }
}

// DirectInput re-enumeration. Same event-driven scheme as Sony HID above —
// only runs when WM_DEVICECHANGE fires, with a 60s fallback safety net.
static ULONGLONG sLastDInputEnumTime = 0;

static void EnumerateDirectInputDevices(InputState& input)
{
    ULONGLONG now = GetTickCount64();
    bool dirty = sDeviceChangeDirty.load(std::memory_order_relaxed);
    if (!dirty && (now - sLastDInputEnumTime < 60000)) return;
    sLastDInputEnumTime = now;

    BuildXInputDeviceList();

    DInputEnumContext ctx;
    ctx.input = &input;
    ctx.nextSlot = 0;

    input.mDirectInput->EnumDevices(
        DI8DEVCLASS_GAMECTRL,
        DInputEnumDevicesCallback,
        &ctx,
        DIEDFL_ATTACHEDONLY
    );
}

// ============================================================================
// Public API
// ============================================================================

void INP_Initialize()
{
    RAWINPUTDEVICE Rid;

    // Mouse
    Rid.usUsagePage = 1;
    Rid.usUsage = 2;
    Rid.dwFlags = 0;
    Rid.hwndTarget = NULL;

    if (!RegisterRawInputDevices(&Rid, 1, sizeof(RAWINPUTDEVICE)))
    {
        LogError("Failed to register RawInput device");
    }

    InputInit();

    // Initialize DirectInput8
    InputState& input = GetEngineState()->mInput;
    HRESULT hr = DirectInput8Create(
        GetModuleHandle(NULL),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        (void**)&input.mDirectInput,
        nullptr
    );

    if (FAILED(hr))
    {
        LogWarning("Failed to initialize DirectInput8 (HRESULT=0x%08X)", hr);
        input.mDirectInput = nullptr;
    }
    else
    {
        LogDebug("DirectInput8 initialized");
        // Initial enumeration
        BuildXInputDeviceList();
        sLastDInputEnumTime = 0;
        EnumerateDirectInputDevices(input);
    }

    // Initial Sony HID enumeration
    sLastHidEnumTime = 0;
    EnumerateSonyHidDevices(input);
}

void INP_Shutdown()
{
    InputState& input = GetEngineState()->mInput;

    // Release all HID devices
    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (input.mHidSlotUsed[i] && input.mHidDevices[i] != INVALID_HANDLE_VALUE)
        {
            if (input.mHidReadPending[i])
                CancelIo(input.mHidDevices[i]);
            if (input.mHidOverlapped[i].hEvent)
                CloseHandle(input.mHidOverlapped[i].hEvent);
            CloseHandle(input.mHidDevices[i]);
            input.mHidDevices[i] = INVALID_HANDLE_VALUE;
        }
        input.mHidSlotUsed[i] = false;
        input.mHidReadPending[i] = false;
    }

    // Release all DirectInput devices
    for (int i = 0; i < INPUT_MAX_GAMEPADS; ++i)
    {
        if (input.mDInputDevices[i])
        {
            input.mDInputDevices[i]->Unacquire();
            input.mDInputDevices[i]->Release();
            input.mDInputDevices[i] = nullptr;
        }
        input.mDInputSlotUsed[i] = false;
    }

    // Release DirectInput interface
    if (input.mDirectInput)
    {
        input.mDirectInput->Release();
        input.mDirectInput = nullptr;
    }

    InputShutdown();
}

// Last poll timestamp per XInput slot. Used to throttle polling of
// DISCONNECTED slots — XInputGetState on an empty slot triggers USB device
// enumeration internally and is the classic cause of periodic frame spikes.
// Microsoft explicitly recommends against polling empty slots every frame.
static ULONGLONG sLastXInputPollTime[XUSER_MAX_COUNT] = { 0 };

void INP_Update()
{
    {
        SCOPED_FRAME_STAT("INP.Advance");
        InputAdvanceFrame();
    }

    InputState& input = GetEngineState()->mInput;

    // === XInput Pass ===
    SCOPED_FRAME_STAT("INP.XInput");
    memset(input.mXinputStates, 0, sizeof(XINPUT_STATE) * INPUT_MAX_GAMEPADS);

    ULONGLONG nowMs = GetTickCount64();
    for (int32_t i = 0; i < XUSER_MAX_COUNT && i < INPUT_MAX_GAMEPADS; i++)
    {
        // Skip slots owned by DirectInput or HID
        if (input.mDInputSlotUsed[i] || input.mHidSlotUsed[i])
            continue;

        // Throttle disconnected-slot polls to 1Hz. Connected slots keep polling
        // every frame so input stays low-latency for active controllers.
        bool wasConnected = input.mGamepads[i].mConnected;
        if (!wasConnected && (nowMs - sLastXInputPollTime[i] < 1000))
        {
            continue;
        }
        sLastXInputPollTime[i] = nowMs;

        if (!XInputGetState(i, &input.mXinputStates[i]))
        {
            input.mGamepads[i].mConnected = true;
            input.mGamepads[i].mType = GamepadType::Standard;

            // Buttons
            input.mGamepads[i].mButtons[GAMEPAD_A] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_A;
            input.mGamepads[i].mButtons[GAMEPAD_B] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_B;
            input.mGamepads[i].mButtons[GAMEPAD_X] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_X;
            input.mGamepads[i].mButtons[GAMEPAD_Y] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_Y;
            input.mGamepads[i].mButtons[GAMEPAD_L1] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            input.mGamepads[i].mButtons[GAMEPAD_R1] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
            input.mGamepads[i].mButtons[GAMEPAD_THUMBL] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
            input.mGamepads[i].mButtons[GAMEPAD_THUMBR] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
            input.mGamepads[i].mButtons[GAMEPAD_START] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_START;
            input.mGamepads[i].mButtons[GAMEPAD_SELECT] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
            input.mGamepads[i].mButtons[GAMEPAD_LEFT] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            input.mGamepads[i].mButtons[GAMEPAD_RIGHT] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            input.mGamepads[i].mButtons[GAMEPAD_UP] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
            input.mGamepads[i].mButtons[GAMEPAD_DOWN] = input.mXinputStates[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;

            // Axes
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_LTRIGGER] = (float)input.mXinputStates[i].Gamepad.bLeftTrigger / 255;
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_RTRIGGER] = (float)input.mXinputStates[i].Gamepad.bRightTrigger / 255;
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_LTHUMB_X] = (float)input.mXinputStates[i].Gamepad.sThumbLX / 32767;
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_LTHUMB_Y] = (float)input.mXinputStates[i].Gamepad.sThumbLY / 32767;
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_RTHUMB_X] = (float)input.mXinputStates[i].Gamepad.sThumbRX / 32767;
            input.mGamepads[i].mAxes[GAMEPAD_AXIS_RTHUMB_Y] = (float)input.mXinputStates[i].Gamepad.sThumbRY / 32767;

            // Set digital inputs for analog triggers
            input.mGamepads[i].mButtons[GAMEPAD_L2] = input.mGamepads[i].mAxes[GAMEPAD_AXIS_LTRIGGER] > 0.2f;
            input.mGamepads[i].mButtons[GAMEPAD_R2] = input.mGamepads[i].mAxes[GAMEPAD_AXIS_RTRIGGER] > 0.2f;
        }
        else
        {
            input.mGamepads[i].mConnected = false;
        }
    }

    // === Sony HID Pass ===
    {
        SCOPED_FRAME_STAT("INP.SonyPoll");
        PollSonyHidDevices(input);
    }
    {
        SCOPED_FRAME_STAT("INP.SonyEnum");
        EnumerateSonyHidDevices(input);
    }

    // === DirectInput Pass (for non-Sony, non-XInput controllers) ===
    if (input.mDirectInput != nullptr)
    {
        {
            SCOPED_FRAME_STAT("INP.DInputPoll");
            PollDirectInputDevices(input);
        }
        {
            SCOPED_FRAME_STAT("INP.DInputEnum");
            EnumerateDirectInputDevices(input);
        }
    }

    {
        SCOPED_FRAME_STAT("INP.Post");
        InputPostUpdate();
    }

    // Both enums have now consumed the dirty flag; reset for next event.
    sDeviceChangeDirty.store(false, std::memory_order_relaxed);
}

void INP_SetCursorPos(int32_t x, int32_t y)
{
    INP_SetMousePosition(x, y);

    POINT screenPoint = { x, y };
    ClientToScreen(GetEngineState()->mSystem.mWindow, &screenPoint);

    SetCursorPos(screenPoint.x, screenPoint.y);
}

void INP_ShowCursor(bool show)
{
    SystemState& system = GetEngineState()->mSystem;

    // Calling ShowCursor(true) twice will cause a sort of ref counting problem,
    // and then future ShowCursor(false) calls won't work.
    if (system.mWindowHasFocus && show != GetEngineState()->mInput.mCursorShown)
    {
        ShowCursor(show);
    }

    GetEngineState()->mInput.mCursorShown = show;
}

void INP_LockCursor(bool lock)
{
    SystemState& system = GetEngineState()->mSystem;
    InputState& input = GetEngineState()->mInput;
    input.mCursorLocked = lock;
}

void INP_TrapCursor(bool trap)
{
    SystemState& system = GetEngineState()->mSystem;
    InputState& input = GetEngineState()->mInput;

    if (system.mWindowHasFocus)
    {
        if (trap)
        {
            RECT rect;
            GetClientRect(system.mWindow, &rect);

            POINT tl;
            tl.x = rect.left;
            tl.y = rect.top;

            POINT br;
            br.x = rect.right;
            br.y = rect.bottom;

            MapWindowPoints(system.mWindow, nullptr, &tl, 1);
            MapWindowPoints(system.mWindow, nullptr, &br, 1);

            rect.left = tl.x;
            rect.top = tl.y;
            rect.right = br.x;
            rect.bottom = br.y;

            ClipCursor(&rect);
        }
        else
        {
            ClipCursor(nullptr);
        }
    }

    input.mCursorTrapped = trap;
}

void INP_TrapCursorToRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    SystemState& system = GetEngineState()->mSystem;
    InputState& input = GetEngineState()->mInput;

    if (system.mWindowHasFocus)
    {
        POINT tl = { (LONG)x, (LONG)y };
        POINT br = { (LONG)(x + w), (LONG)(y + h) };

        MapWindowPoints(system.mWindow, nullptr, &tl, 1);
        MapWindowPoints(system.mWindow, nullptr, &br, 1);

        RECT rect;
        rect.left = tl.x;
        rect.top = tl.y;
        rect.right = br.x;
        rect.bottom = br.y;

        ClipCursor(&rect);
    }

    input.mCursorTrapped = true;
}

const char* INP_ShowSoftKeyboard(bool show)
{
    return nullptr;
}

bool INP_IsSoftKeyboardShown()
{
    return false;
}


#endif