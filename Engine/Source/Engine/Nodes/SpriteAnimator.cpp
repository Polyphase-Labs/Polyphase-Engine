#include "Nodes/SpriteAnimator.h"
#include "Assets/Texture.h"
#include "Assets/SpriteAnimation.h"

#include "AssetManager.h"
#include "Datum.h"
#include "Property.h"
#include "Log.h"

FORCE_LINK_DEF(SpriteAnimator);
DEFINE_NODE(SpriteAnimator, Node);

bool SpriteAnimator::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    SpriteAnimator* anim = static_cast<SpriteAnimator*>(prop->mOwner);
    bool handled = false;

    if (prop->mName == "Animations")
    {
        // Asset list edited in inspector — rebuild the runtime registry next tick.
        anim->mRegistryDirty = true;
    }
    else if (prop->mName == "Editor Play")
    {
        if (*(const bool*)newValue)
        {
            anim->Play();
        }
        else
        {
            anim->Pause();
        }
        handled = true;
    }
    else if (prop->mName == "Editor Stop")
    {
        if (*(const bool*)newValue)
        {
            anim->Stop();
        }
        handled = true;
    }
    else if (prop->mName == "Editor Preview")
    {
        anim->mEditorPreview = *(const bool*)newValue;
        handled = true;
    }

    return handled;
}

SpriteAnimator::SpriteAnimator()
{
    mName = "SpriteAnimator";
}

SpriteAnimator::~SpriteAnimator()
{
}

const char* SpriteAnimator::GetTypeName() const
{
    return "SpriteAnimator";
}

void SpriteAnimator::GatherProperties(std::vector<Property>& outProps)
{
    Node::GatherProperties(outProps);

    SCOPED_CATEGORY("Sprite Animator");

    outProps.push_back(Property(DatumType::Asset, "Animations", this, &mAnimations, 1, HandlePropChange,
                                int32_t(SpriteAnimation::GetStaticType())).MakeVector());
    outProps.push_back(Property(DatumType::String, "Default Animation", this, &mDefaultAnimation));
    outProps.push_back(Property(DatumType::Bool, "Auto Play", this, &mAutoPlay));
    outProps.push_back(Property(DatumType::Bool, "Loop Override", this, &mLoopOverride));
    outProps.push_back(Property(DatumType::Float, "Playback Speed", this, &mPlaybackSpeed));

    // Editor-only preview controls. Match SoundWave's synthetic Play/Stop pattern —
    // backing bools aren't really persisted state, just chrome that triggers callbacks.
    outProps.push_back(Property(DatumType::Bool, "Editor Preview", this, &mEditorPreview, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Bool, "Editor Play", this, &mEditorPlayButton, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Bool, "Editor Stop", this, &mEditorStopButton, 1, HandlePropChange));
}

void SpriteAnimator::Create()
{
    Node::Create();
}

void SpriteAnimator::Destroy()
{
    Node::Destroy();
}

void SpriteAnimator::Start()
{
    Node::Start();

    RebuildRegistry();

    if (mAutoPlay && !mDefaultAnimation.empty())
    {
        PlayAnimation(mDefaultAnimation);
    }
}

void SpriteAnimator::Tick(float deltaTime)
{
    Node::Tick(deltaTime);

    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    TickInternal(deltaTime);
}

void SpriteAnimator::EditorTick(float deltaTime)
{
    Node::EditorTick(deltaTime);

    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    if (mEditorPreview)
    {
        TickInternal(deltaTime);
    }
}

void SpriteAnimator::TickInternal(float deltaTime)
{
    if (!mPlaying || mCurrentName.empty())
        return;

    SpriteAnimEntry* entry = FindEntry(mCurrentName);
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
                // Clamp at last frame, stop playing, emit OnAnimationEnd.
                mCurrentFrame = frameCount - 1;
                mElapsed = 0.0f;
                mPlaying = false;
                std::vector<Datum> args;
                args.push_back(Datum(mCurrentName));
                EmitSignal("OnAnimationEnd", args);
                break;
            }
        }

        if (mCurrentFrame != prevFrame)
        {
            std::vector<Datum> args;
            args.push_back(Datum(mCurrentFrame));
            EmitSignal("OnFrameChanged", args);
        }
    }
}

void SpriteAnimator::Play()
{
    if (mCurrentName.empty())
    {
        // Nothing currently selected — fall back to default.
        if (!mDefaultAnimation.empty() && HasAnimation(mDefaultAnimation))
        {
            PlayAnimation(mDefaultAnimation);
            return;
        }
        return;
    }
    mPlaying = true;
}

