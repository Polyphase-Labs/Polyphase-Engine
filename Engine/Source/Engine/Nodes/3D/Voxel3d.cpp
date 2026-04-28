#include "Nodes/3D/Voxel3d.h"
#include "Renderer.h"
#include "AssetManager.h"
#include "Asset.h"
#include "Log.h"

#include "Assets/MaterialLite.h"
#include "Assets/Texture.h"
#include "Nodes/3D/Camera3d.h"
#include "Maths.h"

#include "Graphics/Graphics.h"

#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>

// Disable warnings for PolyVox external library
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4267) // conversion from 'size_t' to 'type', possible loss of data
#endif

#include <PolyVox/RawVolume.h>
#include <PolyVox/Region.h>
#include <PolyVox/CubicSurfaceExtractor.h>
#include <PolyVox/Mesh.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

FORCE_LINK_DEF(Voxel3D);
DEFINE_NODE(Voxel3D, Mesh3D);

bool Voxel3D::HandlePropChange(Datum* datum, uint32_t index, const void* newValue)
{
    Property* prop = static_cast<Property*>(datum);
    OCT_ASSERT(prop != nullptr);
    Voxel3D* voxelComp = static_cast<Voxel3D*>(prop->mOwner);

    // If atlas settings changed, update the material texture binding
    if (MaterialLite* mat = voxelComp->mDefaultMaterial.Get<MaterialLite>())
    {
        Texture* atlasTex = voxelComp->mAtlasTexture.Get<Texture>();
        if (atlasTex != nullptr && voxelComp->mEnableAtlasTexturing)
        {
            mat->SetTexture(0, atlasTex);
        }
    }

    voxelComp->MarkDirty();

    return false;
}

Voxel3D::Voxel3D()
{
    mName = "Voxel";

    // Initialize bounds centered at origin to cover default volume size
    mBounds.mCenter = glm::vec3(0.0f);
    mBounds.mRadius = glm::length(glm::vec3(mDimensions)) * 0.5f;

    MarkDirty();
}

Voxel3D::~Voxel3D()
{

}

const char* Voxel3D::GetTypeName() const
{
    return "Voxel";
}

void Voxel3D::GatherProperties(std::vector<Property>& outProps)
{
    Mesh3D::GatherProperties(outProps);

    {
        SCOPED_CATEGORY("Voxel");
        outProps.push_back(Property(DatumType::Integer, "Dimension X", this, &mDimensions.x, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Dimension Y", this, &mDimensions.y, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Dimension Z", this, &mDimensions.z, 1, HandlePropChange));
    }

    {
        SCOPED_CATEGORY("Atlas Texturing");
        outProps.push_back(Property(DatumType::Bool, "Enable Atlas", this, &mEnableAtlasTexturing, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Asset, "Atlas Texture", this, &mAtlasTexture, 1, HandlePropChange, int32_t(Texture::GetStaticType())));
        outProps.push_back(Property(DatumType::Integer, "Tiles X", this, &mAtlasTilesX, 1, HandlePropChange));
        outProps.push_back(Property(DatumType::Integer, "Tiles Y", this, &mAtlasTilesY, 1, HandlePropChange));
    }
}

void Voxel3D::Create()
{
    Mesh3D::Create();

    // Create the volume
    bool newVolume = (mVolume == nullptr);
    if (newVolume)
    {
        PolyVox::Region region(
            PolyVox::Vector3DInt32(0, 0, 0),
            PolyVox::Vector3DInt32(mDimensions.x - 1, mDimensions.y - 1, mDimensions.z - 1)
        );
        mVolume = new PolyVox::RawVolume<VoxelType>(region);
    }

    // If this is a new volume (not loaded from scene), fill with a test cube
    // Check BEFORE decompressing, since DecompressVoxelData clears mCompressedData.
    bool hasLoadedData = !mCompressedData.empty();

    // Decompress loaded voxel data if any
    DecompressVoxelData();

    if (newVolume && !hasLoadedData)
    {
        // Fill a smaller cube in the center as a test
        int32_t margin = 4;
        FillRegion(margin, margin, margin,
                   mDimensions.x - margin - 1,
                   mDimensions.y - margin - 1,
                   mDimensions.z - margin - 1, 1);
    }

    GFX_CreateVoxel3DResource(this);

    // Build the mesh immediately so bounds are valid for first frame
    if (mMeshDirty)
    {
        RebuildMeshInternal();
    }

    // Create default material with Lit shading for proper lighting
    mDefaultMaterial = MaterialLite::New(LoadAsset<MaterialLite>("M_Default"));
    if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
    {
        mat->SetShadingModel(ShadingModel::Lit);
        mat->SetVertexColorMode(VertexColorMode::Modulate);

        // If atlas texture was loaded from scene, bind it to the material
        Texture* atlasTex = mAtlasTexture.Get<Texture>();
        if (atlasTex != nullptr && mEnableAtlasTexturing)
        {
            mat->SetTexture(0, atlasTex);
        }
    }

    MarkDirty();
}

