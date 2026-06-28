#pragma once

#include "Nodes/3D/Mesh3d.h"
#include "AssetRef.h"
#include "Vertex.h"
// std::vector<Animation> mBoundExternalAnims below needs the full Animation
// type, so we can't fall back on the forward declarations the file used to
// rely on for AnimEvent / Channel / Animation.
#include "Assets/SkeletalMesh.h"

class BoneMaskAsset;

enum class BoneInfluenceMode
{
    One,
    Four,
    Num
};

enum class AnimationUpdateMode
{
    AlwaysUpdateTimeAndBones,
    AlwaysUpdateTime,
    OnlyUpdateWhenRendered,

    Count
};

// How a slot's animation contributes to the per-bone blend.
// Replace : standard weighted lerp into the accumulator (default).
// Additive: the slot's pose is interpreted as a delta from bind pose;
//           that delta is scaled by mWeight and composed on top of the
//           accumulator. Used for hit reactions, recoil, breathing,
//           lean-into-turn — anything you want to layer on a base anim.
enum class AnimLayerMode
{
    Replace,
    Additive,

    Count
};

class SkeletalMesh;
struct AnimEvent;
struct Channel;
struct Animation;

struct ActiveAnimation
{
    std::string mName;
    float mTime = 0.0f;
    float mSpeed = 1.0f;
    float mWeight = 0.0f;
    int32_t mSlot = 0;
    bool mLoop = false;

    // Optional bone mask. When valid, only bones marked in the mask's
    // resolved bitset receive this slot's contribution; the rest stay at
    // whatever the lower slots (or bind pose) provided.
    AssetRef mBoneMask;

    // Weight fade. Engaged when mWeightFadeRate != 0; on each tick mWeight
    // advances toward mWeightTarget by mWeightFadeRate * dt and clamps. When
    // the target is 0 and reached, the slot self-removes.
    float mWeightTarget = -1.0f;
    float mWeightFadeRate = 0.0f;

    AnimLayerMode mLayerMode = AnimLayerMode::Replace;
};

struct QueuedAnimation
{
    std::string mName;
    std::string mDependentAnim;
    float mTime = 0.0f;
    float mSpeed = 1.0f;;
    float mWeight = 0.0f;
    bool mLoop = false;
    int32_t mSlot = -1;
};

typedef void(*AnimEventHandlerFP)(const AnimEvent& animEvent);

class POLYPHASE_API SkeletalMesh3D : public Mesh3D
{
public:

    DECLARE_NODE(SkeletalMesh3D, Mesh3D);

    SkeletalMesh3D();
    ~SkeletalMesh3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    SkeletalMeshCompResource* GetResource();

    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    virtual bool IsStaticMesh3D() const override;
    virtual bool IsSkeletalMesh3D() const override;

    void SetSkeletalMesh(SkeletalMesh* skeletalMesh);
    SkeletalMesh* GetSkeletalMesh();

    void PlayAnimation(const char* animName, bool loop, float speed = 1.0f, float weight = 1.0f, int32_t priority = -1);
    // Start `animName` on `slot`, restricted to the bones marked by `mask`.
    // mask=nullptr is equivalent to PlayAnimation (full body). The slot's
    // weight is the maximum contribution against lower slots; weight=1 and
    // mask=UpperBody means upper-body bones are 100% driven by this clip
    // while lower bones stay on whatever slot 0 (or bind pose) provided.
    void PlayAnimationMasked(const char* animName, int32_t slot, BoneMaskAsset* mask,
                             bool loop, float speed = 1.0f, float weight = 1.0f);
    // Start `animName` on `slot` as an additive layer — the slot's pose is
    // interpreted as a delta from bind pose and composed on top of lower
    // slots. Mask gates which bones receive the delta. Use for hit reactions,
    // recoil, breathing, lean-into-turn over a base run/idle.
    void PlayAnimationAdditive(const char* animName, int32_t slot, BoneMaskAsset* mask,
                               bool loop, float speed = 1.0f, float weight = 1.0f);
    // Replace the bone mask on an already-playing slot. Pass nullptr to clear.
    void SetSlotMask(int32_t slot, BoneMaskAsset* mask);
    void SetSlotLayerMode(int32_t slot, AnimLayerMode mode);
    // Snap a slot's weight without easing. Clamped to [0, 1].
    void SetSlotWeight(int32_t slot, float weight);
    // Fade a slot's weight from its current value to `targetWeight` over
    // `seconds` (linear). If targetWeight == 0 and the fade completes, the
    // slot self-removes.
    void FadeSlotWeight(int32_t slot, float targetWeight, float seconds);
    void QueueAnimation(const char* animName, bool loop, const char* targetAnim = nullptr, float speed = 1.0f, float weight = 1.0f, int32_t priority = -1);
    void StopAnimation(const char* animName, bool cancelQueued = false);
    void StopAllAnimations(bool cancelQueued = false);
    void CancelQueuedAnimation(const char* animName);
    void CancelAllQueuedAnimations();
    bool IsAnimationPlaying(const char* animName);
    void ResetAnimation();
    float GetAnimationSpeed() const;
    void SetAnimationSpeed(float speed);

    void SetAnimationPaused(bool paused);
    bool IsAnimationPaused() const;

    void SetInheritPose(bool inherit);
    bool IsInheritPoseEnabled() const;

    void SetBoundsRadiusOverride(float radius);
    float GetBoundsRadiusOverride() const;

    bool HasAnimatedThisFrame() const;

    ActiveAnimation* FindActiveAnimation(const char* animName);
    std::vector<ActiveAnimation>& GetActiveAnimations();

