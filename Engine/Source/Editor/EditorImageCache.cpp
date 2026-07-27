#if EDITOR

#include "EditorImageCache.h"

#include "System/System.h"

#if API_VULKAN
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include "backends/imgui_impl_vulkan.h"
#include <stb_image.h>
#endif

#include <unordered_map>
#include <vector>

#if API_VULKAN

namespace
{
    struct CacheEntry
    {
        ImTextureID mTexId = 0;
        Image* mImage = nullptr;
        int32_t mWidth = 0;
        int32_t mHeight = 0;
    };

    std::unordered_map<std::string, CacheEntry> sCache;

    // Entries unlinked from sCache but not yet released. Drained by
    // RetirePending() between frames -- see the header for why.
    std::vector<CacheEntry> sPendingRelease;
}

ImTextureID EditorImageCache::Get(const std::string& absPath)
{
    if (absPath.empty())
    {
        return 0;
    }

    auto it = sCache.find(absPath);
    if (it != sCache.end())
    {
        // Includes negative entries, whose mTexId is 0.
        return it->second.mTexId;
    }

    if (!SYS_DoesFileExist(absPath.c_str(), false))
    {
        sCache[absPath] = {};
        return 0;
    }

    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(absPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        sCache[absPath] = {};
        return 0;
    }

    ImageDesc imgDesc;
    imgDesc.mWidth = (uint32_t)width;
    imgDesc.mHeight = (uint32_t)height;
    imgDesc.mFormat = VK_FORMAT_R8G8B8A8_UNORM;
    imgDesc.mUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgDesc.mMipLevels = 1;
    imgDesc.mLayers = 1;

    SamplerDesc sampDesc;
    sampDesc.mMagFilter = VK_FILTER_LINEAR;
    sampDesc.mMinFilter = VK_FILTER_LINEAR;
    sampDesc.mAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    Image* image = new Image(imgDesc, sampDesc, "EditorImageCache");
    image->Update(pixels);
    image->Transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    stbi_image_free(pixels);

    CacheEntry entry;
    entry.mTexId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
        image->GetSampler(),
        image->GetView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry.mImage = image;
    entry.mWidth = width;
    entry.mHeight = height;

    sCache[absPath] = entry;
    return entry.mTexId;
}

bool EditorImageCache::GetSize(const std::string& absPath, int32_t& outWidth, int32_t& outHeight)
{
    outWidth = 0;
    outHeight = 0;

    auto it = sCache.find(absPath);
    if (it == sCache.end() || it->second.mTexId == 0)
    {
        return false;
    }

    outWidth = it->second.mWidth;
    outHeight = it->second.mHeight;
    return true;
}

void EditorImageCache::Invalidate(const std::string& absPath)
{
    auto it = sCache.find(absPath);
    if (it == sCache.end())
    {
        return;
    }

    if (it->second.mTexId != 0 || it->second.mImage != nullptr)
    {
        sPendingRelease.push_back(it->second);
    }

    sCache.erase(it);
}

void EditorImageCache::InvalidateAll()
{
    for (auto& pair : sCache)
    {
        if (pair.second.mTexId != 0 || pair.second.mImage != nullptr)
        {
            sPendingRelease.push_back(pair.second);
        }
    }

    sCache.clear();
}

void EditorImageCache::RetirePending()
{
    if (sPendingRelease.empty())
    {
        return;
    }

    DeviceWaitIdle();

    for (CacheEntry& entry : sPendingRelease)
    {
        if (entry.mTexId != 0)
        {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)entry.mTexId);
        }
        if (entry.mImage != nullptr)
        {
            GetDestroyQueue()->Destroy(entry.mImage);
        }
    }

    sPendingRelease.clear();
}

void EditorImageCache::Shutdown()
{
    InvalidateAll();
    RetirePending();
}

#else // API_VULKAN

ImTextureID EditorImageCache::Get(const std::string& absPath)
{
    (void)absPath;
    return 0;
}

bool EditorImageCache::GetSize(const std::string& absPath, int32_t& outWidth, int32_t& outHeight)
{
    (void)absPath;
    outWidth = 0;
    outHeight = 0;
    return false;
}

void EditorImageCache::Invalidate(const std::string& absPath) { (void)absPath; }
void EditorImageCache::InvalidateAll() {}
void EditorImageCache::RetirePending() {}
void EditorImageCache::Shutdown() {}

#endif // API_VULKAN

#endif // EDITOR
