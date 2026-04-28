#include "InputMap.h"
#include "Input.h"
#include "Stream.h"
#include "Utilities.h"
#include "System/System.h"
#include "Log.h"

#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"

#include <cstdlib>


InputMap* InputMap::sInstance = nullptr;

static const char* sButtonKeys[GAMEPAD_BUTTON_COUNT] = {
    "A", "B", "C", "X", "Y", "Z",
    "L1", "R1", "L2", "R2",
    "ThumbL", "ThumbR",
    "Start", "Select",
    "Left", "Right", "Up", "Down",
    "LsLeft", "LsRight", "LsUp", "LsDown",
    "RsLeft", "RsRight", "RsUp", "RsDown",
    "Home"
};

static const char* sAxisKeys[GAMEPAD_AXIS_COUNT] = {
    "LTrigger", "RTrigger",
    "LThumbX", "LThumbY",
    "RThumbX", "RThumbY"
};

void InputMap::Create()
{
    Destroy();
    sInstance = new InputMap();
}

void InputMap::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

InputMap* InputMap::Get()
{
    return sInstance;
}

InputMap::InputMap()
{
    ResetToDefaults();
}

void InputMap::ResetToDefaults()
{
    // Clear all mappings
    for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
    {
        mButtonMappings[i] = -1;
    }
    for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
    {
        mAxisPositiveKeys[i] = -1;
        mAxisNegativeKeys[i] = -1;
        mAxisUseMouseX[i] = false;
        mAxisUseMouseY[i] = false;
        mAxisMouseSensitivity[i] = 1.0f;
    }

    mMouseEnabled = true;
    mPointerEnabled = true;

    // Default button mappings
    mButtonMappings[GAMEPAD_A] = POLYPHASE_KEY_SPACE;
    mButtonMappings[GAMEPAD_B] = POLYPHASE_KEY_SHIFT_L;
    // GAMEPAD_C: unmapped (Wii-specific)
    mButtonMappings[GAMEPAD_X] = POLYPHASE_KEY_E;
    mButtonMappings[GAMEPAD_Y] = POLYPHASE_KEY_Q;
    // GAMEPAD_Z: unmapped (Wii-specific)
    mButtonMappings[GAMEPAD_L1] = POLYPHASE_KEY_1;
    mButtonMappings[GAMEPAD_R1] = POLYPHASE_KEY_2;
    mButtonMappings[GAMEPAD_L2] = POLYPHASE_KEY_3;
    mButtonMappings[GAMEPAD_R2] = POLYPHASE_KEY_4;
    mButtonMappings[GAMEPAD_THUMBL] = POLYPHASE_KEY_F;
    mButtonMappings[GAMEPAD_THUMBR] = POLYPHASE_KEY_R;
    mButtonMappings[GAMEPAD_START] = POLYPHASE_KEY_ENTER;
    mButtonMappings[GAMEPAD_SELECT] = POLYPHASE_KEY_BACKSPACE;
    mButtonMappings[GAMEPAD_LEFT] = POLYPHASE_KEY_LEFT;
    mButtonMappings[GAMEPAD_RIGHT] = POLYPHASE_KEY_RIGHT;
    mButtonMappings[GAMEPAD_UP] = POLYPHASE_KEY_UP;
    mButtonMappings[GAMEPAD_DOWN] = POLYPHASE_KEY_DOWN;
    mButtonMappings[GAMEPAD_HOME] = POLYPHASE_KEY_HOME;
    // Stick-as-button entries: unmapped by default (auto-derived from axes)

    // Default axis mappings
    mAxisNegativeKeys[GAMEPAD_AXIS_LTHUMB_X] = POLYPHASE_KEY_A;
    mAxisPositiveKeys[GAMEPAD_AXIS_LTHUMB_X] = POLYPHASE_KEY_D;
    mAxisNegativeKeys[GAMEPAD_AXIS_LTHUMB_Y] = POLYPHASE_KEY_W;
    mAxisPositiveKeys[GAMEPAD_AXIS_LTHUMB_Y] = POLYPHASE_KEY_S;
    mAxisNegativeKeys[GAMEPAD_AXIS_RTHUMB_X] = POLYPHASE_KEY_J;
    mAxisPositiveKeys[GAMEPAD_AXIS_RTHUMB_X] = POLYPHASE_KEY_L;
    mAxisNegativeKeys[GAMEPAD_AXIS_RTHUMB_Y] = POLYPHASE_KEY_I;
    mAxisPositiveKeys[GAMEPAD_AXIS_RTHUMB_Y] = POLYPHASE_KEY_K;
    mAxisPositiveKeys[GAMEPAD_AXIS_LTRIGGER] = POLYPHASE_KEY_Z;
    mAxisPositiveKeys[GAMEPAD_AXIS_RTRIGGER] = POLYPHASE_KEY_X;

    // Default mouse-to-axis: right stick driven by mouse
    mAxisUseMouseX[GAMEPAD_AXIS_RTHUMB_X] = true;
    mAxisUseMouseY[GAMEPAD_AXIS_RTHUMB_Y] = true;
}

