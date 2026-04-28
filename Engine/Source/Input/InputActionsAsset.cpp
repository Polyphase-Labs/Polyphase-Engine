#include "Input/InputActionsAsset.h"
#include "Stream.h"
#include "Log.h"

FORCE_LINK_DEF(InputActionsAsset);
DEFINE_ASSET(InputActionsAsset);

#define INPUT_ACTIONS_FORMAT_VERSION 1

InputActionsAsset::InputActionsAsset()
{
    mType = InputActionsAsset::GetStaticType();
}

void InputActionsAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    uint32_t formatVersion = stream.ReadUint32();
    (void)formatVersion;

    uint32_t actionCount = stream.ReadUint32();
    mActions.resize(actionCount);

    for (uint32_t i = 0; i < actionCount; ++i)
    {
        InputAction& action = mActions[i];

        stream.ReadString(action.category);
        stream.ReadString(action.name);

        // Trigger
        action.trigger.mode = (TriggerMode)stream.ReadUint8();
        action.trigger.holdDuration = stream.ReadFloat();
        action.trigger.multiPressCount = stream.ReadInt32();
        action.trigger.multiPressWindow = stream.ReadFloat();

        // Bindings
        uint32_t bindingCount = stream.ReadUint32();
        action.bindings.resize(bindingCount);

        for (uint32_t b = 0; b < bindingCount; ++b)
        {
            InputActionBinding& binding = action.bindings[b];
            binding.sourceType = (InputSourceType)stream.ReadUint8();
            binding.code = stream.ReadInt32();
            binding.axisDirection = (AxisDirection)stream.ReadUint8();
            binding.axisThreshold = stream.ReadFloat();
            binding.gamepadIndex = stream.ReadInt32();
            binding.requireCtrl = stream.ReadBool();
            binding.requireShift = stream.ReadBool();
            binding.requireAlt = stream.ReadBool();
        }
    }

    LogDebug("InputActionsAsset: Loaded %d actions", (int)mActions.size());
}

void InputActionsAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteUint32(INPUT_ACTIONS_FORMAT_VERSION);
    stream.WriteUint32((uint32_t)mActions.size());

    for (const InputAction& action : mActions)
    {
        stream.WriteString(action.category);
        stream.WriteString(action.name);

        // Trigger
        stream.WriteUint8((uint8_t)action.trigger.mode);
        stream.WriteFloat(action.trigger.holdDuration);
        stream.WriteInt32(action.trigger.multiPressCount);
        stream.WriteFloat(action.trigger.multiPressWindow);

        // Bindings
        stream.WriteUint32((uint32_t)action.bindings.size());

        for (const InputActionBinding& binding : action.bindings)
        {
            stream.WriteUint8((uint8_t)binding.sourceType);
            stream.WriteInt32(binding.code);
            stream.WriteUint8((uint8_t)binding.axisDirection);
            stream.WriteFloat(binding.axisThreshold);
            stream.WriteInt32(binding.gamepadIndex);
            stream.WriteBool(binding.requireCtrl);
            stream.WriteBool(binding.requireShift);
            stream.WriteBool(binding.requireAlt);
        }
    }
}

glm::vec4 InputActionsAsset::GetTypeColor()
{
    return glm::vec4(0.4f, 0.7f, 0.9f, 1.0f);
}

const char* InputActionsAsset::GetTypeName()
{
    return "InputActions";
}
