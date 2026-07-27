#pragma once

#if EDITOR

#include "imgui.h"

#include <stdint.h>
#include <string>

/**
 * @file EditorImageCache.h
 * @brief Shared, engine-owned PNG-on-disk -> ImTextureID cache for editor UI.
 *
 * One decode and one GPU upload per absolute path per editor session. Used by
 * the engine's own addon thumbnails (AddonsWindow) and exposed to native
 * addons through EditorUIHooks::EditorImage_Load, so an addon never has to
 * reach for class Image / DestroyQueue / DeviceWaitIdle -- none of which are
 * annotated POLYPHASE_API, and therefore none of which appear in Polyphase.lib.
 *
 * Ownership: the engine owns every texture for the whole editor session.
 * Callers hold nothing but the opaque handle and must not release it.
 *
 * Backend: Vulkan only. On other editor backends every entry point is a no-op
 * returning 0 -- callers already handle a 0 handle as "no thumbnail".
 */
namespace EditorImageCache
{
    /**
     * Decode and upload the image at `absPath`, returning an ImGui texture
     * handle. Repeat calls with the same path return the same handle without
     * touching the disk. Failures (missing file, undecodable data) are
     * negatively cached, so a per-frame caller costs one hash lookup rather
     * than one open() per frame.
     */
    ImTextureID Get(const std::string& absPath);

    /** Pixel dimensions of a previously-Get()'d image. False if unknown. */
    bool GetSize(const std::string& absPath, int32_t& outWidth, int32_t& outHeight);

    /**
     * Forget `absPath` so the next Get() re-reads it from disk. Use after the
     * file has been overwritten (addon install, thumbnail re-export).
     *
     * The GPU release is deferred: the entry is unlinked now, and the
     * descriptor set plus Image are torn down by RetirePending() at the top of
     * the next frame. That matters because callers hit this mid-frame (
     * AddonsWindow::Close runs inside Draw) and ImGui_ImplVulkan_RemoveTexture
     * is immediate, not DestroyQueue-deferred -- releasing in place would free
     * a descriptor set the current frame's draw list still references.
     */
    void Invalidate(const std::string& absPath);

    /** Invalidate every entry. Same deferred-release semantics. */
    void InvalidateAll();

    /**
     * Drain the pending-release list. Called once per frame from the top of
     * EditorImguiDraw, before ImGui::NewFrame. Cheap no-op when nothing is
     * pending; does a DeviceWaitIdle when something is.
     */
    void RetirePending();

    /**
     * Release everything. Must run while the Vulkan device is still alive --
     * i.e. from EditorImguiPreShutdown, never from a static destructor.
     */
    void Shutdown();
}

#endif // EDITOR
