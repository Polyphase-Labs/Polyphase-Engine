#include "Assets/SkeletalAnimationAsset.h"
#include "Assets/SkeletalMesh.h"
#include "Stream.h"
#include "Log.h"

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
