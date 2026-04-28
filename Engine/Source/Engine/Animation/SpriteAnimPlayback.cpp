#include "Animation/SpriteAnimPlayback.h"
#include "Assets/Texture.h"
#include "Assets/SpriteAnimation.h"

#include "AssetManager.h"
#include "Log.h"

void SpriteAnimPlayback::Tick(float deltaTime,
                              const FrameCallback& onFrameChanged,
                              const AnimEndCallback& onAnimationEnd)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    if (!mPlaying || mCurrentName.empty())
        return;

    SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr)
        return;

    const int32_t frameCount = EntryFrameCount(*entry);
    if (frameCount <= 0)
        return;

    const float fps = EntryFps(*entry);
    if (fps <= 0.0f)
        return;

    mElapsed += deltaTime * mPlaybackSpeed;
    const float secondsPerFrame = 1.0f / fps;

    while (mElapsed >= secondsPerFrame)
    {
        mElapsed -= secondsPerFrame;
        const int32_t prevFrame = mCurrentFrame;
        mCurrentFrame++;

        if (mCurrentFrame >= frameCount)
        {
            const bool loop = mLoopOverride ? true : EntryLoop(*entry);
            if (loop)
            {
                mCurrentFrame = 0;
            }
            else
            {
                mCurrentFrame = frameCount - 1;
                mElapsed = 0.0f;
                mPlaying = false;
                if (onAnimationEnd) onAnimationEnd(mCurrentName);
                break;
            }
        }

        if (mCurrentFrame != prevFrame && onFrameChanged)
        {
            onFrameChanged(mCurrentFrame);
        }

        // AnimateTo target reached — fire the callback (and optionally pause)
        // BEFORE advancing further. Snapshot the callback so a re-arm inside
        // it doesn't get clobbered by our own clear.
        if (mAnimateTo.active && mCurrentFrame == mAnimateTo.targetFrame)
        {
            ScriptFunc cb = mAnimateTo.onFinished;
            const bool pauseAfter = mAnimateTo.pauseOnFinished;
            mAnimateTo.active = false;
            mAnimateTo.targetFrame = -1;
            mAnimateTo.onFinished = ScriptFunc();
            if (pauseAfter) mPlaying = false;
            if (cb.IsValid()) cb.Call();
            break;
        }
    }
}

void SpriteAnimPlayback::Play()
{
    if (mCurrentName.empty())
    {
        if (!mDefaultAnimation.empty() && HasAnimation(mDefaultAnimation))
        {
            PlayAnimation(mDefaultAnimation);
            return;
        }
        return;
    }
    mPlaying = true;
}

void SpriteAnimPlayback::Pause()
{
    mPlaying = false;
}

void SpriteAnimPlayback::Stop()
{
    mPlaying = false;
    mCurrentFrame = 0;
    mElapsed = 0.0f;
    CancelAnimateTo();
}

bool SpriteAnimPlayback::SetFrame(int32_t frameIndex)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    const int32_t frameCount = entry ? EntryFrameCount(*entry) : 0;
    if (frameCount <= 0)
    {
        // No animation selected / empty clip: reset state but report no change.
        if (mCurrentFrame == 0 && mElapsed == 0.0f) return false;
        mCurrentFrame = 0;
        mElapsed = 0.0f;
        CancelAnimateTo();
        return true;
    }

    int32_t clamped = frameIndex;
    if (clamped < 0) clamped = 0;
    if (clamped >= frameCount) clamped = frameCount - 1;

    const bool changed = (clamped != mCurrentFrame);
    mCurrentFrame = clamped;
    mElapsed = 0.0f;

    // SetFrame is a manual override — abandon any in-flight AnimateTo so the
    // callback doesn't fire spuriously when the user expected a quiet jump.
    CancelAnimateTo();
    return changed;
}

bool SpriteAnimPlayback::AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    const int32_t frameCount = entry ? EntryFrameCount(*entry) : 0;
    if (frameCount <= 0)
    {
        return false;
    }

    int32_t clamped = targetFrame;
    if (clamped < 0) clamped = 0;
    if (clamped >= frameCount) clamped = frameCount - 1;

    // Already at target: fire callback immediately rather than playing a full
    // lap to land on the same spot. Matches the user's intent of "go to here
    // and notify me when you're there."
    if (clamped == mCurrentFrame)
    {
        ScriptFunc cb = onFinished;
        if (pauseOnFinished) mPlaying = false;
        if (cb.IsValid()) cb.Call();
        return true;
    }

    mAnimateTo.active = true;
    mAnimateTo.targetFrame = clamped;
    mAnimateTo.pauseOnFinished = pauseOnFinished;
    mAnimateTo.onFinished = onFinished;
    mPlaying = true;
    return true;
}

