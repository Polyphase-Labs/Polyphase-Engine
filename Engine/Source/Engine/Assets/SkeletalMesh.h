#pragma once

#include <string>

#include "Assets/Material.h"
#include "Asset.h"
#include "Vertex.h"
#include "Constants.h"

#include "Graphics/Graphics.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/GraphicsConstants.h"

#include "Maths.h"

#if EDITOR
#include <assimp/scene.h>
#endif

struct Bone
{
    std::string mName;
    int32_t mIndex = -1;
    int32_t mParentIndex = -1;
    glm::mat4 mOffsetMatrix = { };
    glm::mat4 mInvOffsetMatrix = { };
};

struct PositionKey
{
    float mTime = 0.0f;
    glm::vec3 mValue = {};
};

struct RotationKey
{
    float mTime = 0.0f;
    glm::quat mValue = {};
};

struct ScaleKey
{
    float mTime = 0.0f;
    glm::vec3 mValue = {};
};

struct AnimEventKey
{
    float mTime = 0.0f;
    glm::vec3 mValue = {};
};

struct AnimEventTrack
{
    std::string mName;
    std::vector<AnimEventKey> mEventKeys;
};

struct AnimEvent
{
    SkeletalMesh3D* mNode = nullptr;
    std::string mName;
    std::string mAnimation;
    float mTime = 0.0f;
    glm::vec3 mValue = {};
};

struct Channel
{
    int32_t mBoneIndex = -1;
    std::vector<PositionKey> mPositionKeys;
    std::vector<RotationKey> mRotationKeys;
    std::vector<ScaleKey> mScaleKeys;
};

struct Animation
{
    std::string mName;
    float mDuration = 0.0f;
    float mTicksPerSecond = 0.0f;
    std::vector<Channel> mChannels;
    std::vector<AnimEventTrack> mEventTracks;
};

// Contiguous range inside the SkeletalMesh's shared vertex/index buffers
// that should be drawn with its own material. One section per source aiMesh
// when the mesh was imported with "combineMeshes".
struct SkeletalMeshSection
{
    std::string mName;
    uint32_t mFirstIndex = 0;
    uint32_t mIndexCount = 0;
    uint32_t mBaseVertex = 0;
    uint32_t mVertexCount = 0;
    MaterialRef mMaterial;
};

class POLYPHASE_API SkeletalMesh : public Asset
{
public:

    DECLARE_ASSET(SkeletalMesh, Asset);

    SkeletalMesh();
    ~SkeletalMesh();

    SkeletalMeshResource* GetResource();

    // Asset Interface
    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;
    virtual const char* GetTypeImportExt() override;

    class Material* GetMaterial();
    void SetMaterial(class Material* newMaterial);

    // Multi-section accessors. A legacy single-material mesh appears as one
    // implicit section named "Default" covering the entire index range.
    uint32_t GetNumSections() const;
    const SkeletalMeshSection& GetSection(uint32_t index) const;
    SkeletalMeshSection& GetSectionMutable(uint32_t index);
    class Material* GetSectionMaterial(uint32_t index) const;
    void SetSectionMaterial(uint32_t index, class Material* material);
    int32_t FindSectionIndex(const std::string& name) const;
    const std::vector<SkeletalMeshSection>& GetSections() const;

    uint32_t GetNumIndices();
    uint32_t GetNumFaces();
    uint32_t GetNumVertices();

    const IndexType* GetIndices() const;

    int32_t FindBoneIndex(const std::string& name) const;
    const std::vector<Bone>& GetBones() const;
    const Bone& GetBone(int32_t index) const;
    uint32_t GetNumBones() const;

    glm::mat4 GetInvRootTransform() const;

    const std::vector<Animation>& GetAnimations() const;
    const Animation* GetAnimation(const char* name);

    // Get length of animation in seconds
    float GetAnimationDuration(const char* name);

    const std::vector<VertexSkinned>& GetVertices() const;

    void FinalizeBoneTransforms(std::vector<glm::mat4>& inoutTransforms);

    void CopyBindPose(std::vector<glm::mat4>& outTransforms);

    // Decomposed local-bind TRS, cached in InitBindPose. Used by
    // SkeletalMesh3D's blend loop to seed bones from bind pose before
    // accumulating animation layers, so masked-out bones in lower slots
    // and unmasked bones in upper slots both blend against a known pose.
    const glm::vec3& GetBindPosePos(int32_t boneIndex) const;
    const glm::quat& GetBindPoseRot(int32_t boneIndex) const;
    const glm::vec3& GetBindPoseScale(int32_t boneIndex) const;

    // Mark every bone in [rootIndex, descendants...] in outBitset (size = numBones).
    // Exploits the parent-before-child DFS order produced by SetupBoneHierarchy.
    void GatherDescendants(int32_t rootIndex, std::vector<uint8_t>& outBitset, bool includeRoot = true) const;

    // Resolve an include/exclude subtree spec into a per-bone bitset. Unknown
    // bone names are skipped (caller logs). selfOnly = include only the named
    // bones without recursing.
    void GatherSubtreeBoneSet(
        const std::vector<std::string>& includes,
        const std::vector<std::string>& excludes,
        bool selfOnly,
        std::vector<uint8_t>& outBitset) const;

    Bounds GetBounds() const;

    const glm::mat4 GetBindPoseMatrix(int32_t boneIndex) const;

    SkeletalMesh* GetAnimationLookupMesh();
    void SetAnimationLookupMesh(SkeletalMesh* lookupMesh);

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

private:

    void InitBindPose();
    void ComputeBounds();

    MaterialRef mMaterial;
    SkeletalMeshRef mAnimationLookupMesh;
    uint32_t mNumVertices;
    uint32_t mNumIndices;
    uint32_t mNumUvMaps;

    std::vector<Bone> mBones;
    std::vector<Animation> mAnimations;
    std::vector<VertexSkinned> mVertices;
    std::vector<SkeletalMeshSection> mSections;

    glm::mat4 mInvRootTransform;
    std::vector<glm::mat4> mBindPoseMatrices;
    // Decomposed local-bind TRS, parallel to mBindPoseMatrices. Populated by
    // InitBindPose. Zero-sized while bones haven't been initialised.
    std::vector<glm::vec3> mBindPoseDecompPos;
    std::vector<glm::quat> mBindPoseDecompRot;
    std::vector<glm::vec3> mBindPoseDecompScale;
    std::vector<IndexType> mIndices;

    Bounds mBounds;
    float mBoundsScale = 1.1f;

    // Graphics Resource
    SkeletalMeshResource mResource;

#if EDITOR
public:
    void Create(const aiScene& scene,
        const aiMesh& meshData,
        std::vector<Material>* materials = nullptr);

    void SetupBoneHierarchy(
        const aiNode& node,
        const aiMesh& meshData,
        std::vector<uint8_t>& boneIndices,
        std::vector<float>& boneWeights,
        int32_t parentBoneIndex);

    void SetupAnimations(const aiScene& scene);
    void SetupResource(const aiMesh& meshData,
        const std::vector<float>& boneWeights,
        const std::vector<uint8_t>& boneIndices);

    void CreateCombined(const aiScene& scene,
        const std::vector<const aiMesh*>& renderMeshes);
#endif // EDITOR
};
