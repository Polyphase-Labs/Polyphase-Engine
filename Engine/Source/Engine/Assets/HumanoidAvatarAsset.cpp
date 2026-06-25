#include "Assets/HumanoidAvatarAsset.h"
#include "Assets/SkeletalMesh.h"
#include "Stream.h"
#include "Log.h"
#include "Utilities.h"

#include <algorithm>
#include <cctype>

FORCE_LINK_DEF(HumanoidAvatarAsset);
DEFINE_ASSET(HumanoidAvatarAsset);

namespace
{
    const char* kHumanoidBoneNames[] =
    {
        "Hips",
        "Spine",
        "Chest",
        "Neck",
        "Head",
        "LeftShoulder",
        "LeftUpperArm",
        "LeftLowerArm",
        "LeftHand",
        "RightShoulder",
        "RightUpperArm",
        "RightLowerArm",
        "RightHand",
        "LeftUpperLeg",
        "LeftLowerLeg",
        "LeftFoot",
        "LeftToes",
        "RightUpperLeg",
        "RightLowerLeg",
        "RightFoot",
        "RightToes",
    };
    static_assert(sizeof(kHumanoidBoneNames) / sizeof(kHumanoidBoneNames[0]) == (size_t)HumanoidBone::Count,
                  "kHumanoidBoneNames must match HumanoidBone enum");

    // Alias table: for each humanoid slot, a list of lowercase-stripped bone
    // names that commonly map to it across Mixamo, ARP, Rigify, and ad-hoc
    // exporter conventions. We match the lowercase form with any of these
    // characters stripped: ' ', '_', '-', '.', ':'. So "mixamorig:LeftArm",
    // "left_arm", "Left.Arm", and "leftarm" all collapse to "leftarm".
    //
    // First entry per slot wins when multiple candidates exist on the rig —
    // it's the most specific/preferred name. Don't include partial matches
    // that could collide across slots (e.g. don't put "arm" in LeftUpperArm
    // since it would also match LeftLowerArm).
    // Each row: aliases for the slot. After NormalizeBoneName, ".L"/"_L"/" L"
    // and ".R"/"_R"/" R" both collapse to a trailing "l"/"r", so we cover the
    // Blender/GoBot suffix convention (UpperArm.L -> "upperarml") alongside
    // Mixamo's "Left"/"Right" prefix and ARP's "_l"/"_r" suffix.
    const char* const kAliases[(int)HumanoidBone::Count][12] =
    {
        // Hips
        { "hips", "hip", "pelvis", "root", nullptr },
        // Spine
        { "spine", "spine1", "lowerspine", nullptr },
        // Chest
        { "chest", "spine2", "upperspine", "upperchest", nullptr },
        // Neck
        { "neck", nullptr },
        // Head
        { "head", nullptr },
        // LeftShoulder
        { "leftshoulder", "shoulderl", "shoulderleft",
          "leftclavicle", "clavicleleft", "claviclel", "lcollar", nullptr },
        // LeftUpperArm
        { "leftarm", "leftupperarm", "upperarmleft",
          "upperarml", "uparml", "arml",
          "lupperarm", "luparm", nullptr },
        // LeftLowerArm
        { "leftforearm", "leftlowerarm", "lowerarmleft",
          "lowerarml", "loarml", "forearml",
          "llowerarm", "lloarm", nullptr },
        // LeftHand
        { "lefthand", "handleft", "handl", "lhand", nullptr },
        // RightShoulder
        { "rightshoulder", "shoulderr", "shoulderright",
          "rightclavicle", "clavicleright", "clavicler", "rcollar", nullptr },
        // RightUpperArm
        { "rightarm", "rightupperarm", "upperarmright",
          "upperarmr", "uparmr", "armr",
          "rupperarm", "ruparm", nullptr },
        // RightLowerArm
        { "rightforearm", "rightlowerarm", "lowerarmright",
          "lowerarmr", "loarmr", "forearmr",
          "rlowerarm", "rloarm", nullptr },
        // RightHand
        { "righthand", "handright", "handr", "rhand", nullptr },
        // LeftUpperLeg
        { "leftupleg", "leftupperleg", "lefthip", "upperlegleft",
          "upperlegl", "uplegl", "thighl", "legl1",
          "lupperleg", "lupleg", nullptr },
        // LeftLowerLeg
        { "leftleg", "leftlowerleg", "leftknee", "lowerlegleft",
          "lowerlegl", "lolegl", "shinl", "calfl",
          "llowerleg", "lloleg", nullptr },
        // LeftFoot
        { "leftfoot", "footleft", "footl", "lfoot", nullptr },
        // LeftToes
        { "lefttoebase", "lefttoes", "lefttoe", "toebaseleft",
          "toebasel", "toesl", "toel",
          "ltoebase", nullptr },
        // RightUpperLeg
        { "rightupleg", "rightupperleg", "righthip", "upperlegright",
          "upperlegr", "uplegr", "thighr", "legr1",
          "rupperleg", "rupleg", nullptr },
        // RightLowerLeg
        { "rightleg", "rightlowerleg", "rightknee", "lowerlegright",
          "lowerlegr", "lolegr", "shinr", "calfr",
          "rlowerleg", "rloleg", nullptr },
        // RightFoot
        { "rightfoot", "footright", "footr", "rfoot", nullptr },
        // RightToes
        { "righttoebase", "righttoes", "righttoe", "toebaseright",
          "toebaser", "toesr", "toer",
          "rtoebase", nullptr },
    };

