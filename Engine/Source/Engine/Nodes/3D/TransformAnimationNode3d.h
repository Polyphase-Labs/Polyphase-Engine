#pragma once

#include "Nodes/Node.h"
#include "AssetRef.h"
#include "Timeline/TimelineTypes.h"

#include <vector>

class TransformAnimationAsset;
class Node3D;

class TransformAnimationNode3D : public Node
{
public:

    DECLARE_NODE(TransformAnimationNode3D, Node);

    TransformAnimationNode3D();
    virtual ~TransformAnimationNode3D();

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;
    virtual void Start() override;
    virtual void Stop() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual const char* GetTypeName() const override;

    // Playback control
    void Play();
    void Play(TransformAnimationAsset* asset);
    void Pause();
    void StopPlayback();

    void SetAnimation(TransformAnimationAsset* asset);
    TransformAnimationAsset* GetAnimation() const;

    void SetKeyframes(const std::vector<TransformKeyframe>& keyframes);
    const std::vector<TransformKeyframe>& GetInlineKeyframes() const { return mInlineKeyframes; }

    void  SetTime(float time);
    float GetTime() const { return mCurrentTime; }
    float GetDuration() const;
    float GetProgress() const;

    bool IsPlaying() const { return mPlaying; }
    bool IsPaused() const { return mPaused; }

    void SetLoop(bool loop) { mLoop = loop; }
    bool IsLooping() const { return mLoop; }

    void SetPlayRate(float rate) { mPlayRate = rate; }
    float GetPlayRate() const { return mPlayRate; }

    void SetPlayOnStart(bool play) { mPlayOnStart = play; }
    bool GetPlayOnStart() const { return mPlayOnStart; }

    void SetTargetNode(Node3D* target);
    Node3D* GetTargetNode() const;

    void ApplyKeyframe(const TransformKeyframe& kf);
    TransformKeyframe SampleNow() const;

    // Authoring helpers — seed a TransformKeyframe from the target Node3D's
    // current world-space transform and append it to either the bound asset
    // or the inline keyframes vector.
    void AddKeyframeFromTarget();
    void AddInlineKeyframeFromTarget();

#if EDITOR
    virtual bool DrawCustomProperty(Property& prop) override;
#endif

protected:

    TransformKeyframe MakeKeyframeFromTarget() const;


    void EvaluateAtTime(float time);
    TransformKeyframe SampleSource(float time) const;
    const std::vector<TransformKeyframe>* GetActiveKeyframes() const;
    float GetActiveDuration() const;

    TransformAnimationRef mAnimation;
    std::vector<TransformKeyframe> mInlineKeyframes;

    WeakPtr<Node> mTarget;

    float mCurrentTime = 0.0f;
    float mPlayRate = 1.0f;
    bool  mPlaying = false;
    bool  mPaused = false;
    bool  mLoop = false;
    bool  mPlayOnStart = false;

    int32_t mLastFiredKeyframe = -1;

#if EDITOR
    void StartPreview();
    void StopPreview();

    bool      mEditorPreviewing = false;
    bool      mEditorPreviewSnapshotValid = false;
    glm::vec3 mEditorPreviewSavedPos = glm::vec3(0.0f);
    glm::quat mEditorPreviewSavedRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mEditorPreviewSavedScale = glm::vec3(1.0f);
#endif
};
