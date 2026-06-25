#include "Assets/SkeletalAnimationAsset.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/HumanoidAvatarAsset.h"
#include "Stream.h"
#include "Log.h"

#if EDITOR
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#endif

FORCE_LINK_DEF(SkeletalAnimationAsset);
DEFINE_ASSET(SkeletalAnimationAsset);

SkeletalAnimationAsset::SkeletalAnimationAsset()
{
    mType = SkeletalAnimationAsset::GetStaticType();
}

SkeletalAnimationAsset::~SkeletalAnimationAsset()
{
}

void SkeletalAnimationAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    stream.ReadString(mClipName);
    mDuration = stream.ReadFloat();
    mTicksPerSecond = stream.ReadFloat();
    stream.ReadString(mSourceRigName);

    uint32_t numSourceBones = stream.ReadUint32();
    mSourceBoneNames.resize(numSourceBones);
    mSourceParentIndices.resize(numSourceBones);
    mSourceBindPose.resize(numSourceBones);
    for (uint32_t i = 0; i < numSourceBones; ++i)
    {
        stream.ReadString(mSourceBoneNames[i]);
        mSourceParentIndices[i] = stream.ReadInt32();
        mSourceBindPose[i] = stream.ReadMatrix();
    }

    uint32_t numChannels = stream.ReadUint32();
    mChannels.resize(numChannels);
    for (uint32_t i = 0; i < numChannels; ++i)
    {
        SkeletalAnimationChannel& channel = mChannels[i];
        stream.ReadString(channel.mBoneName);
        channel.mSourceBoneIndex = stream.ReadInt32();

        uint32_t numPos = stream.ReadUint32();
        channel.mPositionKeys.resize(numPos);
        for (uint32_t k = 0; k < numPos; ++k)
        {
            channel.mPositionKeys[k].mTime = stream.ReadFloat();
            channel.mPositionKeys[k].mValue = stream.ReadVec3();
        }

        uint32_t numRot = stream.ReadUint32();
        channel.mRotationKeys.resize(numRot);
        for (uint32_t k = 0; k < numRot; ++k)
        {
            channel.mRotationKeys[k].mTime = stream.ReadFloat();
            channel.mRotationKeys[k].mValue = stream.ReadQuat();
        }

        uint32_t numScale = stream.ReadUint32();
        channel.mScaleKeys.resize(numScale);
        for (uint32_t k = 0; k < numScale; ++k)
        {
            channel.mScaleKeys[k].mTime = stream.ReadFloat();
            channel.mScaleKeys[k].mValue = stream.ReadVec3();
        }
    }

    uint32_t numEventTracks = stream.ReadUint32();
    mEventTracks.resize(numEventTracks);
    for (uint32_t i = 0; i < numEventTracks; ++i)
    {
        AnimEventTrack& track = mEventTracks[i];
        stream.ReadString(track.mName);
        uint32_t numEventKeys = stream.ReadUint32();
        track.mEventKeys.resize(numEventKeys);
        for (uint32_t k = 0; k < numEventKeys; ++k)
        {
            track.mEventKeys[k].mTime = stream.ReadFloat();
            track.mEventKeys[k].mValue = stream.ReadVec3();
        }
    }
}

void SkeletalAnimationAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteString(mClipName);
    stream.WriteFloat(mDuration);
    stream.WriteFloat(mTicksPerSecond);
    stream.WriteString(mSourceRigName);

    stream.WriteUint32(uint32_t(mSourceBoneNames.size()));
    for (uint32_t i = 0; i < mSourceBoneNames.size(); ++i)
    {
        stream.WriteString(mSourceBoneNames[i]);
        stream.WriteInt32(i < mSourceParentIndices.size() ? mSourceParentIndices[i] : -1);
        stream.WriteMatrix(i < mSourceBindPose.size() ? mSourceBindPose[i] : glm::mat4(1.0f));
    }

    stream.WriteUint32(uint32_t(mChannels.size()));
    for (uint32_t i = 0; i < mChannels.size(); ++i)
    {
        const SkeletalAnimationChannel& channel = mChannels[i];
        stream.WriteString(channel.mBoneName);
        stream.WriteInt32(channel.mSourceBoneIndex);

        stream.WriteUint32(uint32_t(channel.mPositionKeys.size()));
        for (uint32_t k = 0; k < channel.mPositionKeys.size(); ++k)
        {
            stream.WriteFloat(channel.mPositionKeys[k].mTime);
            stream.WriteVec3(channel.mPositionKeys[k].mValue);
        }

        stream.WriteUint32(uint32_t(channel.mRotationKeys.size()));
        for (uint32_t k = 0; k < channel.mRotationKeys.size(); ++k)
        {
            stream.WriteFloat(channel.mRotationKeys[k].mTime);
            stream.WriteQuat(channel.mRotationKeys[k].mValue);
        }

        stream.WriteUint32(uint32_t(channel.mScaleKeys.size()));
        for (uint32_t k = 0; k < channel.mScaleKeys.size(); ++k)
        {
            stream.WriteFloat(channel.mScaleKeys[k].mTime);
            stream.WriteVec3(channel.mScaleKeys[k].mValue);
        }
    }

    stream.WriteUint32(uint32_t(mEventTracks.size()));
    for (uint32_t i = 0; i < mEventTracks.size(); ++i)
    {
        const AnimEventTrack& track = mEventTracks[i];
        stream.WriteString(track.mName);
        stream.WriteUint32(uint32_t(track.mEventKeys.size()));
        for (uint32_t k = 0; k < track.mEventKeys.size(); ++k)
        {
            stream.WriteFloat(track.mEventKeys[k].mTime);
            stream.WriteVec3(track.mEventKeys[k].mValue);
        }
    }
}

void SkeletalAnimationAsset::Create()
{
    Asset::Create();
}

void SkeletalAnimationAsset::Destroy()
{
    mChannels.clear();
    mEventTracks.clear();
    mSourceBoneNames.clear();
    mSourceParentIndices.clear();
    mSourceBindPose.clear();
    Asset::Destroy();
}

void SkeletalAnimationAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::String, "Clip Name", this, &mClipName));
    outProps.push_back(Property(DatumType::Float, "Duration", this, &mDuration));
    outProps.push_back(Property(DatumType::Float, "Ticks Per Second", this, &mTicksPerSecond));
    outProps.push_back(Property(DatumType::String, "Source Rig", this, &mSourceRigName));
}

glm::vec4 SkeletalAnimationAsset::GetTypeColor()
{
    return glm::vec4(0.70f, 0.10f, 0.55f, 1.0f);
}

const char* SkeletalAnimationAsset::GetTypeName()
{
    return "SkeletalAnimationAsset";
}

float SkeletalAnimationAsset::GetDurationSeconds() const
{
    return (mTicksPerSecond > 0.0f) ? (mDuration / mTicksPerSecond) : 0.0f;
}

#if EDITOR

// Cuts redundant interior keyframes — same pattern SkeletalMesh::SetupAnimations
// uses on imports. Keeps endpoints intact so loop math doesn't break.
template <typename KeyVec>
static void StripRedundantKeys(KeyVec& keys)
{
    int32_t count = int32_t(keys.size());
    for (int32_t i = count - 2; i > 0; --i)
    {
        if (keys[i - 1].mValue == keys[i].mValue &&
            keys[i + 1].mValue == keys[i].mValue)
        {
            keys.erase(keys.begin() + i);
        }
    }
}