// --- Button mappings ---

void InputMap::SetButtonMapping(GamepadButtonCode button, int32_t keyCode)
{
    if (button >= 0 && button < GAMEPAD_BUTTON_COUNT)
    {
        mButtonMappings[button] = keyCode;
    }
}

int32_t InputMap::GetButtonMapping(GamepadButtonCode button) const
{
    if (button >= 0 && button < GAMEPAD_BUTTON_COUNT)
    {
        return mButtonMappings[button];
    }
    return -1;
}

// --- Axis mappings ---

void InputMap::SetAxisMapping(GamepadAxisCode axis, int32_t positiveKey, int32_t negativeKey)
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
    {
        mAxisPositiveKeys[axis] = positiveKey;
        mAxisNegativeKeys[axis] = negativeKey;
    }
}

void InputMap::GetAxisMapping(GamepadAxisCode axis, int32_t& positiveKey, int32_t& negativeKey) const
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
    {
        positiveKey = mAxisPositiveKeys[axis];
        negativeKey = mAxisNegativeKeys[axis];
    }
    else
    {
        positiveKey = -1;
        negativeKey = -1;
    }
}

// --- Mouse-to-axis ---

void InputMap::SetAxisMouseMapping(GamepadAxisCode axis, bool useMouseX, bool useMouseY, float sensitivity)
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
    {
        mAxisUseMouseX[axis] = useMouseX;
        mAxisUseMouseY[axis] = useMouseY;
        mAxisMouseSensitivity[axis] = sensitivity;
    }
}

bool InputMap::GetAxisUsesMouseX(GamepadAxisCode axis) const
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
        return mAxisUseMouseX[axis];
    return false;
}

bool InputMap::GetAxisUsesMouseY(GamepadAxisCode axis) const
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
        return mAxisUseMouseY[axis];
    return false;
}

float InputMap::GetAxisMouseSensitivity(GamepadAxisCode axis) const
{
    if (axis >= 0 && axis < GAMEPAD_AXIS_COUNT)
        return mAxisMouseSensitivity[axis];
    return 1.0f;
}

// --- Mouse/Pointer toggles ---

void InputMap::SetMouseEnabled(bool enabled) { mMouseEnabled = enabled; }
bool InputMap::IsMouseEnabled() const { return mMouseEnabled; }
void InputMap::SetPointerEnabled(bool enabled) { mPointerEnabled = enabled; }
bool InputMap::IsPointerEnabled() const { return mPointerEnabled; }

// --- Keyboard query helpers ---

bool InputMap::IsButtonDownViaKeyboard(GamepadButtonCode button) const
{
    if (button >= 0 && button < GAMEPAD_BUTTON_COUNT)
    {
        int32_t key = mButtonMappings[button];
        if (key >= 0)
        {
            return INP_IsKeyDown(key);
        }
    }
    return false;
}

bool InputMap::IsButtonJustDownViaKeyboard(GamepadButtonCode button) const
{
    if (button >= 0 && button < GAMEPAD_BUTTON_COUNT)
    {
        int32_t key = mButtonMappings[button];
        if (key >= 0)
        {
            return INP_IsKeyJustDown(key);
        }
    }
    return false;
}

bool InputMap::IsButtonJustUpViaKeyboard(GamepadButtonCode button) const
{
    if (button >= 0 && button < GAMEPAD_BUTTON_COUNT)
    {
        int32_t key = mButtonMappings[button];
        if (key >= 0)
        {
            return INP_IsKeyJustUp(key);
        }
    }
    return false;
}