void Voxel3D::Copy(Node* srcNode, bool recurse)
{
    Mesh3D::Copy(srcNode, recurse);

    if (srcNode->GetType() != Voxel3D::GetStaticType())
        return;

    Voxel3D* src = static_cast<Voxel3D*>(srcNode);

    // Copy volume data
    if (src->mVolume != nullptr)
    {
        // Recreate volume if dimensions differ
        if (mVolume == nullptr || mDimensions != src->mDimensions)
        {
            if (mVolume != nullptr)
                delete mVolume;

            mDimensions = src->mDimensions;
            PolyVox::Region region(
                PolyVox::Vector3DInt32(0, 0, 0),
                PolyVox::Vector3DInt32(mDimensions.x - 1, mDimensions.y - 1, mDimensions.z - 1)
            );
            mVolume = new PolyVox::RawVolume<VoxelType>(region);
        }

        // Copy all voxels
        const auto& region = src->mVolume->getEnclosingRegion();
        for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); ++z)
        {
            for (int32_t y = region.getLowerY(); y <= region.getUpperY(); ++y)
            {
                for (int32_t x = region.getLowerX(); x <= region.getUpperX(); ++x)
                {
                    mVolume->setVoxel(x, y, z, src->mVolume->getVoxel(x, y, z));
                }
            }
        }
    }

    // Copy material table
    mMaterialTable = src->mMaterialTable;

    MarkDirty();
}

void Voxel3D::Destroy()
{
    DestroyTriangleCollisionData();
    GFX_DestroyVoxel3DResource(this);

    if (mVolume != nullptr)
    {
        delete mVolume;
        mVolume = nullptr;
    }

    Mesh3D::Destroy();
}

Voxel3DResource* Voxel3D::GetResource()
{
    return &mResource;
}

void Voxel3D::Tick(float deltaTime)
{
    Mesh3D::Tick(deltaTime);
    TickCommon(deltaTime);
}

void Voxel3D::EditorTick(float deltaTime)
{
    Mesh3D::EditorTick(deltaTime);
    TickCommon(deltaTime);
}

void Voxel3D::TickCommon(float deltaTime)
{
    if (mMeshDirty)
    {
        RebuildMeshInternal();
        RecreateCollisionShape();
    }

    UploadMeshData();
}

void Voxel3D::SaveStream(Stream& stream, Platform platform)
{
    Mesh3D::SaveStream(stream, platform);

    // Write dimensions
    stream.WriteInt32(mDimensions.x);
    stream.WriteInt32(mDimensions.y);
    stream.WriteInt32(mDimensions.z);

    // Write voxel data with RLE compression
    if (mVolume != nullptr)
    {
        std::vector<uint8_t> compressed;
        VoxelType lastVoxel = 0;
        uint32_t runLength = 0;

        const auto& region = mVolume->getEnclosingRegion();
        for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); ++z)
        {
            for (int32_t y = region.getLowerY(); y <= region.getUpperY(); ++y)
            {
                for (int32_t x = region.getLowerX(); x <= region.getUpperX(); ++x)
                {
                    VoxelType v = mVolume->getVoxel(x, y, z);
                    if (v == lastVoxel && runLength < 255)
                    {
                        ++runLength;
                    }
                    else
                    {
                        if (runLength > 0)
                        {
                            compressed.push_back(static_cast<uint8_t>(runLength));
                            compressed.push_back(lastVoxel);
                        }
                        lastVoxel = v;
                        runLength = 1;
                    }
                }
            }
        }
        // Flush final run
        if (runLength > 0)
        {
            compressed.push_back(static_cast<uint8_t>(runLength));
            compressed.push_back(lastVoxel);
        }

        stream.WriteUint32(static_cast<uint32_t>(compressed.size()));
        if (compressed.size() > 0)
        {
            stream.WriteBytes(compressed.data(), static_cast<uint32_t>(compressed.size()));
        }

    }
    else
    {
        stream.WriteUint32(0);
    }

    // Atlas texturing configuration
    stream.WriteBool(mEnableAtlasTexturing);
    stream.WriteAsset(mAtlasTexture);
    stream.WriteUint32(mAtlasTilesX);
    stream.WriteUint32(mAtlasTilesY);

    // Write material table (sparse - only entries with mUseTexture=true)
    uint32_t numMaterials = 0;
    for (uint32_t i = 0; i < 256; ++i)
    {
        if (mMaterialTable[i].mUseTexture)
        {
            ++numMaterials;
        }
    }
    stream.WriteUint32(numMaterials);

    for (uint32_t i = 0; i < 256; ++i)
    {
        if (mMaterialTable[i].mUseTexture)
        {
            stream.WriteUint8(static_cast<uint8_t>(i));
            stream.WriteInt32(mMaterialTable[i].mAtlasTile[0]);
            stream.WriteInt32(mMaterialTable[i].mAtlasTile[1]);
            stream.WriteInt32(mMaterialTable[i].mAtlasTile[2]);
            stream.WriteVec4(mMaterialTable[i].mTintColor);
        }
    }
}

