#include "Nodes/Widgets/TransformAnimationWidget.h"
#include "Nodes/Widgets/Widget.h"
#include "Assets/TransformAnimationAsset.h"
#include "SignalBus.h"
#include "Log.h"
#include "Utilities.h"

#if EDITOR
#include "imgui.h"
#endif

FORCE_LINK_DEF(TransformAnimationWidget);
DEFINE_NODE(TransformAnimationWidget, Node);

TransformAnimationWidget::TransformAnimationWidget()
{
    mName = "TransformAnimationWidget";
}

TransformAnimationWidget::~TransformAnimationWidget()
{
}

void TransformAnimationWidget::Create()
{
    Node::Create();
}

void TransformAnimationWidget::Destroy()
{
    Node::Destroy();
}

void TransformAnimationWidget::Tick(float deltaTime)
{
    Node::Tick(deltaTime);

    if (!mPlaying || mPaused)
        return;

    float assetRate = 1.0f;
    bool loop = mLoop;

    TransformAnimationAsset* asset = mAnimation.Get<TransformAnimationAsset>();
    if (asset != nullptr)
    {
        assetRate = asset->GetPlayRate();
        loop = loop || asset->IsLooping();
    }

    mCurrentTime += deltaTime * mPlayRate * assetRate;

    float duration = GetActiveDuration();

    if (duration > 0.0f && mCurrentTime >= duration)
    {
        if (loop)
        {
            while (mCurrentTime >= duration)
            {
                mCurrentTime -= duration;
            }
            mLastFiredKeyframe = -1;
        }
        else
        {
            mCurrentTime = duration;
            mPlaying = false;
        }
    }

    EvaluateAtTime(mCurrentTime);
}

void TransformAnimationWidget::EditorTick(float deltaTime)
{
    Node::EditorTick(deltaTime);

#if EDITOR
    if (!mEditorPreviewing)
        return;

    float duration = GetActiveDuration();

    mCurrentTime += deltaTime * mPlayRate;

    if (duration > 0.0f && mCurrentTime >= duration)
    {
        if (mLoop)
        {
            while (mCurrentTime >= duration) mCurrentTime -= duration;
            mLastFiredKeyframe = -1;
        }
        else
        {
            mCurrentTime = duration;
            StopPreview();
            return;
        }
    }

    EvaluateAtTime(mCurrentTime);
#endif
}

void TransformAnimationWidget::Start()
{
    Node::Start();
    if (mPlayOnStart)
    {
        Play();
    }
}

void TransformAnimationWidget::Stop()
{
    StopPlayback();
    Node::Stop();
}

#if EDITOR
void TransformAnimationWidget::StartPreview()
{
    Widget* target = GetTargetWidget();
    if (target == nullptr)
    {
        LogWarning("TransformAnimationWidget::StartPreview - no Target assigned");
        return;
    }

    mEditorPreviewSavedPos   = target->GetPosition();
    mEditorPreviewSavedRot   = target->GetRotation();
    mEditorPreviewSavedScale = target->GetScale();
    mEditorPreviewSnapshotValid = true;

    mCurrentTime = 0.0f;
    mLastFiredKeyframe = -1;
    mEditorPreviewing = true;
}

void TransformAnimationWidget::StopPreview()
{
    mEditorPreviewing = false;
    mCurrentTime = 0.0f;
    mLastFiredKeyframe = -1;

    if (mEditorPreviewSnapshotValid)
    {
        Widget* target = GetTargetWidget();
        if (target != nullptr)
        {
            target->SetPosition(mEditorPreviewSavedPos);
            target->SetRotation(mEditorPreviewSavedRot);
            target->SetScale(mEditorPreviewSavedScale);
        }
        mEditorPreviewSnapshotValid = false;
    }
}
#endif

void TransformAnimationWidget::GatherProperties(std::vector<Property>& outProps)
{
    Node::GatherProperties(outProps);

    SCOPED_CATEGORY("TransformAnimation");

    outProps.push_back(Property(DatumType::Asset, "Animation", this, &mAnimation, 1, nullptr, int32_t(TransformAnimationAsset::GetStaticType())));
    outProps.push_back(Property(DatumType::Widget, "Target", this, &mTarget));
    outProps.push_back(Property(DatumType::Bool, "Play On Start", this, &mPlayOnStart));
    outProps.push_back(Property(DatumType::Bool, "Loop", this, &mLoop));
    outProps.push_back(Property(DatumType::Float, "Play Rate", this, &mPlayRate));

#if EDITOR
    static bool sDummy = false;
    outProps.push_back(Property(DatumType::Bool, "Add Keyframe To Asset", this, &sDummy));
    outProps.push_back(Property(DatumType::Bool, "Add Inline Keyframe",   this, &sDummy));
    outProps.push_back(Property(DatumType::Bool, "Preview",               this, &sDummy));
#endif

    outProps.push_back(Property(DatumType::TransformKeyframe, "Inline Keyframes", this, &mInlineKeyframes).MakeVector());
}

