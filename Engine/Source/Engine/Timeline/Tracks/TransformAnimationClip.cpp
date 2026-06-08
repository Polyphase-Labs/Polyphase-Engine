#include "Timeline/Tracks/TransformAnimationClip.h"
#include "Assets/TransformAnimationAsset.h"
#include "Utilities.h"

FORCE_LINK_DEF(TransformAnimationClip);
DEFINE_CLIP(TransformAnimationClip);

TransformAnimationClip::TransformAnimationClip()
{
}

TransformAnimationClip::~TransformAnimationClip()
{
}

void TransformAnimationClip::SaveStream(Stream& stream)
{
    TimelineClip::SaveStream(stream);

    stream.WriteAsset(mAnimation);
    stream.WriteFloat(mTimeOffset);
    stream.WriteBool(mLoopWithinClip);
}

void TransformAnimationClip::LoadStream(Stream& stream, uint32_t version)
{
    TimelineClip::LoadStream(stream, version);

    stream.ReadAsset(mAnimation);
    mTimeOffset = stream.ReadFloat();
    mLoopWithinClip = stream.ReadBool();

    mLastFiredKeyframe = -1;
}

void TransformAnimationClip::GatherProperties(std::vector<Property>& outProps)
{
    TimelineClip::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "Animation", this, &mAnimation, 1, nullptr, int32_t(TransformAnimationAsset::GetStaticType())));
    outProps.push_back(Property(DatumType::Float, "Time Offset", this, &mTimeOffset));
    outProps.push_back(Property(DatumType::Bool, "Loop Within Clip", this, &mLoopWithinClip));
}

TransformAnimationAsset* TransformAnimationClip::GetAnimation() const
{
    return mAnimation.Get<TransformAnimationAsset>();
}

void TransformAnimationClip::SetAnimation(TransformAnimationAsset* asset)
{
    mAnimation = asset;
}
