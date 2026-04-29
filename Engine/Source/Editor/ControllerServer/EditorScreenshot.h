#pragma once

#if EDITOR

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

// One-shot capture of the entire editor swapchain image (Game Preview viewport
// plus all imgui chrome — inspector, hierarchy, debug log, etc.). Vulkan-only.
//
// Threading: RequestEditorScreenshot can be called from any thread (the REST
// route handler runs on Crow's I/O thread). ProcessPendingEditorScreenshots
// must be called from the render thread, after the editor frame is rendered
// to the swapchain but before vkQueuePresent.

struct EditorScreenshotData
{
    bool mOk = false;
    std::vector<uint8_t> mRgba; // tightly packed RGBA, top-to-bottom
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    std::string mError;
};

void RequestEditorScreenshot(std::shared_ptr<std::promise<EditorScreenshotData>> promise);

void ProcessPendingEditorScreenshots();

#endif