uint32_t SkeletalAnimationAsset::ParseAnimationsFromFile(
    const std::string& path,
    std::vector<SkeletalAnimationAsset>& outAssets,
    std::vector<std::string>& outNames)
{
    outAssets.clear();
    outNames.clear();

    Assimp::Importer importer;
    // No JoinIdenticalVertices / Triangulate — we don't care about geometry here.
    const aiScene* scene = importer.ReadFile(path, aiProcess_FlipUVs);

    if (scene == nullptr)
    {
        LogError("ParseAnimationsFromFile: failed to load '%s'", path.c_str());
        return 0;
    }

    if (scene->mNumAnimations == 0)
    {
        LogWarning("ParseAnimationsFromFile: '%s' contains no animations", path.c_str());
        return 0;
    }

    // Build the source skeleton from the scene node hierarchy so animations
    // can carry source-bone metadata for later retargeting work. We treat
    // every non-mesh-bearing node as a potential bone; that's a superset but
    // matches what the SkeletalMesh combined importer does, and consumers
    // only key off names.
    std::vector<std::string> sourceBoneNames;
    std::vector<int32_t> sourceParentIndices;
    sourceBoneNames.reserve(64);
    sourceParentIndices.reserve(64);

    std::function<void(const aiNode*, int32_t)> walk =
        [&](const aiNode* node, int32_t parent)
    {
        int32_t myIdx = parent;
        if (node->mParent != nullptr && node->mNumMeshes == 0)
        {
            myIdx = int32_t(sourceBoneNames.size());
            sourceBoneNames.push_back(node->mName.C_Str());
            sourceParentIndices.push_back(parent);
        }
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
        {
            walk(node->mChildren[i], myIdx);
        }
    };
    walk(scene->mRootNode, -1);

    // Bind pose isn't recoverable from animation-only files without a skinned
    // mesh; leave it empty. PR5's retarget bake handles a missing bind pose by
    // falling back to identity, which is the right behaviour for clips imported
    // standalone (the caller can later attach a reference mesh).

    // Filename without extension for naming.
    size_t lastSlash = path.find_last_of("/\\");
    size_t lastDot = path.find_last_of('.');
    std::string baseName = path.substr(
        lastSlash == std::string::npos ? 0 : lastSlash + 1,
        (lastDot == std::string::npos ? path.size() : lastDot) - (lastSlash == std::string::npos ? 0 : lastSlash + 1));

    outAssets.resize(scene->mNumAnimations);
    outNames.resize(scene->mNumAnimations);

    for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
    {
        const aiAnimation* srcAnim = scene->mAnimations[a];
        SkeletalAnimationAsset& dst = outAssets[a];

        dst.mClipName = (srcAnim->mName.length > 0)
            ? std::string(srcAnim->mName.C_Str())
            : (baseName + "_Anim_" + std::to_string(a));
        dst.mDuration = float(srcAnim->mDuration);
        // GLB out of Blender exports with a meaningless mTicksPerSecond; the
        // embedded-mesh path hard-codes 1000 for the same reason, so we match.
        dst.mTicksPerSecond = 1000.0f;
        dst.mSourceRigName = baseName;
        dst.mSourceBoneNames = sourceBoneNames;
        dst.mSourceParentIndices = sourceParentIndices;
        // mSourceBindPose left empty — see note above.

        dst.mChannels.clear();
        dst.mChannels.reserve(srcAnim->mNumChannels);

        for (uint32_t c = 0; c < srcAnim->mNumChannels; ++c)
        {
            const aiNodeAnim* srcChan = srcAnim->mChannels[c];
            const char* nodeName = srcChan->mNodeName.C_Str();

            // Mirror the embedded-mesh importer's Event_* handling — pull
            // position keys into an event track named after the suffix.
            if (strncmp(nodeName, "Event_", 6) == 0)
            {
                AnimEventTrack track;
                track.mName = std::string(nodeName + 6);
                track.mEventKeys.reserve(srcChan->mNumPositionKeys);
                for (uint32_t k = 0; k < srcChan->mNumPositionKeys; ++k)
                {
                    AnimEventKey ek;
                    ek.mTime = float(srcChan->mPositionKeys[k].mTime);
                    ek.mValue.x = float(srcChan->mPositionKeys[k].mValue.x);
                    ek.mValue.y = float(srcChan->mPositionKeys[k].mValue.y);
                    ek.mValue.z = float(srcChan->mPositionKeys[k].mValue.z);
                    track.mEventKeys.push_back(ek);
                }
                if (!track.mEventKeys.empty())
                {
                    dst.mEventTracks.push_back(std::move(track));
                }
                continue;
            }

            SkeletalAnimationChannel dstChan;
            dstChan.mBoneName = nodeName;
            // Map back into our walked-source-bone index. -1 if missing — the
            // runtime resolves channels by name anyway.
            dstChan.mSourceBoneIndex = -1;
            for (uint32_t i = 0; i < sourceBoneNames.size(); ++i)
            {
                if (sourceBoneNames[i] == dstChan.mBoneName)
                {
                    dstChan.mSourceBoneIndex = int32_t(i);
                    break;
                }
            }

            dstChan.mPositionKeys.resize(srcChan->mNumPositionKeys);
            for (uint32_t k = 0; k < srcChan->mNumPositionKeys; ++k)
            {
                dstChan.mPositionKeys[k].mTime = float(srcChan->mPositionKeys[k].mTime);
                dstChan.mPositionKeys[k].mValue.x = float(srcChan->mPositionKeys[k].mValue.x);
                dstChan.mPositionKeys[k].mValue.y = float(srcChan->mPositionKeys[k].mValue.y);
                dstChan.mPositionKeys[k].mValue.z = float(srcChan->mPositionKeys[k].mValue.z);
            }

            dstChan.mRotationKeys.resize(srcChan->mNumRotationKeys);
            for (uint32_t k = 0; k < srcChan->mNumRotationKeys; ++k)
            {
                dstChan.mRotationKeys[k].mTime = float(srcChan->mRotationKeys[k].mTime);
                dstChan.mRotationKeys[k].mValue.x = float(srcChan->mRotationKeys[k].mValue.x);
                dstChan.mRotationKeys[k].mValue.y = float(srcChan->mRotationKeys[k].mValue.y);
                dstChan.mRotationKeys[k].mValue.z = float(srcChan->mRotationKeys[k].mValue.z);
                dstChan.mRotationKeys[k].mValue.w = float(srcChan->mRotationKeys[k].mValue.w);
            }

            dstChan.mScaleKeys.resize(srcChan->mNumScalingKeys);
            for (uint32_t k = 0; k < srcChan->mNumScalingKeys; ++k)
            {
                dstChan.mScaleKeys[k].mTime = float(srcChan->mScalingKeys[k].mTime);
                dstChan.mScaleKeys[k].mValue.x = float(srcChan->mScalingKeys[k].mValue.x);
                dstChan.mScaleKeys[k].mValue.y = float(srcChan->mScalingKeys[k].mValue.y);
                dstChan.mScaleKeys[k].mValue.z = float(srcChan->mScalingKeys[k].mValue.z);
            }

            StripRedundantKeys(dstChan.mPositionKeys);
            StripRedundantKeys(dstChan.mRotationKeys);
            StripRedundantKeys(dstChan.mScaleKeys);

            dst.mChannels.push_back(std::move(dstChan));
        }

        outNames[a] = dst.mClipName;
    }

    return uint32_t(scene->mNumAnimations);
}