float InputMap::GetAxisValueFromMapping(GamepadAxisCode axis) const
{
    if (axis < 0 || axis >= GAMEPAD_AXIS_COUNT)
        return 0.0f;

    float value = 0.0f;

    // Keyboard keys
    int32_t posKey = mAxisPositiveKeys[axis];
    int32_t negKey = mAxisNegativeKeys[axis];

    if (posKey >= 0 && INP_IsKeyDown(posKey))
        value += 1.0f;
    if (negKey >= 0 && INP_IsKeyDown(negKey))
        value -= 1.0f;

    // Mouse delta -> axis (reads raw delta directly from InputState)
    if (mAxisUseMouseX[axis])
    {
        int32_t dx = 0;
        int32_t dy = 0;
        INP_GetMouseDelta(dx, dy);
        float sensitivity = mAxisMouseSensitivity[axis];
        value += dx * sensitivity * 0.01f;
    }
    if (mAxisUseMouseY[axis])
    {
        int32_t dx = 0;
        int32_t dy = 0;
        INP_GetMouseDelta(dx, dy);
        float sensitivity = mAxisMouseSensitivity[axis];
        value += dy * sensitivity * 0.01f;
    }

    // Clamp to [-1, 1]
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;

    return value;
}

// --- Display string helpers ---

