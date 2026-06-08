#include "Timeline/Tracks/TransformAnimationTrack.h"
#include "Timeline/Tracks/TransformAnimationClip.h"
#include "Timeline/TimelineInstance.h"
#include "Timeline/TimelineTypes.h"
#include "Assets/TransformAnimationAsset.h"
#include "SignalBus.h"
#include "Nodes/Node.h"
#include "Utilities.h"

FORCE_LINK_DEF(TransformAnimationTrack);
DEFINE_TRACK(TransformAnimationTrack);

TransformAnimationTrack::TransformAnimationTrack()
{
}

TransformAnimationTrack::~TransformAnimationTrack()
{
}

void TransformAnimationTrack::Evaluate(float time, Node* target, TimelineInstance* inst)
{
    if (target == nullptr)
        return;

    for (uint32_t i = 0; i < mClips.size(); ++i)
    {
        if (mClips[i]->GetType() != TransformAnimationClip::GetStaticType())
            continue;

        TransformAnimationClip* clip = static_cast<TransformAnimationClip*>(mClips[i]);
        TransformAnimationAsset* asset = clip->GetAnimation();
        if (asset == nullptr)
            continue;

        if (clip->ContainsTime(time))
        {
            float localTime = clip->GetLocalTime(time);
            float assetTime = localTime + clip->GetTimeOffset();

            float assetDuration = asset->GetDuration();
            if (clip->GetLoopWithinClip() && assetDuration > 0.0f)
            {
                assetTime = fmodf(assetTime, assetDuration);
                if (assetTime < 0.0f)
                {
                    assetTime += assetDuration;
                }
            }
            else if (assetDuration > 0.0f)
            {
                assetTime = glm::clamp(assetTime, 0.0f, assetDuration);
            }

            TransformKeyframe kf = asset->Sample(assetTime);
            ApplyTransformKeyframeToNode(target, kf);

            // Signal firing using the same dedupe pattern as standalone players.
            const std::vector<TransformKeyframe>& kfs = asset->GetKeyframes();
            int32_t lastFired = clip->GetLastFiredKeyframe();
            for (int32_t k = lastFired + 1; k < (int32_t)kfs.size(); ++k)
            {
                const TransformKeyframe& candidate = kfs[k];
                if (candidate.mTime <= assetTime)
                {
                    if (!candidate.mSignal.empty())
                    {
                        GetSignalBus()->Emit(candidate.mSignal, {});
                    }
                    clip->SetLastFiredKeyframe(k);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            // Playhead left the clip — reset the fired counter so signals fire again next pass.
            clip->SetLastFiredKeyframe(-1);
        }
    }
}

void TransformAnimationTrack::Reset(Node* target, TimelineInstance* inst)
{
    for (uint32_t i = 0; i < mClips.size(); ++i)
    {
        if (mClips[i]->GetType() == TransformAnimationClip::GetStaticType())
        {
            TransformAnimationClip* clip = static_cast<TransformAnimationClip*>(mClips[i]);
            clip->SetLastFiredKeyframe(-1);
        }
    }
}

glm::vec4 TransformAnimationTrack::GetTrackColor() const
{
    return glm::vec4(0.95f, 0.65f, 0.20f, 1.0f);
}

TypeId TransformAnimationTrack::GetDefaultClipType() const
{
    return TransformAnimationClip::GetStaticType();
}
