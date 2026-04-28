#pragma once

#include "Nodes/3D/Mesh3d.h"
#include "Vertex.h"
#include <vector>

class btTriangleIndexVertexArray;
class btBvhTriangleMeshShape;
struct btTriangleInfoMap;
class Texture;

class POLYPHASE_API Terrain3D : public Mesh3D
{
public:
    DECLARE_NODE(Terrain3D, Mesh3D);

    Terrain3D();
    ~Terrain3D();

    // Grid configuration
    int32_t mResolutionX = 128;       // vertices along X axis
    int32_t mResolutionZ = 128;       // vertices along Z axis
    float mWorldWidth = 128.0f;       // world-space extent X
    float mWorldDepth = 128.0f;       // world-space extent Z
    float mHeightScale = 10.0f;        // multiplier for height values

    // Heightmap data (row-major, resX * resZ floats)
    std::vector<float> mHeightmap;

    // Splatmap: 4 material weights per vertex, packed RGBA uint32
    std::vector<uint32_t> mSplatmap;

    // Material slots (up to 4, MaterialLite only for Wii/3DS compat)
    static constexpr int32_t MAX_MATERIAL_SLOTS = 4;
    MaterialRef mMaterialSlots[MAX_MATERIAL_SLOTS];
    bool mUseMaterialSlots = false;  // When false, vertex color is white (no splatmap tinting)

    // Atlas texturing (same approach as Voxel3D)
    TextureRef mAtlasTexture;
    bool mEnableAtlasTexturing = false;
    uint32_t mAtlasTilesX = 4;          // atlas grid columns
    uint32_t mAtlasTilesY = 4;          // atlas grid rows
    int32_t mSlotAtlasTile[MAX_MATERIAL_SLOTS] = {0, 1, 2, 3}; // tile index per slot
    float mTextureTiling = 32.0f;       // UV repetitions across the terrain
    glm::vec4 mSlotTintColor[MAX_MATERIAL_SLOTS] = {
        glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f)
    };

    // Baked splatmap: blends atlas tiles per-pixel for smooth material transitions
    bool mBakeSplatmap = false;          // When true, generates a blended texture
    uint32_t mBakeResolution = 512;      // Baked texture resolution (NxN)
    bool mDebugSplatmap = false;         // When true, renders raw splatmap weights as vertex color
    TextureRef mBakedMapTexture;         // Saved baked map asset (persists across PIE/runtime)

    // Auto-generation rules per material slot
    struct SlotRule
    {
        float mHeightMin = 0.0f;    // Normalized height [0,1] where this slot starts
        float mHeightMax = 1.0f;    // Normalized height [0,1] where this slot ends
        float mSlopeMin = 0.0f;     // Min slope angle in degrees (0=flat)
        float mSlopeMax = 90.0f;    // Max slope angle in degrees (90=vertical)
        float mBlend = 0.1f;        // Blend falloff at rule edges [0,1]
        float mStrength = 1.0f;     // Overall weight multiplier
    };
    SlotRule mSlotRules[MAX_MATERIAL_SLOTS];

    void GenerateSplatmapFromRules();

    // Snapping
    float mSnapGridSize = 0.0f;  // 0 = no snapping

    // Heightmap import texture reference
    TextureRef mHeightmapTexture;

    // Lifecycle overrides
    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Copy(Node* srcNode, bool recurse) override;
    Terrain3DResource* GetResource();
    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;
    virtual bool IsStaticMesh3D() const override;
    virtual bool IsSkeletalMesh3D() const override;
    virtual Material* GetMaterial() override;
    virtual void Render() override;
    virtual Bounds GetLocalBounds() const override;

    // Heightmap API
    void SetHeight(int32_t x, int32_t z, float height);
    float GetHeight(int32_t x, int32_t z) const;
    float GetHeightAtWorldPos(float worldX, float worldZ) const;
    void SetHeightFromTexture(Texture* texture);
    void FlattenAll(float height = 0.0f);
    void Resize(int32_t resX, int32_t resZ, float worldW, float worldD);

    // Splatmap/Material API
    void SetMaterialWeight(int32_t x, int32_t z, int32_t slot, float weight);
    float GetMaterialWeight(int32_t x, int32_t z, int32_t slot) const;
    void SetMaterialSlot(int32_t slot, Material* mat);
    Material* GetMaterialSlot(int32_t slot) const;

    // Baked splatmap
    void BakeSplatmapTexture();
    void BakeAndSaveMap(const std::string& savePath = "");
    void BakeAndSaveHeightmap(const std::string& savePath = "");

    // Mesh control
    void MarkDirty();
    void RebuildMesh();
    bool IsDirty() const { return mMeshDirty; }
    uint32_t GetNumVertices() const { return mNumVertices; }
    uint32_t GetNumIndices() const { return mNumIndices; }
    const std::vector<VertexColor>& GetVertices() const { return mVertices; }
    const std::vector<IndexType>& GetIndices() const { return mIndices; }
    int32_t GetResolutionX() const { return mResolutionX; }
    int32_t GetResolutionZ() const { return mResolutionZ; }
    float GetWorldWidth() const { return mWorldWidth; }
    float GetWorldDepth() const { return mWorldDepth; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void TickCommon(float deltaTime);
    void InitHeightmap();
    void RebuildMeshInternal();
    void UploadMeshData();
    void UpdateBounds();
    void RecreateCollisionShape();
    void DestroyTriangleCollisionData();

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
    Terrain3DResource mResource;

    // Default material
    MaterialRef mDefaultMaterial;

    // Baked splatmap texture (runtime-generated)
    Texture* mBakedTexture = nullptr;

    // Triangle collision data
    std::vector<glm::vec3> mCollisionVertices;
    std::vector<int32_t> mCollisionIndices;
    btTriangleIndexVertexArray* mTriangleIndexVertexArray = nullptr;
    btTriangleInfoMap* mTriangleInfoMap = nullptr;
};