const char* InputMap::GetKeyCodeName(int32_t keyCode)
{
    if (keyCode == POLYPHASE_KEY_A) return "A";
    if (keyCode == POLYPHASE_KEY_B) return "B";
    if (keyCode == POLYPHASE_KEY_C) return "C";
    if (keyCode == POLYPHASE_KEY_D) return "D";
    if (keyCode == POLYPHASE_KEY_E) return "E";
    if (keyCode == POLYPHASE_KEY_F) return "F";
    if (keyCode == POLYPHASE_KEY_G) return "G";
    if (keyCode == POLYPHASE_KEY_H) return "H";
    if (keyCode == POLYPHASE_KEY_I) return "I";
    if (keyCode == POLYPHASE_KEY_J) return "J";
    if (keyCode == POLYPHASE_KEY_K) return "K";
    if (keyCode == POLYPHASE_KEY_L) return "L";
    if (keyCode == POLYPHASE_KEY_M) return "M";
    if (keyCode == POLYPHASE_KEY_N) return "N";
    if (keyCode == POLYPHASE_KEY_O) return "O";
    if (keyCode == POLYPHASE_KEY_P) return "P";
    if (keyCode == POLYPHASE_KEY_Q) return "Q";
    if (keyCode == POLYPHASE_KEY_R) return "R";
    if (keyCode == POLYPHASE_KEY_S) return "S";
    if (keyCode == POLYPHASE_KEY_T) return "T";
    if (keyCode == POLYPHASE_KEY_U) return "U";
    if (keyCode == POLYPHASE_KEY_V) return "V";
    if (keyCode == POLYPHASE_KEY_W) return "W";
    if (keyCode == POLYPHASE_KEY_X) return "X";
    if (keyCode == POLYPHASE_KEY_Y) return "Y";
    if (keyCode == POLYPHASE_KEY_Z) return "Z";
    if (keyCode == POLYPHASE_KEY_0) return "0";
    if (keyCode == POLYPHASE_KEY_1) return "1";
    if (keyCode == POLYPHASE_KEY_2) return "2";
    if (keyCode == POLYPHASE_KEY_3) return "3";
    if (keyCode == POLYPHASE_KEY_4) return "4";
    if (keyCode == POLYPHASE_KEY_5) return "5";
    if (keyCode == POLYPHASE_KEY_6) return "6";
    if (keyCode == POLYPHASE_KEY_7) return "7";
    if (keyCode == POLYPHASE_KEY_8) return "8";
    if (keyCode == POLYPHASE_KEY_9) return "9";
    if (keyCode == POLYPHASE_KEY_SPACE) return "Space";
    if (keyCode == POLYPHASE_KEY_ENTER) return "Enter";
    if (keyCode == POLYPHASE_KEY_BACKSPACE) return "Backspace";
    if (keyCode == POLYPHASE_KEY_TAB) return "Tab";
    if (keyCode == POLYPHASE_KEY_ESCAPE) return "Escape";
    if (keyCode == POLYPHASE_KEY_SHIFT_L) return "Left Shift";
    if (keyCode == POLYPHASE_KEY_CONTROL_L) return "Left Ctrl";
    if (keyCode == POLYPHASE_KEY_ALT_L) return "Left Alt";
    if (keyCode == POLYPHASE_KEY_UP) return "Up";
    if (keyCode == POLYPHASE_KEY_DOWN) return "Down";
    if (keyCode == POLYPHASE_KEY_LEFT) return "Left";
    if (keyCode == POLYPHASE_KEY_RIGHT) return "Right";
    if (keyCode == POLYPHASE_KEY_INSERT) return "Insert";
    if (keyCode == POLYPHASE_KEY_DELETE) return "Delete";
    if (keyCode == POLYPHASE_KEY_HOME) return "Home";
    if (keyCode == POLYPHASE_KEY_END) return "End";
    if (keyCode == POLYPHASE_KEY_PAGE_UP) return "Page Up";
    if (keyCode == POLYPHASE_KEY_PAGE_DOWN) return "Page Down";
    if (keyCode == POLYPHASE_KEY_F1) return "F1";
    if (keyCode == POLYPHASE_KEY_F2) return "F2";
    if (keyCode == POLYPHASE_KEY_F3) return "F3";
    if (keyCode == POLYPHASE_KEY_F4) return "F4";
    if (keyCode == POLYPHASE_KEY_F5) return "F5";
    if (keyCode == POLYPHASE_KEY_F6) return "F6";
    if (keyCode == POLYPHASE_KEY_F7) return "F7";
    if (keyCode == POLYPHASE_KEY_F8) return "F8";
    if (keyCode == POLYPHASE_KEY_F9) return "F9";
    if (keyCode == POLYPHASE_KEY_F10) return "F10";
    if (keyCode == POLYPHASE_KEY_F11) return "F11";
    if (keyCode == POLYPHASE_KEY_F12) return "F12";
    if (keyCode == POLYPHASE_KEY_NUMPAD0) return "Numpad 0";
    if (keyCode == POLYPHASE_KEY_NUMPAD1) return "Numpad 1";
    if (keyCode == POLYPHASE_KEY_NUMPAD2) return "Numpad 2";
    if (keyCode == POLYPHASE_KEY_NUMPAD3) return "Numpad 3";
    if (keyCode == POLYPHASE_KEY_NUMPAD4) return "Numpad 4";
    if (keyCode == POLYPHASE_KEY_NUMPAD5) return "Numpad 5";
    if (keyCode == POLYPHASE_KEY_NUMPAD6) return "Numpad 6";
    if (keyCode == POLYPHASE_KEY_NUMPAD7) return "Numpad 7";
    if (keyCode == POLYPHASE_KEY_NUMPAD8) return "Numpad 8";
    if (keyCode == POLYPHASE_KEY_NUMPAD9) return "Numpad 9";
    if (keyCode == POLYPHASE_KEY_PERIOD) return "Period";
    if (keyCode == POLYPHASE_KEY_COMMA) return "Comma";
    if (keyCode == POLYPHASE_KEY_PLUS) return "Plus";
    if (keyCode == POLYPHASE_KEY_MINUS) return "Minus";
    if (keyCode == POLYPHASE_KEY_COLON) return "Semicolon";
    if (keyCode == POLYPHASE_KEY_QUESTION) return "Slash";
    if (keyCode == POLYPHASE_KEY_SQUIGGLE) return "Tilde";
    if (keyCode == POLYPHASE_KEY_LEFT_BRACKET) return "Left Bracket";
    if (keyCode == POLYPHASE_KEY_BACK_SLASH) return "Backslash";
    if (keyCode == POLYPHASE_KEY_RIGHT_BRACKET) return "Right Bracket";
    if (keyCode == POLYPHASE_KEY_QUOTE) return "Quote";
    if (keyCode == POLYPHASE_KEY_DECIMAL) return "Decimal";
    return "\xe2\x80\x94";
}

