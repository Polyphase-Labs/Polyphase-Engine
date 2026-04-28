#pragma once

#include "Nodes/3D/Mesh3d.h"
#include "Vertex.h"
#include "AssetRef.h"

#include "Graphics/GraphicsTypes.h"

#include <vector>

class TileMap;
class TileSet;

// 2D tile map renderer. Lays tile quads in the XY plane (+X right, +Y up)
// with Z used for layer ordering. References a TileMap asset which owns the
// sparse cell data; this node is responsible for translating those cells
// into a triangle mesh and submitting it to the renderer.
//
// Architecturally this mirrors Voxel3D — single CPU vertex array, single GPU
// vertex+index buffer, dirty rebuild on Tick. Per-chunk GPU buffers can be
// added in a later phase if huge maps need finer-grain culling.
class POLYPHASE_API TileMap2D : public Mesh3D
{
public:

    DECLARE_NODE(TileMap2D, Mesh3D);

    TileMap2D();
    virtual ~TileMap2D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Copy(Node* srcNode, bool recurse) override;

    virtual void Tick(float deltaTime) override;
    virtual void EditorTick(float deltaTime) override;

    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform, uint32_t version) override;

    virtual bool IsStaticMesh3D() const override;
    virtual bool IsSkeletalMesh3D() const override;
    virtual Material* GetMaterial() override;

    virtual void Render() override;

    virtual Bounds GetLocalBounds() const override;

    // Asset binding
    TileMap* GetTileMap() const;
    void SetTileMap(TileMap* tileMap);

    // Coordinate helpers
    glm::ivec2 WorldToCell(const glm::vec2& worldXY) const;
    glm::vec2 CellToWorld(const glm::ivec2& cell) const;
    glm::vec2 CellCenterToWorld(const glm::ivec2& cell) const;

    // Mesh control
    void MarkDirty();
    void RebuildMesh();
    bool IsDirty() const { return mMeshDirty; }

    // Read-only mesh accessors used by the Vulkan draw helpers.
    TileMap2DResource* GetResource() { return &mResource; }
    uint32_t GetNumVertices() const { return mNumVertices; }
    uint32_t GetNumIndices() const { return mNumIndices; }
    const std::vector<VertexColor>& GetVertices() const { return mVertices; }
    const std::vector<IndexType>& GetIndices() const { return mIndices; }

    static bool HandlePropChange(class Datum* datum, uint32_t index, const void* newValue);

protected:

    void TickCommon(float deltaTime);
    void RebuildMeshInternal();
    void UploadMeshData();
    void UpdateBounds();
    void EnsureMaterialBinding();

    AssetRef mTileMap;

    // Generated mesh (CPU side, single buffer for all chunks)
    std::vector<VertexColor> mVertices;
    std::vector<IndexType> mIndices;
    uint32_t mNumVertices = 0;
    uint32_t mNumIndices = 0;

    bool mMeshDirty = true;
    bool mUploadDirty[MAX_FRAMES] = {};

    Bounds mBounds;

    TileMap2DResource mResource;

    // Default material is a MaterialLite that binds the TileSet's atlas texture
    // at slot 0. Recreated lazily so it can pull settings from the asset.
    MaterialRef mDefaultMaterial;

    // Spacing between consecutive layers along Z when computing world positions.
    float mLayerZSpacing = 0.01f;
};
