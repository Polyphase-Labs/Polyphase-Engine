#pragma once

#if EDITOR

#include "EngineTypes.h"
#include "Maths.h"
#include "Graphics/GraphicsTypes.h"

struct Vertex;
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

#endif // EDITOR
