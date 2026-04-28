#include "Nodes/3D/TileMap2d.h"
#include "Renderer.h"
#include "AssetManager.h"
#include "Asset.h"
#include "Engine.h"
#include "Log.h"

#include "Assets/MaterialLite.h"
#include "Assets/Texture.h"
#include "Assets/TileSet.h"
#include "Assets/TileMap.h"

#include "Graphics/Graphics.h"

#include <algorithm>

FORCE_LINK_DEF(TileMap2D);
DEFINE_NODE(TileMap2D, Mesh3D);

bool TileMap2D::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    TileMap2D* node = static_cast<TileMap2D*>(prop->mOwner);

    node->EnsureMaterialBinding();
    node->MarkDirty();

    return false;
}

TileMap2D::TileMap2D()
{
    mName = "TileMap";

    mBounds.mCenter = glm::vec3(0.0f);
    mBounds.mRadius = 1.0f;

    MarkDirty();
}

TileMap2D::~TileMap2D()
{
}

const char* TileMap2D::GetTypeName() const
{
    return "TileMap2D";
}

void TileMap2D::GatherProperties(std::vector<Property>& outProps)
{
    Mesh3D::GatherProperties(outProps);

    SCOPED_CATEGORY("TileMap");
    outProps.push_back(Property(DatumType::Asset, "Tile Map", this, &mTileMap, 1, HandlePropChange, int32_t(TileMap::GetStaticType())));
}

void TileMap2D::Create()
{
    Mesh3D::Create();
    GFX_CreateTileMap2DResource(this);

    // Use the engine's preconfigured unlit textured material — same one TextMesh3D
    // uses. Manually flipping ShadingModel on M_Default after the fact does not
    // propagate to the underlying shader pipeline, which produces white tiles.
    mDefaultMaterial = MaterialLite::New(LoadAsset<MaterialLite>("M_DefaultUnlit"));
    if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
    {
        mat->SetVertexColorMode(VertexColorMode::Modulate);
    }

    EnsureMaterialBinding();

    if (mMeshDirty)
    {
        RebuildMeshInternal();
    }
}

void TileMap2D::Destroy()
{
    GFX_DestroyTileMap2DResource(this);
    Mesh3D::Destroy();
}

void TileMap2D::Copy(Node* srcNode, bool recurse)
{
    Mesh3D::Copy(srcNode, recurse);

    if (srcNode->GetType() != TileMap2D::GetStaticType())
        return;

    TileMap2D* src = static_cast<TileMap2D*>(srcNode);
    mTileMap = src->mTileMap;
    mLayerZSpacing = src->mLayerZSpacing;

    EnsureMaterialBinding();
    MarkDirty();
}

void TileMap2D::Tick(float deltaTime)
{
    Mesh3D::Tick(deltaTime);
    TickCommon(deltaTime);
}

void TileMap2D::EditorTick(float deltaTime)
{
    Mesh3D::EditorTick(deltaTime);
    TickCommon(deltaTime);
}

void TileMap2D::TickCommon(float deltaTime)
{
    // Refresh the texture binding every tick so changes to the underlying
    // TileSet's atlas (or swapping the TileMap entirely) propagate without
    // requiring a HandlePropChange call. Mirrors TextMesh3D::TickCommon.
    EnsureMaterialBinding();

    // Drain the asset's dirty-chunk set. In Phase 1 we just rebuild the entire
    // mesh on any change; per-chunk rebuilds are a Phase 4 optimization.
    //
    // CRITICAL: must call MarkDirty() (not just set mMeshDirty) so the
    // per-frame mUploadDirty[] flags are also set. RebuildMeshInternal only
    // touches the CPU vertex array — without flagging the upload, the new
    // vertex data sits in CPU memory and the GPU keeps drawing the previous
    // mesh. Symptom: live pencil drag-paint stays invisible until mouse-up,
    // because the action's Execute path calls MarkDirty() which then triggers
    // the long-overdue upload.
    TileMap* tileMap = mTileMap.Get<TileMap>();
    if (tileMap != nullptr && !tileMap->GetDirtyChunks().empty())
    {
        MarkDirty();
        const_cast<TileMap*>(tileMap)->ClearDirtyChunks();
    }

    if (mMeshDirty)
    {
        RebuildMeshInternal();
    }

    UploadMeshData();
}

void TileMap2D::SaveStream(Stream& stream, Platform platform)
{
    Mesh3D::SaveStream(stream, platform);
    stream.WriteAsset(mTileMap);
    stream.WriteFloat(mLayerZSpacing);
}

void TileMap2D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Mesh3D::LoadStream(stream, platform, version);

    if (version >= ASSET_VERSION_TILEMAP_BASE)
    {
        stream.ReadAsset(mTileMap);
        mLayerZSpacing = stream.ReadFloat();
    }

    MarkDirty();
}

