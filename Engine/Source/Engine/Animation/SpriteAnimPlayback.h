#pragma once

#include "PolyphaseAPI.h"
#include "AssetRef.h"
#include "Assets/SpriteAnimation.h"
#include "ScriptFunc.h"

#include "glm/glm.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Texture;

// One playable animation entry — backed by a SpriteAnimation asset
// (sourceAsset != null) OR by a runtime-built texture list.
struct SpriteAnimPlaybackEntry
{
    std::string name;
    SpriteAnimationRef sourceAsset;

    // Used only when sourceAsset is null (script-built).
    std::vector<TextureRef> frames;
    float fps = 12.0f;
    bool loop = true;
};

// Reusable sprite-animation playback core. Owns the registry, current frame,
// elapsed time, and the public play/pause/stop API. No knowledge of nodes,
// signals, or rendering — those concerns live in the host node (SpriteAnimator,
// AnimatedWidget, AnimatedSprite3D) which calls Tick(dt) and reads the output
// queries each frame.
//
// Frame-changed and animation-end callbacks are passed into Tick rather than
// stored, so the host can route them to its own EmitSignal / property updates
// without this struct having to know about Node.
class POLYPHASE_API SpriteAnimPlayback
{
public:
    // Property-backed
    std::vector<SpriteAnimationRef> mAnimations;
    std::string mDefaultAnimation;
    bool mAutoPlay = true;
    bool mLoopOverride = false;
    float mPlaybackSpeed = 1.0f;

    // Runtime state
    std::unordered_map<std::string, SpriteAnimPlaybackEntry> mRegistry;
    std::string mCurrentName;
    int32_t mCurrentFrame = 0;
    float mElapsed = 0.0f;
    bool mPlaying = false;
    bool mRegistryDirty = true;

    using FrameCallback = std::function<void(int32_t frameIndex)>;
    using AnimEndCallback = std::function<void(const std::string& animName)>;
    using AnimStartCallback = std::function<void(const std::string& animName)>;

    // Advances state by deltaTime. Calls onFrameChanged whenever mCurrentFrame
    // increments, and onAnimationEnd when a non-looping clip finishes.
    void Tick(float deltaTime,
              const FrameCallback& onFrameChanged = nullptr,
              const AnimEndCallback& onAnimationEnd = nullptr);

    void Play();
    void Pause();
    void Stop();
    void PlayAnimation(const std::string& name,
                       const AnimStartCallback& onAnimationStart = nullptr);

    // Jump to a specific frame in the current animation. Clamps to [0..N-1]
    // when out of range. Resets the inter-frame elapsed timer so playback
    // (if running) resumes cleanly from the new frame. Does NOT change
    // mPlaying — caller controls Play/Pause separately. Returns true if the
    // frame index actually changed (useful for skipping a redundant rebind).
    bool SetFrame(int32_t frameIndex);

    // Play forward until reaching targetFrame, then optionally pause and call
    // onFinished. If targetFrame is already the current frame, fires
    // immediately (without playing a full lap). Cancels any prior AnimateTo.
    // Implicitly Play()s so it starts even if the animator was paused.
    // Returns true if a target was set; false if the target couldn't be
    // resolved (no animation playing, frameCount == 0, etc.).
    bool AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished);
    bool AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished);
    void CancelAnimateTo();
    bool HasAnimateToTarget() const { return mAnimateTo.active; }

    void RebuildRegistry();
    void AddAnimationAsset(SpriteAnimation* asset);
    void AddAnimationByPath(const std::string& assetPath);

    void CreateAnimation(const std::string& name);
    void CreateAnimation(const std::string& name, const std::vector<Texture*>& frames);
    void AddImage(const std::string& name, Texture* tex);
    void AddImage(const std::string& name, const std::string& path);
    void AddImages(const std::string& name, const std::vector<std::string>& paths);

    void RemoveAnimation(const std::string& name);
    bool HasAnimation(const std::string& name) const;

    // Output queries — values are tuned for the engine's Quad encoding
    // (final = (baseUV + offset) * scale). UVRect is the raw (u0,v0,u1,v1)
    // for material-vec4 use cases.
    Texture* GetCurrentTexture() const;
    glm::vec2 GetCurrentUVScale() const;
    glm::vec2 GetCurrentUVOffset() const;
    glm::vec4 GetCurrentUVRect() const;

    int32_t GetCurrentFrameCount() const;

    // Smooth playback progress in [0, 1] across the current clip.
    // 0 = start of frame 0, ~1 = end of last frame. Sub-frame interpolated
    // via mElapsed so progress bars look smooth at low FPS. Snaps to 1.0
    // when a non-looping clip has finished. Returns 0 when no clip is playing.
    float GetProgress() const;

    // AnimateTo state — exposed so derived/host nodes can inspect / clear.
    struct AnimateToState
    {
        bool       active = false;
        int32_t    targetFrame = -1;
        bool       pauseOnFinished = true;
        ScriptFunc onFinished;
    };
    AnimateToState mAnimateTo;

private:
    SpriteAnimPlaybackEntry* FindEntry(const std::string& name);
    const SpriteAnimPlaybackEntry* FindEntry(const std::string& name) const;
    int32_t EntryFrameCount(const SpriteAnimPlaybackEntry& entry) const;
    float EntryFps(const SpriteAnimPlaybackEntry& entry) const;
    bool EntryLoop(const SpriteAnimPlaybackEntry& entry) const;

    Texture* ResolveCurrentTexture() const;
    bool ResolveCurrentUV(glm::vec2& outUV0, glm::vec2& outUV1) const;
};
