#include "Assets/TransformAnimationAsset.h"
#include "Log.h"
#include "Utilities.h"

FORCE_LINK_DEF(TransformAnimationAsset);
DEFINE_ASSET(TransformAnimationAsset);

TransformAnimationAsset::TransformAnimationAsset()
{
    mType = TransformAnimationAsset::GetStaticType();
}

TransformAnimationAsset::~TransformAnimationAsset()
{
}

void TransformAnimationAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    mDuration = stream.ReadFloat();
    mLoop = stream.ReadBool();
    mPlayRate = stream.ReadFloat();

    uint32_t count = stream.ReadUint32();
    mKeyframes.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        TransformKeyframe& kf = mKeyframes[i];
        kf.mTime = stream.ReadFloat();
        kf.mPosition = stream.ReadVec3();
        kf.mRotation = stream.ReadQuat();
        kf.mScale = stream.ReadVec3();
        kf.mInterpMode = (InterpMode)stream.ReadUint8();
        stream.ReadString(kf.mSignal);
    }
}

void TransformAnimationAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteFloat(mDuration);
    stream.WriteBool(mLoop);
    stream.WriteFloat(mPlayRate);

    stream.WriteUint32((uint32_t)mKeyframes.size());
    for (uint32_t i = 0; i < mKeyframes.size(); ++i)
    {
        const TransformKeyframe& kf = mKeyframes[i];
        stream.WriteFloat(kf.mTime);
        stream.WriteVec3(kf.mPosition);
        stream.WriteQuat(kf.mRotation);
        stream.WriteVec3(kf.mScale);
        stream.WriteUint8((uint8_t)kf.mInterpMode);
        stream.WriteString(kf.mSignal);
    }
}

void TransformAnimationAsset::Create()
{
    Asset::Create();
}

void TransformAnimationAsset::Destroy()
{
    mKeyframes.clear();
    Asset::Destroy();
}

void TransformAnimationAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Float, "Duration", this, &mDuration));
    outProps.push_back(Property(DatumType::Bool, "Loop", this, &mLoop));
    outProps.push_back(Property(DatumType::Float, "Play Rate", this, &mPlayRate));
    outProps.push_back(Property(DatumType::TransformKeyframe, "Keyframes", this, &mKeyframes).MakeVector());
}

glm::vec4 TransformAnimationAsset::GetTypeColor()
{
    return glm::vec4(0.95f, 0.65f, 0.20f, 1.0f);
}

const char* TransformAnimationAsset::GetTypeName()
{
    return "TransformAnimationAsset";
}

TransformKeyframe TransformAnimationAsset::Sample(float time) const
{
    if (mKeyframes.empty())
    {
        return TransformKeyframe();
    }

    if (mKeyframes.size() == 1 || time <= mKeyframes[0].mTime)
    {
        return mKeyframes[0];
    }

    if (time >= mKeyframes.back().mTime)
    {
        return mKeyframes.back();
    }

    // Linear scan — keyframe counts are small.
    size_t low = 0;
    size_t high = mKeyframes.size() - 1;
    for (size_t i = 0; i + 1 < mKeyframes.size(); ++i)
    {
        if (mKeyframes[i].mTime <= time && time <= mKeyframes[i + 1].mTime)
        {
            low = i;
            high = i + 1;
            break;
        }
    }

    const TransformKeyframe& a = mKeyframes[low];
    const TransformKeyframe& b = mKeyframes[high];

    float span = b.mTime - a.mTime;
    float t = (span > 0.0f) ? glm::clamp((time - a.mTime) / span, 0.0f, 1.0f) : 0.0f;

    return TransformKeyframe::Lerp(a, b, t);
}

void TransformAnimationAsset::SetKeyframes(const std::vector<TransformKeyframe>& keyframes)
{
    mKeyframes = keyframes;
}

void TransformAnimationAsset::AddKeyframe(const TransformKeyframe& kf)
{
    // Insert sorted by time.
    for (size_t i = 0; i < mKeyframes.size(); ++i)
    {
        if (kf.mTime < mKeyframes[i].mTime)
        {
            mKeyframes.insert(mKeyframes.begin() + i, kf);
            return;
        }
    }
    mKeyframes.push_back(kf);
}

void TransformAnimationAsset::RemoveKeyframe(size_t index)
{
    if (index < mKeyframes.size())
    {
        mKeyframes.erase(mKeyframes.begin() + index);
    }
}

void TransformAnimationAsset::ClearKeyframes()
{
    mKeyframes.clear();
}