void SpriteAnimator::Pause()
{
    mPlaying = false;
}

void SpriteAnimator::Stop()
{
    mPlaying = false;
    mCurrentFrame = 0;
    mElapsed = 0.0f;
}

void SpriteAnimator::PlayAnimation(const std::string& name)
{
    if (mRegistryDirty)
    {
        RebuildRegistry();
    }

    if (!HasAnimation(name))
    {
        LogWarning("SpriteAnimator: animation '%s' not found", name.c_str());
        return;
    }

    mCurrentName = name;
    mCurrentFrame = 0;
    mElapsed = 0.0f;
    mPlaying = true;

    std::vector<Datum> args;
    args.push_back(Datum(name));
    EmitSignal("OnAnimationStart", args);
}

void SpriteAnimator::SetSpeed(float speed)
{
    mPlaybackSpeed = speed;
}

void SpriteAnimator::AddAnimation(SpriteAnimation* asset)
{
    if (asset == nullptr)
        return;

    mAnimations.push_back(SpriteAnimationRef(asset));
    mRegistryDirty = true;
}

void SpriteAnimator::AddAnimation(const std::string& assetPath)
{
    if (assetPath.empty())
        return;

    Asset* asset = ::LoadAsset(assetPath);
    if (asset == nullptr)
    {
        LogWarning("SpriteAnimator::AddAnimation: failed to load '%s'", assetPath.c_str());
        return;
    }

    SpriteAnimation* spriteAnim = asset->As<SpriteAnimation>();
    if (spriteAnim == nullptr)
    {
        LogWarning("SpriteAnimator::AddAnimation: '%s' is not a SpriteAnimation", assetPath.c_str());
        return;
    }

    AddAnimation(spriteAnim);
}

void SpriteAnimator::CreateAnimation(const std::string& name)
{
    if (name.empty())
        return;

    SpriteAnimEntry entry;
    entry.name = name;
    entry.fps = 12.0f;
    entry.loop = true;
    mRegistry[name] = std::move(entry);
}

void SpriteAnimator::CreateAnimation(const std::string& name, const std::vector<Texture*>& frames)
{
    if (name.empty())
        return;

    SpriteAnimEntry entry;
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

void SpriteAnimator::AddImage(const std::string& name, Texture* tex)
{
    if (name.empty() || tex == nullptr)
        return;

    auto it = mRegistry.find(name);
    if (it == mRegistry.end())
    {
        // Auto-create an empty entry if it doesn't exist yet.
        CreateAnimation(name);
        it = mRegistry.find(name);
        if (it == mRegistry.end())
            return;
    }

    // Adding an image to an asset-driven entry would silently get clobbered on
    // RebuildRegistry. Disallow it; users wanting hybrids should script-build.
    if (it->second.sourceAsset != nullptr)
    {
        LogWarning("SpriteAnimator::AddImage: '%s' is asset-driven; cannot mutate frames at runtime",
                   name.c_str());
        return;
    }

    it->second.frames.push_back(TextureRef(tex));
}

void SpriteAnimator::AddImage(const std::string& name, const std::string& path)
{
    if (path.empty())
        return;

    Asset* asset = ::LoadAsset(path);
    if (asset == nullptr)
    {
        LogWarning("SpriteAnimator::AddImage: failed to load '%s'", path.c_str());
        return;
    }

    Texture* tex = asset->As<Texture>();
    if (tex == nullptr)
    {
        LogWarning("SpriteAnimator::AddImage: '%s' is not a Texture", path.c_str());
        return;
    }

    AddImage(name, tex);
}

void SpriteAnimator::AddImages(const std::string& name, const std::vector<std::string>& paths)
{
    for (const std::string& path : paths)
    {
        AddImage(name, path);
    }
}

void SpriteAnimator::RemoveAnimation(const std::string& name)
{
    mRegistry.erase(name);
    if (mCurrentName == name)
    {
        Stop();
        mCurrentName.clear();
    }
}

bool SpriteAnimator::HasAnimation(const std::string& name) const
{
    return mRegistry.find(name) != mRegistry.end();
}

void SpriteAnimator::RebuildRegistry()
{
    // Preserve script-built entries (those with sourceAsset == null and a
    // non-empty frames list); rebuild asset-backed entries from mAnimations.
    std::unordered_map<std::string, SpriteAnimEntry> preserved;
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
        if (asset == nullptr)
            continue;

        // Each asset's clip name is its Animation Name field, falling back to
        // the asset's resource name. This lets users name two animations
        // independently of their asset filename.
        std::string clipName = asset->GetAnimationName();
        if (clipName.empty())
        {
            clipName = asset->GetName();
        }

        SpriteAnimEntry entry;
        entry.name = clipName;
        entry.sourceAsset = ref;
        entry.fps = asset->GetFps();
        entry.loop = asset->GetLoop();

        mRegistry[clipName] = std::move(entry);
    }

    mRegistryDirty = false;

    // If the currently-playing animation got removed, stop.
    if (!mCurrentName.empty() && !HasAnimation(mCurrentName))
    {
        Stop();
        mCurrentName.clear();
    }
}