namespace
{
    glm::quat ExtractRotation(const glm::mat4& m)
    {
        glm::vec3 scale;
        glm::quat rot;
        glm::vec3 trans;
        glm::vec3 skew;
        glm::vec4 persp;
        if (!glm::decompose(m, scale, rot, trans, skew, persp))
        {
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        // glm::decompose returns the conjugate of the rotation that builds the
        // matrix in GLM's column-major convention; conjugate again to get back
        // the "natural" quaternion.
        return glm::conjugate(rot);
    }

    glm::vec3 ExtractTranslation(const glm::mat4& m)
    {
        return glm::vec3(m[3]);
    }

    // Sample a channel's rotation track at time t (slerp between surrounding keys).
    glm::quat SampleRotation(const SkeletalAnimationChannel* ch, float t, glm::quat fallback)
    {
        if (ch == nullptr || ch->mRotationKeys.empty())
        {
            return fallback;
        }
        const auto& keys = ch->mRotationKeys;
        if (t <= keys.front().mTime) return keys.front().mValue;
        if (t >= keys.back().mTime)  return keys.back().mValue;
        for (size_t i = 0; i + 1 < keys.size(); ++i)
        {
            if (t >= keys[i].mTime && t <= keys[i + 1].mTime)
            {
                float span = keys[i + 1].mTime - keys[i].mTime;
                float a = (span > 0.0f) ? (t - keys[i].mTime) / span : 0.0f;
                return glm::slerp(keys[i].mValue, keys[i + 1].mValue, a);
            }
        }
        return keys.back().mValue;
    }

    glm::vec3 SamplePosition(const SkeletalAnimationChannel* ch, float t, glm::vec3 fallback)
    {
        if (ch == nullptr || ch->mPositionKeys.empty())
        {
            return fallback;
        }
        const auto& keys = ch->mPositionKeys;
        if (t <= keys.front().mTime) return keys.front().mValue;
        if (t >= keys.back().mTime)  return keys.back().mValue;
        for (size_t i = 0; i + 1 < keys.size(); ++i)
        {
            if (t >= keys[i].mTime && t <= keys[i + 1].mTime)
            {
                float span = keys[i + 1].mTime - keys[i].mTime;
                float a = (span > 0.0f) ? (t - keys[i].mTime) / span : 0.0f;
                return glm::mix(keys[i].mValue, keys[i + 1].mValue, a);
            }
        }
        return keys.back().mValue;
    }
}

SkeletalAnimationAsset SkeletalAnimationAsset::Retarget(
    const SkeletalAnimationAsset& srcClip,
    const HumanoidAvatarAsset& srcAvatar,
    const HumanoidAvatarAsset& dstAvatar,
    RetargetMode mode,
    std::string* outDiagnostics)
{
    SkeletalAnimationAsset out;
    out.mClipName = srcClip.mClipName;
    out.mDuration = srcClip.mDuration;
    out.mTicksPerSecond = srcClip.mTicksPerSecond;
    out.mEventTracks = srcClip.mEventTracks;

    SkeletalMesh* srcMesh = srcAvatar.GetReferenceMesh();
    SkeletalMesh* dstMesh = dstAvatar.GetReferenceMesh();

    // Source-skeleton metadata for the OUTPUT clip describes the TARGET rig.
    if (dstMesh != nullptr)
    {
        out.mSourceRigName = dstMesh->GetName();
        const std::vector<Bone>& bones = dstMesh->GetBones();
        out.mSourceBoneNames.resize(bones.size());
        out.mSourceParentIndices.resize(bones.size());
        out.mSourceBindPose.resize(bones.size());
        for (uint32_t i = 0; i < bones.size(); ++i)
        {
            out.mSourceBoneNames[i] = bones[i].mName;
            out.mSourceParentIndices[i] = bones[i].mParentIndex;
            out.mSourceBindPose[i] = dstMesh->GetBindPoseMatrix(int32_t(i));
        }
    }

    // ReferencePose requires both reference meshes — without them we don't
    // have bind poses to base the world-space math on. Fall back to NameRemap
    // with a diagnostic note so the caller knows.
    bool referencePoseDowngraded = false;
    if (mode == RetargetMode::ReferencePose &&
        (srcMesh == nullptr || dstMesh == nullptr))
    {
        mode = RetargetMode::NameRemap;
        referencePoseDowngraded = true;
    }

    uint32_t remapped = 0;
    uint32_t skipped = 0;

    if (mode == RetargetMode::NameRemap)
    {
        // Tier 1 — straight passthrough by name. Channels keyed by source
        // bone name get re-keyed by the target's slot mapping.
        std::unordered_map<std::string, const SkeletalAnimationChannel*> srcByName;
        srcByName.reserve(srcClip.mChannels.size());
        for (const SkeletalAnimationChannel& c : srcClip.mChannels)
        {
            srcByName[c.mBoneName] = &c;
        }

        for (uint32_t s = 0; s < (uint32_t)HumanoidBone::Count; ++s)
        {
            HumanoidBone slot = (HumanoidBone)s;
            const std::string& srcBoneName = srcAvatar.GetBoneName(slot);
            const std::string& dstBoneName = dstAvatar.GetBoneName(slot);
            if (srcBoneName.empty() || dstBoneName.empty()) continue;

            auto it = srcByName.find(srcBoneName);
            if (it == srcByName.end()) { ++skipped; continue; }

            const SkeletalAnimationChannel& srcChan = *it->second;
            SkeletalAnimationChannel dstChan;
            dstChan.mBoneName = dstBoneName;
            dstChan.mSourceBoneIndex = -1;
            dstChan.mPositionKeys = srcChan.mPositionKeys;
            dstChan.mRotationKeys = srcChan.mRotationKeys;
            dstChan.mScaleKeys = srcChan.mScaleKeys;

            out.mChannels.push_back(std::move(dstChan));
            ++remapped;
        }
    }
    else // ReferencePose — world-space retarget
    {
        // The world-space retarget walks both rigs' full bone hierarchies per
        // keyframe. Bones are processed parent-before-child so each retargeted
        // local rotation can be derived from its parent's already-computed
        // target-world rotation. SkeletalMesh::SetupBoneHierarchy emits bones
        // via DFS, so iterating in index order is already parent-first.
        const std::vector<Bone>& srcBones = srcMesh->GetBones();
        const std::vector<Bone>& dstBones = dstMesh->GetBones();
        const uint32_t srcN = (uint32_t)srcBones.size();
        const uint32_t dstN = (uint32_t)dstBones.size();

        // ---- precompute: source channels by source bone index ----
        std::vector<const SkeletalAnimationChannel*> srcChanByBone(srcN, nullptr);
        for (const SkeletalAnimationChannel& ch : srcClip.mChannels)
        {
            int32_t bi = srcMesh->FindBoneIndex(ch.mBoneName);
            if (bi >= 0 && bi < (int32_t)srcN) srcChanByBone[bi] = &ch;
        }

        // ---- precompute: humanoid slot mappings (both ends present + bones exist) ----
        struct SlotMap
        {
            HumanoidBone slot;
            int32_t      srcBoneIdx;
            int32_t      dstBoneIdx;
            std::string  dstBoneName;
            int32_t      outChannelIdx; // index into outChannels
        };
        std::vector<SlotMap> slotMaps;
        slotMaps.reserve((int)HumanoidBone::Count);

        // dstBoneIdx -> slotMaps index, for fast lookup during target walk.
        std::vector<int32_t> dstSlotByBone(dstN, -1);

        for (uint32_t s = 0; s < (uint32_t)HumanoidBone::Count; ++s)
        {
            HumanoidBone slot = (HumanoidBone)s;
            const std::string& srcName = srcAvatar.GetBoneName(slot);
            const std::string& dstName = dstAvatar.GetBoneName(slot);
            if (srcName.empty() || dstName.empty()) continue;
            int32_t srcIdx = srcMesh->FindBoneIndex(srcName);
            int32_t dstIdx = dstMesh->FindBoneIndex(dstName);
            if (srcIdx < 0 || dstIdx < 0) continue;
            if (srcChanByBone[srcIdx] == nullptr) { ++skipped; continue; }

            SlotMap m;
            m.slot = slot;
            m.srcBoneIdx = srcIdx;
            m.dstBoneIdx = dstIdx;
            m.dstBoneName = dstName;
            m.outChannelIdx = -1; // assigned below
            slotMaps.push_back(m);
            dstSlotByBone[dstIdx] = (int32_t)slotMaps.size() - 1;
        }

        // ---- precompute: local + world bind rotations per source / target bone ----
        std::vector<glm::quat> srcLocalBindRot(srcN);
        std::vector<glm::vec3> srcLocalBindPos(srcN);
        std::vector<glm::quat> srcWorldBindRot(srcN);
        for (uint32_t i = 0; i < srcN; ++i)
        {
            srcLocalBindRot[i] = ExtractRotation(srcMesh->GetBindPoseMatrix(int32_t(i)));
            srcLocalBindPos[i] = ExtractTranslation(srcMesh->GetBindPoseMatrix(int32_t(i)));
            // World bind: the bone's local-to-model resting transform == inverse(offset)
            srcWorldBindRot[i] = ExtractRotation(srcBones[i].mInvOffsetMatrix);
        }
        std::vector<glm::quat> dstLocalBindRot(dstN);
        std::vector<glm::vec3> dstLocalBindPos(dstN);
        std::vector<glm::quat> dstWorldBindRot(dstN);
        for (uint32_t i = 0; i < dstN; ++i)
        {
            dstLocalBindRot[i] = ExtractRotation(dstMesh->GetBindPoseMatrix(int32_t(i)));
            dstLocalBindPos[i] = ExtractTranslation(dstMesh->GetBindPoseMatrix(int32_t(i)));
            dstWorldBindRot[i] = ExtractRotation(dstBones[i].mInvOffsetMatrix);
        }

        // ---- gather timestamps: union of rotation keyframes across mapped slots ----
        std::set<float> rotTimesSet;
        std::set<float> hipsPosTimesSet;
        int32_t hipsSlotIdx = -1;
        for (uint32_t i = 0; i < slotMaps.size(); ++i)
        {
            const SlotMap& sm = slotMaps[i];
            const SkeletalAnimationChannel* ch = srcChanByBone[sm.srcBoneIdx];
            for (const auto& k : ch->mRotationKeys) rotTimesSet.insert(k.mTime);
            if (sm.slot == HumanoidBone::Hips)
            {
                hipsSlotIdx = (int32_t)i;
                for (const auto& k : ch->mPositionKeys) hipsPosTimesSet.insert(k.mTime);
            }
        }
        std::vector<float> rotTimes(rotTimesSet.begin(), rotTimesSet.end());
        std::vector<float> hipsPosTimes(hipsPosTimesSet.begin(), hipsPosTimesSet.end());

        // ---- pre-allocate output channels (one per slot map) ----
        out.mChannels.resize(slotMaps.size());
        for (uint32_t i = 0; i < slotMaps.size(); ++i)
        {
            out.mChannels[i].mBoneName = slotMaps[i].dstBoneName;
            out.mChannels[i].mSourceBoneIndex = -1;
            slotMaps[i].outChannelIdx = (int32_t)i;
        }

        // ---- per-frame world-space walks ----
        std::vector<glm::quat> srcWorldRot(srcN, glm::quat(1, 0, 0, 0));
        std::vector<glm::quat> dstWorldRot(dstN, glm::quat(1, 0, 0, 0));

        for (float t : rotTimes)
        {
            // STEP 1 - Source mesh hierarchy walk. For each bone, take the
            // animated local rotation (channel or bind) and accumulate world.
            for (uint32_t i = 0; i < srcN; ++i)
            {
                glm::quat localRot = SampleRotation(srcChanByBone[i], t, srcLocalBindRot[i]);
                int32_t parent = srcBones[i].mParentIndex;
                glm::quat parentWorld = (parent < 0)
                    ? glm::quat(1, 0, 0, 0)
                    : srcWorldRot[parent];
                srcWorldRot[i] = parentWorld * localRot;
            }

            // STEP 2 - Target mesh hierarchy walk. For bones that map to a
            // humanoid slot with source data, retarget through world space;
            // for the rest, hold bind. Either way, accumulate world so child
            // bones see the right parent.
            for (uint32_t i = 0; i < dstN; ++i)
            {
                glm::quat localRot;
                int32_t slotIdx = dstSlotByBone[i];

                if (slotIdx >= 0)
                {
                    const SlotMap& sm = slotMaps[slotIdx];
                    glm::quat srcW     = srcWorldRot[sm.srcBoneIdx];
                    glm::quat srcWBind = srcWorldBindRot[sm.srcBoneIdx];
                    glm::quat dstWBind = dstWorldBindRot[i];

                    // World-space delta the animation applied on the source.
                    glm::quat worldDelta = srcW * glm::inverse(srcWBind);

                    // Apply that delta to the target's world bind, then push
                    // back into target-local through the parent's currently-
                    // accumulated target world rotation.
                    glm::quat targetWorld = worldDelta * dstWBind;
                    int32_t parent = dstBones[i].mParentIndex;
                    glm::quat parentWorld = (parent < 0)
                        ? glm::quat(1, 0, 0, 0)
                        : dstWorldRot[parent];
                    localRot = glm::normalize(glm::inverse(parentWorld) * targetWorld);

                    // Emit the keyframe.
                    RotationKey rk;
                    rk.mTime = t;
                    rk.mValue = localRot;
                    out.mChannels[sm.outChannelIdx].mRotationKeys.push_back(rk);
                }
                else
                {
                    localRot = dstLocalBindRot[i];
                }

                int32_t parent = dstBones[i].mParentIndex;
                glm::quat parentWorld = (parent < 0)
                    ? glm::quat(1, 0, 0, 0)
                    : dstWorldRot[parent];
                dstWorldRot[i] = parentWorld * localRot;
            }
        }

        // ---- Hips position retarget (uniform Y-ratio scale) ----
        if (hipsSlotIdx >= 0 && !hipsPosTimes.empty())
        {
            const SlotMap& hipsMap = slotMaps[hipsSlotIdx];
            const SkeletalAnimationChannel* srcHipsCh = srcChanByBone[hipsMap.srcBoneIdx];
            const glm::vec3 srcRoot = srcLocalBindPos[hipsMap.srcBoneIdx];
            const glm::vec3 dstRoot = dstLocalBindPos[hipsMap.dstBoneIdx];
            float scaleY = (std::abs(srcRoot.y) > 1e-4f) ? (dstRoot.y / srcRoot.y) : 1.0f;
            glm::vec3 s(scaleY);

            out.mChannels[hipsMap.outChannelIdx].mPositionKeys.reserve(hipsPosTimes.size());
            for (float t : hipsPosTimes)
            {
                PositionKey pk;
                pk.mTime = t;
                pk.mValue = SamplePosition(srcHipsCh, t, srcRoot) * s;
                out.mChannels[hipsMap.outChannelIdx].mPositionKeys.push_back(pk);
            }
        }

        // ---- Backfill empty attribute tracks with single bind-pose keys ----
        // The runtime's Find{Position,Rotation,Scale}Index helpers assert that
        // every channel has at least one key per attribute. Tier-2 only emits
        // rotation keys for mapped slots and position keys for Hips; scale is
        // never animated. Insert a single key at t=0 holding the target's
        // bind-pose value for any track that's still empty. Runs AFTER the
        // hips position pass so we don't insert a stray key into a track
        // we're about to fill.
        for (uint32_t i = 0; i < slotMaps.size(); ++i)
        {
            SkeletalAnimationChannel& ch = out.mChannels[i];
            const SlotMap& sm = slotMaps[i];

            if (ch.mPositionKeys.empty())
            {
                PositionKey pk;
                pk.mTime = 0.0f;
                pk.mValue = dstLocalBindPos[sm.dstBoneIdx];
                ch.mPositionKeys.push_back(pk);
            }
            if (ch.mScaleKeys.empty())
            {
                ScaleKey sk;
                sk.mTime = 0.0f;
                sk.mValue = glm::vec3(1.0f);
                ch.mScaleKeys.push_back(sk);
            }
            if (ch.mRotationKeys.empty())
            {
                // Should never hit for a mapped slot — rotTimes was built from
                // mapped slots' channels. Guard for defensive sanity.
                RotationKey rk;
                rk.mTime = 0.0f;
                rk.mValue = dstLocalBindRot[sm.dstBoneIdx];
                ch.mRotationKeys.push_back(rk);
            }
        }

        remapped = (uint32_t)slotMaps.size();
    }

    if (outDiagnostics != nullptr)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Retarget %s%s: %u slots remapped, %u skipped (no source channel).",
                 mode == RetargetMode::NameRemap ? "NameRemap" : "ReferencePose",
                 referencePoseDowngraded ? " (DOWNGRADED - reference mesh missing on one avatar)" : "",
                 remapped, skipped);
        *outDiagnostics = buf;
    }

    return out;
}