bool SpriteAnimPlayback::AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    const int32_t frameCount = entry ? EntryFrameCount(*entry) : 0;
    if (frameCount <= 0)
    {
        return false;
    }

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    // Round to nearest frame; clamp to last frame at progress=1.0.
    int32_t target = static_cast<int32_t>(progress * static_cast<float>(frameCount));
    if (target >= frameCount) target = frameCount - 1;
    return AnimateTo(target, pauseOnFinished, onFinished);
}

void SpriteAnimPlayback::CancelAnimateTo()
{
    mAnimateTo.active = false;
    mAnimateTo.targetFrame = -1;
    mAnimateTo.onFinished = ScriptFunc();
}

void SpriteAnimPlayback::PlayAnimation(const std::string& name,
                                       const AnimStartCallback& onAnimationStart)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    if (!HasAnimation(name))
    {
        LogWarning("SpriteAnimPlayback: animation '%s' not found", name.c_str());
        return;
    }

    mCurrentName = name;
    mCurrentFrame = 0;
    mElapsed = 0.0f;
    mPlaying = true;

    // PlayAnimation is a fundamental state change — abandon any prior
    // AnimateTo target since the user is moving to a different clip.
    CancelAnimateTo();

    if (onAnimationStart) onAnimationStart(name);
}

void SpriteAnimPlayback::AddAnimationAsset(SpriteAnimation* asset)
{
    if (asset == nullptr) return;
    mAnimations.push_back(SpriteAnimationRef(asset));
    mRegistryDirty = true;
}

void SpriteAnimPlayback::AddAnimationByPath(const std::string& assetPath)
{
    if (assetPath.empty()) return;
    Asset* asset = ::LoadAsset(assetPath);
    if (asset == nullptr)
    {
        LogWarning("SpriteAnimPlayback::AddAnimation: failed to load '%s'", assetPath.c_str());
        return;
    }
    SpriteAnimation* spriteAnim = asset->As<SpriteAnimation>();
    if (spriteAnim == nullptr)
    {
        LogWarning("SpriteAnimPlayback::AddAnimation: '%s' is not a SpriteAnimation", assetPath.c_str());
        return;
    }
    AddAnimationAsset(spriteAnim);
}

void SpriteAnimPlayback::CreateAnimation(const std::string& name)
{
    if (name.empty()) return;
    SpriteAnimPlaybackEntry entry;
    entry.name = name;
    entry.fps = 12.0f;
    entry.loop = true;
    mRegistry[name] = std::move(entry);
}

void SpriteAnimPlayback::CreateAnimation(const std::string& name, const std::vector<Texture*>& frames)
{
    if (name.empty()) return;
    SpriteAnimPlaybackEntry entry;
    entry.name = name;
    entry.fps = 12.0f;
    entry.loop = true;
    entry.frames.reserve(frames.size());
    for (Texture* tex : frames)
    {
        entry.frames.push_back(TextureRef(tex));
    }
    mRegistry[name] = std::move(entry);
}

void SpriteAnimPlayback::AddImage(const std::string& name, Texture* tex)
{
    if (name.empty() || tex == nullptr) return;

    auto it = mRegistry.find(name);
    if (it == mRegistry.end())
    {
        CreateAnimation(name);
        it = mRegistry.find(name);
        if (it == mRegistry.end()) return;
    }

    if (it->second.sourceAsset != nullptr)
    {
        LogWarning("SpriteAnimPlayback::AddImage: '%s' is asset-driven; cannot mutate frames at runtime",
                   name.c_str());
        return;
    }

    it->second.frames.push_back(TextureRef(tex));
}

void SpriteAnimPlayback::AddImage(const std::string& name, const std::string& path)
{
    if (path.empty()) return;
    Asset* asset = ::LoadAsset(path);
    if (asset == nullptr)
    {
        LogWarning("SpriteAnimPlayback::AddImage: failed to load '%s'", path.c_str());
        return;
    }
    Texture* tex = asset->As<Texture>();
    if (tex == nullptr)
    {
        LogWarning("SpriteAnimPlayback::AddImage: '%s' is not a Texture", path.c_str());
        return;
    }
    AddImage(name, tex);
}

void SpriteAnimPlayback::AddImages(const std::string& name, const std::vector<std::string>& paths)
{
    for (const std::string& path : paths)
    {
        AddImage(name, path);
    }
}

void SpriteAnimPlayback::RemoveAnimation(const std::string& name)
{
    mRegistry.erase(name);
    if (mCurrentName == name)
    {
        Stop();
        mCurrentName.clear();
    }
}

bool SpriteAnimPlayback::HasAnimation(const std::string& name) const
{
    return mRegistry.find(name) != mRegistry.end();
}

