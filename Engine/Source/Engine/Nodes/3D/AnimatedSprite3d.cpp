#include "Nodes/3D/AnimatedSprite3d.h"
#include "Assets/Texture.h"
#include "Assets/Material.h"
#include "Assets/MaterialLite.h"
#include "Assets/SpriteAnimation.h"

#include "Datum.h"
#include "Property.h"
#include "Log.h"

FORCE_LINK_DEF(AnimatedSprite3D);
DEFINE_NODE(AnimatedSprite3D, Node3D);

bool AnimatedSprite3D::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    AnimatedSprite3D* self = static_cast<AnimatedSprite3D*>(prop->mOwner);
    bool handled = false;

    if (prop->mName == "Animations")
    {
        self->mPlayback.mRegistryDirty = true;
    }
    else if (prop->mName == "Material")
    {
        Asset** newAsset = (Asset**)newValue;
        self->mMaterial = (newAsset != nullptr) ? *newAsset : nullptr;
        // Reapply current frame to the new material immediately so the user
        // sees the current frame on the swapped material in the editor.
        self->ApplyCurrentFrameToMaterial();
        handled = true;
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

AnimatedSprite3D::AnimatedSprite3D()
{
    mName = "AnimatedSprite3D";
}

AnimatedSprite3D::~AnimatedSprite3D()
{
}

const char* AnimatedSprite3D::GetTypeName() const
{
    return "AnimatedSprite3D";
}

void AnimatedSprite3D::GatherProperties(std::vector<Property>& outProps)
{
    Node3D::GatherProperties(outProps);

    // SCOPED_CATEGORY declares a same-named variable each invocation, so
    // wrap each in its own block to switch categories within the function.
    {
        SCOPED_CATEGORY("Sprite Animation");
        outProps.push_back(Property(DatumType::Asset, "Animations", this, &mPlayback.mAnimations, 1, HandlePropChange,
                                    int32_t(SpriteAnimation::GetStaticType())).MakeVector());
        outProps.push_back(Property(DatumType::String, "Default Animation", this, &mPlayback.mDefaultAnimation));
        outProps.push_back(Property(DatumType::Bool, "Auto Play", this, &mPlayback.mAutoPlay));
        outProps.push_back(Property(DatumType::Bool, "Loop Override", this, &mPlayback.mLoopOverride));
        outProps.push_back(Property(DatumType::Float, "Playback Speed", this, &mPlayback.mPlaybackSpeed));
    }

    {
        SCOPED_CATEGORY("Material Target");
        outProps.push_back(Property(DatumType::Asset, "Material", this, &mMaterial, 1, HandlePropChange,
                                    int32_t(Material::GetStaticType())));
        outProps.push_back(Property(DatumType::Bool, "Affect Diffuse",  this, &mAffectDiffuse));
        outProps.push_back(Property(DatumType::Bool, "Affect Alpha",    this, &mAffectAlpha));
        outProps.push_back(Property(DatumType::Bool, "Affect Emission", this, &mAffectEmission));

        // MaterialLite slot indices.
        outProps.push_back(Property(DatumType::Integer, "Diffuse Slot",  this, &mDiffuseSlot));
        outProps.push_back(Property(DatumType::Integer, "Alpha Slot",    this, &mAlphaSlot));
        outProps.push_back(Property(DatumType::Integer, "Emission Slot", this, &mEmissionSlot));
        outProps.push_back(Property(DatumType::Integer, "Atlas UV Map",  this, &mAtlasUvMap));

        // Full Material named-parameter overrides (ignored by MaterialLite).
        outProps.push_back(Property(DatumType::String, "Diffuse Param Name",  this, &mDiffuseParamName));
        outProps.push_back(Property(DatumType::String, "Alpha Param Name",    this, &mAlphaParamName));
        outProps.push_back(Property(DatumType::String, "Emission Param Name", this, &mEmissionParamName));

        outProps.push_back(Property(DatumType::Bool,   "Push UV Rect",        this, &mPushUVRect));
        outProps.push_back(Property(DatumType::String, "UV Rect Param Name",  this, &mUVRectParamName));
    }

    {
        SCOPED_CATEGORY("Editor");
        outProps.push_back(Property(DatumType::Bool, "Editor Preview", this, &mEditorPreview, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Bool, "Editor Play",    this, &mEditorPlayButton, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Bool, "Editor Stop",    this, &mEditorStopButton, 1, HandlePropChange));
    }
}

void AnimatedSprite3D::Create()
{
    Node3D::Create();
}

void AnimatedSprite3D::Destroy()
{
    Node3D::Destroy();
}

void AnimatedSprite3D::Start()
{
    Node3D::Start();

    mPlayback.RebuildRegistry();

    if (mPlayback.mAutoPlay && !mPlayback.mDefaultAnimation.empty())
    {
        PlayAnimation(mPlayback.mDefaultAnimation);
    }
}

void AnimatedSprite3D::Tick(float deltaTime)
{
    Node3D::Tick(deltaTime);

    bool frameChanged = false;
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
        ApplyCurrentFrameToMaterial();
    }
}