#endif // EDITOR

void SkeletalAnimationAsset::CopyFromEmbedded(const Animation& embedded, const SkeletalMesh* sourceMesh)
{
    mClipName = embedded.mName;
    mDuration = embedded.mDuration;
    mTicksPerSecond = embedded.mTicksPerSecond;
    mEventTracks = embedded.mEventTracks;

    mChannels.clear();
    mChannels.reserve(embedded.mChannels.size());

    for (const Channel& src : embedded.mChannels)
    {
        SkeletalAnimationChannel dst;
        dst.mSourceBoneIndex = src.mBoneIndex;
        dst.mPositionKeys = src.mPositionKeys;
        dst.mRotationKeys = src.mRotationKeys;
        dst.mScaleKeys = src.mScaleKeys;

        if (sourceMesh != nullptr &&
            src.mBoneIndex >= 0 &&
            uint32_t(src.mBoneIndex) < sourceMesh->GetNumBones())
        {
            dst.mBoneName = sourceMesh->GetBone(src.mBoneIndex).mName;
        }

        mChannels.push_back(std::move(dst));
    }

    if (sourceMesh != nullptr)
    {
        mSourceRigName = sourceMesh->GetName();
        const std::vector<Bone>& srcBones = sourceMesh->GetBones();

        mSourceBoneNames.resize(srcBones.size());
        mSourceParentIndices.resize(srcBones.size());
        mSourceBindPose.resize(srcBones.size());
        for (uint32_t i = 0; i < srcBones.size(); ++i)
        {
            mSourceBoneNames[i] = srcBones[i].mName;
            mSourceParentIndices[i] = srcBones[i].mParentIndex;
            mSourceBindPose[i] = sourceMesh->GetBindPoseMatrix(int32_t(i));
        }
    }
}