void Voxel3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    Mesh3D::LoadStream(stream, platform, version);

    // Read dimensions
    mDimensions.x = stream.ReadInt32();
    mDimensions.y = stream.ReadInt32();
    mDimensions.z = stream.ReadInt32();

    // Read compressed voxel data
    uint32_t compressedSize = stream.ReadUint32();
    if (compressedSize > 0)
    {
        mCompressedData.resize(compressedSize);
        stream.ReadBytes(mCompressedData.data(), compressedSize);
    }


    // When instantiated from a scene, Create() is called before LoadStream(), so the
    // volume already exists but was filled with default data. Recreate it with the
    // correct dimensions and decompress the loaded voxel data now.
    if (mVolume != nullptr)
    {
        delete mVolume;
        PolyVox::Region region(
            PolyVox::Vector3DInt32(0, 0, 0),
            PolyVox::Vector3DInt32(mDimensions.x - 1, mDimensions.y - 1, mDimensions.z - 1)
        );
        mVolume = new PolyVox::RawVolume<VoxelType>(region);
        DecompressVoxelData();
    }

    // Atlas texturing configuration (added in ASSET_VERSION_VOXEL3D_ATLAS)
    if (version >= ASSET_VERSION_VOXEL3D_ATLAS)
    {
        mEnableAtlasTexturing = stream.ReadBool();
        stream.ReadAsset(mAtlasTexture);
        mAtlasTilesX = stream.ReadUint32();
        mAtlasTilesY = stream.ReadUint32();

        // Read material table
        uint32_t numMaterials = stream.ReadUint32();
        for (uint32_t m = 0; m < numMaterials; ++m)
        {
            uint8_t id = stream.ReadUint8();
            VoxelMaterialInfo& info = mMaterialTable[id];

            if (version >= ASSET_VERSION_VOXEL3D_ATLAS_INT32)
            {
                // New format: int32_t tile indices
                info.mAtlasTile[0] = stream.ReadInt32();
                info.mAtlasTile[1] = stream.ReadInt32();
                info.mAtlasTile[2] = stream.ReadInt32();
            }
            else
            {
                // Old format: uint16_t tile indices
                info.mAtlasTile[0] = static_cast<int32_t>(stream.ReadUint16());
                info.mAtlasTile[1] = static_cast<int32_t>(stream.ReadUint16());
                info.mAtlasTile[2] = static_cast<int32_t>(stream.ReadUint16());
            }

            info.mTintColor = stream.ReadVec4();
            info.mUseTexture = true;
        }
    }

    MarkDirty();
}

bool Voxel3D::IsStaticMesh3D() const
{
    return false;
}

bool Voxel3D::IsSkeletalMesh3D() const
{
    return false;
}

Material* Voxel3D::GetMaterial()
{
    Material* mat = mMaterialOverride.Get<Material>();

    if (mat == nullptr)
    {
        mat = mDefaultMaterial.Get<Material>();
    }

    return mat;
}

void Voxel3D::Render()
{
    GFX_DrawVoxel3D(this);
}

void Voxel3D::SetVoxel(int32_t x, int32_t y, int32_t z, VoxelType value)
{
    if (mVolume != nullptr)
    {
        const auto& region = mVolume->getEnclosingRegion();
        if (region.containsPoint(x, y, z))
        {
            if (mVolume->getVoxel(x, y, z) != value)
            {
                mVolume->setVoxel(x, y, z, value);
                MarkDirty();
            }
        }
    }
}

VoxelType Voxel3D::GetVoxel(int32_t x, int32_t y, int32_t z) const
{
    if (mVolume != nullptr)
    {
        const auto& region = mVolume->getEnclosingRegion();
        if (region.containsPoint(x, y, z))
        {
            return mVolume->getVoxel(x, y, z);
        }
    }
    return 0;
}

glm::vec3 Voxel3D::GetVoxelWorldPosition(int32_t x, int32_t y, int32_t z)
{
    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    // Voxel center in local space.
    // PolyVox's decodeMesh subtracts 0.5 from vertex positions, so a voxel
    // at grid index (x,y,z) has its mesh centered at (x,y,z) in decoded space.
    // After RebuildMeshInternal subtracts centerOffset (halfDims), the mesh
    // center in local space is (x - halfDims).
    float lx = (float(x) - dims.x * 0.5f);
    float ly = (float(y) - dims.y * 0.5f);
    float lz = (float(z) - dims.z * 0.5f);

    return glm::vec3(
        nodePos.x + lx * nodeScale.x,
        nodePos.y + ly * nodeScale.y,
        nodePos.z + lz * nodeScale.z
    );
}

