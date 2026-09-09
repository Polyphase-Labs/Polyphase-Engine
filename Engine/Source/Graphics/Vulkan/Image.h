#pragma once

#if API_VULKAN

#include "Graphics/Vulkan/VramAllocator.h"
#include "Maths.h"

#include <vulkan/vulkan.h>

class DestroyQueue;

struct ImageDesc
{
    uint32_t mWidth = 4;
    uint32_t mHeight = 4;
    VkFormat mFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags mUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    uint32_t mMipLevels = 1;
    uint32_t mLayers = 1;
};

struct SamplerDesc
{
    VkFilter mMagFilter = VK_FILTER_LINEAR;
    VkFilter mMinFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode mAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkBorderColor mBorderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    float mMaxAnisotropy = 1.0;
    bool mAnisotropyEnable = false;
};

class Image
{
public:
    Image(ImageDesc imageDesc, SamplerDesc samplerDesc, const char* debugObjectName);

    // External image? Kind of a hack needed for using swapchain images with RenderPassCache
    Image(VkImage image, VkImageView imageView, VkSampler sampler, VkFormat format, uint32_t width, uint32_t height);

    VkImage Get() const;
    VkImageView GetView() const;
    VkSampler GetSampler() const;

    VkFormat GetFormat() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

    // waitForCompletion: Update()'s upload/transition commands are submitted
    // fire-and-forget (no fence) by default -- fine for a texture that gets
    // re-uploaded every frame (any single stale frame is imperceptible), but
    // wrong for a texture uploaded ONCE and sampled immediately: on a slow
    // GPU queue (seen on Android/mobile; desktop's queue is fast enough to
    // mask it) the sample can land before the upload actually finishes,
    // rendering stale/undefined content forever since nothing re-uploads to
    // self-correct. Pass true for exactly that one-shot case -- see
    // CreateTextureResource()'s call (Texture::Create()'s initial upload).
    void Update(const void* srcData, bool waitForCompletion = false);

    void Transition(VkImageLayout layout, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
    void GenerateMips();
    void Clear(glm::vec4 color);

    uint64_t GetId() const;

private:

    friend class DestroyQueue;
    ~Image();

    uint64_t mId = 0;
    VkImage mImage = VK_NULL_HANDLE;
    VkImageView mImageView = VK_NULL_HANDLE;
    VkSampler mSampler = VK_NULL_HANDLE;
    VramAllocation mMemory;

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    VkFormat mFormat = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags mUsage = {};
    uint32_t mMipLevels = 0;
    uint32_t mLayers = 0;

    VkFilter mMagFilter = VK_FILTER_LINEAR;
    VkFilter mMinFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode mAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkBorderColor mBorderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    float mMaxAnisotropy = 1.0;
    bool mAnisotropyEnable = false;

    // UNDEFINED, not PREINITIALIZED: every Image this class creates is
    // OPTIMAL-tiled + DEVICE_LOCAL and is never host-written, and
    // PREINITIALIZED is only meaningful for LINEAR-tiled host-written memory.
    // Must match ciImage.initialLayout in the constructor. The first upload's
    // transition then takes the UNDEFINED->TRANSFER_DST barrier (srcAccess=0),
    // which is spec-valid; the old PREINITIALIZED path used
    // VK_ACCESS_HOST_WRITE_BIT under a stage mask with no HOST stage -- an
    // ill-formed barrier desktop drivers forgive but mobile GPUs (Adreno/Mali
    // texture compression metadata) don't, sampling as black on the first
    // upload of a fresh image. Only a one-shot texture ever exposed it; a
    // per-frame stream re-uploads and self-heals from frame 2.
    VkImageLayout mLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    bool mExternal = false;
};

#endif