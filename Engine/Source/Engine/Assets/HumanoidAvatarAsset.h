#pragma once

#include "Asset.h"
#include "AssetRef.h"
#include "Factory.h"

#include <string>
#include <vector>

class SkeletalMesh;

// Standard humanoid bone slots. Matches Unity's small-set Humanoid avatar.
// Additions go at the end so older HumanoidAvatarAssets keep loading; the
// SaveStream/LoadStream version-gate handles size changes.
enum class HumanoidBone : uint8_t
{
    Hips,
    Spine,
    Chest,
    Neck,
    Head,
    LeftShoulder,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,
    RightShoulder,
    RightUpperArm,
    RightLowerArm,
    RightHand,
    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,
    LeftToes,
    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    RightToes,

    Count
};

POLYPHASE_API const char* HumanoidBoneName(HumanoidBone bone);

class POLYPHASE_API HumanoidAvatarAsset : public Asset
{
public:

    DECLARE_ASSET(HumanoidAvatarAsset, Asset);

    HumanoidAvatarAsset();
    ~HumanoidAvatarAsset();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    // The SkeletalMesh this avatar describes. Optional — clips can be retargeted
    // without a reference mesh if both source and target avatars are filled in
    // by hand, but having a mesh lets the editor "Auto-map" button do its job
    // and lets PR5's retarget bake read reference-pose matrices.
    SkeletalMesh* GetReferenceMesh() const;
    void SetReferenceMesh(SkeletalMesh* mesh);

    const std::string& GetBoneName(HumanoidBone slot) const;
    void SetBoneName(HumanoidBone slot, const std::string& boneName);

    // Try to fill empty slots from the reference mesh's bones by matching the
    // built-in alias tables (Mixamo, ARP, common defaults). Existing non-empty
    // mappings are left alone unless overwriteAll is true. Returns the number
    // of slots that were filled.
    uint32_t AutoMap(bool overwriteAll = false);

    // Convenience: validate that every slot maps to a bone present in the
    // reference mesh. Fills outMissing / outUnknownBones for editor display.
    bool Validate(std::vector<HumanoidBone>* outMissing,
                  std::vector<std::string>* outUnknownBones) const;

    // Reference (bind-pose) local transform for the given humanoid slot,
    // sampled from the reference mesh. Identity when the slot is unmapped
    // or there is no reference mesh.
    glm::mat4 GetReferenceLocalBind(HumanoidBone slot) const;

protected:

    SkeletalMeshRef mReferenceMesh;
    std::vector<std::string> mBoneNames; // size == HumanoidBone::Count
};