VoxelRayResult Voxel3D::RayTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance)
{
    VoxelRayResult result;

    if (mVolume == nullptr)
        return result;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();
    glm::vec3 invScale = 1.0f / nodeScale;

    // Transform ray into voxel-local space.
    // The +0.5 accounts for PolyVox's decode offset: voxel x's mesh is
    // centered at integer x, spanning [x-0.5, x+0.5]. Adding 0.5 aligns
    // DDA cell boundaries with mesh boundaries so floor() maps correctly.
    glm::vec3 localO = (rayOrigin - nodePos) * invScale + halfDims + 0.5f;
    glm::vec3 localD = rayDir * invScale;

    // Intersect ray with voxel grid AABB [0, dims) in local space
    float tMin = 0.0f;
    float tMax = 1e30f;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (glm::abs(localD[axis]) > 1e-8f)
        {
            float t1 = (0.0f - localO[axis]) / localD[axis];
            float t2 = (dims[axis] - localO[axis]) / localD[axis];
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
        }
        else if (localO[axis] < 0.0f || localO[axis] >= dims[axis])
        {
            return result;
        }
    }

    if (tMin > tMax || tMax < 0.0f)
        return result;
    if (tMin < 0.0f) tMin = 0.0f;

    // Clamp to max ray distance (convert to local-space t)
    float localLen = glm::length(localD);
    float maxLocalT = maxDistance * localLen;
    if (tMin > maxLocalT) return result;
    if (tMax > maxLocalT) tMax = maxLocalT;

    // Entry point in local voxel space
    glm::vec3 start = localO + localD * tMin;

    // Current voxel cell (clamp to valid range)
    glm::ivec3 v = glm::clamp(glm::ivec3(glm::floor(start)), glm::ivec3(0), mDimensions - 1);

    // DDA step direction
    glm::ivec3 step;
    step.x = localD.x >= 0.0f ? 1 : -1;
    step.y = localD.y >= 0.0f ? 1 : -1;
    step.z = localD.z >= 0.0f ? 1 : -1;

    // tDelta: how far along t to cross one full voxel in each axis
    glm::vec3 tDelta;
    tDelta.x = glm::abs(localD.x) > 1e-8f ? glm::abs(1.0f / localD.x) : 1e30f;
    tDelta.y = glm::abs(localD.y) > 1e-8f ? glm::abs(1.0f / localD.y) : 1e30f;
    tDelta.z = glm::abs(localD.z) > 1e-8f ? glm::abs(1.0f / localD.z) : 1e30f;

    // tMax per axis: t value at the next voxel boundary
    glm::ivec3 nextBound;
    nextBound.x = step.x > 0 ? v.x + 1 : v.x;
    nextBound.y = step.y > 0 ? v.y + 1 : v.y;
    nextBound.z = step.z > 0 ? v.z + 1 : v.z;

    glm::vec3 tMaxAxis;
    tMaxAxis.x = glm::abs(localD.x) > 1e-8f ? (float(nextBound.x) - localO.x) / localD.x : 1e30f;
    tMaxAxis.y = glm::abs(localD.y) > 1e-8f ? (float(nextBound.y) - localO.y) / localD.y : 1e30f;
    tMaxAxis.z = glm::abs(localD.z) > 1e-8f ? (float(nextBound.z) - localO.z) / localD.z : 1e30f;

    glm::ivec3 prevVoxel(-1, -1, -1);
    int maxIter = mDimensions.x + mDimensions.y + mDimensions.z + 1;

    for (int i = 0; i < maxIter; ++i)
    {
        if (v.x < 0 || v.x >= mDimensions.x ||
            v.y < 0 || v.y >= mDimensions.y ||
            v.z < 0 || v.z >= mDimensions.z)
            break;

        // Distance check
        float currentT = glm::min(tMaxAxis.x, glm::min(tMaxAxis.y, tMaxAxis.z));
        if (currentT > maxLocalT)
            break;

        VoxelType val = mVolume->getVoxel(v.x, v.y, v.z);

        if (val > 0)
        {
            // Compute world-space hit position
            float entryT = currentT - glm::min(tDelta.x, glm::min(tDelta.y, tDelta.z));
            if (entryT < tMin) entryT = tMin;
            float worldT = entryT / localLen;
            glm::vec3 hitPos = rayOrigin + rayDir * worldT;

            result.mHit = true;
            result.mVoxel = v;
            result.mPrevVoxel = prevVoxel;
            result.mHitPosition = hitPos;
            result.mValue = val;
            return result;
        }

        prevVoxel = v;

        // Step to next voxel
        if (tMaxAxis.x < tMaxAxis.y)
        {
            if (tMaxAxis.x < tMaxAxis.z) { v.x += step.x; tMaxAxis.x += tDelta.x; }
            else { v.z += step.z; tMaxAxis.z += tDelta.z; }
        }
        else
        {
            if (tMaxAxis.y < tMaxAxis.z) { v.y += step.y; tMaxAxis.y += tDelta.y; }
            else { v.z += step.z; tMaxAxis.z += tDelta.z; }
        }
    }

    return result;
}

VoxelRayResult Voxel3D::RayTestScreen(Camera3D* camera, int32_t screenX, int32_t screenY, float maxDistance)
{
    if (camera == nullptr)
        return {};

    glm::vec3 origin = camera->GetWorldPosition();
    glm::vec3 target = camera->ScreenToWorldPosition(screenX, screenY);
    glm::vec3 dir = Maths::SafeNormalize(target - origin);
    return RayTest(origin, dir, maxDistance);
}

VoxelRayResult Voxel3D::RayTestCenterCamera(Camera3D* camera, float maxDistance)
{
    if (camera == nullptr)
        return {};

    glm::vec3 origin = camera->GetWorldPosition();
    glm::vec3 dir = camera->GetForwardVector();
    return RayTest(origin, dir, maxDistance);
}

VoxelPointResult Voxel3D::GetVoxelAtWorldPosition(const glm::vec3& worldPos)
{
    VoxelPointResult result;

    if (mVolume == nullptr)
        return result;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    // Convert world position to voxel coordinates.
    // Uses the same +0.5 PolyVox alignment as RayTest.
    glm::vec3 localPos = (worldPos - nodePos) / nodeScale + halfDims + 0.5f;
    glm::ivec3 v = glm::ivec3(glm::floor(localPos));

    if (v.x < 0 || v.x >= mDimensions.x ||
        v.y < 0 || v.y >= mDimensions.y ||
        v.z < 0 || v.z >= mDimensions.z)
        return result;

    result.mValid = true;
    result.mVoxel = v;
    result.mValue = mVolume->getVoxel(v.x, v.y, v.z);
    result.mWorldPosition = GetVoxelWorldPosition(v.x, v.y, v.z);
    return result;
}

void Voxel3D::Fill(VoxelType value)
{
    if (mVolume != nullptr)
    {
        const auto& region = mVolume->getEnclosingRegion();
        for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); ++z)
        {
            for (int32_t y = region.getLowerY(); y <= region.getUpperY(); ++y)
            {
                for (int32_t x = region.getLowerX(); x <= region.getUpperX(); ++x)
                {
                    mVolume->setVoxel(x, y, z, value);
                }
            }
        }
        MarkDirty();
    }
}

