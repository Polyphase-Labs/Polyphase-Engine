#pragma once

#include "Nodes/Widgets/Quad.h"
#include "Animation/SpriteAnimPlayback.h"
#include "AssetRef.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>

class Texture;
class SpriteAnimation;

// A Quad that plays sprite animations on itself. Combines the rendering of
// Quad with the playback engine of SpriteAnimator into a single node — drop
// it in, assign animations, and it shows them. No sibling SpriteAnimator or
// glue script needed.
class POLYPHASE_API AnimatedWidget : public Quad
{
public:

    DECLARE_NODE(AnimatedWidget, Quad);

    AnimatedWidget();
    virtual ~AnimatedWidget();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    // Playback API mirrors SpriteAnimator.
    void Play();
    void Pause();
    void Stop();
    void PlayAnimation(const std::string& name);
    void SetFrame(int32_t frameIndex);
    bool AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished);
    bool AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished);
    void CancelAnimateTo() { mPlayback.CancelAnimateTo(); }
    void SetSpeed(float speed);
    float GetSpeed() const { return mPlayback.mPlaybackSpeed; }
    bool IsPlaying() const { return mPlayback.mPlaying; }

    void SetAutoPlay(bool v) { mPlayback.mAutoPlay = v; }
    bool GetAutoPlay() const { return mPlayback.mAutoPlay; }
    void SetLoopOverride(bool v) { mPlayback.mLoopOverride = v; }
    bool GetLoopOverride() const { return mPlayback.mLoopOverride; }
    void SetDefaultAnimation(const std::string& name) { mPlayback.mDefaultAnimation = name; }
    const std::string& GetDefaultAnimation() const { return mPlayback.mDefaultAnimation; }

    void AddAnimation(SpriteAnimation* asset)              { mPlayback.AddAnimationAsset(asset); }
    void AddAnimation(const std::string& path)             { mPlayback.AddAnimationByPath(path); }
    void CreateAnimation(const std::string& name)          { mPlayback.CreateAnimation(name); }
    void CreateAnimation(const std::string& name, const std::vector<Texture*>& frames)
                                                            { mPlayback.CreateAnimation(name, frames); }
    void AddImage(const std::string& name, Texture* tex)   { mPlayback.AddImage(name, tex); }
    void AddImage(const std::string& name, const std::string& path)
                                                            { mPlayback.AddImage(name, path); }
    void AddImages(const std::string& name, const std::vector<std::string>& paths)
                                                            { mPlayback.AddImages(name, paths); }
    void RemoveAnimation(const std::string& name)          { mPlayback.RemoveAnimation(name); }
    bool HasAnimation(const std::string& name) const       { return mPlayback.HasAnimation(name); }

    float GetProgress() const                              { return mPlayback.GetProgress(); }
    Texture* GetCurrentTexture() const                     { return mPlayback.GetCurrentTexture(); }
    int32_t GetCurrentFrameIndex() const                   { return mPlayback.mCurrentFrame; }
    const std::string& GetCurrentAnimationName() const     { return mPlayback.mCurrentName; }

    bool GetEditorPreview() const { return mEditorPreview; }
    void SetEditorPreview(bool v) { mEditorPreview = v; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    // Push the playback's current frame to this Quad's texture + UV.
    void ApplyCurrentFrameToSelf();

    SpriteAnimPlayback mPlayback;
    bool mEditorPreview = false;
    bool mEditorPlayButton = false;
    bool mEditorStopButton = false;
};
