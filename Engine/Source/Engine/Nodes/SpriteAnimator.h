#pragma once

#include "Nodes/Node.h"
#include "Assets/SpriteAnimation.h"
#include "AssetRef.h"
#include "ScriptFunc.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <unordered_map>

class Texture;

// One playable animation entry inside a SpriteAnimator. May be backed either
// by a SpriteAnimation asset (sourceAsset != null) — in which case Discrete vs
// AtlasGrid mode and atlas UV math come from the asset — or by a runtime-built
// list of textures (script-built via CreateAnimation/AddImage), which is
// always Discrete-mode.
struct SpriteAnimEntry
{
    std::string name;
    SpriteAnimationRef sourceAsset;

    // Used only when sourceAsset is null (script-built entries).
    std::vector<TextureRef> frames;
    float fps = 12.0f;
    bool loop = true;
};

// A logical animator that advances a named animation over time and exposes the
// current frame as a Texture* (and atlas UVs). Bind GetCurrentTexture() to a
// Quad's SetTexture, or to a Material parameter via SetTextureParameter, to
// drive any kind of sprite-driven visual.
//
// Designed as a plain Node (not Node3D / not Widget) — it has no transform of
// its own, just produces output. Sits alongside whatever node renders the
// sprite (Quad, MeshRenderer with material, etc.).
class POLYPHASE_API SpriteAnimator : public Node
{
public:

    DECLARE_NODE(SpriteAnimator, Node);

    SpriteAnimator();
    virtual ~SpriteAnimator();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
#if EDITOR
    virtual bool DrawCustomProperty(Property& prop) override;
#endif

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Start() override;
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    // Lifecycle / playback
    void Play();
    void Pause();
    void Stop();
    void PlayAnimation(const std::string& name);
    void SetFrame(int32_t frameIndex);
    bool AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished);
    bool AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished);
    void CancelAnimateTo();
    void SetSpeed(float speed);
    float GetSpeed() const { return mPlaybackSpeed; }
    bool IsPlaying() const { return mPlaying; }

    bool GetAutoPlay() const { return mAutoPlay; }
    void SetAutoPlay(bool autoPlay) { mAutoPlay = autoPlay; }
    bool GetLoopOverride() const { return mLoopOverride; }
    void SetLoopOverride(bool loop) { mLoopOverride = loop; }
    const std::string& GetDefaultAnimation() const { return mDefaultAnimation; }
    void SetDefaultAnimation(const std::string& name) { mDefaultAnimation = name; }

    // Asset-driven registration
    void AddAnimation(SpriteAnimation* asset);
    void AddAnimation(const std::string& assetPath);

    // Runtime-built registration (always Discrete mode)
    void CreateAnimation(const std::string& name);
    void CreateAnimation(const std::string& name, const std::vector<Texture*>& frames);
    void AddImage(const std::string& name, Texture* tex);
    void AddImage(const std::string& name, const std::string& path);
    void AddImages(const std::string& name, const std::vector<std::string>& paths);

    void RemoveAnimation(const std::string& name);
    bool HasAnimation(const std::string& name) const;

    float GetProgress() const;

    // Output for binding (works uniformly for Discrete and AtlasGrid frames)
    Texture* GetCurrentTexture() const;
    glm::vec2 GetCurrentUVScale() const;   // (1,1) for discrete; (uv1-uv0) for atlas
    glm::vec2 GetCurrentUVOffset() const;  // (0,0) for discrete; uv0 for atlas
    glm::vec4 GetCurrentUVRect() const;    // (u0,v0,u1,v1) packed for material vec4 params
    const std::string& GetCurrentAnimationName() const { return mCurrentName; }
    int32_t GetCurrentFrameIndex() const { return mCurrentFrame; }

    // Editor preview: when true, EditorTick advances the animation so users
    // can scrub through frames without entering Play mode. Off by default.
    bool GetEditorPreview() const { return mEditorPreview; }
    void SetEditorPreview(bool enabled) { mEditorPreview = enabled; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void RebuildRegistry();
    void TickInternal(float deltaTime);
    bool ResolveCurrentUV(glm::vec2& outUV0, glm::vec2& outUV1) const;
    Texture* ResolveCurrentTexture() const;
    SpriteAnimEntry* FindEntry(const std::string& name);
    const SpriteAnimEntry* FindEntry(const std::string& name) const;
    int32_t EntryFrameCount(const SpriteAnimEntry& entry) const;
    float EntryFps(const SpriteAnimEntry& entry) const;
    bool EntryLoop(const SpriteAnimEntry& entry) const;

    // Property-backed
    std::vector<SpriteAnimationRef> mAnimations;
    std::string mDefaultAnimation;
    bool mAutoPlay = true;
    bool mLoopOverride = false;
    float mPlaybackSpeed = 1.0f;

    // Synthetic editor-only "Editor Preview" toggle (stored, not serialized as a
    // gameplay property — but exposed in inspector for live preview).
    bool mEditorPreview = false;

    // Synthetic Play/Stop button placeholders for inspector buttons (SoundWave-style).
    bool mEditorPlayButton = false;
    bool mEditorStopButton = false;

    // Runtime registry — unifies asset entries and script-built entries by name.
    std::unordered_map<std::string, SpriteAnimEntry> mRegistry;

    // Playback state
    std::string mCurrentName;
    int32_t mCurrentFrame = 0;
    float mElapsed = 0.0f;
    bool mPlaying = false;
    bool mRegistryDirty = true;

    // AnimateTo: set by AnimateTo()/AnimateToProgress(); checked + cleared in
    // TickInternal when target frame is reached.
    bool       mAnimateToActive = false;
    int32_t    mAnimateToTarget = -1;
    bool       mAnimateToPause = true;
    ScriptFunc mAnimateToCallback;
};