void Voxel3D::FillRegion(int32_t x0, int32_t y0, int32_t z0,
                          int32_t x1, int32_t y1, int32_t z1, VoxelType value)
{
    if (mVolume != nullptr)
    {
        const auto& region = mVolume->getEnclosingRegion();

        // Clamp to volume bounds
        x0 = glm::max(x0, region.getLowerX());
        y0 = glm::max(y0, region.getLowerY());
        z0 = glm::max(z0, region.getLowerZ());
        x1 = glm::min(x1, region.getUpperX());
        y1 = glm::min(y1, region.getUpperY());
        z1 = glm::min(z1, region.getUpperZ());

        for (int32_t z = z0; z <= z1; ++z)
        {
            for (int32_t y = y0; y <= y1; ++y)
            {
                for (int32_t x = x0; x <= x1; ++x)
                {
                    mVolume->setVoxel(x, y, z, value);
                }
            }
        }
        MarkDirty();
    }
}

// Helper: convert world position to voxel-local coords (with +0.5 PolyVox alignment)
static glm::vec3 WorldToVoxelLocal(const glm::vec3& worldPos, const glm::vec3& nodePos,
                                    const glm::vec3& nodeScale, const glm::vec3& halfDims)
{
    return (worldPos - nodePos) / nodeScale + halfDims + 0.5f;
}

void Voxel3D::FillSphere(const glm::vec3& worldCenter, float radius, VoxelType value)
{
    if (mVolume == nullptr) return;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    glm::vec3 localCenter = WorldToVoxelLocal(worldCenter, nodePos, nodeScale, halfDims);
    glm::vec3 radiusInVoxels = radius / nodeScale;

    glm::ivec3 lo = glm::max(glm::ivec3(glm::floor(localCenter - radiusInVoxels)), glm::ivec3(0));
    glm::ivec3 hi = glm::min(glm::ivec3(glm::floor(localCenter + radiusInVoxels)), mDimensions - 1);
    float r2 = radius * radius;

    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x)
            {
                glm::vec3 voxWorld = GetVoxelWorldPosition(x, y, z);
                if (glm::dot(voxWorld - worldCenter, voxWorld - worldCenter) <= r2)
                    mVolume->setVoxel(x, y, z, value);
            }
    MarkDirty();
}

void Voxel3D::FillCylinder(const glm::vec3& worldCenter, float radius, float height, int32_t axis, VoxelType value)
{
    if (mVolume == nullptr || axis < 0 || axis > 2) return;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    glm::vec3 localCenter = WorldToVoxelLocal(worldCenter, nodePos, nodeScale, halfDims);
    glm::vec3 extent = radius / nodeScale;
    extent[axis] = (height * 0.5f) / nodeScale[axis];

    glm::ivec3 lo = glm::max(glm::ivec3(glm::floor(localCenter - extent)), glm::ivec3(0));
    glm::ivec3 hi = glm::min(glm::ivec3(glm::floor(localCenter + extent)), mDimensions - 1);
    float r2 = radius * radius;
    float halfH = height * 0.5f;

    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x)
            {
                glm::vec3 voxWorld = GetVoxelWorldPosition(x, y, z);
                glm::vec3 diff = voxWorld - worldCenter;
                if (glm::abs(diff[axis]) > halfH) continue;
                float perpDist2 = 0.0f;
                for (int a = 0; a < 3; ++a)
                    if (a != axis) perpDist2 += diff[a] * diff[a];
                if (perpDist2 <= r2)
                    mVolume->setVoxel(x, y, z, value);
            }
    MarkDirty();
}

std::vector<VoxelInfo> Voxel3D::GetVoxelsInSphere(const glm::vec3& worldCenter, float radius)
{
    std::vector<VoxelInfo> results;
    if (mVolume == nullptr) return results;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    glm::vec3 localCenter = WorldToVoxelLocal(worldCenter, nodePos, nodeScale, halfDims);
    glm::vec3 radiusInVoxels = radius / nodeScale;

    glm::ivec3 lo = glm::max(glm::ivec3(glm::floor(localCenter - radiusInVoxels)), glm::ivec3(0));
    glm::ivec3 hi = glm::min(glm::ivec3(glm::floor(localCenter + radiusInVoxels)), mDimensions - 1);
    float r2 = radius * radius;

    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x)
            {
                glm::vec3 voxWorld = GetVoxelWorldPosition(x, y, z);
                if (glm::dot(voxWorld - worldCenter, voxWorld - worldCenter) <= r2)
                {
                    VoxelInfo info;
                    info.mCoord = { x, y, z };
                    info.mWorldPosition = voxWorld;
                    info.mValue = mVolume->getVoxel(x, y, z);
                    results.push_back(info);
                }
            }
    return results;
}

std::vector<VoxelInfo> Voxel3D::GetVoxelsInBox(const glm::vec3& worldMin, const glm::vec3& worldMax)
{
    std::vector<VoxelInfo> results;
    if (mVolume == nullptr) return results;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    glm::vec3 localMin = WorldToVoxelLocal(worldMin, nodePos, nodeScale, halfDims);
    glm::vec3 localMax = WorldToVoxelLocal(worldMax, nodePos, nodeScale, halfDims);

    glm::ivec3 lo = glm::max(glm::ivec3(glm::floor(glm::min(localMin, localMax))), glm::ivec3(0));
    glm::ivec3 hi = glm::min(glm::ivec3(glm::floor(glm::max(localMin, localMax))), mDimensions - 1);

    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x)
            {
                VoxelInfo info;
                info.mCoord = { x, y, z };
                info.mWorldPosition = GetVoxelWorldPosition(x, y, z);
                info.mValue = mVolume->getVoxel(x, y, z);
                results.push_back(info);
            }
    return results;
}