bool TileMap2D::IsStaticMesh3D() const
{
    return false;
}

bool TileMap2D::IsSkeletalMesh3D() const
{
    return false;
}

Material* TileMap2D::GetMaterial()
{
    Material* mat = mMaterialOverride.Get<Material>();
    if (mat == nullptr)
    {
        mat = mDefaultMaterial.Get<Material>();
    }
    return mat;
}

void TileMap2D::Render()
{
    GFX_DrawTileMap2D(this);
}

Bounds TileMap2D::GetLocalBounds() const
{
    return mBounds;
}

TileMap* TileMap2D::GetTileMap() const
{
    return mTileMap.Get<TileMap>();
}

void TileMap2D::SetTileMap(TileMap* tileMap)
{
    mTileMap = tileMap;
    EnsureMaterialBinding();
    MarkDirty();
}

void TileMap2D::EnsureMaterialBinding()
{
    MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>();
    if (mat == nullptr)
        return;

    Texture* atlasTex = nullptr;
    TileMap* tileMap = mTileMap.Get<TileMap>();
    if (tileMap != nullptr)
    {
        TileSet* tileSet = tileMap->GetTileSet();
        if (tileSet != nullptr)
        {
            atlasTex = tileSet->GetTexture();
        }
    }

    if (atlasTex != nullptr)
    {
        mat->SetTexture(0, atlasTex);
    }
}

glm::ivec2 TileMap2D::WorldToCell(const glm::vec2& worldXY) const
{
    glm::ivec2 tileSize = { 16, 16 };
    glm::vec2 origin = { 0.0f, 0.0f };
    TileMap* tileMap = mTileMap.Get<TileMap>();
    if (tileMap != nullptr)
    {
        tileSize = tileMap->GetTileSize();
        origin = tileMap->GetOrigin();
    }
    if (tileSize.x <= 0 || tileSize.y <= 0)
        return { 0, 0 };

    glm::vec3 nodePos = const_cast<TileMap2D*>(this)->GetWorldPosition();
    glm::vec2 local = worldXY - glm::vec2(nodePos.x, nodePos.y) - origin;
    return {
        int32_t(std::floor(local.x / float(tileSize.x))),
        int32_t(std::floor(local.y / float(tileSize.y)))
    };
}

glm::vec2 TileMap2D::CellToWorld(const glm::ivec2& cell) const
{
    glm::ivec2 tileSize = { 16, 16 };
    glm::vec2 origin = { 0.0f, 0.0f };
    TileMap* tileMap = mTileMap.Get<TileMap>();
    if (tileMap != nullptr)
    {
        tileSize = tileMap->GetTileSize();
        origin = tileMap->GetOrigin();
    }

    glm::vec3 nodePos = const_cast<TileMap2D*>(this)->GetWorldPosition();
    return {
        nodePos.x + origin.x + float(cell.x * tileSize.x),
        nodePos.y + origin.y + float(cell.y * tileSize.y)
    };
}

glm::vec2 TileMap2D::CellCenterToWorld(const glm::ivec2& cell) const
{
    glm::vec2 origin = CellToWorld(cell);
    glm::ivec2 tileSize = { 16, 16 };
    TileMap* tileMap = mTileMap.Get<TileMap>();
    if (tileMap != nullptr)
        tileSize = tileMap->GetTileSize();
    return origin + glm::vec2(tileSize.x * 0.5f, tileSize.y * 0.5f);
}

