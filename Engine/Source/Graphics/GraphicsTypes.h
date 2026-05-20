#pragma once

#include <stdint.h>
#include <limits.h>
#include "Graphics/GraphicsConstants.h"
#include "Vertex.h"

#if API_VULKAN
#include "Graphics/Vulkan/Buffer.h"
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/MultiBuffer.h"
#include "Graphics/Vulkan/DescriptorSet.h"
#elif API_GX
#include <gccore.h>
#elif API_C3D
#include <3ds.h>
#include <citro3d.h>
#include "Graphics/C3D/DoubleBuffer.h"
#endif

#if API_VULKAN
extern class VulkanContext* gVulkanContext;
class Pipeline;
class Shader;
#elif API_GX
extern struct GxContext gGxContext;
#elif API_C3D
extern struct C3dContext gC3dContext;
#endif

struct GraphicsState
{
#if API_VULKAN
    class VulkanContext* mVulkanContext = gVulkanContext;
#elif API_GX
    struct GxContext* mGxContext = &gGxContext;
#elif API_C3D
    struct C3dContext* mC3dContext = &gC3dContext;
#endif

    float mResolutionScale = 1.0f;
};

enum class PixelFormat
{
    LA4,
    RGB565,
    RGBA8,
    CMPR,
    RGBA5551,

    R8,
    R32U,
    R32F,
    RGBA16F,

    Depth24Stencil8,
    Depth32FStencil8,
    Depth16,
    Depth32F,

    Count
};

enum class PipelineConfig
{
    Shadow,
    Forward,
    Opaque,
    Translucent,
    Additive,
    ShadowMeshBack,
    ShadowMeshFront,
    PostProcess,
    NullPostProcess,
    Quad,
    Text,
    Poly,
    Selected,
    HitCheck,
    Wireframe,
    Collision,
    Line,

    Count
};

enum class RenderPassId
{
    Shadows,
    Forward,
    Selected,
    Ui,
    Clear,

    HitCheck,

    Count
};

enum class FilterType
{
    Nearest,
    Linear,

    Count
};

enum class WrapMode
{
    Clamp,
    Repeat,
    Mirror,

    Count
};

#if API_VULKAN
typedef uint32_t IndexType;
#else
typedef uint16_t IndexType;
#endif

struct TextureResource
{
#if API_VULKAN
    Image* mImage = nullptr;
#elif defined(POLYPHASE_PLATFORM_ADDON)
    // PSPGU / generic-addon storage. Fields are POD; addon code owns them.
    void*    mPixels    = nullptr;
    uint32_t mWidth     = 0;
    uint32_t mHeight    = 0;
    uint32_t mBufWidth  = 0;     // pitch in pixels (power-of-two or 16-aligned)
    uint16_t mPsm       = 0;     // GU_PSM_* value
    uint8_t  mMipCount  = 0;
    uint8_t  mSwizzled  = 0;     // 0 = linear, 1 = swizzled
#elif API_GX
    GXTexObj mGxTexObj = {};
    TPLFile mTplFile = {};
    void* mTplData = nullptr;
    // Streaming RGBA8 path (video player). The GX hardware reads textures in
    // 4x4-tile GX_TF_RGBA8 layout with separate AR/GB planes; CPU swizzles
    // here every UpdatePixels. Padded to multiple of 4 in each dimension.
    void*    mTiledBuf     = nullptr;
    uint32_t mTiledBufSize = 0;
    uint32_t mTexWidth     = 0;
    uint32_t mTexHeight    = 0;
#elif API_C3D
    C3D_Tex mTex = {};
    void* mT3dsData = nullptr;
    // Streaming-texture path (raw RGBA8 input, e.g. video player). Allocated in
    // linear memory so GX_DisplayTransfer can DMA it through the GPU's tiler
    // into mTex on each UpdatePixels call. nullptr for normal cooked .t3x
    // textures (which are pre-tiled at cook time).
    void*    mLinearBuf     = nullptr;
    uint32_t mLinearBufSize = 0;
    uint32_t mTexWidth      = 0;  // physical (padded to 8-multiple) tex dims
    uint32_t mTexHeight     = 0;
#endif
};

struct MaterialResource
{
#if API_VULKAN
    Shader* mVertexShaders[(uint32_t)VertexType::Max] = {};
    Shader* mFragmentShader = nullptr;
#endif
};