std::vector<VoxelInfo> Voxel3D::GetVoxelsInCylinder(const glm::vec3& worldCenter, float radius, float height, int32_t axis)
{
    std::vector<VoxelInfo> results;
    if (mVolume == nullptr || axis < 0 || axis > 2) return results;

    glm::vec3 dims = glm::vec3(mDimensions);
    glm::vec3 halfDims = dims * 0.5f;
    glm::vec3 nodePos = GetWorldPosition();
    glm::vec3 nodeScale = GetWorldScale();

    glm::vec3 localCenter = WorldToVoxelLocal(worldCenter, nodePos, nodeScale, halfDims);
    glm::vec3 extent = radius / nodeScale;
    extent[axis] = (height * 0.5f) / nodeScale[axis];

    glm::ivec3 lo = glm::max(glm::ivec3(glm::floor(localCenter - extent)), glm::ivec3(0));
    glm::ivec3 hi = glm::min(glm::ivec3(glm::floor(localCenter + extent)), mDimensions - 1);
    float r2 = radius * radius;
    float halfH = height * 0.5f;

    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x)
            {
                glm::vec3 voxWorld = GetVoxelWorldPosition(x, y, z);
                glm::vec3 diff = voxWorld - worldCenter;
                if (glm::abs(diff[axis]) > halfH) continue;
                float perpDist2 = 0.0f;
                for (int a = 0; a < 3; ++a)
                    if (a != axis) perpDist2 += diff[a] * diff[a];
                if (perpDist2 <= r2)
                {
                    VoxelInfo info;
                    info.mCoord = { x, y, z };
                    info.mWorldPosition = voxWorld;
                    info.mValue = mVolume->getVoxel(x, y, z);
                    results.push_back(info);
                }
            }
    return results;
}

