#if EDITOR

#include "EditorScreenshot.h"

#include "Log.h"

#if API_VULKAN
#include "Graphics/Vulkan/VulkanContext.h"
#include "Graphics/Vulkan/Image.h"
#include "Graphics/Vulkan/Buffer.h"
#include "Graphics/Vulkan/DestroyQueue.h"
#include "Graphics/Vulkan/VulkanUtils.h"
#include <vulkan/vulkan.h>
#endif

#include <cstring>
#include <mutex>

static std::mutex sPendingMutex;
static std::vector<std::shared_ptr<std::promise<EditorScreenshotData>>> sPending;

void RequestEditorScreenshot(std::shared_ptr<std::promise<EditorScreenshotData>> promise)
{
    if (promise == nullptr)
        return;

    std::lock_guard<std::mutex> lock(sPendingMutex);
    sPending.push_back(std::move(promise));
}

#if API_VULKAN
static bool IsBgraFormat(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
        return true;
    default:
        return false;
    }
}
#endif

void ProcessPendingEditorScreenshots()
{
    std::vector<std::shared_ptr<std::promise<EditorScreenshotData>>> pending;
    {
        std::lock_guard<std::mutex> lock(sPendingMutex);
        if (sPending.empty())
            return;
        pending.swap(sPending);
    }

    EditorScreenshotData data;

#if API_VULKAN
    Image* swapImg = GetVulkanContext() ? GetVulkanContext()->GetSwapchainImage() : nullptr;
    if (swapImg == nullptr || swapImg->GetWidth() == 0 || swapImg->GetHeight() == 0)
    {
        data.mError = "Swapchain image not available";
    }
    else
    {
        uint32_t w = swapImg->GetWidth();
        uint32_t h = swapImg->GetHeight();
        size_t bufSize = (size_t)w * h * 4;
        VkFormat fmt = swapImg->GetFormat();

        Buffer* stagingBuffer = new Buffer(
            BufferType::Transfer, bufSize, "EditorScreenshot Staging", nullptr, true);

        // The swapchain image was just rendered to. Render passes leave it in
        // PRESENT_SRC_KHR (mPostLayout, see VulkanContext::CreateRenderPasses).
        // The Image wrapper's mLayout is unreliable for swapchain images (it's
        // only updated by Image::Transition, but render passes transition the
        // image implicitly), so use the lower-level free function with the
        // explicit, known layouts.
        TransitionImageLayout(swapImg->Get(), swapImg->GetFormat(),
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              1, 1);

        VkCommandBuffer cb = BeginCommandBuffer();
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { w, h, 1 };

        vkCmdCopyImageToBuffer(cb, swapImg->Get(),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               stagingBuffer->Get(), 1, &region);
        EndCommandBuffer(cb);

        TransitionImageLayout(swapImg->Get(), swapImg->GetFormat(),
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              1, 1);

        DeviceWaitIdle();

        void* mapped = stagingBuffer->Map();
        if (mapped != nullptr)
        {
            data.mRgba.resize(bufSize);
            const uint8_t* pixels = static_cast<const uint8_t*>(mapped);

            if (IsBgraFormat(fmt))
            {
                for (size_t i = 0; i < bufSize; i += 4)
                {
                    data.mRgba[i + 0] = pixels[i + 2];
                    data.mRgba[i + 1] = pixels[i + 1];
                    data.mRgba[i + 2] = pixels[i + 0];
                    data.mRgba[i + 3] = pixels[i + 3];
                }
            }
            else
            {
                memcpy(data.mRgba.data(), pixels, bufSize);
            }

            data.mWidth = w;
            data.mHeight = h;
            data.mOk = true;

            stagingBuffer->Unmap();
        }
        else
        {
            data.mError = "Failed to map staging buffer";
        }

        GetDestroyQueue()->Destroy(stagingBuffer);
    }
#else
    data.mError = "Editor screenshot requires Vulkan backend";
#endif

    for (auto& p : pending)
    {
        try { p->set_value(data); }
        catch (...) {}
    }
}

#endif