const char* InputMap::GetGamepadButtonName(GamepadButtonCode button)
{
    switch (button)
    {
    case GAMEPAD_A: return "Gamepad.A";
    case GAMEPAD_B: return "Gamepad.B";
    case GAMEPAD_C: return "Gamepad.C";
    case GAMEPAD_X: return "Gamepad.X";
    case GAMEPAD_Y: return "Gamepad.Y";
    case GAMEPAD_Z: return "Gamepad.Z";
    case GAMEPAD_L1: return "Gamepad.L1";
    case GAMEPAD_R1: return "Gamepad.R1";
    case GAMEPAD_L2: return "Gamepad.L2";
    case GAMEPAD_R2: return "Gamepad.R2";
    case GAMEPAD_THUMBL: return "Gamepad.L3";
    case GAMEPAD_THUMBR: return "Gamepad.R3";
    case GAMEPAD_START: return "Gamepad.Start";
    case GAMEPAD_SELECT: return "Gamepad.Select";
    case GAMEPAD_LEFT: return "Gamepad.Left";
    case GAMEPAD_RIGHT: return "Gamepad.Right";
    case GAMEPAD_UP: return "Gamepad.Up";
    case GAMEPAD_DOWN: return "Gamepad.Down";
    case GAMEPAD_L_LEFT: return "Gamepad.LsLeft";
    case GAMEPAD_L_RIGHT: return "Gamepad.LsRight";
    case GAMEPAD_L_UP: return "Gamepad.LsUp";
    case GAMEPAD_L_DOWN: return "Gamepad.LsDown";
    case GAMEPAD_R_LEFT: return "Gamepad.RsLeft";
    case GAMEPAD_R_RIGHT: return "Gamepad.RsRight";
    case GAMEPAD_R_UP: return "Gamepad.RsUp";
    case GAMEPAD_R_DOWN: return "Gamepad.RsDown";
    case GAMEPAD_HOME: return "Gamepad.Home";
    default: return "Unknown";
    }
}

const char* InputMap::GetGamepadAxisName(GamepadAxisCode axis)
{
    switch (axis)
    {
    case GAMEPAD_AXIS_LTRIGGER: return "Left Trigger";
    case GAMEPAD_AXIS_RTRIGGER: return "Right Trigger";
    case GAMEPAD_AXIS_LTHUMB_X: return "Left Stick X";
    case GAMEPAD_AXIS_LTHUMB_Y: return "Left Stick Y";
    case GAMEPAD_AXIS_RTHUMB_X: return "Right Stick X";
    case GAMEPAD_AXIS_RTHUMB_Y: return "Right Stick Y";
    default: return "Unknown";
    }
}

// --- Presets ---

std::string InputMap::GetPresetsDirectory()
{
    std::string dir;

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        dir = std::string(appData) + "/PolyphaseEditor/InputPresets";
    }
#elif PLATFORM_LINUX
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        dir = std::string(home) + "/.config/PolyphaseEditor/InputPresets";
    }
#endif

    if (dir.empty())
    {
        dir = "Engine/Saves/InputPresets";
    }

    return dir;
}

static void EnsurePresetsDirectory()
{
    std::string dir = InputMap::GetPresetsDirectory();

#if PLATFORM_WINDOWS
    const char* appData = getenv("APPDATA");
    if (appData != nullptr)
    {
        std::string parentDir = std::string(appData) + "/PolyphaseEditor";
        if (!DoesDirExist(parentDir.c_str()))
        {
            SYS_CreateDirectory(parentDir.c_str());
        }
    }
#elif PLATFORM_LINUX
    const char* home = getenv("HOME");
    if (home != nullptr)
    {
        std::string configDir = std::string(home) + "/.config";
        if (!DoesDirExist(configDir.c_str()))
            SYS_CreateDirectory(configDir.c_str());
        std::string parentDir = configDir + "/PolyphaseEditor";
        if (!DoesDirExist(parentDir.c_str()))
            SYS_CreateDirectory(parentDir.c_str());
    }
#endif

    if (!DoesDirExist(dir.c_str()))
    {
        SYS_CreateDirectory(dir.c_str());
    }
}

bool InputMap::SavePreset(const std::string& name) const
{
    EnsurePresetsDirectory();

    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("version", 1, alloc);

    // Buttons
    rapidjson::Value buttons(rapidjson::kObjectType);
    for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
    {
        rapidjson::Value key(sButtonKeys[i], alloc);
        buttons.AddMember(key, (int)mButtonMappings[i], alloc);
    }
    doc.AddMember("buttons", buttons, alloc);

    // Axes
    rapidjson::Value axes(rapidjson::kObjectType);
    for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
    {
        rapidjson::Value axisObj(rapidjson::kObjectType);
        axisObj.AddMember("positive", (int)mAxisPositiveKeys[i], alloc);
        axisObj.AddMember("negative", (int)mAxisNegativeKeys[i], alloc);
        axisObj.AddMember("mouseX", mAxisUseMouseX[i], alloc);
        axisObj.AddMember("mouseY", mAxisUseMouseY[i], alloc);
        axisObj.AddMember("sensitivity", mAxisMouseSensitivity[i], alloc);

        rapidjson::Value key(sAxisKeys[i], alloc);
        axes.AddMember(key, axisObj, alloc);
    }
    doc.AddMember("axes", axes, alloc);

    doc.AddMember("mouseEnabled", mMouseEnabled, alloc);
    doc.AddMember("pointerEnabled", mPointerEnabled, alloc);

    // Write to file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";
    Stream stream(buffer.GetString(), (uint32_t)buffer.GetSize());
    bool success = stream.WriteFile(filePath.c_str());

    if (success)
        LogDebug("Input preset saved: %s", name.c_str());
    else
        LogError("Failed to save input preset: %s", name.c_str());

    return success;
}