    std::string NormalizeBoneName(const std::string& name)
    {
        std::string out;
        out.reserve(name.size());
        for (char c : name)
        {
            if (c == ' ' || c == '_' || c == '-' || c == '.' || c == ':')
            {
                continue;
            }
            out.push_back((char)std::tolower((unsigned char)c));
        }

        // Strip common rig-prefix noise so "mixamorig:LeftArm" matches "leftarm".
        // We do this AFTER stripping punctuation so any combination works.
        static const char* kPrefixes[] =
        {
            "mixamorig",
            "rig",
            "armature",
            "def", // Rigify "DEF-" prefix collapses to "def"
            "mch", // Rigify "MCH-"
            "ctrl",
            "skl",
            "bone",
        };
        for (const char* p : kPrefixes)
        {
            size_t plen = strlen(p);
            if (out.size() > plen && out.compare(0, plen, p) == 0)
            {
                out.erase(0, plen);
            }
        }

        return out;
    }
}

const char* HumanoidBoneName(HumanoidBone bone)
{
    uint32_t idx = (uint32_t)bone;
    if (idx >= (uint32_t)HumanoidBone::Count)
    {
        return "<invalid>";
    }
    return kHumanoidBoneNames[idx];
}

HumanoidAvatarAsset::HumanoidAvatarAsset()
{
    mType = HumanoidAvatarAsset::GetStaticType();
    mBoneNames.resize((size_t)HumanoidBone::Count);
}

HumanoidAvatarAsset::~HumanoidAvatarAsset()
{
}

void HumanoidAvatarAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    stream.ReadAsset(mReferenceMesh);

    uint32_t count = stream.ReadUint32();
    mBoneNames.assign((size_t)HumanoidBone::Count, "");

    // Tolerate older saves with fewer slots (we only ever append).
    const uint32_t readCount = std::min(count, (uint32_t)HumanoidBone::Count);
    for (uint32_t i = 0; i < readCount; ++i)
    {
        stream.ReadString(mBoneNames[i]);
    }
    // Drain any trailing slot strings from a future-version asset so the next
    // reader picks up at the right offset.
    for (uint32_t i = readCount; i < count; ++i)
    {
        std::string discard;
        stream.ReadString(discard);
    }
}

void HumanoidAvatarAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteAsset(mReferenceMesh);

    stream.WriteUint32((uint32_t)HumanoidBone::Count);
    for (uint32_t i = 0; i < (uint32_t)HumanoidBone::Count; ++i)
    {
        stream.WriteString(mBoneNames[i]);
    }
}

void HumanoidAvatarAsset::Create()
{
    Asset::Create();
    if (mBoneNames.size() != (size_t)HumanoidBone::Count)
    {
        mBoneNames.assign((size_t)HumanoidBone::Count, "");
    }
}

void HumanoidAvatarAsset::Destroy()
{
    mBoneNames.clear();
    mReferenceMesh = nullptr;
    Asset::Destroy();
}

void HumanoidAvatarAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);

    outProps.push_back(Property(DatumType::Asset, "Reference Mesh", this, &mReferenceMesh, 1, nullptr,
                                int32_t(SkeletalMesh::GetStaticType())));
    // Per-slot mappings are exposed through the dedicated inspector panel in
    // EditorImgui — we don't try to autogenerate 21 string properties here.
}

glm::vec4 HumanoidAvatarAsset::GetTypeColor()
{
    return glm::vec4(0.20f, 0.55f, 0.70f, 1.0f);
}

