#pragma once

#if EDITOR

#include "EngineTypes.h"
#include "Maths.h"
#include "Graphics/GraphicsTypes.h"

struct Vertex;
struct AssetStub;
class StaticMesh;
class MaterialLite;
class Texture;

enum class ImagePlanePivot
{
    Center,
    BottomCenter,
    TopLeft,

    Count
};

enum class ImagePlaneSizeMode
{
    AspectWidth1,
    AspectHeight1,
    PixelsPerUnit,

    Count
};

struct ImagePlaneParams
{
    Texture* mSourceTexture = nullptr;
    glm::vec2 mUvMin = { 0.0f, 0.0f };
    glm::vec2 mUvMax = { 1.0f, 1.0f };
    ImagePlanePivot mPivot = ImagePlanePivot::Center;
    ImagePlaneSizeMode mSizeMode = ImagePlaneSizeMode::AspectWidth1;
    float mPixelsPerUnit = 100.0f;
    ShadingModel mShadingModel = ShadingModel::Unlit;
    BlendMode mBlendMode = BlendMode::Opaque;
    bool mTwoSided = false;
};

void BuildImagePlaneGeometry(const ImagePlaneParams& params, Vertex outVerts[4], IndexType outIndices[6]);

void ApplyImagePlaneToAssets(const ImagePlaneParams& params, StaticMesh* mesh, MaterialLite* material);

bool IsImagePlaneCandidate(StaticMesh* mesh);

bool ExtractImagePlaneParams(StaticMesh* mesh, ImagePlaneParams& outParams);

// Returns an AssetStub for an image-plane StaticMesh that fronts the given
// texture, creating the SM_<base> + M_<base> assets with default params
// (Unlit / Opaque / Center / AspectWidth1 / full UV) if no matching
// image-plane mesh already exists. Drag-and-drop entry points use this so
// dropping a Texture into the scene behaves like dropping a StaticMesh.
// Returns nullptr if generation isn't possible (texture not loaded, target
// directory is read-only, etc.).
AssetStub* FindOrCreateImagePlaneStubForTexture(AssetStub* textureStub);

#endif // EDITOR
