#if EDITOR

#include "ImagePlane/ImagePlaneBuilder.h"

#include "Assets/StaticMesh.h"
#include "Assets/MaterialLite.h"
#include "Assets/Texture.h"
#include "AssetDir.h"
#include "AssetManager.h"
#include "EditorState.h"
#include "EditorUtils.h"
#include "Vertex.h"
#include "Constants.h"

#include <algorithm>

static float ComputeCroppedPixels(float uvSpan, uint32_t texDim)
{
    return uvSpan * float(texDim);
}

void BuildImagePlaneGeometry(const ImagePlaneParams& params, Vertex outVerts[4], IndexType outIndices[6])
{
    Texture* tex = params.mSourceTexture;
    uint32_t texW = tex ? tex->GetWidth() : 1;
    uint32_t texH = tex ? tex->GetHeight() : 1;

    float uMin = glm::clamp(params.mUvMin.x, 0.0f, 1.0f);
    float vMin = glm::clamp(params.mUvMin.y, 0.0f, 1.0f);
    float uMax = glm::clamp(params.mUvMax.x, 0.0f, 1.0f);
    float vMax = glm::clamp(params.mUvMax.y, 0.0f, 1.0f);
    if (uMax <= uMin) uMax = glm::min(1.0f, uMin + 0.0001f);
    if (vMax <= vMin) vMax = glm::min(1.0f, vMin + 0.0001f);

    float cropWpx = ComputeCroppedPixels(uMax - uMin, texW);
    float cropHpx = ComputeCroppedPixels(vMax - vMin, texH);
    if (cropWpx <= 0.0f) cropWpx = 1.0f;
    if (cropHpx <= 0.0f) cropHpx = 1.0f;

    float sizeX = 1.0f;
    float sizeY = 1.0f;
    switch (params.mSizeMode)
    {
    case ImagePlaneSizeMode::AspectWidth1:
        sizeX = cropWpx / cropHpx;
        sizeY = 1.0f;
        break;
    case ImagePlaneSizeMode::AspectHeight1:
        sizeX = 1.0f;
        sizeY = cropHpx / cropWpx;
        break;
    case ImagePlaneSizeMode::PixelsPerUnit:
    {
        float ppu = (params.mPixelsPerUnit > 0.0001f) ? params.mPixelsPerUnit : 100.0f;
        sizeX = cropWpx / ppu;
        sizeY = cropHpx / ppu;
        break;
    }
    default:
        break;
    }

    float ox = 0.0f;
    float oy = 0.0f;
    switch (params.mPivot)
    {
    case ImagePlanePivot::Center:
        ox = -sizeX * 0.5f;
        oy = -sizeY * 0.5f;
        break;
    case ImagePlanePivot::BottomCenter:
        ox = -sizeX * 0.5f;
        oy = 0.0f;
        break;
    case ImagePlanePivot::TopLeft:
        ox = 0.0f;
        oy = -sizeY;
        break;
    default:
        break;
    }

    const glm::vec3 normal(0.0f, 0.0f, 1.0f);

    outVerts[0].mPosition = glm::vec3(ox,         oy,         0.0f);
    outVerts[0].mTexcoord0 = glm::vec2(uMin, vMax);
    outVerts[0].mTexcoord1 = outVerts[0].mTexcoord0;
    outVerts[0].mNormal = normal;

    outVerts[1].mPosition = glm::vec3(ox + sizeX, oy,         0.0f);
    outVerts[1].mTexcoord0 = glm::vec2(uMax, vMax);
    outVerts[1].mTexcoord1 = outVerts[1].mTexcoord0;
    outVerts[1].mNormal = normal;

    outVerts[2].mPosition = glm::vec3(ox + sizeX, oy + sizeY, 0.0f);
    outVerts[2].mTexcoord0 = glm::vec2(uMax, vMin);
    outVerts[2].mTexcoord1 = outVerts[2].mTexcoord0;
    outVerts[2].mNormal = normal;

    outVerts[3].mPosition = glm::vec3(ox,         oy + sizeY, 0.0f);
    outVerts[3].mTexcoord0 = glm::vec2(uMin, vMin);
    outVerts[3].mTexcoord1 = outVerts[3].mTexcoord0;
    outVerts[3].mNormal = normal;

    outIndices[0] = 0;
    outIndices[1] = 1;
    outIndices[2] = 2;
    outIndices[3] = 0;
    outIndices[4] = 2;
    outIndices[5] = 3;
}