const char* TransformAnimationWidget::GetTypeName() const
{
    return "TransformAnimationWidget";
}

void TransformAnimationWidget::Play()
{
    mPlaying = true;
    mPaused = false;
    mLastFiredKeyframe = -1;

    float duration = GetActiveDuration();
    if (duration > 0.0f && mCurrentTime >= duration)
    {
        mCurrentTime = 0.0f;
    }
}

void TransformAnimationWidget::Play(TransformAnimationAsset* asset)
{
    SetAnimation(asset);
    Play();
}

void TransformAnimationWidget::Pause()
{
    mPaused = true;
}

void TransformAnimationWidget::StopPlayback()
{
    mPlaying = false;
    mPaused = false;
    mCurrentTime = 0.0f;
    mLastFiredKeyframe = -1;
}

void TransformAnimationWidget::SetAnimation(TransformAnimationAsset* asset)
{
    mAnimation = asset;
}

TransformAnimationAsset* TransformAnimationWidget::GetAnimation() const
{
    return mAnimation.Get<TransformAnimationAsset>();
}

void TransformAnimationWidget::SetKeyframes(const std::vector<TransformKeyframe>& keyframes)
{
    mInlineKeyframes = keyframes;
}

void TransformAnimationWidget::SetTime(float time)
{
    mCurrentTime = time;
    EvaluateAtTime(mCurrentTime);
}

float TransformAnimationWidget::GetDuration() const
{
    return GetActiveDuration();
}

float TransformAnimationWidget::GetProgress() const
{
    float duration = GetActiveDuration();
    return (duration > 0.0f) ? glm::clamp(mCurrentTime / duration, 0.0f, 1.0f) : 0.0f;
}

void TransformAnimationWidget::SetTargetWidget(Widget* target)
{
    mTarget = ResolveWeakPtr<Node>(static_cast<Node*>(target));
}

Widget* TransformAnimationWidget::GetTargetWidget() const
{
    Node* node = mTarget.Get();
    return (node != nullptr && node->IsWidget()) ? static_cast<Widget*>(node) : nullptr;
}

void TransformAnimationWidget::ApplyKeyframe(const TransformKeyframe& kf)
{
    ApplyTransformKeyframeToNode(mTarget.Get(), kf);
}

TransformKeyframe TransformAnimationWidget::SampleNow() const
{
    return SampleSource(mCurrentTime);
}

TransformKeyframe TransformAnimationWidget::SampleSource(float time) const
{
    TransformAnimationAsset* asset = mAnimation.Get<TransformAnimationAsset>();
    if (asset != nullptr)
    {
        return asset->Sample(time);
    }

    if (mInlineKeyframes.empty())
        return TransformKeyframe();

    const std::vector<TransformKeyframe>& kfs = mInlineKeyframes;

    if (kfs.size() == 1 || time <= kfs[0].mTime)
        return kfs[0];
    if (time >= kfs.back().mTime)
        return kfs.back();

    size_t low = 0;
    size_t high = kfs.size() - 1;
    for (size_t i = 0; i + 1 < kfs.size(); ++i)
    {
        if (kfs[i].mTime <= time && time <= kfs[i + 1].mTime)
        {
            low = i;
            high = i + 1;
            break;
        }
    }

    float span = kfs[high].mTime - kfs[low].mTime;
    float t = (span > 0.0f) ? glm::clamp((time - kfs[low].mTime) / span, 0.0f, 1.0f) : 0.0f;
    return TransformKeyframe::Lerp(kfs[low], kfs[high], t);
}

const std::vector<TransformKeyframe>* TransformAnimationWidget::GetActiveKeyframes() const
{
    TransformAnimationAsset* asset = mAnimation.Get<TransformAnimationAsset>();
    if (asset != nullptr)
    {
        return &asset->GetKeyframes();
    }
    if (!mInlineKeyframes.empty())
    {
        return &mInlineKeyframes;
    }
    return nullptr;
}

float TransformAnimationWidget::GetActiveDuration() const
{
    TransformAnimationAsset* asset = mAnimation.Get<TransformAnimationAsset>();
    if (asset != nullptr)
    {
        return asset->GetDuration();
    }
    if (!mInlineKeyframes.empty())
    {
        return mInlineKeyframes.back().mTime;
    }
    return 0.0f;
}

