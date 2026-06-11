#pragma once

#include "Asset.h"
#include "Factory.h"
#include "Timeline/TimelineTypes.h"

#include <vector>

class TransformAnimationAsset : public Asset
{
public:

    DECLARE_ASSET(TransformAnimationAsset, Asset);

    TransformAnimationAsset();
    ~TransformAnimationAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    TransformKeyframe Sample(float time) const;

    size_t GetKeyframeCount() const { return mKeyframes.size(); }
    const TransformKeyframe& GetKeyframe(size_t i) const { return mKeyframes[i]; }
    const std::vector<TransformKeyframe>& GetKeyframes() const { return mKeyframes; }
    std::vector<TransformKeyframe>& GetKeyframesMutable() { return mKeyframes; }

    void SetKeyframes(const std::vector<TransformKeyframe>& keyframes);
    void AddKeyframe(const TransformKeyframe& kf);
    void RemoveKeyframe(size_t index);
    void ClearKeyframes();

    float GetDuration() const { return mDuration; }
    void SetDuration(float duration) { mDuration = duration; }

    bool IsLooping() const { return mLoop; }
    void SetLooping(bool loop) { mLoop = loop; }

    float GetPlayRate() const { return mPlayRate; }
    void SetPlayRate(float rate) { mPlayRate = rate; }

protected:

    std::vector<TransformKeyframe> mKeyframes;
    float mDuration = 1.0f;
    bool  mLoop = false;
    float mPlayRate = 1.0f;
};