void SpriteAnimPlayback::RebuildRegistry()
{
    // Preserve script-built entries, rebuild asset-backed ones from mAnimations.
    std::unordered_map<std::string, SpriteAnimPlaybackEntry> preserved;
    for (auto& kv : mRegistry)
    {
        if (kv.second.sourceAsset == nullptr)
        {
            preserved.emplace(kv.first, std::move(kv.second));
        }
    }
    mRegistry = std::move(preserved);

    for (const SpriteAnimationRef& ref : mAnimations)
    {
        SpriteAnimation* asset = ref.Get<SpriteAnimation>();
        if (asset == nullptr) continue;

        std::string clipName = asset->GetAnimationName();
        if (clipName.empty()) clipName = asset->GetName();

        SpriteAnimPlaybackEntry entry;
        entry.name = clipName;
        entry.sourceAsset = ref;
        entry.fps = asset->GetFps();
        entry.loop = asset->GetLoop();
        mRegistry[clipName] = std::move(entry);
    }

    mRegistryDirty = false;

    if (!mCurrentName.empty() && !HasAnimation(mCurrentName))
    {
        Stop();
        mCurrentName.clear();
    }
}

SpriteAnimPlaybackEntry* SpriteAnimPlayback::FindEntry(const std::string& name)
{
    auto it = mRegistry.find(name);
    return (it != mRegistry.end()) ? &it->second : nullptr;
}

const SpriteAnimPlaybackEntry* SpriteAnimPlayback::FindEntry(const std::string& name) const
{
    auto it = mRegistry.find(name);
    return (it != mRegistry.end()) ? &it->second : nullptr;
}

int32_t SpriteAnimPlayback::EntryFrameCount(const SpriteAnimPlaybackEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFrameCount() : 0;
    }
    return int32_t(entry.frames.size());
}

float SpriteAnimPlayback::EntryFps(const SpriteAnimPlaybackEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFps() : entry.fps;
    }
    return entry.fps;
}

bool SpriteAnimPlayback::EntryLoop(const SpriteAnimPlaybackEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetLoop() : entry.loop;
    }
    return entry.loop;
}

Texture* SpriteAnimPlayback::ResolveCurrentTexture() const
{
    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr) return nullptr;

    if (entry->sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry->sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFrameTexture(mCurrentFrame) : nullptr;
    }

    if (mCurrentFrame < 0 || mCurrentFrame >= int32_t(entry->frames.size()))
        return nullptr;
    return entry->frames[mCurrentFrame].Get<Texture>();
}

bool SpriteAnimPlayback::ResolveCurrentUV(glm::vec2& outUV0, glm::vec2& outUV1) const
{
    outUV0 = glm::vec2(0.0f, 0.0f);
    outUV1 = glm::vec2(1.0f, 1.0f);

    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr || entry->sourceAsset == nullptr)
    {
        return true;
    }

    SpriteAnimation* asset = entry->sourceAsset.Get<SpriteAnimation>();
    if (asset == nullptr) return false;
    return asset->GetFrameUV(mCurrentFrame, outUV0, outUV1);
}

Texture* SpriteAnimPlayback::GetCurrentTexture() const
{
    return ResolveCurrentTexture();
}

glm::vec2 SpriteAnimPlayback::GetCurrentUVScale() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1)) return glm::vec2(1.0f, 1.0f);
    return uv1 - uv0;
}

glm::vec2 SpriteAnimPlayback::GetCurrentUVOffset() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1)) return glm::vec2(0.0f, 0.0f);

    // Quad uses final = (baseUV + offset) * scale, so offset = uv0 / scale.
    const glm::vec2 scale = uv1 - uv0;
    glm::vec2 offset(0.0f);
    if (scale.x > 0.0001f) offset.x = uv0.x / scale.x;
    if (scale.y > 0.0001f) offset.y = uv0.y / scale.y;
    return offset;
}

glm::vec4 SpriteAnimPlayback::GetCurrentUVRect() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1)) return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    return glm::vec4(uv0.x, uv0.y, uv1.x, uv1.y);
}

int32_t SpriteAnimPlayback::GetCurrentFrameCount() const
{
    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    return entry ? EntryFrameCount(*entry) : 0;
}

float SpriteAnimPlayback::GetProgress() const
{
    const SpriteAnimPlaybackEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr) return 0.0f;

    const int32_t frameCount = EntryFrameCount(*entry);
    if (frameCount <= 0) return 0.0f;

    const float fps = EntryFps(*entry);
    const float fractional = (fps > 0.0f)
        ? (static_cast<float>(mCurrentFrame) + mElapsed * fps)
        : static_cast<float>(mCurrentFrame);

    float progress = fractional / static_cast<float>(frameCount);

    // Non-looping clips park at frame N-1 with elapsed=0 on completion, which
    // would otherwise read as (N-1)/N. Snap to 1.0 so progress-bar callers
    // see "100% done" at end of clip.
    if (!mPlaying && mCurrentFrame >= frameCount - 1)
    {
        progress = 1.0f;
    }

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}
