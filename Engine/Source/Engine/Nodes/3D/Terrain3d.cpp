#include "Nodes/3D/Terrain3d.h"
#include "Renderer.h"
#include "AssetManager.h"
#include "Asset.h"
#include "Log.h"
#include "Assets/MaterialLite.h"
#include "Assets/Texture.h"
#include "Maths.h"
#include "Utilities.h"
#include "System/System.h"
#include "Graphics/Graphics.h"

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>

#include <algorithm>
#include <cmath>

FORCE_LINK_DEF(Terrain3D);
DEFINE_NODE(Terrain3D, Mesh3D);

bool Terrain3D::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    Terrain3D* terrain = static_cast<Terrain3D*>(prop->mOwner);

    // When the heightmap texture property changes, import it automatically
    if (prop->mName == "Heightmap Texture")
    {
        Texture* tex = terrain->mHeightmapTexture.Get<Texture>();
        if (tex != nullptr)
        {
            terrain->SetHeightFromTexture(tex);
        }
    }

    // Snap position to grid when snap grid size is set
    if (terrain->mSnapGridSize > 0.0f)
    {
        glm::vec3 pos = terrain->GetWorldPosition();
        float snap = terrain->mSnapGridSize;
        pos.x = std::round(pos.x / snap) * snap;
        pos.z = std::round(pos.z / snap) * snap;
        terrain->SetWorldPosition(pos);
    }

    terrain->MarkDirty();

    // Update material texture binding (priority: saved baked map > raw atlas)
    // Runtime baking only happens on explicit user action (Bake button / BakeAndSaveMap)
    if (MaterialLite* mat = terrain->mDefaultMaterial.Get<MaterialLite>())
    {
        Texture* bakedMap = terrain->mBakedMapTexture.Get<Texture>();
        if (bakedMap != nullptr)
        {
            mat->SetTexture(0, bakedMap);
        }
        else if (terrain->mEnableAtlasTexturing)
        {
            Texture* atlasTex = terrain->mAtlasTexture.Get<Texture>();
            if (atlasTex != nullptr)
                mat->SetTexture(0, atlasTex);
        }
        else
        {
            mat->SetTexture(0, nullptr);
        }
        mat->SetVertexColorMode(VertexColorMode::Modulate);
    }

    return false;
}

Terrain3D::Terrain3D()
{
    mName = "Terrain";

    // Initialize bounds from default world size
    mBounds.mCenter = glm::vec3(0.0f);
    mBounds.mRadius = glm::length(glm::vec3(mWorldWidth, 0.0f, mWorldDepth)) * 0.5f;

    // Default auto-gen rules: grass(flat low), rock(steep), snow(high), dirt(mid slopes)
    mSlotRules[0] = { 0.0f, 0.55f,  0.0f, 30.0f, 0.1f, 1.0f };  // Slot 0: Grass — low flat areas
    mSlotRules[1] = { 0.0f, 1.0f,  30.0f, 90.0f, 0.1f, 1.0f };  // Slot 1: Rock  — steep slopes
    mSlotRules[2] = { 0.6f, 1.0f,   0.0f, 45.0f, 0.1f, 1.0f };  // Slot 2: Snow  — high elevation
    mSlotRules[3] = { 0.3f, 0.7f,  10.0f, 40.0f, 0.1f, 0.5f };  // Slot 3: Dirt  — mid-elevation slopes

    MarkDirty();
}

Terrain3D::~Terrain3D()
{

}

const char* Terrain3D::GetTypeName() const
{
    return "Terrain";
}