bool InputMap::LoadPreset(const std::string& name)
{
    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";

    if (!SYS_DoesFileExist(filePath.c_str(), false)) {
        return false;
    }
    Stream stream;

    if (!stream.ReadFile(filePath.c_str(), false))
    {
        LogWarning("Failed to load input preset: %s", name.c_str());
        return false;
    }

    std::string jsonStr(stream.GetData(), stream.GetSize());
    rapidjson::Document doc;
    doc.Parse(jsonStr.c_str());

    if (doc.HasParseError())
    {
        LogWarning("Failed to parse input preset: %s", name.c_str());
        return false;
    }

    // Buttons
    if (doc.HasMember("buttons") && doc["buttons"].IsObject())
    {
        const rapidjson::Value& buttons = doc["buttons"];
        for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
        {
            if (buttons.HasMember(sButtonKeys[i]) && buttons[sButtonKeys[i]].IsInt())
                mButtonMappings[i] = buttons[sButtonKeys[i]].GetInt();
        }
    }

    // Axes
    if (doc.HasMember("axes") && doc["axes"].IsObject())
    {
        const rapidjson::Value& axes = doc["axes"];
        for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
        {
            if (axes.HasMember(sAxisKeys[i]) && axes[sAxisKeys[i]].IsObject())
            {
                const rapidjson::Value& axisObj = axes[sAxisKeys[i]];
                if (axisObj.HasMember("positive") && axisObj["positive"].IsInt())
                    mAxisPositiveKeys[i] = axisObj["positive"].GetInt();
                if (axisObj.HasMember("negative") && axisObj["negative"].IsInt())
                    mAxisNegativeKeys[i] = axisObj["negative"].GetInt();
                if (axisObj.HasMember("mouseX") && axisObj["mouseX"].IsBool())
                    mAxisUseMouseX[i] = axisObj["mouseX"].GetBool();
                if (axisObj.HasMember("mouseY") && axisObj["mouseY"].IsBool())
                    mAxisUseMouseY[i] = axisObj["mouseY"].GetBool();
                if (axisObj.HasMember("sensitivity") && axisObj["sensitivity"].IsNumber())
                    mAxisMouseSensitivity[i] = axisObj["sensitivity"].GetFloat();
            }
        }
    }

    if (doc.HasMember("mouseEnabled") && doc["mouseEnabled"].IsBool())
        mMouseEnabled = doc["mouseEnabled"].GetBool();
    if (doc.HasMember("pointerEnabled") && doc["pointerEnabled"].IsBool())
        mPointerEnabled = doc["pointerEnabled"].GetBool();

    LogDebug("Input preset loaded: %s", name.c_str());
    return true;
}

bool InputMap::DeletePreset(const std::string& name)
{
    std::string filePath = GetPresetsDirectory() + "/" + name + ".json";
    return (remove(filePath.c_str()) == 0);
}

std::vector<std::string> InputMap::GetPresetNames() const
{
    std::vector<std::string> names;
    std::string dir = GetPresetsDirectory();

    if (!DoesDirExist(dir.c_str()))
        return names;

    DirEntry entry;
    SYS_OpenDirectory(dir + "/", entry);

    while (entry.mValid)
    {
        if (!entry.mDirectory)
        {
            std::string filename = entry.mFilename;
            // Only include .json files
            size_t extPos = filename.rfind(".json");
            if (extPos != std::string::npos && extPos == filename.size() - 5)
            {
                names.push_back(filename.substr(0, extPos));
            }
        }
        SYS_IterateDirectory(entry);
    }

    SYS_CloseDirectory(entry);
    return names;
}