SpriteAnimEntry* SpriteAnimator::FindEntry(const std::string& name)
{
    auto it = mRegistry.find(name);
    return (it != mRegistry.end()) ? &it->second : nullptr;
}

const SpriteAnimEntry* SpriteAnimator::FindEntry(const std::string& name) const
{
    auto it = mRegistry.find(name);
    return (it != mRegistry.end()) ? &it->second : nullptr;
}

int32_t SpriteAnimator::EntryFrameCount(const SpriteAnimEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFrameCount() : 0;
    }
    return int32_t(entry.frames.size());
}

float SpriteAnimator::EntryFps(const SpriteAnimEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFps() : entry.fps;
    }
    return entry.fps;
}

bool SpriteAnimator::EntryLoop(const SpriteAnimEntry& entry) const
{
    if (entry.sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry.sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetLoop() : entry.loop;
    }
    return entry.loop;
}

Texture* SpriteAnimator::ResolveCurrentTexture() const
{
    const SpriteAnimEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr)
        return nullptr;

    if (entry->sourceAsset != nullptr)
    {
        SpriteAnimation* asset = entry->sourceAsset.Get<SpriteAnimation>();
        return asset ? asset->GetFrameTexture(mCurrentFrame) : nullptr;
    }

    if (mCurrentFrame < 0 || mCurrentFrame >= int32_t(entry->frames.size()))
        return nullptr;

    return entry->frames[mCurrentFrame].Get<Texture>();
}

bool SpriteAnimator::ResolveCurrentUV(glm::vec2& outUV0, glm::vec2& outUV1) const
{
    outUV0 = glm::vec2(0.0f, 0.0f);
    outUV1 = glm::vec2(1.0f, 1.0f);

    const SpriteAnimEntry* entry = FindEntry(mCurrentName);
    if (entry == nullptr || entry->sourceAsset == nullptr)
    {
        // Discrete script-built entries always cover the full texture.
        return true;
    }

    SpriteAnimation* asset = entry->sourceAsset.Get<SpriteAnimation>();
    if (asset == nullptr)
        return false;

    return asset->GetFrameUV(mCurrentFrame, outUV0, outUV1);
}

Texture* SpriteAnimator::GetCurrentTexture() const
{
    return ResolveCurrentTexture();
}

glm::vec2 SpriteAnimator::GetCurrentUVScale() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1))
        return glm::vec2(1.0f, 1.0f);
    return uv1 - uv0;
}

glm::vec2 SpriteAnimator::GetCurrentUVOffset() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1))
        return glm::vec2(0.0f, 0.0f);
    return uv0;
}

glm::vec4 SpriteAnimator::GetCurrentUVRect() const
{
    glm::vec2 uv0, uv1;
    if (!ResolveCurrentUV(uv0, uv1))
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    return glm::vec4(uv0.x, uv0.y, uv1.x, uv1.y);
}

#if EDITOR
#include "imgui.h"

bool SpriteAnimator::DrawCustomProperty(Property& prop)
{
    // Augment the synthetic "Editor Preview" toggle with a live status readout
    // beneath it so users can see what's playing without opening the debug log.
    // Returning false lets the default property widget still draw.
    if (prop.mName == "Editor Preview")
    {
        const SpriteAnimEntry* entry = !mCurrentName.empty() ? FindEntry(mCurrentName) : nullptr;
        const int32_t total = entry ? EntryFrameCount(*entry) : 0;
        const char* state = mPlaying ? "playing" : "paused";

        if (mCurrentName.empty())
        {
            ImGui::TextDisabled("Animation: (none)");
        }
        else
        {
            ImGui::Text("Animation: %s [%d / %d] (%s)",
                        mCurrentName.c_str(),
                        mCurrentFrame, total,
                        state);
        }

        // Hint about the bind-to-Quad workflow so first-time users know how to
        // see the animation in the viewport.
        ImGui::TextDisabled("Tip: bind GetCurrentTexture() to a Quad in script.");
    }
    return false;
}
#endif