const char* HumanoidAvatarAsset::GetTypeName()
{
    return "HumanoidAvatarAsset";
}

SkeletalMesh* HumanoidAvatarAsset::GetReferenceMesh() const
{
    return mReferenceMesh.Get<SkeletalMesh>();
}

void HumanoidAvatarAsset::SetReferenceMesh(SkeletalMesh* mesh)
{
    mReferenceMesh = mesh;
}

const std::string& HumanoidAvatarAsset::GetBoneName(HumanoidBone slot) const
{
    static const std::string kEmpty;
    uint32_t idx = (uint32_t)slot;
    if (idx >= mBoneNames.size())
    {
        return kEmpty;
    }
    return mBoneNames[idx];
}

void HumanoidAvatarAsset::SetBoneName(HumanoidBone slot, const std::string& boneName)
{
    uint32_t idx = (uint32_t)slot;
    if (idx >= mBoneNames.size())
    {
        mBoneNames.resize((size_t)HumanoidBone::Count);
    }
    if (idx < mBoneNames.size())
    {
        mBoneNames[idx] = boneName;
    }
}

uint32_t HumanoidAvatarAsset::AutoMap(bool overwriteAll)
{
    SkeletalMesh* mesh = GetReferenceMesh();
    if (mesh == nullptr)
    {
        LogWarning("HumanoidAvatarAsset::AutoMap: no reference mesh set.");
        return 0;
    }

    // Pre-normalize every bone name on the mesh for O(slot * bone) matching.
    const std::vector<Bone>& bones = mesh->GetBones();
    std::vector<std::string> normalized;
    normalized.reserve(bones.size());
    for (const Bone& b : bones)
    {
        normalized.push_back(NormalizeBoneName(b.mName));
    }

    uint32_t filled = 0;
    for (uint32_t s = 0; s < (uint32_t)HumanoidBone::Count; ++s)
    {
        if (!overwriteAll && !mBoneNames[s].empty())
        {
            continue;
        }

        std::string match;
        for (const char* alias : kAliases[s])
        {
            if (alias == nullptr) break;
            for (uint32_t b = 0; b < normalized.size(); ++b)
            {
                if (normalized[b] == alias)
                {
                    match = bones[b].mName;
                    break;
                }
            }
            if (!match.empty()) break;
        }

        if (!match.empty())
        {
            mBoneNames[s] = match;
            ++filled;
        }
    }

    return filled;
}

bool HumanoidAvatarAsset::Validate(std::vector<HumanoidBone>* outMissing,
                                   std::vector<std::string>* outUnknownBones) const
{
    if (outMissing) outMissing->clear();
    if (outUnknownBones) outUnknownBones->clear();

    SkeletalMesh* mesh = GetReferenceMesh();
    bool ok = true;

    for (uint32_t s = 0; s < (uint32_t)HumanoidBone::Count; ++s)
    {
        const std::string& boneName = mBoneNames[s];
        if (boneName.empty())
        {
            // Optional slots — Hips/Spine/Head are essential; toes/shoulders less so.
            // We don't gate "valid" on optionals being filled; the editor surfaces
            // the missing ones for the user to triage.
            if (outMissing) outMissing->push_back((HumanoidBone)s);
            continue;
        }
        if (mesh != nullptr && mesh->FindBoneIndex(boneName) < 0)
        {
            ok = false;
            if (outUnknownBones) outUnknownBones->push_back(boneName);
        }
    }

    return ok;
}

glm::mat4 HumanoidAvatarAsset::GetReferenceLocalBind(HumanoidBone slot) const
{
    SkeletalMesh* mesh = GetReferenceMesh();
    if (mesh == nullptr)
    {
        return glm::mat4(1.0f);
    }

    int32_t idx = mesh->FindBoneIndex(GetBoneName(slot));
    if (idx < 0)
    {
        return glm::mat4(1.0f);
    }

    // The mesh's per-bone bind-pose matrix is in component-local space already
    // (see SkeletalMesh::InitBindPose). For retargeting we want the bone's
    // LOCAL bind relative to its parent.
    glm::mat4 myBindWorld = mesh->GetBindPoseMatrix(idx);
    const Bone& bone = mesh->GetBone(idx);
    if (bone.mParentIndex < 0)
    {
        return myBindWorld;
    }
    glm::mat4 parentBindWorld = mesh->GetBindPoseMatrix(bone.mParentIndex);
    return glm::inverse(parentBindWorld) * myBindWorld;
}
