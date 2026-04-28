#pragma once

#include "Nodes/3D/Mesh3d.h"
#include "Vertex.h"
#include <array>

// Forward-declare PolyVox types so this header compiles on platforms that
// don't ship PolyVox headers (GX, C3D). The full includes live in Voxel3d.cpp.
namespace PolyVox { template<typename VoxelType> class RawVolume; }

class btTriangleIndexVertexArray;
class btBvhTriangleMeshShape;
struct btTriangleInfoMap;
class Camera3D;

// Voxel type: 0 = air (empty), 1-255 = solid material IDs
using VoxelType = uint8_t;

// Face direction for per-face textures
enum class VoxelFace : uint8_t
{
    Top = 0,    // +Y
    Bottom,     // -Y
    Side,       // +X, -X, +Z, -Z
    Count
};

// Per-material texture configuration for atlas-based texturing
struct VoxelMaterialInfo
{
    int32_t mAtlasTile[3] = {0, 0, 0};  // [Top, Bottom, Side] tile indices
    glm::vec4 mTintColor = glm::vec4(1.0f);  // Optional color multiplier
    bool mUseTexture = false;  // false = use vertex color (backward compat)
};

struct VoxelRayResult
{
    bool mHit = false;
    glm::ivec3 mVoxel = { 0, 0, 0 };
    glm::ivec3 mPrevVoxel = { -1, -1, -1 };
    glm::vec3 mHitPosition = { 0.0f, 0.0f, 0.0f };
    VoxelType mValue = 0;
};

struct VoxelPointResult
{
    bool mValid = false;
    glm::ivec3 mVoxel = { 0, 0, 0 };
    glm::vec3 mWorldPosition = { 0.0f, 0.0f, 0.0f };
    VoxelType mValue = 0;
};

struct VoxelInfo
{
    glm::ivec3 mCoord = { 0, 0, 0 };
    glm::vec3 mWorldPosition = { 0.0f, 0.0f, 0.0f };
    VoxelType mValue = 0;
};

class POLYPHASE_API Voxel3D : public Mesh3D
{
public:
    DECLARE_NODE(Voxel3D, Mesh3D);

    Voxel3D();
    ~Voxel3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Copy(Node* srcNode, bool recurse) override;
    Voxel3DResource* GetResource();

    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;

    virtual bool IsStaticMesh3D() const override;
    virtual bool IsSkeletalMesh3D() const override;
    virtual Material* GetMaterial() override;

    virtual void Render() override;

    // Voxel API
    void SetVoxel(int32_t x, int32_t y, int32_t z, VoxelType value);
    VoxelType GetVoxel(int32_t x, int32_t y, int32_t z) const;
    void Fill(VoxelType value);
    void FillRegion(int32_t x0, int32_t y0, int32_t z0,
                    int32_t x1, int32_t y1, int32_t z1, VoxelType value);
    void FillSphere(const glm::vec3& worldCenter, float radius, VoxelType value);
    void FillCylinder(const glm::vec3& worldCenter, float radius, float height, int32_t axis, VoxelType value);

    // Shape queries (return voxels within geometry)
    std::vector<VoxelInfo> GetVoxelsInSphere(const glm::vec3& worldCenter, float radius);
    std::vector<VoxelInfo> GetVoxelsInBox(const glm::vec3& worldMin, const glm::vec3& worldMax);
    std::vector<VoxelInfo> GetVoxelsInCylinder(const glm::vec3& worldCenter, float radius, float height, int32_t axis);
    std::vector<VoxelInfo> GetVoxelNeighbors(int32_t x, int32_t y, int32_t z);

    // Mesh control
    void MarkDirty();
    void RebuildMesh();
    bool IsDirty() const { return mMeshDirty; }

    // Coordinates
    glm::vec3 GetVoxelWorldPosition(int32_t x, int32_t y, int32_t z);

    // Ray testing (3D DDA traversal)
    VoxelRayResult RayTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance = 100.0f);
    VoxelRayResult RayTestScreen(Camera3D* camera, int32_t screenX, int32_t screenY, float maxDistance = 100.0f);
    VoxelRayResult RayTestCenterCamera(Camera3D* camera, float maxDistance = 100.0f);

    // Point query (no ray, just coordinate lookup)
    VoxelPointResult GetVoxelAtWorldPosition(const glm::vec3& worldPos);

    // Dimensions
    glm::ivec3 GetDimensions() const { return mDimensions; }
    void SetDimensions(glm::ivec3 dims);

    // Atlas texturing API
    void SetAtlasTexture(Texture* atlas, uint32_t tilesX = 16, uint32_t tilesY = 16);
    Texture* GetAtlasTexture() const;
    void SetAtlasEnabled(bool enabled);
    bool IsAtlasEnabled() const { return mEnableAtlasTexturing; }

    void SetMaterialTexture(VoxelType id, int32_t topTile, int32_t bottomTile, int32_t sideTile);
    void SetMaterialTexture(VoxelType id, int32_t allFacesTile);
    void SetMaterialTint(VoxelType id, const glm::vec4& tint);
    void DisableMaterialTexture(VoxelType id);
    const VoxelMaterialInfo& GetMaterialInfo(VoxelType id) const { return mMaterialTable[id]; }

    virtual Bounds GetLocalBounds() const override;

    uint32_t GetNumVertices() const { return mNumVertices; }
    uint32_t GetNumIndices() const { return mNumIndices; }
    const std::vector<VertexColor>& GetVertices() const { return mVertices; }
    const std::vector<IndexType>& GetIndices() const { return mIndices; }
    uint32_t mAtlasTilesX = 16;
    uint32_t mAtlasTilesY = 16;

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void TickCommon(float deltaTime);
    void RebuildMeshInternal();
    void UploadMeshData();
    void UpdateBounds();
    void RecreateCollisionShape();
    void DestroyTriangleCollisionData();
    void DecompressVoxelData();
    glm::vec4 MaterialIdToColor(VoxelType materialId) const;

    // Volume data
    PolyVox::RawVolume<VoxelType>* mVolume = nullptr;
    glm::ivec3 mDimensions = { 32, 32, 32 };

    // Compressed voxel data (for serialization)
    std::vector<uint8_t> mCompressedData;

    // Generated mesh data (CPU side)
    std::vector<VertexColor> mVertices;
    std::vector<IndexType> mIndices;
    uint32_t mNumVertices = 0;
    uint32_t mNumIndices = 0;

    // Dirty tracking
    bool mMeshDirty = true;
    bool mUploadDirty[MAX_FRAMES] = {};

    // Bounds cache
    Bounds mBounds;

    // GPU resource
    Voxel3DResource mResource;

    // Default material
    MaterialRef mDefaultMaterial;

    // Triangle collision data (kept alive while shape is in use)
    std::vector<glm::vec3> mCollisionVertices;
    std::vector<int32_t> mCollisionIndices;
    btTriangleIndexVertexArray* mTriangleIndexVertexArray = nullptr;
    btTriangleInfoMap* mTriangleInfoMap = nullptr;

    // Atlas texturing configuration
    TextureRef mAtlasTexture;
    bool mEnableAtlasTexturing = false;
    std::array<VoxelMaterialInfo, 256> mMaterialTable;
};
