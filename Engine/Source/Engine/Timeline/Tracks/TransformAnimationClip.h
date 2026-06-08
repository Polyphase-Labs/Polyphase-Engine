#pragma once

#include "Timeline/TimelineClip.h"
#include "AssetRef.h"

class TransformAnimationAsset;

class TransformAnimationClip : public TimelineClip
{
public:

    DECLARE_CLIP(TransformAnimationClip, TimelineClip);

    TransformAnimationClip();
    virtual ~TransformAnimationClip();

    virtual void SaveStream(Stream& stream) override;
    virtual void LoadStream(Stream& stream, uint32_t version) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    TransformAnimationAsset* GetAnimation() const;
    const TransformAnimationRef& GetAnimationRef() const { return mAnimation; }
    void SetAnimation(TransformAnimationAsset* asset);

    float GetTimeOffset() const { return mTimeOffset; }
    void  SetTimeOffset(float t) { mTimeOffset = t; }

    bool  GetLoopWithinClip() const { return mLoopWithinClip; }
    void  SetLoopWithinClip(bool l) { mLoopWithinClip = l; }

    int32_t GetLastFiredKeyframe() const { return mLastFiredKeyframe; }
    void    SetLastFiredKeyframe(int32_t i) { mLastFiredKeyframe = i; }

protected:

    TransformAnimationRef mAnimation;
    float   mTimeOffset = 0.0f;
    bool    mLoopWithinClip = false;
    int32_t mLastFiredKeyframe = -1;
};
