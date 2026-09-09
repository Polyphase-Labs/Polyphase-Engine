#pragma once

#include <string>
#include "glm/glm.hpp"
#include "Asset.h"

#include "Graphics/GraphicsTypes.h"

class POLYPHASE_API Texture : public Asset
{

public:

    DECLARE_ASSET(Texture, Asset);

    Texture();
    virtual ~Texture();

    TextureResource* GetResource();

    // Asset Interface
    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;
    virtual const char* GetTypeImportExt() override;

    void Init(uint32_t width, uint32_t height, uint8_t* data);

    // Streams a new RGBA8 pixel buffer into the existing GPU image.
    // The texture must already be Create()'d, dimensions must match, and byteSize must equal width * height * 4.
    void UpdatePixels(const uint8_t* data, size_t byteSize);

#if EDITOR
    // Install a fixed-up RGBA8 pixel buffer at new dimensions and (re)create
    // the GPU resource. Used by TextureImportFixupModal after the user picks
    // Pad / Resize on a non-POT import where Create() was deferred.
    void FinalizeDeferredImport(std::vector<uint8_t>&& pixels, uint32_t width, uint32_t height);
#endif

    // Decode an in-memory PNG/JPG/TGA/BMP buffer into `out` and Create() it.
    // Runtime-safe (used by HTTP responses to materialise textures from URLs).
    // Returns false on decode failure or unsupported dimensions.
    static bool LoadFromMemory(const uint8_t* data, size_t size, Texture& out);

    void SetMipmapped(bool mipmapped);
    // False whenever Filter Type is Nearest, regardless of the Mipmapped
    // property: mip levels are averaged, so they would blur pixel art at
    // distance on every backend. GetMipLevels() follows the same rule.
    bool IsMipmapped() const;
    bool IsRenderTarget() const;
    bool IsSrgb() const;
    bool IsForcedHighQuality() const;

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t GetMipLevels() const;
    uint32_t GetLayers() const;
    PixelFormat GetFormat() const;
    FilterType GetFilterType() const;
    WrapMode GetWrapMode() const;
    int32_t GetLowQualityDownsampleFactor() const;

    // Maximum normalized UV that contains actual content. (1, 1) = the full
    // physical texture is content (no padding). Less than 1 in either axis
    // means the platform-side resource padded the texture (e.g. 3DS PoT
    // requirement on a 240x135 source produces a 256x256 physical texture
    // with content in the top-left 240/256, 135/256 region — UV max is then
    // (240/256, 135/256)). Renderers that draw a UV [0,1] rectangle should
    // multiply their UVs by this so they only sample the content area.
    glm::vec2 GetUVMax() const { return mUvMax; }
    void SetUVMax(glm::vec2 uvMax) { mUvMax = uvMax; }

    void SetFormat(PixelFormat format);
    void SetFilterType(FilterType filterType);
    void SetWrapMode(WrapMode wrapMode);
    void SetForceHighQuality(bool forceHq);

    static bool HandlePropChange(class Datum* datum, uint32_t index, const void* newValue);

#if EDITOR
    // Tear down and rebuild the GPU resource from mPixels after a sampler-
    // affecting change (filter, wrap, mipmapped). Bumps the resource
    // generation. Editor-only: the runtime frees mPixels in Create().
    void RecreateResource();
#endif

    const std::vector<uint8_t>& GetPixels() const { return mPixels; }

    // Keep mPixels resident after Create() in runtime builds too (the editor
    // always keeps it, to save the asset). Opt in for runtime-generated
    // textures whose pixels must be read back on the CPU -- a webcam/photo
    // snapshot that gets PNG-encoded and uploaded, a procedural image that
    // gets saved. Costs width * height * 4 bytes per texture, so leave it off
    // for anything that is only ever drawn (a streaming video/webcam feed).
    // Set it before Create(); UpdatePixels() keeps the copy in sync while on.
    void SetRetainPixels(bool retain) { mRetainPixels = retain; }
    bool GetRetainPixels() const { return mRetainPixels; }

    // Bumped every time the backing GPU resource is torn down and rebuilt --
    // Create(), Destroy(), or an editor property change (Filter Type, Wrap Mode,
    // Mipmapped) that forces a rebuild behind an unchanged Texture*.
    //
    // Anything caching a backend handle *derived* from the resource must key on
    // this as well as the Texture pointer. The ImGui descriptor sets the editor
    // builds from the Vulkan image view + sampler are the motivating case: a
    // pointer-only cache keeps handing ImGui a descriptor set pointing at the
    // freed image, and the GPU faults a few frames later (device lost / TDR).
    uint32_t GetResourceGeneration() const { return mResourceGeneration; }

protected:

    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mMipLevels;
    uint32_t mLayers;
    PixelFormat mFormat;
    FilterType mFilterType;
    WrapMode mWrapMode;
    bool mMipmapped;
    bool mRenderTarget;
    bool mSrgb;
    bool mForceHighQuality;
    uint8_t mLowQualityDownsampleFactor;
    // Placed here on purpose: it lands in the 3 bytes of padding before mUvMax
    // (three 4-byte enums + four bools + one uint8 = 17 bytes from a 4-aligned
    // base; glm::vec2 needs 4-byte alignment), so it changes neither
    // sizeof(Texture) nor any existing member offset. Already-built addon DLLs
    // that include this header stay ABI-compatible. See SetRetainPixels().
    bool mRetainPixels = false;

    // Content UV maximum. See GetUVMax() doc above. Default (1,1) — set by the
    // graphics backend when it has to pad the physical texture beyond the
    // logical content size (e.g. 3DS PoT requirement).
    glm::vec2 mUvMax = glm::vec2(1.0f, 1.0f);

    // See GetResourceGeneration().
    uint32_t mResourceGeneration = 0;

    // This pixel array is used as an intermediate storage between LoadStream() and Create()
    // It is cleared and shrunk within Create() except when compiled for EDITOR
    std::vector<uint8_t> mPixels;

    // Graphics Resource
    TextureResource mResource;
};