void AnimatedSprite3D::EditorTick(float deltaTime)
{
    Node3D::EditorTick(deltaTime);

    if (mPlayback.mRegistryDirty)
    {
        mPlayback.RebuildRegistry();
        ApplyCurrentFrameToMaterial();
    }

    if (mEditorPreview)
    {
        bool frameChanged = false;
        mPlayback.Tick(deltaTime,
            [&frameChanged](int32_t) { frameChanged = true; },
            nullptr);
        if (frameChanged) ApplyCurrentFrameToMaterial();
    }
}

void AnimatedSprite3D::Play()
{
    mPlayback.Play();
    if (mPlayback.mPlaying) ApplyCurrentFrameToMaterial();
}

void AnimatedSprite3D::Pause()
{
    mPlayback.Pause();
}

void AnimatedSprite3D::Stop()
{
    mPlayback.Stop();
    ApplyCurrentFrameToMaterial();
}

void AnimatedSprite3D::PlayAnimation(const std::string& name)
{
    mPlayback.PlayAnimation(name,
        [this](const std::string& n)
        {
            std::vector<Datum> args;
            args.push_back(Datum(n));
            EmitSignal("OnAnimationStart", args);
        });
    ApplyCurrentFrameToMaterial();
}

void AnimatedSprite3D::SetFrame(int32_t frameIndex)
{
    if (mPlayback.SetFrame(frameIndex))
    {
        std::vector<Datum> args;
        args.push_back(Datum(mPlayback.mCurrentFrame));
        EmitSignal("OnFrameChanged", args);
        ApplyCurrentFrameToMaterial();
    }
}

bool AnimatedSprite3D::AnimateTo(int32_t targetFrame, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    const bool ok = mPlayback.AnimateTo(targetFrame, pauseOnFinished, onFinished);
    ApplyCurrentFrameToMaterial();
    return ok;
}

bool AnimatedSprite3D::AnimateToProgress(float progress, bool pauseOnFinished, const ScriptFunc& onFinished)
{
    const bool ok = mPlayback.AnimateToProgress(progress, pauseOnFinished, onFinished);
    ApplyCurrentFrameToMaterial();
    return ok;
}

void AnimatedSprite3D::SetSpeed(float speed)
{
    mPlayback.mPlaybackSpeed = speed;
}

void AnimatedSprite3D::SetMaterial(Material* mat)
{
    mMaterial = mat;
    ApplyCurrentFrameToMaterial();
}

Material* AnimatedSprite3D::GetMaterial() const
{
    return mMaterial.Get<Material>();
}

void AnimatedSprite3D::ApplyCurrentFrameToMaterial()
{
    Material* mat = mMaterial.Get<Material>();
    if (mat == nullptr) return;

    Texture* tex = mPlayback.GetCurrentTexture();
    if (tex == nullptr) return;

    // MaterialLite uses indexed slots (0..3), not named params. Full Material
    // uses named shader params. Dispatch based on type so the same node works
    // with both — the inspector exposes both sets of fields and only the
    // relevant one is consulted at runtime.
    MaterialLite* lite = mat->As<MaterialLite>();
    if (lite != nullptr)
    {
        if (mAffectDiffuse)  lite->SetTexture(mDiffuseSlot,  tex);
        if (mAffectAlpha)    lite->SetTexture(mAlphaSlot,    tex);
        if (mAffectEmission) lite->SetTexture(mEmissionSlot, tex);

        // For atlas-mode animations, push the cell UV transform into one of
        // MaterialLite's two UV maps. Discrete-mode animations evaluate to
        // scale=(1,1) offset=(0,0) which is a no-op — safe to always set.
        // Note: this affects EVERY texture slot bound to this UV map. If the
        // material has static textures that shouldn't be cropped, route those
        // to the OTHER UV map (MaterialLite has 2: 0 and 1).
        if (mAtlasUvMap >= 0 && mAtlasUvMap < 2)
        {
            lite->SetUvScale(mPlayback.GetCurrentUVScale(), mAtlasUvMap);
            lite->SetUvOffset(mPlayback.GetCurrentUVOffset(), mAtlasUvMap);
        }
        return;
    }

    if (mAffectDiffuse  && !mDiffuseParamName.empty())  mat->SetTextureParameter(mDiffuseParamName,  tex);
    if (mAffectAlpha    && !mAlphaParamName.empty())    mat->SetTextureParameter(mAlphaParamName,    tex);
    if (mAffectEmission && !mEmissionParamName.empty()) mat->SetTextureParameter(mEmissionParamName, tex);

    if (mPushUVRect && !mUVRectParamName.empty())
    {
        glm::vec4 uvRect = mPlayback.GetCurrentUVRect();
        mat->SetVectorParameter(mUVRectParamName, uvRect);
    }
}