void TileMap2D::MarkDirty()
{
    mMeshDirty = true;
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

void TileMap2D::RebuildMesh()
{
    RebuildMeshInternal();
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

void TileMap2D::RebuildMeshInternal()
{
    mVertices.clear();
    mIndices.clear();

    TileMap* tileMap = mTileMap.Get<TileMap>();
    TileSet* tileSet = (tileMap != nullptr) ? tileMap->GetTileSet() : nullptr;

    if (tileMap == nullptr || tileSet == nullptr)
    {
        mNumVertices = 0;
        mNumIndices = 0;
        UpdateBounds();
        mMeshDirty = false;
        return;
    }

    glm::ivec2 tileSize = tileMap->GetTileSize();
    glm::vec2 mapOrigin = tileMap->GetOrigin();
    if (tileSize.x <= 0 || tileSize.y <= 0)
    {
        mNumVertices = 0;
        mNumIndices = 0;
        UpdateBounds();
        mMeshDirty = false;
        return;
    }

    float tw = float(tileSize.x);
    float th = float(tileSize.y);

    // The engine multiplies vertex colors by GlobalUniforms::mColorScale in
    // the forward shader (matches the GX/C3D fixed-function TEV scale on Wii
    // and 3DS). Engine code that builds vertex meshes pre-divides white so the
    // shader's scale brings it back to 1.0. See StaticMesh.cpp:154 and
    // PaintManager.cpp:514 for the same pattern.
    int32_t colorScale = GetEngineConfig()->mColorScale;
    uint32_t whiteColor = 0xFFFFFFFFu;
    if (colorScale == 2)
        whiteColor = 0x7F7F7F7Fu;
    else if (colorScale == 4)
        whiteColor = 0x3F3F3F3Fu;

    // Walk all layers in Z order. Layers with mZOrder=0 sit at z=0; subsequent
    // layers stack up by mLayerZSpacing.
    int32_t numLayers = tileMap->GetNumLayers();
    for (int32_t li = 0; li < numLayers; ++li)
    {
        const TileMapLayer* layer = tileMap->GetLayer(li);
        if (layer == nullptr || !layer->mVisible)
            continue;

        float lz = float(layer->mZOrder) * mLayerZSpacing;

        for (const auto& kv : layer->mChunks)
        {
            int32_t cx, cy;
            TileMap::UnpackChunkKey(kv.first, cx, cy);
            int32_t baseX = cx * kTileChunkSize;
            int32_t baseY = cy * kTileChunkSize;

            const TileChunk& chunk = kv.second;
            for (int32_t ly = 0; ly < kTileChunkSize; ++ly)
            {
                for (int32_t lx = 0; lx < kTileChunkSize; ++lx)
                {
                    const TileCell& cell = chunk.mCells[ly * kTileChunkSize + lx];
                    if (cell.mTileIndex < 0)
                        continue;
                    if (cell.mFlags & 0x08)  // hidden
                        continue;

                    glm::vec2 uv0, uv1;
                    if (!tileSet->GetTileUVs(cell.mTileIndex, uv0, uv1))
                        continue;

                    // Honor flip flags so painted variants render correctly.
                    if (cell.mFlags & 0x01) std::swap(uv0.x, uv1.x);
                    if (cell.mFlags & 0x02) std::swap(uv0.y, uv1.y);

                    int32_t worldCellX = baseX + lx;
                    int32_t worldCellY = baseY + ly;

                    float x0 = mapOrigin.x + float(worldCellX) * tw;
                    float y0 = mapOrigin.y + float(worldCellY) * th;
                    float x1 = x0 + tw;
                    float y1 = y0 + th;

                    IndexType base = IndexType(mVertices.size());

                    VertexColor v;
                    v.mNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                    v.mTexcoord1 = glm::vec2(0.0f);
                    v.mColor = whiteColor;

                    v.mPosition = glm::vec3(x0, y0, lz);
                    v.mTexcoord0 = glm::vec2(uv0.x, uv1.y);
                    mVertices.push_back(v);

                    v.mPosition = glm::vec3(x1, y0, lz);
                    v.mTexcoord0 = glm::vec2(uv1.x, uv1.y);
                    mVertices.push_back(v);

                    v.mPosition = glm::vec3(x1, y1, lz);
                    v.mTexcoord0 = glm::vec2(uv1.x, uv0.y);
                    mVertices.push_back(v);

                    v.mPosition = glm::vec3(x0, y1, lz);
                    v.mTexcoord0 = glm::vec2(uv0.x, uv0.y);
                    mVertices.push_back(v);

                    mIndices.push_back(base + 0);
                    mIndices.push_back(base + 1);
                    mIndices.push_back(base + 2);
                    mIndices.push_back(base + 0);
                    mIndices.push_back(base + 2);
                    mIndices.push_back(base + 3);
                }
            }
        }
    }

    mNumVertices = uint32_t(mVertices.size());
    mNumIndices = uint32_t(mIndices.size());

    UpdateBounds();
    mMeshDirty = false;
}

void TileMap2D::UploadMeshData()
{
    uint32_t frameIndex = Renderer::Get()->GetFrameIndex();
    if (mUploadDirty[frameIndex] && mNumVertices > 0 && mNumIndices > 0)
    {
        GFX_UpdateTileMap2DResource(this, mVertices, mIndices);
        mUploadDirty[frameIndex] = false;
    }
}

void TileMap2D::UpdateBounds()
{
    if (mVertices.empty())
    {
        mBounds.mCenter = glm::vec3(0.0f);
        mBounds.mRadius = 0.0f;
        return;
    }

    glm::vec3 minPos = mVertices[0].mPosition;
    glm::vec3 maxPos = mVertices[0].mPosition;
    for (const VertexColor& v : mVertices)
    {
        minPos = glm::min(minPos, v.mPosition);
        maxPos = glm::max(maxPos, v.mPosition);
    }

    mBounds.mCenter = (minPos + maxPos) * 0.5f;
    mBounds.mRadius = glm::length(maxPos - minPos) * 0.5f;
}
