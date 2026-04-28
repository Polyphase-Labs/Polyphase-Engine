#include "Nodes/Widgets/AnimatedWidget.h"
#include "Assets/Texture.h"
#include "Assets/SpriteAnimation.h"

#include "Datum.h"
#include "Property.h"
#include "Log.h"

FORCE_LINK_DEF(AnimatedWidget);
DEFINE_NODE(AnimatedWidget, Quad);

bool AnimatedWidget::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    AnimatedWidget* self = static_cast<AnimatedWidget*>(prop->mOwner);
    bool handled = false;

    if (prop->mName == "Animations")
    {
        self->mPlayback.mRegistryDirty = true;
    }
    else if (prop->mName == "Editor Play")
    {
        if (*(const bool*)newValue) self->Play();
        else self->Pause();
        handled = true;
    }
    else if (prop->mName == "Editor Stop")
    {
        if (*(const bool*)newValue) self->Stop();
        handled = true;
    }
    else if (prop->mName == "Editor Preview")
    {
        self->mEditorPreview = *(const bool*)newValue;
        handled = true;
    }

    return handled;
}

AnimatedWidget::AnimatedWidget()
{
    mName = "AnimatedWidget";
}

AnimatedWidget::~AnimatedWidget()
{
}

const char* AnimatedWidget::GetTypeName() const
{
    return "AnimatedWidget";
}

void AnimatedWidget::GatherProperties(std::vector<Property>& outProps)
{
    Quad::GatherProperties(outProps);

    SCOPED_CATEGORY("Sprite Animation");

    outProps.push_back(Property(DatumType::Asset, "Animations", this, &mPlayback.mAnimations, 1, HandlePropChange,
                                int32_t(SpriteAnimation::GetStaticType())).MakeVector());
    outProps.push_back(Property(DatumType::String, "Default Animation", this, &mPlayback.mDefaultAnimation));
    outProps.push_back(Property(DatumType::Bool, "Auto Play", this, &mPlayback.mAutoPlay));
    outProps.push_back(Property(DatumType::Bool, "Loop Override", this, &mPlayback.mLoopOverride));
    outProps.push_back(Property(DatumType::Float, "Playback Speed", this, &mPlayback.mPlaybackSpeed));

    outProps.push_back(Property(DatumType::Bool, "Editor Preview", this, &mEditorPreview, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Bool, "Editor Play", this, &mEditorPlayButton, 1, HandlePropChange));
    outProps.push_back(Property(DatumType::Bool, "Editor Stop", this, &mEditorStopButton, 1, HandlePropChange));
}

void AnimatedWidget::Create()
{
    Quad::Create();
}

void AnimatedWidget::Destroy()
{
    Quad::Destroy();
}

void AnimatedWidget::Start()
{
    Quad::Start();

    mPlayback.RebuildRegistry();

    if (mPlayback.mAutoPlay && !mPlayback.mDefaultAnimation.empty())
    {
        PlayAnimation(mPlayback.mDefaultAnimation);
    }
}

void AnimatedWidget::Tick(float deltaTime)
{
    Quad::Tick(deltaTime);

    bool frameChanged = false;
    int32_t prevFrame = mPlayback.mCurrentFrame;
    std::string prevName = mPlayback.mCurrentName;

    mPlayback.Tick(deltaTime,
        [this, &frameChanged](int32_t frameIndex)
        {
            frameChanged = true;
            std::vector<Datum> args;
            args.push_back(Datum(frameIndex));
            EmitSignal("OnFrameChanged", args);
        },
        [this](const std::string& animName)
        {
            std::vector<Datum> args;
            args.push_back(Datum(animName));
            EmitSignal("OnAnimationEnd", args);
        });

    if (frameChanged || mPlayback.mCurrentName != prevName)
    {
        ApplyCurrentFrameToSelf();
    }
}

void AnimatedWidget::EditorTick(float deltaTime)
{
    Quad::EditorTick(deltaTime);

    if (mPlayback.mRegistryDirty)
    {
        mPlayback.RebuildRegistry();
        // Rebuild may have reset the current animation; sync visuals.
        ApplyCurrentFrameToSelf();
    }

    if (mEditorPreview)
    {
        bool frameChanged = false;
        mPlayback.Tick(deltaTime,
            [&frameChanged](int32_t) { frameChanged = true; },
            nullptr);
        if (frameChanged) ApplyCurrentFrameToSelf();
    }
}

void AnimatedWidget::Play()
{
    mPlayback.Play();
    if (mPlayback.mPlaying) ApplyCurrentFrameToSelf();
}

void AnimatedWidget::Pause()
{
    mPlayback.Pause();
}

void AnimatedWidget::Stop()
{
    mPlayback.Stop();
    ApplyCurrentFrameToSelf();
}

void AnimatedWidget::PlayAnimation(const std::string& name)
{
    mPlayback.PlayAnimation(name,
        [this](const std::string& n)
        {
            std::vector<Datum> args;
            args.push_back(Datum(n));
            EmitSignal("OnAnimationStart", args);
        });
    ApplyCurrentFrameToSelf();
}

void AnimatedWidget::SetFrame(int32_t frameIndex)
{
    if (mPlayback.SetFrame(frameIndex))
    {
        std::vector<Datum> args;
        args.push_back(Datum(mPlayback.mCurrentFrame));
        EmitSignal("OnFrameChanged", args);
        ApplyCurrentFrameToSelf();
    }
}

bool AnimatedWidget::AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    const bool ok = mPlayback.AnimateTo(targetFrame, pauseOnFinished, onFinished);
    ApplyCurrentFrameToSelf();
    return ok;
}

bool AnimatedWidget::AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    const bool ok = mPlayback.AnimateToProgress(progress, pauseOnFinished, onFinished);
    ApplyCurrentFrameToSelf();
    return ok;
}

void AnimatedWidget::SetSpeed(float speed)
{
    mPlayback.mPlaybackSpeed = speed;
}

void AnimatedWidget::ApplyCurrentFrameToSelf()
{
    Texture* tex = mPlayback.GetCurrentTexture();
    if (tex != nullptr)
    {
        SetTexture(tex);
    }
    SetUvScale(mPlayback.GetCurrentUVScale());
    SetUvOffset(mPlayback.GetCurrentUVOffset());
}
