#if EDITOR

#include "InputMapModule.h"
#include "../JsonSettings.h"
#include "Input/InputMap.h"

#include "document.h"
#include "imgui.h"

DEFINE_PREFERENCES_MODULE(InputMapModule, "Input", "")

InputMapModule::InputMapModule()
{
}

InputMapModule::~InputMapModule()
{
}

void InputMapModule::Render()
{
    ImGui::Text("Keyboard-to-Gamepad Input Mappings");
    ImGui::Spacing();
    ImGui::TextWrapped("Configure keyboard keys that simulate gamepad buttons and axes during Play-in-Editor.");
    ImGui::Spacing();

    InputMap* inputMap = InputMap::Get();
    if (inputMap == nullptr)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "InputMap not initialized.");
        return;
    }

    // Show summary of mapped buttons
    int mappedButtons = 0;
    for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
    {
        if (inputMap->GetButtonMapping((GamepadButtonCode)i) >= 0)
            ++mappedButtons;
    }
    ImGui::Text("Mapped buttons: %d / %d", mappedButtons, GAMEPAD_BUTTON_COUNT);

    int mappedAxes = 0;
    for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
    {
        int32_t pos = -1, neg = -1;
        inputMap->GetAxisMapping((GamepadAxisCode)i, pos, neg);
        if (pos >= 0 || neg >= 0)
            ++mappedAxes;
    }
    ImGui::Text("Mapped axes: %d / %d", mappedAxes, GAMEPAD_AXIS_COUNT);

    ImGui::Spacing();
    ImGui::TextWrapped("Open the Input Map Window from Developer > Input Map Window for full editing.");
}

// Button name strings for serialization keys
static const char* sButtonSerializeNames[GAMEPAD_BUTTON_COUNT] = {
    "A", "B", "C", "X", "Y", "Z",
    "L1", "R1", "L2", "R2",
    "ThumbL", "ThumbR",
    "Start", "Select",
    "Left", "Right", "Up", "Down",
    "LsLeft", "LsRight", "LsUp", "LsDown",
    "RsLeft", "RsRight", "RsUp", "RsDown",
    "Home"
};

static const char* sAxisSerializeNames[GAMEPAD_AXIS_COUNT] = {
    "LTrigger", "RTrigger",
    "LThumbX", "LThumbY",
    "RThumbX", "RThumbY"
};

void InputMapModule::LoadSettings(const rapidjson::Document& doc)
{
    InputMap* inputMap = InputMap::Get();
    if (inputMap == nullptr)
        return;

    // Load button mappings
    if (doc.HasMember("buttons") && doc["buttons"].IsObject())
    {
        const rapidjson::Value& buttons = doc["buttons"];
        for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
        {
            const char* name = sButtonSerializeNames[i];
            if (buttons.HasMember(name) && buttons[name].IsInt())
            {
                inputMap->SetButtonMapping((GamepadButtonCode)i, buttons[name].GetInt());
            }
        }
    }

    // Load axis mappings
    if (doc.HasMember("axes") && doc["axes"].IsObject())
    {
        const rapidjson::Value& axes = doc["axes"];
        for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
        {
            const char* name = sAxisSerializeNames[i];
            if (axes.HasMember(name) && axes[name].IsObject())
            {
                const rapidjson::Value& axisObj = axes[name];

                int32_t posKey = -1;
                int32_t negKey = -1;
                if (axisObj.HasMember("positive") && axisObj["positive"].IsInt())
                    posKey = axisObj["positive"].GetInt();
                if (axisObj.HasMember("negative") && axisObj["negative"].IsInt())
                    negKey = axisObj["negative"].GetInt();
                inputMap->SetAxisMapping((GamepadAxisCode)i, posKey, negKey);

                bool mouseX = false;
                bool mouseY = false;
                float sensitivity = 1.0f;
                if (axisObj.HasMember("mouseX") && axisObj["mouseX"].IsBool())
                    mouseX = axisObj["mouseX"].GetBool();
                if (axisObj.HasMember("mouseY") && axisObj["mouseY"].IsBool())
                    mouseY = axisObj["mouseY"].GetBool();
                if (axisObj.HasMember("sensitivity") && axisObj["sensitivity"].IsNumber())
                    sensitivity = axisObj["sensitivity"].GetFloat();
                inputMap->SetAxisMouseMapping((GamepadAxisCode)i, mouseX, mouseY, sensitivity);
            }
        }
    }

    // Load mouse/pointer toggles
    if (doc.HasMember("mouseEnabled") && doc["mouseEnabled"].IsBool())
        inputMap->SetMouseEnabled(doc["mouseEnabled"].GetBool());
    if (doc.HasMember("pointerEnabled") && doc["pointerEnabled"].IsBool())
        inputMap->SetPointerEnabled(doc["pointerEnabled"].GetBool());
}

void InputMapModule::SaveSettings(rapidjson::Document& doc)
{
    InputMap* inputMap = InputMap::Get();
    if (inputMap == nullptr)
        return;

    if (!doc.IsObject())
        doc.SetObject();

    auto& alloc = doc.GetAllocator();

    JsonSettings::SetInt(doc, "version", 1);

    // Save button mappings
    {
        rapidjson::Value buttons(rapidjson::kObjectType);
        for (int32_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
        {
            int32_t keyCode = inputMap->GetButtonMapping((GamepadButtonCode)i);
            rapidjson::Value key(sButtonSerializeNames[i], alloc);
            buttons.AddMember(key, keyCode, alloc);
        }

        if (doc.HasMember("buttons"))
            doc["buttons"] = buttons;
        else
            doc.AddMember("buttons", buttons, alloc);
    }

    // Save axis mappings
    {
        rapidjson::Value axes(rapidjson::kObjectType);
        for (int32_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i)
        {
            GamepadAxisCode axis = (GamepadAxisCode)i;
            int32_t posKey = -1, negKey = -1;
            inputMap->GetAxisMapping(axis, posKey, negKey);

            rapidjson::Value axisObj(rapidjson::kObjectType);
            axisObj.AddMember("positive", posKey, alloc);
            axisObj.AddMember("negative", negKey, alloc);
            axisObj.AddMember("mouseX", inputMap->GetAxisUsesMouseX(axis), alloc);
            axisObj.AddMember("mouseY", inputMap->GetAxisUsesMouseY(axis), alloc);
            axisObj.AddMember("sensitivity", inputMap->GetAxisMouseSensitivity(axis), alloc);

            rapidjson::Value key(sAxisSerializeNames[i], alloc);
            axes.AddMember(key, axisObj, alloc);
        }

        if (doc.HasMember("axes"))
            doc["axes"] = axes;
        else
            doc.AddMember("axes", axes, alloc);
    }

    // Save mouse/pointer toggles
    JsonSettings::SetBool(doc, "mouseEnabled", inputMap->IsMouseEnabled());
    JsonSettings::SetBool(doc, "pointerEnabled", inputMap->IsPointerEnabled());
}

#endif