struct StaticMeshResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
    Buffer* mIndexBuffer = nullptr;
#elif defined(POLYPHASE_PLATFORM_ADDON)
    // PSPGU stores pre-packed vertices in PSP format (tex/color/normal/pos
    // interleaved). Counts are kept so the draw can issue sceGuDrawArray
    // without re-querying the StaticMesh asset.
    void*    mVertexData     = nullptr;
    void*    mIndexData      = nullptr;
    uint32_t mNumVertices    = 0;
    uint32_t mNumIndices     = 0;
    uint32_t mVertexStride   = 0;       // bytes per PSP vertex
    uint32_t mVertexFlags    = 0;       // sceGuDrawArray mode bits for THIS mesh
#elif API_GX
    void* mDisplayList = nullptr;
    uint32_t mDisplayListSize = 0;
    void* mColorDisplayList = nullptr;
    uint32_t mColorDisplayListSize = 0;
#elif API_C3D
    void* mVertexData = nullptr;
    void* mIndexData = nullptr;
#endif
};

struct SkeletalMeshResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
    Buffer* mIndexBuffer = nullptr;
#elif API_C3D
    void* mVertexData = nullptr;
    void* mIndexData = nullptr;
#endif
};

struct StaticMeshCompResource
{
#if API_VULKAN
    Buffer* mColorVertexBuffer = nullptr;
#elif defined(POLYPHASE_PLATFORM_ADDON)
    void* mColorVertexData = nullptr;       // instance colour buffer
#elif API_C3D
    void* mColorVertexData = nullptr;
#endif
};

struct InstancedMeshCompResource
{
#if API_VULKAN
    Buffer* mInstanceDataBuffer = nullptr;
    Buffer* mVertexColorBuffer = nullptr;
#endif

    bool mDirty = true;
};

struct SkeletalMeshCompResource
{
#if API_VULKAN
    MultiBuffer* mVertexBuffer = nullptr;
#elif API_C3D
    DoubleBuffer mVertexData;
#endif
};

struct TextMeshCompResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
#elif API_C3D
    DoubleBuffer mVertexData;
#endif
};

struct Voxel3DResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
    Buffer* mIndexBuffer = nullptr;
#elif API_GX
    void* mVertexData = nullptr;
    uint32_t mVertexDataSize = 0;
    void* mDisplayList = nullptr;
    uint32_t mDisplayListSize = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
    DoubleBuffer mIndexData;
#endif
};

struct Terrain3DResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
    Buffer* mIndexBuffer = nullptr;
#elif API_GX
    void* mVertexData = nullptr;
    uint32_t mVertexDataSize = 0;
    void* mDisplayList = nullptr;
    uint32_t mDisplayListSize = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
    DoubleBuffer mIndexData;
#endif
};

struct TileMap2DResource
{
#if API_VULKAN
    Buffer* mVertexBuffer = nullptr;
    Buffer* mIndexBuffer = nullptr;
#elif API_GX
    void* mVertexData = nullptr;
    uint32_t mVertexDataSize = 0;
    void* mDisplayList = nullptr;
    uint32_t mDisplayListSize = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
    DoubleBuffer mIndexData;
#endif
};

struct QuadResource
{
#if API_VULKAN
    MultiBuffer* mVertexBuffer = nullptr;
#elif API_C3D
    DoubleBuffer mVertexData;
#endif
};

struct PolyResource
{
#if API_VULKAN
    MultiBuffer* mVertexBuffer = nullptr;
    uint32_t mNumVerts = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
#endif
};

struct TextResource
{
#if API_VULKAN
    MultiBuffer* mVertexBuffer = nullptr;
    uint32_t mNumBufferCharsAllocated = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
    uint32_t mNumBufferCharsAllocated = 0;
#endif
};

struct ParticleCompResource
{
#if API_VULKAN
    MultiBuffer* mVertexBuffer = nullptr;
    MultiBuffer* mIndexBuffer = nullptr;
    uint32_t mNumVerticesAllocated = 0;
#elif API_C3D
    DoubleBuffer mVertexData;
    DoubleBuffer mIndexData;
    uint32_t mNumVerticesAllocated = 0;
#endif
};