void ApplyImagePlaneToAssets(const ImagePlaneParams& params, StaticMesh* mesh, MaterialLite* material)
{
    if (mesh == nullptr || material == nullptr || params.mSourceTexture == nullptr)
        return;

    MaterialLiteParams mp = material->GetLiteParams();
    mp.mShadingModel = params.mShadingModel;
    mp.mBlendMode = params.mBlendMode;
    mp.mCullMode = params.mTwoSided ? CullMode::None : CullMode::Back;
    mp.mNumTextures = 1;
    mp.mTextures[0] = params.mSourceTexture;
    mp.mUvMaps[0] = 0;
    mp.mTevModes[0] = TevMode::Replace;
    mp.mColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    mp.mOpacity = 1.0f;
    material->SetLiteParams(mp);

    Vertex verts[4];
    IndexType indices[6];
    BuildImagePlaneGeometry(params, verts, indices);

    mesh->CreateRaw(4, verts, 6, indices);
    mesh->SetMaterial(material);
}

bool IsImagePlaneCandidate(StaticMesh* mesh)
{
    if (mesh == nullptr)
        return false;
    if (mesh->GetNumVertices() != 4 || mesh->GetNumIndices() != 6)
        return false;
    if (mesh->HasVertexColor())
        return false;

    Material* mat = mesh->GetMaterial();
    if (mat == nullptr || !mat->IsLite())
        return false;

    MaterialLite* matLite = mat->As<MaterialLite>();
    if (matLite == nullptr)
        return false;

    return matLite->GetTexture(0) != nullptr;
}

bool ExtractImagePlaneParams(StaticMesh* mesh, ImagePlaneParams& outParams)
{
    if (!IsImagePlaneCandidate(mesh))
        return false;

    MaterialLite* matLite = mesh->GetMaterial()->As<MaterialLite>();
    Texture* tex = matLite->GetTexture(0);
    outParams.mSourceTexture = tex;

    Vertex* verts = mesh->GetVertices();

    float uMin = verts[0].mTexcoord0.x;
    float uMax = verts[0].mTexcoord0.x;
    float vMin = verts[0].mTexcoord0.y;
    float vMax = verts[0].mTexcoord0.y;
    float xMin = verts[0].mPosition.x;
    float xMax = verts[0].mPosition.x;
    float yMin = verts[0].mPosition.y;
    float yMax = verts[0].mPosition.y;

    for (int i = 1; i < 4; ++i)
    {
        uMin = glm::min(uMin, verts[i].mTexcoord0.x);
        uMax = glm::max(uMax, verts[i].mTexcoord0.x);
        vMin = glm::min(vMin, verts[i].mTexcoord0.y);
        vMax = glm::max(vMax, verts[i].mTexcoord0.y);
        xMin = glm::min(xMin, verts[i].mPosition.x);
        xMax = glm::max(xMax, verts[i].mPosition.x);
        yMin = glm::min(yMin, verts[i].mPosition.y);
        yMax = glm::max(yMax, verts[i].mPosition.y);
    }

    outParams.mUvMin = glm::vec2(uMin, vMin);
    outParams.mUvMax = glm::vec2(uMax, vMax);

    float sizeX = glm::max(xMax - xMin, 0.0001f);
    float sizeY = glm::max(yMax - yMin, 0.0001f);

    auto approx = [](float a, float b) { return glm::abs(a - b) < 0.001f; };

    if (approx(xMin, -sizeX * 0.5f) && approx(yMin, -sizeY * 0.5f))
        outParams.mPivot = ImagePlanePivot::Center;
    else if (approx(xMin, -sizeX * 0.5f) && approx(yMin, 0.0f))
        outParams.mPivot = ImagePlanePivot::BottomCenter;
    else if (approx(xMin, 0.0f) && approx(yMax, 0.0f))
        outParams.mPivot = ImagePlanePivot::TopLeft;
    else
        outParams.mPivot = ImagePlanePivot::Center;

    uint32_t texW = tex ? tex->GetWidth() : 1;
    uint32_t texH = tex ? tex->GetHeight() : 1;
    float cropWpx = (uMax - uMin) * float(texW);
    float cropHpx = (vMax - vMin) * float(texH);

    outParams.mSizeMode = ImagePlaneSizeMode::PixelsPerUnit;
    outParams.mPixelsPerUnit = (sizeX > 0.0001f && cropWpx > 0.0001f) ? (cropWpx / sizeX) : 100.0f;
    (void)cropHpx;

    outParams.mShadingModel = matLite->GetShadingModel();
    outParams.mBlendMode = matLite->GetBlendMode();
    outParams.mTwoSided = (matLite->GetCullMode() == CullMode::None);

    return true;
}

