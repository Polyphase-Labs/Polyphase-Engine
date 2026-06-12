#include "InputPromptStyle.h"
#include "Stream.h"
#include "Property.h"

FORCE_LINK_DEF(InputPromptStyle);
DEFINE_ASSET(InputPromptStyle);

InputPromptStyle::InputPromptStyle()
{
    // See InputPromptMap.cpp for why this assignment is required —
    // without it SaveStream writes mType = 0 and the loader fails on reload.
    mType = InputPromptStyle::GetStaticType();
    mName = "InputPromptStyle";
}

InputPromptStyle::~InputPromptStyle()
{
}

void InputPromptStyle::Create()
{
    Asset::Create();
}

void InputPromptStyle::Destroy()
{
    Asset::Destroy();
}

void InputPromptStyle::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    if (mVersion < ASSET_VERSION_INPUT_PROMPT_STYLE)
        return;

    mIconSize = stream.ReadFloat();
    mSpacing  = stream.ReadFloat();
    mTint     = stream.ReadVec4();

    for (int32_t i = 0; i < kPriorityCount; ++i)
        mPriority[i] = (InputPromptKind)stream.ReadUint8();

    uint32_t numPrewarm = stream.ReadUint32();
    mPrewarmActions.resize(numPrewarm);
    for (uint32_t i = 0; i < numPrewarm; ++i)
        stream.ReadString(mPrewarmActions[i]);
}

void InputPromptStyle::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteFloat(mIconSize);
    stream.WriteFloat(mSpacing);
    stream.WriteVec4(mTint);

    for (int32_t i = 0; i < kPriorityCount; ++i)
        stream.WriteUint8((uint8_t)mPriority[i]);

    stream.WriteUint32((uint32_t)mPrewarmActions.size());
    for (const std::string& a : mPrewarmActions)
        stream.WriteString(a);
}

void InputPromptStyle::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    SCOPED_CATEGORY("Style");
    outProps.push_back(Property(DatumType::Float, "Icon Size", this, &mIconSize, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Float, "Spacing",   this, &mSpacing,  1, HandlePropChange));
    outProps.push_back(Property(DatumType::Color, "Tint",      this, &mTint,     1, HandlePropChange));
    // Priority chips + prewarm list have a custom UI in the editor inspector.
}

glm::vec4 InputPromptStyle::GetTypeColor()
{
    return glm::vec4(0.45f, 0.7f, 0.95f, 1.0f);
}

const char* InputPromptStyle::GetTypeName()
{
    return "InputPromptStyle";
}

InputPromptKind InputPromptStyle::GetPriorityAt(int32_t i) const
{
    if (i >= 0 && i < kPriorityCount)
        return mPriority[i];
    return InputPromptKind::Text;
}

void InputPromptStyle::SetPriorityAt(int32_t i, InputPromptKind kind)
{
    if (i >= 0 && i < kPriorityCount)
        mPriority[i] = kind;
}

bool InputPromptStyle::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    return false;
}