    QueuedAnimation* FindQueuedAnimation(const char* animName, const char* dependName = nullptr);
    std::vector<QueuedAnimation>& GetQueuedAnimations();

    glm::mat4 GetBoneTransform(const std::string& name) const;
    glm::vec3 GetBonePosition(const std::string& name) const;
    glm::quat GetBoneRotationQuat(const std::string& name) const;
    glm::vec3 GetBoneRotationEuler(const std::string& name) const;
    glm::vec3 GetBoneScale(const std::string& name) const;

    glm::mat4 GetBoneTransform(int32_t index) const;
    glm::vec3 GetBonePosition(int32_t boneIndex) const;
    glm::quat GetBoneRotationQuat(int32_t boneIndex) const;
    glm::vec3 GetBoneRotationEuler(int32_t boneIndex) const;
    glm::vec3 GetBoneScale(int32_t boneIndex) const;

    void SetBoneTransform(int32_t boneIndex, const glm::mat4& transform);
    void SetBonePosition(int32_t boneIndex, glm::vec3 position);
    void SetBoneRotation(int32_t boneIndex, glm::vec3 rotation);
    void SetBoneScale(int32_t boneIndex, glm::vec2 scale);

    uint32_t GetNumBones() const;
    BoneInfluenceMode GetBoneInfluenceMode() const;

    AnimationUpdateMode GetAnimationUpdateMode() const;
    void SetAnimationUpdateMode(AnimationUpdateMode mode);

    Vertex* GetSkinnedVertices();
    uint32_t GetNumSkinnedVertices();

    virtual Material* GetMaterial() override;
    virtual void Render() override;

    // Per-section material slot accessors. A "slot" maps 1:1 onto
    // SkeletalMesh::GetSection(i). Resolution order for a slot:
    //   1. component override (mSectionMaterialOverrides[slot])
    //   2. section's own material (SkeletalMesh::GetSection(slot).mMaterial)
    //   3. legacy mMaterialOverride (whole-mesh override)
    //   4. legacy mesh-default mMaterial
    //   5. renderer default material
    uint32_t GetNumMaterialSlots() const;
    Material* GetMaterialSlot(uint32_t slot) const;
    void SetMaterialSlot(uint32_t slot, Material* material);
    int32_t FindMaterialSlot(const std::string& sectionName) const;

    void UpdateAnimation(float deltaTime, bool updateBones);

    virtual Bounds GetLocalBounds() const override;

    int32_t FindBoneIndex(const std::string& name) const;

    void SetAnimEventHandler(AnimEventHandlerFP handler);
    AnimEventHandlerFP GetAnimEventHandler();
    void SetScriptAnimEventHandler(const ScriptFunc& func);

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void TickCommon(float deltaTime);

    glm::vec3 InterpolateScale(float time, const Channel& channel);
    glm::quat InterpolateRotation(float time, const Channel& channel);
    glm::vec3 InterpolatePosition(float time, const Channel& channel);
    void DetectTriggeredAnimEvents(
        const Animation& animation,
        float prevTickTime,
        float tickTime,
        float animationSpeed,
        std::vector<AnimEvent>& outEvents);

    uint32_t FindScaleIndex(float time, const Channel& channel);
    uint32_t FindRotationIndex(float time, const Channel& channel);
    uint32_t FindPositionIndex(float time, const Channel& channel);

    void UpdateAttachedChildren(float deltaTime);
    void CpuSkinVertices();

    // External SkeletalAnimationAsset references. Resolution order in
    // FindAnimation: embedded mesh animations first (preserves existing
    // PlayAnimation behaviour), then animation-lookup-mesh chain, then
    // bound external assets. Channels in external assets are keyed by bone
    // NAME and get resolved into target-mesh bone indices lazily into the
    // mBoundExternalAnims cache below.
    const std::vector<SkeletalAnimationRef>& GetAnimationAssets() const { return mAnimationAssets; }
    std::vector<SkeletalAnimationRef>& GetAnimationAssetsMutable();
    void AddAnimationAsset(class SkeletalAnimationAsset* asset);
    void RemoveAnimationAsset(class SkeletalAnimationAsset* asset);

    const Animation* FindAnimation(const char* animName);
    void InvalidateAnimationBindings();

    SkeletalMeshRef mSkeletalMesh;
    std::vector<glm::mat4> mBoneMatrices;
    std::vector<Vertex> mSkinnedVertices; // Used by CPU skinning only.
    std::vector<MaterialRef> mSectionMaterialOverrides;
    std::vector<SkeletalAnimationRef> mAnimationAssets;

    // Lazy cache of external animation clips with channel bone indices
    // resolved against the currently assigned target mesh. Rebuilt on
    // first FindAnimation after the mesh or asset list changes.
    std::vector<Animation> mBoundExternalAnims;
    bool mAnimBindingsValid = false;

    ScriptableFP<AnimEventHandlerFP> mAnimEventHandler;
    std::string mDefaultAnimation;
    float mAnimationSpeed = 1.0f;
    std::vector<ActiveAnimation> mActiveAnimations;
    std::vector<QueuedAnimation> mQueuedAnimations;
    float mBoundsRadiusOverride = 0.0f;
    bool mAnimationPaused;
    bool mRevertToBindPose;
    bool mInheritPose;
    bool mHasAnimatedThisFrame;

    BoneInfluenceMode mBoneInfluenceMode;
    AnimationUpdateMode mAnimationUpdateMode;

    // Graphics Resource
    SkeletalMeshCompResource mResource;
};