static std::string MakeImagePlaneBaseName(const std::string& texName, const char* prefix)
{
    std::string base = texName;
    if (base.length() >= 2 && base[0] == 'T' && base[1] == '_')
        base = base.substr(2);
    return std::string(prefix) + base;
}

AssetStub* FindOrCreateImagePlaneStubForTexture(AssetStub* textureStub)
{
    if (textureStub == nullptr)
        return nullptr;
    if (textureStub->mAsset == nullptr)
        AssetManager::Get()->LoadAsset(*textureStub);
    if (textureStub->mAsset == nullptr)
        return nullptr;

    Texture* tex = textureStub->mAsset->As<Texture>();
    if (tex == nullptr)
        return nullptr;

    AssetDir* targetDir = textureStub->mDirectory;
    if (targetDir == nullptr || targetDir->mEngineDir || targetDir->mAddonDir)
    {
        targetDir = GetEditorState()->GetAssetDirectory();
    }
    if (targetDir == nullptr || targetDir->mEngineDir || targetDir->mAddonDir)
        return nullptr;

    const std::string meshBaseName = MakeImagePlaneBaseName(tex->GetName(), "SM_");
    const std::string matBaseName  = MakeImagePlaneBaseName(tex->GetName(), "M_");

    AssetStub* existingStub = AssetManager::Get()->GetAssetStub(meshBaseName);
    if (existingStub != nullptr && existingStub->mType == StaticMesh::GetStaticType())
    {
        if (existingStub->mAsset == nullptr)
            AssetManager::Get()->LoadAsset(*existingStub);
        StaticMesh* existing = (existingStub->mAsset != nullptr) ? existingStub->mAsset->As<StaticMesh>() : nullptr;
        if (IsImagePlaneCandidate(existing))
        {
            MaterialLite* existingMat = existing->GetMaterial()->As<MaterialLite>();
            if (existingMat != nullptr && existingMat->GetTexture(0) == tex)
            {
                return existingStub;
            }
        }
        // Note: if the existing stub is present but fails IsImagePlaneCandidate
        // (e.g. a half-saved .oct left over from a prior crash), we fall
        // through to EditorAddUniqueAsset below, which creates a freshly-
        // named SM_<base>_1 / M_<base>_1 pair. Rebuilding the original in
        // place would require re-calling StaticMesh::Create(), and Asset::Create
        // asserts !mLoaded -- there's no public Reload() API yet. The stale
        // stub stays dormant; CreateTriangleCollisionShape's guards keep it
        // from crashing on subsequent loads.
    }

    AssetStub* matStub  = EditorAddUniqueAsset(matBaseName.c_str(),  targetDir, MaterialLite::GetStaticType(), true);
    AssetStub* meshStub = EditorAddUniqueAsset(meshBaseName.c_str(), targetDir, StaticMesh::GetStaticType(),  true);

    MaterialLite* matLite = (matStub  && matStub->mAsset)  ? matStub->mAsset->As<MaterialLite>() : nullptr;
    StaticMesh*   mesh    = (meshStub && meshStub->mAsset) ? meshStub->mAsset->As<StaticMesh>() : nullptr;
    if (matLite == nullptr || mesh == nullptr)
        return nullptr;

    ImagePlaneParams params{};
    params.mSourceTexture = tex;

    ApplyImagePlaneToAssets(params, mesh, matLite);
    AssetManager::Get()->SaveAsset(*matStub);
    AssetManager::Get()->SaveAsset(*meshStub);

    return meshStub;
}

#endif // EDITOR
