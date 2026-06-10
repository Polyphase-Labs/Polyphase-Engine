#pragma once

// Vulkan handles exposed to native addons that want to run their own Vulkan
// rendering inside an engine custom render pass. The engine's general GFX_*
// API only covers a fixed set of draw shapes (meshes, lines, splats); addons
// that need to bind their own pipelines (e.g. the Gaussian Splatting addon's
// proper anisotropic-falloff path) need a deeper handle on the Vulkan device,
// the currently-recording command buffer, and the active render pass.
//
// This header is Vulkan-only — it's included on Vulkan-backend builds in the
// engine and only from addons that link against Polyphase.lib on those
// platforms. GX / C3D backends do not provide an equivalent.
//
// Lifetime: every handle returned is owned by the engine. Addons must not
// destroy any of them. The VkCommandBuffer and VkRenderPass are valid only
// for the duration of the custom render pass callback that's currently
// running — capture them, use them, do not store them across frames.

#include <vulkan/vulkan.h>

#include <cstdint>

struct GfxVulkanAddonHandles
{
    VkDevice         mDevice                = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice        = VK_NULL_HANDLE;
    VkCommandBuffer  mCurrentCommandBuffer  = VK_NULL_HANDLE;
    VkRenderPass     mCurrentRenderPass     = VK_NULL_HANDLE;  // the one the engine is mid-pass on

    // Scene viewport rectangle the engine just set with GFX_SetViewport.
    // (x, y) is the offset within the swapchain image; (w, h) is the size.
    // Addons MUST re-apply this with vkCmdSetViewport if they bind their own
    // pipeline with VK_DYNAMIC_STATE_VIEWPORT; setting (0,0,w,h) would write
    // into the wrong sub-region during editor-viewport / game-preview renders.
    uint32_t         mSceneViewportX        = 0;
    uint32_t         mSceneViewportY        = 0;
    uint32_t         mSceneViewportWidth    = 0;
    uint32_t         mSceneViewportHeight   = 0;

    // Legacy aliases — same as mSceneViewport{Width,Height}, kept so older
    // code that consumed these still works.
    uint32_t         mViewportWidth         = 0;
    uint32_t         mViewportHeight        = 0;

    uint32_t         mCurrentFrameIndex     = 0;
    uint32_t         mMaxFramesInFlight     = 0;
    VkQueue          mGraphicsQueue         = VK_NULL_HANDLE;
};

// Returns true if the running backend is Vulkan and the values were filled.
// Returns false on GX / C3D backends. Safe to call from any addon; on non-
// Vulkan builds the addon should fall back to GFX_DrawSplats or a proxy mesh.
//
// Must be called from inside a custom render pass callback (registered via
// Renderer::RegisterCustomRenderPass) — otherwise the command buffer and
// render pass fields will be VK_NULL_HANDLE.
bool GFX_GetVulkanAddonHandles(GfxVulkanAddonHandles& out);