TransformKeyframe TransformAnimationWidget::MakeKeyframeFromTarget() const
{
    TransformKeyframe kf;
    Widget* target = GetTargetWidget();
    if (target != nullptr)
    {
        glm::vec2 pos2 = target->GetPosition();
        glm::vec2 scl2 = target->GetScale();
        kf.mPosition = glm::vec3(pos2.x, pos2.y, 0.0f);
        kf.mScale    = glm::vec3(scl2.x, scl2.y, 1.0f);
        kf.mRotation = glm::quat(glm::radians(glm::vec3(0.0f, 0.0f, target->GetRotation())));
    }
    return kf;
}

void TransformAnimationWidget::AddKeyframeFromTarget()
{
    TransformAnimationAsset* asset = GetAnimation();
    if (asset == nullptr)
    {
        LogWarning("TransformAnimationWidget::AddKeyframeFromTarget - no Animation asset assigned");
        return;
    }

    TransformKeyframe kf = MakeKeyframeFromTarget();
    if (asset->GetKeyframeCount() > 0)
    {
        kf.mTime = asset->GetKeyframe(asset->GetKeyframeCount() - 1).mTime + 1.0f;
    }
    asset->AddKeyframe(kf);

    if (kf.mTime > asset->GetDuration())
    {
        asset->SetDuration(kf.mTime);
    }

#if EDITOR
    asset->SetDirtyFlag();
#endif
}

void TransformAnimationWidget::AddInlineKeyframeFromTarget()
{
    TransformKeyframe kf = MakeKeyframeFromTarget();
    if (!mInlineKeyframes.empty())
    {
        kf.mTime = mInlineKeyframes.back().mTime + 1.0f;
    }
    mInlineKeyframes.push_back(kf);
}

#if EDITOR
bool TransformAnimationWidget::DrawCustomProperty(Property& prop)
{
    if (prop.mName == "Add Keyframe To Asset")
    {
        TransformAnimationAsset* asset = GetAnimation();
        bool hasTarget = (GetTargetWidget() != nullptr);
        bool hasAsset = (asset != nullptr);

        if (!hasTarget || !hasAsset) ImGui::BeginDisabled();
        if (ImGui::Button("Add Keyframe To Asset", ImVec2(-1, 0)))
        {
            AddKeyframeFromTarget();
        }
        if (!hasTarget || !hasAsset) ImGui::EndDisabled();

        if (!hasTarget) ImGui::TextDisabled("  (assign a Target Widget)");
        else if (!hasAsset) ImGui::TextDisabled("  (assign an Animation asset)");
        return true;
    }

    if (prop.mName == "Add Inline Keyframe")
    {
        bool hasTarget = (GetTargetWidget() != nullptr);
        if (!hasTarget) ImGui::BeginDisabled();
        if (ImGui::Button("Add Inline Keyframe", ImVec2(-1, 0)))
        {
            AddInlineKeyframeFromTarget();
        }
        if (!hasTarget) ImGui::EndDisabled();

        if (!hasTarget) ImGui::TextDisabled("  (assign a Target Widget)");
        return true;
    }

    if (prop.mName == "Preview")
    {
        bool hasTarget = (GetTargetWidget() != nullptr);
        bool hasKeyframes = (GetActiveKeyframes() != nullptr);
        bool ready = hasTarget && hasKeyframes;

        const char* label = mEditorPreviewing ? "Stop Preview" : "Preview";
        if (!ready && !mEditorPreviewing) ImGui::BeginDisabled();
        if (ImGui::Button(label, ImVec2(-1, 0)))
        {
            if (mEditorPreviewing)  StopPreview();
            else                    StartPreview();
        }
        if (!ready && !mEditorPreviewing) ImGui::EndDisabled();

        if (mEditorPreviewing)
        {
            float dur = GetActiveDuration();
            ImGui::Text("  t = %.2f / %.2f", mCurrentTime, dur);
        }
        else if (!hasTarget)        ImGui::TextDisabled("  (assign a Target Widget)");
        else if (!hasKeyframes)     ImGui::TextDisabled("  (no keyframes — add some first)");
        return true;
    }

    return false;
}
#endif

void TransformAnimationWidget::EvaluateAtTime(float time)
{
    TransformKeyframe kf = SampleSource(time);
    ApplyKeyframe(kf);

    const std::vector<TransformKeyframe>* keyframes = GetActiveKeyframes();
    if (keyframes != nullptr)
    {
        for (int32_t i = mLastFiredKeyframe + 1; i < (int32_t)keyframes->size(); ++i)
        {
            const TransformKeyframe& candidate = (*keyframes)[i];
            if (candidate.mTime <= time)
            {
                if (!candidate.mSignal.empty())
                {
                    GetSignalBus()->Emit(candidate.mSignal, {});
                }
                mLastFiredKeyframe = i;
            }
            else
            {
                break;
            }
        }
    }
}
