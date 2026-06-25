#pragma once

#include "Asset.h"
#include "Factory.h"
#include "Assets/SkeletalMesh.h"

#include <string>
#include <vector>

class SkeletalMesh;

// Bone-animation channel keyed by source bone NAME (not bone index).
// The bound runtime resolves the name into a target-mesh bone index once
// per (asset, target-mesh) pair and caches the result on SkeletalMesh3D —
// see BoundSkeletalAnimation.
struct SkeletalAnimationChannel
{
    std::string mBoneName;
    int32_t mSourceBoneIndex = -1;
    std::vector<PositionKey> mPositionKeys;
    std::vector<RotationKey> mRotationKeys;
    std::vector<ScaleKey> mScaleKeys;
};

class POLYPHASE_API SkeletalAnimationAsset : public Asset
{
public:

    DECLARE_ASSET(SkeletalAnimationAsset, Asset);

    SkeletalAnimationAsset();
    ~SkeletalAnimationAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    const std::string& GetClipName() const { return mClipName; }
    void SetClipName(const std::string& name) { mClipName = name; }

    float GetDuration() const { return mDuration; }
    float GetTicksPerSecond() const { return mTicksPerSecond; }
    float GetDurationSeconds() const;
    void SetDuration(float duration) { mDuration = duration; }
    void SetTicksPerSecond(float tps) { mTicksPerSecond = tps; }

    const std::vector<SkeletalAnimationChannel>& GetChannels() const { return mChannels; }
    std::vector<SkeletalAnimationChannel>& GetChannelsMutable() { return mChannels; }

    const std::vector<AnimEventTrack>& GetEventTracks() const { return mEventTracks; }
    std::vector<AnimEventTrack>& GetEventTracksMutable() { return mEventTracks; }

    const std::string& GetSourceRigName() const { return mSourceRigName; }
    void SetSourceRigName(const std::string& name) { mSourceRigName = name; }

    const std::vector<std::string>& GetSourceBoneNames() const { return mSourceBoneNames; }
    std::vector<std::string>& GetSourceBoneNamesMutable() { return mSourceBoneNames; }

    const std::vector<int32_t>& GetSourceParentIndices() const { return mSourceParentIndices; }
    std::vector<int32_t>& GetSourceParentIndicesMutable() { return mSourceParentIndices; }

    const std::vector<glm::mat4>& GetSourceBindPose() const { return mSourceBindPose; }
    std::vector<glm::mat4>& GetSourceBindPoseMutable() { return mSourceBindPose; }

    // Editor-time helper. Copies an embedded Animation off a SkeletalMesh into
    // this asset, converting channel mBoneIndex into bone names + capturing
    // the source skeleton metadata for later retargeting work.
    void CopyFromEmbedded(const Animation& embedded, const SkeletalMesh* sourceMesh);

#if EDITOR
    // Static convenience: parse animations from an .fbx/.glb/.gltf/.dae file
    // without requiring a render mesh, producing one SkeletalAnimationAsset
    // descriptor per aiAnimation in the scene. The caller decides whether to
    // register each result as an asset (see ActionManager::ImportAnimations).
    // Returns the number of clips populated into outAssets (matches outNames).
    static uint32_t ParseAnimationsFromFile(
        const std::string& path,
        std::vector<SkeletalAnimationAsset>& outAssets,
        std::vector<std::string>& outNames);

    enum class RetargetMode : uint8_t
    {
        // Tier 1: pass keyframes through verbatim, just rename the channel's
        // bone from src avatar's slot name to the target avatar's. Works for
        // same/similar skeletons that only differ in naming convention
        // (Mixamo-to-Mixamo, ARP-to-ARP-but-renamed).
        NameRemap,

        // Tier 2: rotations are projected through (source bind, target bind)
        // so the resulting clip reproduces the source motion on a differently-
        // proportioned target rig. Requires both avatars to have a reference
        // mesh with a real bind pose; falls back to NameRemap behaviour for
        // slots where bind pose isn't available.
        ReferencePose,
    };

    // Bake a target-compatible clip from a source clip and a humanoid avatar
    // pair. The returned clip is fully populated but NOT registered with
    // AssetManager — caller does that. Pass nullptr for the optional outErr
    // string if you don't need diagnostics.
    static SkeletalAnimationAsset Retarget(
        const SkeletalAnimationAsset& srcClip,
        const class HumanoidAvatarAsset& srcAvatar,
        const class HumanoidAvatarAsset& dstAvatar,
        RetargetMode mode,
        std::string* outDiagnostics = nullptr);
#endif

protected:

    std::string mClipName;
    float mDuration = 0.0f;
    float mTicksPerSecond = 1000.0f;

    std::vector<SkeletalAnimationChannel> mChannels;
    std::vector<AnimEventTrack> mEventTracks;

    // Source skeleton metadata. Populated when the asset is extracted from a
    // SkeletalMesh import; used by humanoid retargeting in later PRs and by
    // the bound-channel resolver as a sanity reference. Optional for clips
    // imported from animation-only files where no rig is available.
    std::string mSourceRigName;
    std::vector<std::string> mSourceBoneNames;
    std::vector<int32_t> mSourceParentIndices;
    std::vector<glm::mat4> mSourceBindPose;
};