void Terrain3D::GatherProperties(std::vector<Property>& outProps)
{
    Mesh3D::GatherProperties(outProps);

    {
        SCOPED_CATEGORY("Terrain");
        outProps.push_back(Property(DatumType::Integer, "Resolution X", this, &mResolutionX, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Resolution Z", this, &mResolutionZ, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Float, "World Width", this, &mWorldWidth, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Float, "World Depth", this, &mWorldDepth, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Float, "Height Scale", this, &mHeightScale, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Float, "Snap Grid", this, &mSnapGridSize, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Bool, "Use Material Slots", this, &mUseMaterialSlots, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Asset, "Heightmap Texture", this, &mHeightmapTexture, 1, HandlePropChange, int32_t(Texture::GetStaticType())));
    }

    {
        SCOPED_CATEGORY("Atlas Texturing");
        outProps.push_back(Property(DatumType::Bool, "Enable Atlas", this, &mEnableAtlasTexturing, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Asset, "Atlas Texture", this, &mAtlasTexture, 1, HandlePropChange, int32_t(Texture::GetStaticType())));
        outProps.push_back(Property(DatumType::Integer, "Tiles X", this, &mAtlasTilesX, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Tiles Y", this, &mAtlasTilesY, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Float, "Texture Tiling", this, &mTextureTiling, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Slot 0 Tile", this, &mSlotAtlasTile[0], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Slot 1 Tile", this, &mSlotAtlasTile[1], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Slot 2 Tile", this, &mSlotAtlasTile[2], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Slot 3 Tile", this, &mSlotAtlasTile[3], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Color, "Slot 0 Tint", this, &mSlotTintColor[0], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Color, "Slot 1 Tint", this, &mSlotTintColor[1], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Color, "Slot 2 Tint", this, &mSlotTintColor[2], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Color, "Slot 3 Tint", this, &mSlotTintColor[3], 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Bool, "Bake Splatmap", this, &mBakeSplatmap, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Bake Resolution", this, &mBakeResolution, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Asset, "Baked Map", this, &mBakedMapTexture, 1, HandlePropChange, int32_t(Texture::GetStaticType())));
    }
}

void Terrain3D::Create()
{
    Mesh3D::Create();

    InitHeightmap();

    GFX_CreateTerrain3DResource(this);

    // Build the mesh and collision immediately so bounds and raycasts work on first frame
    if (mMeshDirty)
    {
        RebuildMeshInternal();
        RecreateCollisionShape();
    }

    // Enable collision by default so physics raycasts (editor sculpting, NavMesh) work
    EnableCollision(true);

    // Create default material with Lit shading for proper lighting
    mDefaultMaterial = MaterialLite::New(LoadAsset<MaterialLite>("M_Default"));
    if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
    {
        mat->SetShadingModel(ShadingModel::Lit);
        mat->SetVertexColorMode(VertexColorMode::Modulate);

        // Texture binding is handled in TickCommon after LoadStream sets asset refs.
        // Just check if a baked map or atlas is already available at Create time.
        Texture* bakedMap = mBakedMapTexture.Get<Texture>();
        if (bakedMap != nullptr)
        {
            mat->SetTexture(0, bakedMap);
        }
        else if (mEnableAtlasTexturing)
        {
            Texture* atlasTex = mAtlasTexture.Get<Texture>();
            if (atlasTex != nullptr)
                mat->SetTexture(0, atlasTex);
        }
    }

    MarkDirty();
}

void Terrain3D::InitHeightmap()
{
    size_t totalVerts = (size_t)mResolutionX * mResolutionZ;

    if (mHeightmap.size() != totalVerts)
    {
        mHeightmap.assign(totalVerts, 0.0f);
    }

    if (mSplatmap.size() != totalVerts)
    {
        mSplatmap.assign(totalVerts, 0x000000FF); // Default: full weight on slot 0 (R=255)
    }
}

void Terrain3D::Destroy()
{
    DestroyTriangleCollisionData();
    GFX_DestroyTerrain3DResource(this);
    Mesh3D::Destroy();
}

void Terrain3D::Copy(Node* srcNode, bool recurse)
{
    Mesh3D::Copy(srcNode, recurse);

    if (srcNode->GetType() != Terrain3D::GetStaticType())
        return;

    Terrain3D* src = static_cast<Terrain3D*>(srcNode);

    mResolutionX = src->mResolutionX;
    mResolutionZ = src->mResolutionZ;
    mWorldWidth = src->mWorldWidth;
    mWorldDepth = src->mWorldDepth;
    mHeightScale = src->mHeightScale;
    mSnapGridSize = src->mSnapGridSize;
    mHeightmap = src->mHeightmap;
    mSplatmap = src->mSplatmap;
    mUseMaterialSlots = src->mUseMaterialSlots;
    mHeightmapTexture = src->mHeightmapTexture;
    mAtlasTexture = src->mAtlasTexture;
    mEnableAtlasTexturing = src->mEnableAtlasTexturing;
    mAtlasTilesX = src->mAtlasTilesX;
    mAtlasTilesY = src->mAtlasTilesY;
    mTextureTiling = src->mTextureTiling;
    mBakeSplatmap = src->mBakeSplatmap;
    mBakeResolution = src->mBakeResolution;
    mBakedMapTexture = src->mBakedMapTexture;

    for (int32_t i = 0; i < MAX_MATERIAL_SLOTS; ++i)
    {
        mMaterialSlots[i] = src->mMaterialSlots[i];
        mSlotAtlasTile[i] = src->mSlotAtlasTile[i];
        mSlotTintColor[i] = src->mSlotTintColor[i];
    }

    MarkDirty();
}

Terrain3DResource* Terrain3D::GetResource()
{
    return &mResource;
}

void Terrain3D::Tick(float deltaTime)
{
    Mesh3D::Tick(deltaTime);
    TickCommon(deltaTime);
}

void Terrain3D::EditorTick(float deltaTime)
{
    Mesh3D::EditorTick(deltaTime);
    TickCommon(deltaTime);
}

void Terrain3D::TickCommon(float deltaTime)
{
    if (mMeshDirty)
    {
        RebuildMeshInternal();
        RecreateCollisionShape();
    }

    // Auto-bind saved baked map on first tick after LoadStream
    if (mBakedMapTexture.Get<Texture>() != nullptr)
    {
        if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
        {
            if (mat->GetTexture(0) != mBakedMapTexture.Get<Texture>())
                mat->SetTexture(0, mBakedMapTexture.Get<Texture>());
        }
    }

    UploadMeshData();
}

void Terrain3D::SaveStream(Stream& stream, Platform platform)
{
    Mesh3D::SaveStream(stream, platform);

    stream.WriteInt32(mResolutionX);
    stream.WriteInt32(mResolutionZ);
    stream.WriteFloat(mWorldWidth);
    stream.WriteFloat(mWorldDepth);
    stream.WriteFloat(mHeightScale);
    stream.WriteFloat(mSnapGridSize);
    stream.WriteBool(mUseMaterialSlots);

    // Write heightmap data (per-float for endian safety on GCN/Wii)
    uint32_t heightmapSize = static_cast<uint32_t>(mHeightmap.size());
    stream.WriteUint32(heightmapSize);
    for (uint32_t i = 0; i < heightmapSize; ++i)
    {
        stream.WriteFloat(mHeightmap[i]);
    }

    // Write splatmap data (per-uint32 for endian safety on GCN/Wii)
    uint32_t splatmapSize = static_cast<uint32_t>(mSplatmap.size());
    stream.WriteUint32(splatmapSize);
    for (uint32_t i = 0; i < splatmapSize; ++i)
    {
        stream.WriteUint32(mSplatmap[i]);
    }

    // Write material slot references
    for (int32_t i = 0; i < MAX_MATERIAL_SLOTS; ++i)
    {
        stream.WriteAsset(mMaterialSlots[i]);
    }

    // Write heightmap texture reference
    stream.WriteAsset(mHeightmapTexture);

    // Atlas texturing (ASSET_VERSION_TERRAIN3D_ATLAS)
    stream.WriteBool(mEnableAtlasTexturing);
    stream.WriteAsset(mAtlasTexture);
    stream.WriteUint32(mAtlasTilesX);
    stream.WriteUint32(mAtlasTilesY);
    stream.WriteFloat(mTextureTiling);
    stream.WriteBool(mBakeSplatmap);
    stream.WriteUint32(mBakeResolution);
    stream.WriteAsset(mBakedMapTexture);
    for (int32_t i = 0; i < MAX_MATERIAL_SLOTS; ++i)
    {
        stream.WriteInt32(mSlotAtlasTile[i]);
        stream.WriteVec4(mSlotTintColor[i]);
    }
}

void Terrain3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Mesh3D::LoadStream(stream, platform, version);

    if (version >= ASSET_VERSION_TERRAIN3D)
    {
        mResolutionX = stream.ReadInt32();
        mResolutionZ = stream.ReadInt32();
        mWorldWidth = stream.ReadFloat();
        mWorldDepth = stream.ReadFloat();
        mHeightScale = stream.ReadFloat();
        mSnapGridSize = stream.ReadFloat();

        if (version >= ASSET_VERSION_TERRAIN3D_MATSLOTS)
        {
            mUseMaterialSlots = stream.ReadBool();
        }

        // Read heightmap data (per-float for endian safety on GCN/Wii)
        uint32_t heightmapSize = stream.ReadUint32();
        mHeightmap.resize(heightmapSize);
        for (uint32_t i = 0; i < heightmapSize; ++i)
        {
            mHeightmap[i] = stream.ReadFloat();
        }

        // Read splatmap data (per-uint32 for endian safety on GCN/Wii)
        uint32_t splatmapSize = stream.ReadUint32();
        mSplatmap.resize(splatmapSize);
        for (uint32_t i = 0; i < splatmapSize; ++i)
        {
            mSplatmap[i] = stream.ReadUint32();
        }

        // Read material slot references
        for (int32_t i = 0; i < MAX_MATERIAL_SLOTS; ++i)
        {
            stream.ReadAsset(mMaterialSlots[i]);
        }

        // Read heightmap texture reference
        stream.ReadAsset(mHeightmapTexture);

        // Atlas texturing
        if (version >= ASSET_VERSION_TERRAIN3D_ATLAS)
        {
            mEnableAtlasTexturing = stream.ReadBool();
            stream.ReadAsset(mAtlasTexture);
            mAtlasTilesX = stream.ReadUint32();
            mAtlasTilesY = stream.ReadUint32();
            mTextureTiling = stream.ReadFloat();
            if (version >= ASSET_VERSION_TERRAIN3D_BAKE)
            {
                mBakeSplatmap = stream.ReadBool();
                mBakeResolution = stream.ReadUint32();
            }
            if (version >= ASSET_VERSION_TERRAIN3D_BAKEDMAP)
            {
                stream.ReadAsset(mBakedMapTexture);
            }
            for (int32_t i = 0; i < MAX_MATERIAL_SLOTS; ++i)
            {
                mSlotAtlasTile[i] = stream.ReadInt32();
                mSlotTintColor[i] = stream.ReadVec4();
            }
        }
    }

    MarkDirty();
}

bool Terrain3D::IsStaticMesh3D() const
{
    return false;
}

bool Terrain3D::IsSkeletalMesh3D() const
{
    return false;
}

Material* Terrain3D::GetMaterial()
{
    Material* mat = mMaterialOverride.Get<Material>();

    if (mat == nullptr)
    {
        mat = mDefaultMaterial.Get<Material>();
    }

    return mat;
}

void Terrain3D::Render()
{
    GFX_DrawTerrain3D(this);
}

Bounds Terrain3D::GetLocalBounds() const
{
    return mBounds;
}

void Terrain3D::SetHeight(int32_t x, int32_t z, float height)
{
    if (x >= 0 && x < mResolutionX && z >= 0 && z < mResolutionZ)
    {
        mHeightmap[z * mResolutionX + x] = height;
        MarkDirty();
    }
}

float Terrain3D::GetHeight(int32_t x, int32_t z) const
{
    if (x >= 0 && x < mResolutionX && z >= 0 && z < mResolutionZ)
    {
        return mHeightmap[z * mResolutionX + x];
    }
    return 0.0f;
}

float Terrain3D::GetHeightAtWorldPos(float worldX, float worldZ) const
{
    if (mResolutionX < 2 || mResolutionZ < 2)
        return 0.0f;

    float stepX = mWorldWidth / (mResolutionX - 1);
    float stepZ = mWorldDepth / (mResolutionZ - 1);
    float fx = (worldX + mWorldWidth * 0.5f) / stepX;
    float fz = (worldZ + mWorldDepth * 0.5f) / stepZ;

    int ix = (int)std::floor(fx);
    int iz = (int)std::floor(fz);
    float tx = fx - ix;
    float tz = fz - iz;

    // Clamp to valid interpolation range
    ix = std::max(0, std::min(ix, (int)(mResolutionX - 2)));
    iz = std::max(0, std::min(iz, (int)(mResolutionZ - 2)));

    // Bilinear interpolation
    float h00 = GetHeight(ix, iz);
    float h10 = GetHeight(ix + 1, iz);
    float h01 = GetHeight(ix, iz + 1);
    float h11 = GetHeight(ix + 1, iz + 1);

    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;

    return (h0 + (h1 - h0) * tz) * mHeightScale;
}

void Terrain3D::SetHeightFromTexture(Texture* texture)
{
    if (texture == nullptr)
        return;

    uint32_t texW = texture->GetWidth();
    uint32_t texH = texture->GetHeight();

    if (texW == 0 || texH == 0)
        return;

    // Texture pixels are RGBA8, 4 bytes per pixel.
    // Note: mPixels is only available in EDITOR builds (cleared after GPU upload in runtime).
    const uint32_t bpp = 4; // RGBA8

    if (texture->GetPixels().empty())
    {
        LogWarning("SetHeightFromTexture: texture has no CPU-side pixel data");
        return;
    }

    for (int32_t iz = 0; iz < mResolutionZ; ++iz)
    {
        for (int32_t ix = 0; ix < mResolutionX; ++ix)
        {
            float u = (float)ix / (mResolutionX - 1);
            float v = (float)iz / (mResolutionZ - 1);

            uint32_t px = std::min((uint32_t)(u * (texW - 1)), texW - 1);
            uint32_t py = std::min((uint32_t)(v * (texH - 1)), texH - 1);
            uint32_t pixelIdx = (py * texW + px) * bpp;

            if (pixelIdx < texture->GetPixels().size())
            {
                // Sample the R channel as normalized height [0, 1]
                float heightNorm = texture->GetPixels()[pixelIdx] / 255.0f;
                mHeightmap[iz * mResolutionX + ix] = heightNorm;
            }
        }
    }

    MarkDirty();
}

void Terrain3D::FlattenAll(float height)
{
    std::fill(mHeightmap.begin(), mHeightmap.end(), height);
    MarkDirty();
}

void Terrain3D::Resize(int32_t resX, int32_t resZ, float worldW, float worldD)
{
    mResolutionX = std::max(resX, (int32_t)2);
    mResolutionZ = std::max(resZ, (int32_t)2);
    mWorldWidth = worldW;
    mWorldDepth = worldD;

    // Force reallocate at the new resolution
    size_t totalVerts = (size_t)mResolutionX * mResolutionZ;
    mHeightmap.assign(totalVerts, 0.0f);
    mSplatmap.assign(totalVerts, 0x000000FF);

    MarkDirty();
}

void Terrain3D::SetMaterialWeight(int32_t x, int32_t z, int32_t slot, float weight)
{
    if (x < 0 || x >= mResolutionX || z < 0 || z >= mResolutionZ || slot < 0 || slot >= MAX_MATERIAL_SLOTS)
        return;

    size_t idx = z * mResolutionX + x;
    uint32_t packed = mSplatmap[idx];

    // Unpack RGBA bytes
    uint8_t weights[4];
    weights[0] = (packed >> 0) & 0xFF;
    weights[1] = (packed >> 8) & 0xFF;
    weights[2] = (packed >> 16) & 0xFF;
    weights[3] = (packed >> 24) & 0xFF;

    weights[slot] = (uint8_t)(std::max(0.0f, std::min(1.0f, weight)) * 255.0f);

    mSplatmap[idx] = weights[0] | (weights[1] << 8) | (weights[2] << 16) | (weights[3] << 24);
    MarkDirty();
}

float Terrain3D::GetMaterialWeight(int32_t x, int32_t z, int32_t slot) const
{
    if (x < 0 || x >= mResolutionX || z < 0 || z >= mResolutionZ || slot < 0 || slot >= MAX_MATERIAL_SLOTS)
        return 0.0f;

    size_t idx = z * mResolutionX + x;
    uint32_t packed = mSplatmap[idx];

    uint8_t weights[4];
    weights[0] = (packed >> 0) & 0xFF;
    weights[1] = (packed >> 8) & 0xFF;
    weights[2] = (packed >> 16) & 0xFF;
    weights[3] = (packed >> 24) & 0xFF;

    return weights[slot] / 255.0f;
}

void Terrain3D::SetMaterialSlot(int32_t slot, Material* mat)
{
    if (slot >= 0 && slot < MAX_MATERIAL_SLOTS)
    {
        mMaterialSlots[slot] = mat;
    }
}

Material* Terrain3D::GetMaterialSlot(int32_t slot) const
{
    if (slot >= 0 && slot < MAX_MATERIAL_SLOTS)
    {
        return mMaterialSlots[slot].Get<Material>();
    }
    return nullptr;
}

void Terrain3D::BakeSplatmapTexture()
{
    if (!mEnableAtlasTexturing || !mUseMaterialSlots)
    {
        LogWarning("BakeSplatmapTexture: Enable Atlas and Use Material Slots must both be on");
        return;
    }

    Texture* atlasTex = mAtlasTexture.Get<Texture>();
    if (atlasTex == nullptr)
    {
        LogWarning("BakeSplatmapTexture: No atlas texture set");
        return;
    }

    if (atlasTex->GetPixels().empty())
    {
        LogWarning("BakeSplatmapTexture: Atlas texture has no CPU pixel data (try re-importing the texture)");
        return;
    }

    uint32_t atlasW = atlasTex->GetWidth();
    uint32_t atlasH = atlasTex->GetHeight();
    if (atlasW == 0 || atlasH == 0)
        return;

    const std::vector<uint8_t>& atlasPixels = atlasTex->GetPixels();
    uint32_t bakeRes = std::max(mBakeResolution, (uint32_t)64);
    float tiling = std::max(1.0f, mTextureTiling);

    // Tile pixel dimensions in atlas
    uint32_t tilePxW = atlasW / std::max(mAtlasTilesX, (uint32_t)1);
    uint32_t tilePxH = atlasH / std::max(mAtlasTilesY, (uint32_t)1);

    std::vector<uint8_t> bakedPixels(bakeRes * bakeRes * 4);

    for (uint32_t py = 0; py < bakeRes; ++py)
    {
        for (uint32_t px = 0; px < bakeRes; ++px)
        {
            // Terrain UV [0,1]
            float u = (float)px / (bakeRes - 1);
            float v = (float)py / (bakeRes - 1);

            // Grid position (floating-point) for splatmap interpolation
            float gx = u * (mResolutionX - 1);
            float gz = v * (mResolutionZ - 1);

            // Explicit int32_t to keep template deduction working on devkitARM
            // where int32_t is `long int` (different from `int`), so std::max(0,…)
            // would fail to deduce a single T.
            int32_t gx0 = std::max<int32_t>(0, std::min<int32_t>((int32_t)gx, mResolutionX - 2));
            int32_t gz0 = std::max<int32_t>(0, std::min<int32_t>((int32_t)gz, mResolutionZ - 2));
            float fx = gx - gx0;
            float fz = gz - gz0;

            // Bilinear interpolation of splatmap weights
            float slotWeights[MAX_MATERIAL_SLOTS] = {};

            uint32_t s00 = mSplatmap[gz0 * mResolutionX + gx0];
            uint32_t s10 = mSplatmap[gz0 * mResolutionX + gx0 + 1];
            uint32_t s01 = mSplatmap[(gz0 + 1) * mResolutionX + gx0];
            uint32_t s11 = mSplatmap[(gz0 + 1) * mResolutionX + gx0 + 1];

            for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
            {
                int shift = s * 8;
                float w00 = ((s00 >> shift) & 0xFF) / 255.0f;
                float w10 = ((s10 >> shift) & 0xFF) / 255.0f;
                float w01 = ((s01 >> shift) & 0xFF) / 255.0f;
                float w11 = ((s11 >> shift) & 0xFF) / 255.0f;

                float w0 = w00 + (w10 - w00) * fx;
                float w1 = w01 + (w11 - w01) * fx;
                slotWeights[s] = w0 + (w1 - w0) * fz;
            }

            // Normalize weights
            float totalW = 0.0f;
            for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
                totalW += slotWeights[s];
            if (totalW > 0.0f)
            {
                for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
                    slotWeights[s] /= totalW;
            }

            // Tiled UV for sampling the atlas tile texture
            // tiling = "grid cells per tile repeat" (matches non-bake atlas mode)
            // Convert to total repeats: (resolution-1) cells / cellsPerTile
            float repeatsX = (float)(mResolutionX - 1) / tiling;
            float repeatsZ = (float)(mResolutionZ - 1) / tiling;
            float tiledU = u * repeatsX;
            float tiledV = v * repeatsZ;
            float localU = tiledU - std::floor(tiledU);
            float localV = tiledV - std::floor(tiledV);

            // Blend material tile samples weighted by splatmap
            float blendR = 0.0f, blendG = 0.0f, blendB = 0.0f, blendA = 0.0f;

            for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
            {
                if (slotWeights[s] < 0.001f)
                    continue;

                int32_t tile = mSlotAtlasTile[s];
                uint32_t tileCol = tile % mAtlasTilesX;
                uint32_t tileRow = tile / mAtlasTilesX;

                // Sample position within tile
                uint32_t sampleX = tileCol * tilePxW + (uint32_t)(localU * (tilePxW - 1));
                uint32_t sampleY = tileRow * tilePxH + (uint32_t)(localV * (tilePxH - 1));
                sampleX = std::min(sampleX, atlasW - 1);
                sampleY = std::min(sampleY, atlasH - 1);

                uint32_t atlasIdx = (sampleY * atlasW + sampleX) * 4;
                if (atlasIdx + 3 < atlasPixels.size())
                {
                    float w = slotWeights[s];
                    float tR = mSlotTintColor[s].r;
                    float tG = mSlotTintColor[s].g;
                    float tB = mSlotTintColor[s].b;

                    blendR += atlasPixels[atlasIdx + 0] * w * tR;
                    blendG += atlasPixels[atlasIdx + 1] * w * tG;
                    blendB += atlasPixels[atlasIdx + 2] * w * tB;
                    blendA += atlasPixels[atlasIdx + 3] * w;
                }
            }

            uint32_t outIdx = (py * bakeRes + px) * 4;
            bakedPixels[outIdx + 0] = (uint8_t)std::min(255.0f, blendR);
            bakedPixels[outIdx + 1] = (uint8_t)std::min(255.0f, blendG);
            bakedPixels[outIdx + 2] = (uint8_t)std::min(255.0f, blendB);
            bakedPixels[outIdx + 3] = (uint8_t)std::min(255.0f, blendA);
        }
    }

    // Always create a fresh texture (Asset::Create asserts if called twice)
    if (mBakedTexture != nullptr)
    {
        // Unbind from material first
        if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
        {
            mat->SetTexture(0, nullptr);
        }
        mBakedTexture->Destroy();
        mBakedTexture = nullptr;
    }

    mBakedTexture = NewTransientAsset<Texture>();
    mBakedTexture->SetName("TerrainBakedSplatmap");
    mBakedTexture->Init(bakeRes, bakeRes, bakedPixels.data());
    mBakedTexture->Create();

    // Bind to default material
    if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
    {
        mat->SetTexture(0, mBakedTexture);
    }

    LogDebug("BakeSplatmapTexture: Baked %dx%d texture (atlas %dx%d, tiles %dx%d, tiling %.1f, tints: [%.2f,%.2f,%.2f] [%.2f,%.2f,%.2f] [%.2f,%.2f,%.2f] [%.2f,%.2f,%.2f])",
             bakeRes, bakeRes, atlasW, atlasH, mAtlasTilesX, mAtlasTilesY, tiling,
             mSlotTintColor[0].r, mSlotTintColor[0].g, mSlotTintColor[0].b,
             mSlotTintColor[1].r, mSlotTintColor[1].g, mSlotTintColor[1].b,
             mSlotTintColor[2].r, mSlotTintColor[2].g, mSlotTintColor[2].b,
             mSlotTintColor[3].r, mSlotTintColor[3].g, mSlotTintColor[3].b);
}

// Helper: create a texture from pixel data, show save dialog, write to chosen path,
// and register with the asset system. Returns the saved Texture, or nullptr on cancel.
static Texture* SaveTextureAsset(uint32_t width, uint32_t height, uint8_t* pixelData, const std::string& providedPath = "")
{
#if EDITOR
    std::string savePath = providedPath;
    if (savePath.empty())
    {
        savePath = SYS_SaveFileDialog();
        if (savePath.empty())
            return nullptr; // User cancelled
    }

    std::replace(savePath.begin(), savePath.end(), '\\', '/');

    // Ensure .oct extension
    if (savePath.size() < 4 || savePath.substr(savePath.size() - 4) != ".oct")
        savePath += ".oct";

    std::string assetName = Asset::GetNameFromPath(savePath);

    // Check if this asset already exists in the system
    AssetManager* am = AssetManager::Get();
    AssetStub* stub = am->GetAssetStub(assetName);
    Texture* tex = nullptr;

    if (stub != nullptr && stub->mAsset != nullptr)
    {
        // Overwrite existing asset:
        // 1. Unload the old asset
        // 2. Create a fresh texture with the new pixel data
        // 3. Replace the stub's asset pointer
        tex = static_cast<Texture*>(stub->mAsset);
        tex->Destroy();
        delete tex;

        tex = static_cast<Texture*>(Asset::CreateInstance(Texture::GetStaticType()));
        tex->Init(width, height, pixelData);
        tex->Create();
        tex->SetName(assetName);
        tex->SetUuid(stub->mUuid);
        stub->mAsset = tex;
    }
    else
    {
        // Create new texture
        tex = static_cast<Texture*>(Asset::CreateInstance(Texture::GetStaticType()));
        tex->Init(width, height, pixelData);
        tex->Create();
        tex->SetName(assetName);
    }

    // Write directly to the user's chosen path
    tex->SaveFile(savePath.c_str(), Platform::Count);

    // Register with asset manager if not already known
    if (stub == nullptr)
    {
        std::string dirPath = Asset::GetDirectoryFromPath(savePath);
        AssetDir* dir = am->GetAssetDirFromPath(dirPath);
        if (dir == nullptr)
            dir = am->GetRootDirectory();

        stub = am->RegisterAsset(assetName, Texture::GetStaticType(), dir, nullptr, false);
        if (stub != nullptr)
        {
            stub->mAsset = tex;
            stub->mPath = savePath;
            tex->SetUuid(stub->mUuid);
        }
    }

    LogDebug("Terrain3D: Saved texture '%s' to %s", assetName.c_str(), savePath.c_str());
    return tex;
#else
    return nullptr;
#endif
}

void Terrain3D::BakeAndSaveMap(const std::string& savePath)
{
#if EDITOR
    // Try to bake from atlas — if no atlas is set, create a default white texture
    BakeSplatmapTexture();

    std::vector<uint8_t> defaultPixels;
    uint32_t bakeW = 0, bakeH = 0;
    uint8_t* pixelData = nullptr;

    if (mBakedTexture != nullptr && !mBakedTexture->GetPixels().empty())
    {
        bakeW = mBakedTexture->GetWidth();
        bakeH = mBakedTexture->GetHeight();
        pixelData = const_cast<uint8_t*>(mBakedTexture->GetPixels().data());
    }
    else
    {
        // No atlas set — save a default white placeholder texture
        uint32_t res = std::max(mBakeResolution, (uint32_t)64);
        defaultPixels.assign(res * res * 4, 255);
        bakeW = res;
        bakeH = res;
        pixelData = defaultPixels.data();
    }

    // If we already have a saved baked map, reuse its path (auto-overwrite, no dialog)
    std::string resolvedPath = savePath;
    if (resolvedPath.empty())
    {
        Texture* existing = mBakedMapTexture.Get<Texture>();
        if (existing != nullptr)
        {
            AssetStub* existingStub = AssetManager::Get()->GetAssetStub(existing->GetName());
            if (existingStub != nullptr && !existingStub->mPath.empty())
            {
                resolvedPath = existingStub->mPath;
            }
        }
    }

    Texture* savedTex = SaveTextureAsset(bakeW, bakeH, pixelData, resolvedPath);

    if (savedTex != nullptr)
    {
        mBakedMapTexture = savedTex;

        if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
        {
            mat->SetTexture(0, savedTex);
        }
    }
#endif
}

void Terrain3D::BakeAndSaveHeightmap(const std::string& savePath)
{
#if EDITOR

    uint32_t res = (uint32_t)std::max(mResolutionX, mResolutionZ);
    std::vector<uint8_t> pixels(res * res * 4);

    // Find height range for normalization
    float minH = 0.0f, maxH = 0.0f;
    if (!mHeightmap.empty())
    {
        minH = maxH = mHeightmap[0];
        for (float h : mHeightmap)
        {
            if (h < minH) minH = h;
            if (h > maxH) maxH = h;
        }
    }
    float range = maxH - minH;
    if (range < 0.0001f) range = 1.0f;

    for (uint32_t py = 0; py < res; ++py)
    {
        for (uint32_t px = 0; px < res; ++px)
        {
            float u = (float)px / (res - 1);
            float v = (float)py / (res - 1);

            int32_t gx = std::min((int32_t)(u * (mResolutionX - 1)), mResolutionX - 1);
            int32_t gz = std::min((int32_t)(v * (mResolutionZ - 1)), mResolutionZ - 1);

            float h = mHeightmap[gz * mResolutionX + gx];
            uint8_t gray = (uint8_t)(((h - minH) / range) * 255.0f);

            uint32_t idx = (py * res + px) * 4;
            pixels[idx + 0] = gray;
            pixels[idx + 1] = gray;
            pixels[idx + 2] = gray;
            pixels[idx + 3] = 255;
        }
    }

    SaveTextureAsset(res, res, pixels.data(), savePath);
#endif
}

// Compute smooth blend: returns 1.0 inside range, fades to 0.0 at edges over blend distance
static float ComputeRuleWeight(float value, float rangeMin, float rangeMax, float blend)
{
    if (blend <= 0.0f)
        return (value >= rangeMin && value <= rangeMax) ? 1.0f : 0.0f;

    float w = 1.0f;

    // Fade in at bottom edge
    if (value < rangeMin)
        w = 0.0f;
    else if (value < rangeMin + blend)
        w = std::min(w, (value - rangeMin) / blend);

    // Fade out at top edge
    if (value > rangeMax)
        w = 0.0f;
    else if (value > rangeMax - blend)
        w = std::min(w, (rangeMax - value) / blend);

    return std::max(0.0f, w);
}

void Terrain3D::GenerateSplatmapFromRules()
{
    if (mHeightmap.empty() || mResolutionX < 2 || mResolutionZ < 2)
        return;

    float stepX = mWorldWidth / (mResolutionX - 1);

    // Find height range for normalization
    float minH = mHeightmap[0], maxH = mHeightmap[0];
    for (float h : mHeightmap)
    {
        if (h < minH) minH = h;
        if (h > maxH) maxH = h;
    }
    float heightRange = maxH - minH;
    if (heightRange < 0.0001f) heightRange = 1.0f;

    for (int32_t iz = 0; iz < mResolutionZ; ++iz)
    {
        for (int32_t ix = 0; ix < mResolutionX; ++ix)
        {
            size_t idx = iz * mResolutionX + ix;

            // Normalized height [0,1]
            float h = (mHeightmap[idx] - minH) / heightRange;

            // Compute slope angle from normal (central differences)
            float hL = (ix > 0) ? mHeightmap[iz * mResolutionX + (ix - 1)] : mHeightmap[idx];
            float hR = (ix < mResolutionX - 1) ? mHeightmap[iz * mResolutionX + (ix + 1)] : mHeightmap[idx];
            float hD = (iz > 0) ? mHeightmap[(iz - 1) * mResolutionX + ix] : mHeightmap[idx];
            float hU = (iz < mResolutionZ - 1) ? mHeightmap[(iz + 1) * mResolutionX + ix] : mHeightmap[idx];

            glm::vec3 normal = glm::normalize(glm::vec3(
                (hL - hR) * mHeightScale,
                2.0f * stepX,
                (hD - hU) * mHeightScale
            ));

            // Slope: 0° = flat (normal pointing up), 90° = vertical wall
            float slopeDeg = glm::degrees(std::acos(std::max(0.0f, std::min(1.0f, normal.y))));

            // Evaluate each slot's rule
            float weights[MAX_MATERIAL_SLOTS] = {};
            for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
            {
                const SlotRule& rule = mSlotRules[s];
                float heightW = ComputeRuleWeight(h, rule.mHeightMin, rule.mHeightMax, rule.mBlend);
                float slopeW = ComputeRuleWeight(slopeDeg, rule.mSlopeMin, rule.mSlopeMax, rule.mBlend * 90.0f);
                weights[s] = heightW * slopeW * rule.mStrength;
            }

            // Normalize weights to sum to 1
            float total = 0.0f;
            for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
                total += weights[s];

            uint8_t packed[4] = {};
            if (total > 0.0f)
            {
                for (int32_t s = 0; s < MAX_MATERIAL_SLOTS; ++s)
                    packed[s] = (uint8_t)((weights[s] / total) * 255.0f);
            }
            else
            {
                packed[0] = 255; // Default to slot 0
            }

            mSplatmap[idx] = packed[0] | (packed[1] << 8) | (packed[2] << 16) | (packed[3] << 24);
        }
    }

    MarkDirty();
    LogDebug("GenerateSplatmapFromRules: Generated splatmap from elevation/slope rules");
}

void Terrain3D::MarkDirty()
{
    mMeshDirty = true;
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

void Terrain3D::RebuildMesh()
{
    RebuildMeshInternal();
    RecreateCollisionShape();

    // Mark upload dirty so GPU buffers get updated
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

// Helper: compute position for a grid vertex
static glm::vec3 TerrainGridPos(const std::vector<float>& heightmap, int32_t ix, int32_t iz,
                                 int32_t resX, float stepX, float stepZ, float halfW, float halfD, float heightScale)
{
    return glm::vec3(
        ix * stepX - halfW,
        heightmap[iz * resX + ix] * heightScale,
        iz * stepZ - halfD
    );
}

// Helper: compute smooth normal via central differences
static glm::vec3 TerrainGridNormal(const std::vector<float>& heightmap, int32_t ix, int32_t iz,
                                    int32_t resX, int32_t resZ, float stepX, float heightScale)
{
    size_t idx = iz * resX + ix;
    float hL = (ix > 0) ? heightmap[iz * resX + (ix - 1)] : heightmap[idx];
    float hR = (ix < resX - 1) ? heightmap[iz * resX + (ix + 1)] : heightmap[idx];
    float hD = (iz > 0) ? heightmap[(iz - 1) * resX + ix] : heightmap[idx];
    float hU = (iz < resZ - 1) ? heightmap[(iz + 1) * resX + ix] : heightmap[idx];
    return glm::normalize(glm::vec3((hL - hR) * heightScale, 2.0f * stepX, (hD - hU) * heightScale));
}

// Helper: find dominant material slot from packed splatmap value
static int32_t GetDominantSlot(uint32_t packed)
{
    uint8_t w[4] = {
        (uint8_t)((packed >> 0) & 0xFF),
        (uint8_t)((packed >> 8) & 0xFF),
        (uint8_t)((packed >> 16) & 0xFF),
        (uint8_t)((packed >> 24) & 0xFF)
    };
    int32_t best = 0;
    for (int32_t s = 1; s < 4; ++s)
        if (w[s] > w[best]) best = s;
    return best;
}

// Helper: map local UV [0,1] into atlas tile with half-texel inset
static glm::vec2 AtlasTileUV(float localU, float localV, int32_t tile,
                              uint32_t tilesX, uint32_t tilesY, float texW, float texH)
{
    float tileW = 1.0f / (float)tilesX;
    float tileH = 1.0f / (float)tilesY;
    uint32_t tx = tile % tilesX;
    uint32_t ty = tile / tilesX;
    float insetU = (texW > 0.0f) ? (0.5f / texW) : 0.0f;
    float insetV = (texH > 0.0f) ? (0.5f / texH) : 0.0f;
    return glm::vec2(
        (float)tx * tileW + insetU + localU * (tileW - 2.0f * insetU),
        (float)ty * tileH + insetV + localV * (tileH - 2.0f * insetV)
    );
}

void Terrain3D::RebuildMeshInternal()
{
    if (mHeightmap.empty() || mResolutionX < 2 || mResolutionZ < 2)
    {
        mMeshDirty = false;
        return;
    }

    float stepX = mWorldWidth / (mResolutionX - 1);
    float stepZ = mWorldDepth / (mResolutionZ - 1);
    float halfW = mWorldWidth * 0.5f;
    float halfD = mWorldDepth * 0.5f;

    // When bake splatmap is enabled, use simple UVs — the baked texture has the blending baked in
    bool useAtlas = mEnableAtlasTexturing && mAtlasTexture.Get() != nullptr && !mBakeSplatmap;
    float atlasTW = 0.0f, atlasTH = 0.0f;
    if (useAtlas)
    {
        Texture* atlasTex = mAtlasTexture.Get<Texture>();
        atlasTW = (float)atlasTex->GetWidth();
        atlasTH = (float)atlasTex->GetHeight();
    }

    mVertices.clear();
    mIndices.clear();

    if (useAtlas)
    {
        // Atlas mode: per-triangle unique vertices to avoid UV discontinuities
        // at tile repeat boundaries. Same approach as Voxel3D.
        uint32_t numTris = (mResolutionX - 1) * (mResolutionZ - 1) * 2;
        mVertices.reserve(numTris * 3);
        mIndices.reserve(numTris * 3);

        float cellsPerTile = std::max(1.0f, mTextureTiling);

        for (int32_t iz = 0; iz < mResolutionZ - 1; ++iz)
        {
            for (int32_t ix = 0; ix < mResolutionX - 1; ++ix)
            {
                // Four corner grid indices for this quad
                int32_t corners[4][2] = {
                    {ix, iz}, {ix + 1, iz}, {ix, iz + 1}, {ix + 1, iz + 1}
                };

                // Two triangles: (0,2,1) and (1,2,3)
                int32_t tris[2][3] = { {0, 2, 1}, {1, 2, 3} };

                for (int32_t t = 0; t < 2; ++t)
                {
                    // Compute triangle centroid grid position to determine tile period
                    float centX = 0.0f, centZ = 0.0f;
                    for (int32_t c = 0; c < 3; ++c)
                    {
                        centX += (float)corners[tris[t][c]][0];
                        centZ += (float)corners[tris[t][c]][1];
                    }
                    centX /= 3.0f;
                    centZ /= 3.0f;

                    // Determine which tile period the centroid is in
                    float periodX = std::floor(centX / cellsPerTile) * cellsPerTile;
                    float periodZ = std::floor(centZ / cellsPerTile) * cellsPerTile;

                    // Determine dominant material slot ONCE for the whole triangle
                    // using the centroid's nearest grid position. This prevents UV
                    // smearing when vertices at a material boundary pick different tiles.
                    int32_t triSlot = 0;
                    if (mUseMaterialSlots)
                    {
                        int32_t centGX = std::max<int32_t>(0, std::min<int32_t>((int32_t)std::round(centX), mResolutionX - 1));
                        int32_t centGZ = std::max<int32_t>(0, std::min<int32_t>((int32_t)std::round(centZ), mResolutionZ - 1));
                        size_t centIdx = centGZ * mResolutionX + centGX;
                        if (centIdx < mSplatmap.size())
                        {
                            triSlot = GetDominantSlot(mSplatmap[centIdx]);
                        }
                    }

                    int32_t triTile = mSlotAtlasTile[triSlot];
                    uint32_t triColor = mUseMaterialSlots
                        ? ColorFloat4ToUint32(mSlotTintColor[triSlot])
                        : 0xFFFFFFFF;

                    IndexType base = (IndexType)mVertices.size();

                    for (int32_t c = 0; c < 3; ++c)
                    {
                        int32_t gx = corners[tris[t][c]][0];
                        int32_t gz = corners[tris[t][c]][1];

                        VertexColor v;
                        v.mPosition = TerrainGridPos(mHeightmap, gx, gz, mResolutionX,
                                                     stepX, stepZ, halfW, halfD, mHeightScale);
                        v.mNormal = TerrainGridNormal(mHeightmap, gx, gz, mResolutionX,
                                                      mResolutionZ, stepX, mHeightScale);

                        // Compute UV relative to centroid's tile period — always continuous
                        float localU = ((float)gx - periodX) / cellsPerTile;
                        float localV = ((float)gz - periodZ) / cellsPerTile;

                        // All 3 vertices use the same atlas tile (determined by triangle centroid)
                        v.mTexcoord0 = AtlasTileUV(localU, localV, triTile,
                                                    mAtlasTilesX, mAtlasTilesY, atlasTW, atlasTH);
                        v.mTexcoord1 = glm::vec2(0.0f);
                        v.mColor = triColor;

                        mVertices.push_back(v);
                    }

                    mIndices.push_back(base);
                    mIndices.push_back(base + 1);
                    mIndices.push_back(base + 2);
                }
            }
        }
    }
    else
    {
        // Non-atlas mode (or baked splatmap mode): shared vertices with simple UV mapping
        bool isBaked = mBakeSplatmap && mEnableAtlasTexturing;
        mVertices.reserve(mResolutionX * mResolutionZ);

        for (int32_t iz = 0; iz < mResolutionZ; ++iz)
        {
            for (int32_t ix = 0; ix < mResolutionX; ++ix)
            {
                size_t idx = iz * mResolutionX + ix;
                VertexColor v;

                v.mPosition = TerrainGridPos(mHeightmap, ix, iz, mResolutionX,
                                             stepX, stepZ, halfW, halfD, mHeightScale);
                v.mNormal = TerrainGridNormal(mHeightmap, ix, iz, mResolutionX,
                                              mResolutionZ, stepX, mHeightScale);

                if (mDebugSplatmap)
                {
                    // Debug: show raw splatmap as vertex color tint (R=slot0, G=slot1, B=slot2, A=slot3)
                    v.mTexcoord0 = glm::vec2(
                        (float)ix / (mResolutionX - 1),
                        (float)iz / (mResolutionZ - 1)
                    );
                    v.mColor = (idx < mSplatmap.size()) ? mSplatmap[idx] : 0x000000FFu;
                }
                else if (isBaked)
                {
                    // Baked splatmap: UVs are [0,1] — tiling is baked into the texture
                    v.mTexcoord0 = glm::vec2(
                        (float)ix / (mResolutionX - 1),
                        (float)iz / (mResolutionZ - 1)
                    );
                    // White vertex color — blending is in the baked texture
                    v.mColor = 0xFFFFFFFF;
                }
                else
                {
                    v.mTexcoord0 = glm::vec2(
                        (float)ix / (mResolutionX - 1) * mTextureTiling,
                        (float)iz / (mResolutionZ - 1) * mTextureTiling
                    );

                    if (mUseMaterialSlots)
                        v.mColor = (idx < mSplatmap.size()) ? mSplatmap[idx] : 0x000000FFu;
                    else
                        v.mColor = 0xFFFFFFFF;
                }
                v.mTexcoord1 = glm::vec2(0.0f);

                mVertices.push_back(v);
            }
        }

        mIndices.reserve((mResolutionX - 1) * (mResolutionZ - 1) * 6);
        for (int32_t iz = 0; iz < mResolutionZ - 1; ++iz)
        {
            for (int32_t ix = 0; ix < mResolutionX - 1; ++ix)
            {
                IndexType tl = iz * mResolutionX + ix;
                IndexType tr = tl + 1;
                IndexType bl = tl + mResolutionX;
                IndexType br = bl + 1;

                mIndices.push_back(tl);
                mIndices.push_back(bl);
                mIndices.push_back(tr);

                mIndices.push_back(tr);
                mIndices.push_back(bl);
                mIndices.push_back(br);
            }
        }
    }

    mNumVertices = static_cast<uint32_t>(mVertices.size());
    mNumIndices = static_cast<uint32_t>(mIndices.size());

    UpdateBounds();
    mMeshDirty = false;
}

void Terrain3D::UploadMeshData()
{
    uint32_t frameIndex = Renderer::Get()->GetFrameIndex();

    if (mUploadDirty[frameIndex] && mNumVertices > 0 && mNumIndices > 0)
    {
        GFX_UpdateTerrain3DResource(this, mVertices, mIndices);
        mUploadDirty[frameIndex] = false;
    }
}

void Terrain3D::UpdateBounds()
{
    if (mVertices.empty())
    {
        mBounds.mCenter = glm::vec3(0.0f);
        mBounds.mRadius = 0.0f;
        return;
    }

    glm::vec3 minPos = mVertices[0].mPosition;
    glm::vec3 maxPos = mVertices[0].mPosition;

    for (const auto& v : mVertices)
    {
        minPos = glm::min(minPos, v.mPosition);
        maxPos = glm::max(maxPos, v.mPosition);
    }

    mBounds.mCenter = (minPos + maxPos) * 0.5f;
    mBounds.mRadius = glm::length(maxPos - minPos) * 0.5f;
}

void Terrain3D::RecreateCollisionShape()
{
    DestroyTriangleCollisionData();

    if (mVertices.empty() || mIndices.empty())
    {
        SetCollisionShape(GetEmptyCollisionShape());
        return;
    }

    // Build collision vertex/index arrays from the generated mesh
    mCollisionVertices.resize(mVertices.size());
    for (size_t i = 0; i < mVertices.size(); ++i)
    {
        mCollisionVertices[i] = mVertices[i].mPosition;
    }

    mCollisionIndices.resize(mIndices.size());
    for (size_t i = 0; i < mIndices.size(); ++i)
    {
        mCollisionIndices[i] = static_cast<int32_t>(mIndices[i]);
    }

    mTriangleIndexVertexArray = new btTriangleIndexVertexArray();

    btIndexedMesh mesh;
    mesh.m_numTriangles = static_cast<int>(mCollisionIndices.size() / 3);
    mesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(mCollisionIndices.data());
    mesh.m_triangleIndexStride = sizeof(int32_t) * 3;
    mesh.m_numVertices = static_cast<int>(mCollisionVertices.size());
    mesh.m_vertexBase = reinterpret_cast<const unsigned char*>(mCollisionVertices.data());
    mesh.m_vertexStride = sizeof(glm::vec3);

    mTriangleIndexVertexArray->addIndexedMesh(mesh, PHY_INTEGER);

    btBvhTriangleMeshShape* triShape = new btBvhTriangleMeshShape(mTriangleIndexVertexArray, true);

    mTriangleInfoMap = new btTriangleInfoMap();
    btGenerateInternalEdgeInfo(triShape, mTriangleInfoMap);

    SetCollisionShape(triShape);
}

void Terrain3D::DestroyTriangleCollisionData()
{
    if (mTriangleInfoMap != nullptr)
    {
        delete mTriangleInfoMap;
        mTriangleInfoMap = nullptr;
    }

    if (mTriangleIndexVertexArray != nullptr)
    {
        delete mTriangleIndexVertexArray;
        mTriangleIndexVertexArray = nullptr;
    }

    mCollisionVertices.clear();
    mCollisionIndices.clear();
}