std::vector<VoxelInfo> Voxel3D::GetVoxelNeighbors(int32_t x, int32_t y, int32_t z)
{
    static const glm::ivec3 offsets[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    std::vector<VoxelInfo> results;
    results.reserve(6);

    for (int i = 0; i < 6; ++i)
    {
        glm::ivec3 n = glm::ivec3(x, y, z) + offsets[i];
        VoxelInfo info;
        info.mCoord = n;
        info.mWorldPosition = GetVoxelWorldPosition(n.x, n.y, n.z);

        if (n.x >= 0 && n.x < mDimensions.x &&
            n.y >= 0 && n.y < mDimensions.y &&
            n.z >= 0 && n.z < mDimensions.z &&
            mVolume != nullptr)
        {
            info.mValue = mVolume->getVoxel(n.x, n.y, n.z);
        }

        results.push_back(info);
    }
    return results;
}

void Voxel3D::MarkDirty()
{
    mMeshDirty = true;
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

void Voxel3D::RebuildMesh()
{
    RebuildMeshInternal();
    RecreateCollisionShape();

    // Mark upload dirty so GPU buffers get updated
    for (int i = 0; i < MAX_FRAMES; ++i)
    {
        mUploadDirty[i] = true;
    }
}

void Voxel3D::SetDimensions(glm::ivec3 dims)
{
    if (mDimensions != dims)
    {
        mDimensions = dims;

        // Recreate volume if it exists
        if (mVolume != nullptr)
        {
            delete mVolume;
            PolyVox::Region region(
                PolyVox::Vector3DInt32(0, 0, 0),
                PolyVox::Vector3DInt32(mDimensions.x - 1, mDimensions.y - 1, mDimensions.z - 1)
            );
            mVolume = new PolyVox::RawVolume<VoxelType>(region);
        }

        MarkDirty();
    }
}

Bounds Voxel3D::GetLocalBounds() const
{
    return mBounds;
}

// Helper: Determine face type from normal
static VoxelFace GetFaceFromNormal(const glm::vec3& n)
{
    if (n.y > 0.5f) return VoxelFace::Top;
    if (n.y < -0.5f) return VoxelFace::Bottom;
    return VoxelFace::Side;
}

// Helper: Project world position onto face plane (returns 2D coordinates)
static glm::vec2 ProjectToFacePlane(const glm::vec3& pos, const glm::vec3& normal)
{
    if (glm::abs(normal.x) > 0.5f)
        return glm::vec2(pos.z, pos.y);  // X-face: use Z, Y
    else if (glm::abs(normal.y) > 0.5f)
        return glm::vec2(pos.x, pos.z);  // Y-face: use X, Z
    else
        return glm::vec2(pos.x, pos.y);  // Z-face: use X, Y
}

// Helper: Convert local UV to atlas UV for a given tile
static glm::vec2 LocalToAtlasUV(glm::vec2 local, int32_t tile, uint32_t tilesX, uint32_t tilesY)
{
    float tileW = 1.0f / static_cast<float>(tilesX);
    float tileH = 1.0f / static_cast<float>(tilesY);
    uint32_t tx = tile % tilesX;
    uint32_t ty = tile / tilesX;
    return glm::vec2(
        static_cast<float>(tx) * tileW + local.x * tileW,
        static_cast<float>(ty) * tileH + local.y * tileH
    );
}

void Voxel3D::RebuildMeshInternal()
{
    if (mVolume == nullptr)
    {
        mMeshDirty = false;
        return;
    }

    // Extract cubic mesh from volume.
    // PolyVox only checks negative neighbors, so faces at the upper boundary
    // (e.g., between voxel at x=31 and air at x=32) are missed unless we
    // expand the extraction region by 1 on the upper side.
    auto enclosing = mVolume->getEnclosingRegion();
    PolyVox::Region region(
        enclosing.getLowerCorner(),
        enclosing.getUpperCorner() + PolyVox::Vector3DInt32(1, 1, 1)
    );
    // Disable quad merging so each voxel face is a separate 1x1 quad.
    // This is required for correct per-voxel atlas UV mapping.
    auto mesh = PolyVox::extractCubicMesh(mVolume, region, PolyVox::DefaultIsQuadNeeded<VoxelType>(), false);
    auto decodedMesh = PolyVox::decodeMesh(mesh);

    // Convert to engine vertex format
    // Offset to center mesh around origin
    glm::vec3 centerOffset = glm::vec3(mDimensions) * 0.5f;

    // Build a temporary vertex array from the decoded mesh
    size_t numDecodedVerts = decodedMesh.getNoOfVertices();
    std::vector<VertexColor> decoded;
    decoded.reserve(numDecodedVerts);
    for (uint32_t i = 0; i < numDecodedVerts; ++i)
    {
        const auto& pv = decodedMesh.getVertex(i);
        VertexColor v;
        v.mPosition = glm::vec3(pv.position.getX(), pv.position.getY(), pv.position.getZ()) - centerOffset;
        v.mNormal = glm::vec3(0.0f);
        v.mTexcoord0 = glm::vec2(0.0f);
        // Store material ID in texcoord1.x for atlas UV lookup later
        v.mTexcoord1 = glm::vec2(static_cast<float>(pv.data), 0.0f);
        v.mColor = ColorFloat4ToUint32(MaterialIdToColor(pv.data));
        decoded.push_back(v);
    }

    // PolyVox's cubic extractor shares vertices across face boundaries.
    // Assigning face normals in-place causes the last triangle touching a shared
    // vertex to overwrite the normal, producing incorrect shading on adjacent faces.
    // Fix: unpack every triangle to 3 unique vertices (flat shading) so no vertex
    // is shared across faces. This gives each face a clean, correct normal.
    size_t numIndices = decodedMesh.getNoOfIndices();
    mVertices.clear();
    mVertices.reserve(numIndices); // 3 unique verts per triangle corner
    mIndices.clear();
    mIndices.reserve(numIndices);

    for (size_t i = 0; i + 2 < numIndices; i += 3)
    {
        VertexColor v0 = decoded[decodedMesh.getIndex(static_cast<uint32_t>(i))];
        VertexColor v1 = decoded[decodedMesh.getIndex(static_cast<uint32_t>(i + 1))];
        VertexColor v2 = decoded[decodedMesh.getIndex(static_cast<uint32_t>(i + 2))];

        glm::vec3 edge1 = v1.mPosition - v0.mPosition;
        glm::vec3 edge2 = v2.mPosition - v0.mPosition;
        glm::vec3 cross = glm::cross(edge1, edge2);
        float len = glm::length(cross);
        glm::vec3 n = (len > 1e-6f) ? (cross / len) : glm::vec3(0.0f, 1.0f, 0.0f);

        v0.mNormal = v1.mNormal = v2.mNormal = n;

        // Generate atlas UVs if enabled
        if (mEnableAtlasTexturing && mAtlasTexture.Get() != nullptr)
        {
            // All vertices in a face share the same material ID (stored in texcoord1.x)
            VoxelType matId = static_cast<VoxelType>(v0.mTexcoord1.x);
            const VoxelMaterialInfo& info = mMaterialTable[matId];

            if (info.mUseTexture)
            {
                VoxelFace face = GetFaceFromNormal(n);
                int32_t tile = info.mAtlasTile[static_cast<int>(face)];

                // Project positions onto face plane.
                // PolyVox applies a -0.5 offset to decoded positions, so add it
                // back to get clean integer grid coordinates for UV mapping.
                glm::vec3 uvOffset = centerOffset + glm::vec3(0.5f);
                glm::vec2 p0 = ProjectToFacePlane(v0.mPosition + uvOffset, n);
                glm::vec2 p1 = ProjectToFacePlane(v1.mPosition + uvOffset, n);
                glm::vec2 p2 = ProjectToFacePlane(v2.mPosition + uvOffset, n);

                // Compute UV relative to triangle minimum.
                // glm::fract fails for integers (fract(5.0)=0), so instead
                // subtract the floor of the min to get UVs in [0,1] range.
                glm::vec2 minP = glm::min(glm::min(p0, p1), p2);
                glm::vec2 origin = glm::floor(minP);
                glm::vec2 uv0 = p0 - origin;
                glm::vec2 uv1 = p1 - origin;
                glm::vec2 uv2 = p2 - origin;

                // Convert to atlas coordinates
                v0.mTexcoord0 = LocalToAtlasUV(uv0, tile, mAtlasTilesX, mAtlasTilesY);
                v1.mTexcoord0 = LocalToAtlasUV(uv1, tile, mAtlasTilesX, mAtlasTilesY);
                v2.mTexcoord0 = LocalToAtlasUV(uv2, tile, mAtlasTilesX, mAtlasTilesY);

                // Apply tint color
                uint32_t tintColor = ColorFloat4ToUint32(info.mTintColor);
                v0.mColor = v1.mColor = v2.mColor = tintColor;
            }
        }

        IndexType base = static_cast<IndexType>(mVertices.size());
        mVertices.push_back(v0);
        mVertices.push_back(v1);
        mVertices.push_back(v2);
        mIndices.push_back(base);
        mIndices.push_back(base + 1);
        mIndices.push_back(base + 2);
    }

    mNumVertices = static_cast<uint32_t>(mVertices.size());
    mNumIndices = static_cast<uint32_t>(mIndices.size());

    UpdateBounds();
    mMeshDirty = false;
}

void Voxel3D::UploadMeshData()
{
    uint32_t frameIndex = Renderer::Get()->GetFrameIndex();

    if (mUploadDirty[frameIndex] && mNumVertices > 0 && mNumIndices > 0)
    {
        GFX_UpdateVoxel3DResource(this, mVertices, mIndices);
        mUploadDirty[frameIndex] = false;
    }
}

void Voxel3D::UpdateBounds()
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

void Voxel3D::RecreateCollisionShape()
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

void Voxel3D::DestroyTriangleCollisionData()
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

void Voxel3D::DecompressVoxelData()
{
    if (mCompressedData.empty() || mVolume == nullptr)
    {
        return;
    }

    const auto& region = mVolume->getEnclosingRegion();
    size_t dataIndex = 0;

    int32_t x = region.getLowerX();
    int32_t y = region.getLowerY();
    int32_t z = region.getLowerZ();

    while (dataIndex + 1 < mCompressedData.size())
    {
        uint8_t runLength = mCompressedData[dataIndex++];
        VoxelType value = mCompressedData[dataIndex++];

        for (uint32_t i = 0; i < runLength; ++i)
        {
            if (z > region.getUpperZ())
            {
                break;
            }

            mVolume->setVoxel(x, y, z, value);

            ++x;
            if (x > region.getUpperX())
            {
                x = region.getLowerX();
                ++y;
                if (y > region.getUpperY())
                {
                    y = region.getLowerY();
                    ++z;
                }
            }
        }
    }

    mCompressedData.clear();
}

glm::vec4 Voxel3D::MaterialIdToColor(VoxelType materialId) const
{
    // Simple material ID to color mapping
    // 0 = air (shouldn't render)
    // 1-7 = basic colors
    switch (materialId)
    {
        case 0: return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Air
        case 1: return glm::vec4(0.8f, 0.8f, 0.8f, 1.0f); // White/Stone
        case 2: return glm::vec4(0.6f, 0.4f, 0.2f, 1.0f); // Brown/Dirt
        case 3: return glm::vec4(0.2f, 0.8f, 0.2f, 1.0f); // Green/Grass
        case 4: return glm::vec4(0.2f, 0.4f, 0.8f, 1.0f); // Blue/Water
        case 5: return glm::vec4(0.8f, 0.2f, 0.2f, 1.0f); // Red
        case 6: return glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); // Yellow
        case 7: return glm::vec4(0.4f, 0.4f, 0.4f, 1.0f); // Gray
        default:
        {
            // Generate a color from material ID for IDs > 7
            float hue = (materialId * 0.618033988749895f);
            hue = hue - glm::floor(hue);
            // Simple HSV to RGB (saturation=0.7, value=0.9)
            float h = hue * 6.0f;
            float c = 0.9f * 0.7f;
            float x = c * (1.0f - glm::abs(glm::mod(h, 2.0f) - 1.0f));
            float m = 0.9f - c;
            glm::vec3 rgb;
            if (h < 1.0f) rgb = glm::vec3(c, x, 0.0f);
            else if (h < 2.0f) rgb = glm::vec3(x, c, 0.0f);
            else if (h < 3.0f) rgb = glm::vec3(0.0f, c, x);
            else if (h < 4.0f) rgb = glm::vec3(0.0f, x, c);
            else if (h < 5.0f) rgb = glm::vec3(x, 0.0f, c);
            else rgb = glm::vec3(c, 0.0f, x);
            return glm::vec4(rgb + m, 1.0f);
        }
    }
}

// Atlas texturing API

void Voxel3D::SetAtlasTexture(Texture* atlas, uint32_t tilesX, uint32_t tilesY)
{
    mAtlasTexture = atlas;
    mAtlasTilesX = tilesX;
    mAtlasTilesY = tilesY;

    // Only set texture on material when we have a valid atlas.
    // Setting nullptr would make the shader sample a black default texture,
    // which with Modulate vertex color mode produces all-black output.
    if (atlas != nullptr)
    {
        if (MaterialLite* mat = mDefaultMaterial.Get<MaterialLite>())
        {
            mat->SetTexture(0, atlas);
        }
    }

    MarkDirty();
}

Texture* Voxel3D::GetAtlasTexture() const
{
    return mAtlasTexture.Get<Texture>();
}

void Voxel3D::SetAtlasEnabled(bool enabled)
{
    if (mEnableAtlasTexturing != enabled)
    {
        mEnableAtlasTexturing = enabled;
        MarkDirty();
    }
}

void Voxel3D::SetMaterialTexture(VoxelType id, int32_t topTile, int32_t bottomTile, int32_t sideTile)
{
    VoxelMaterialInfo& info = mMaterialTable[id];
    info.mAtlasTile[static_cast<int>(VoxelFace::Top)] = topTile;
    info.mAtlasTile[static_cast<int>(VoxelFace::Bottom)] = bottomTile;
    info.mAtlasTile[static_cast<int>(VoxelFace::Side)] = sideTile;
    info.mUseTexture = true;
    MarkDirty();
}

void Voxel3D::SetMaterialTexture(VoxelType id, int32_t allFacesTile)
{
    SetMaterialTexture(id, allFacesTile, allFacesTile, allFacesTile);
}

void Voxel3D::SetMaterialTint(VoxelType id, const glm::vec4& tint)
{
    mMaterialTable[id].mTintColor = tint;
    MarkDirty();
}

void Voxel3D::DisableMaterialTexture(VoxelType id)
{
    mMaterialTable[id].mUseTexture = false;
    MarkDirty();
}
