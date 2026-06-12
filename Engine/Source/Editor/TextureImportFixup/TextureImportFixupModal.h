#pragma once

#include "Engine.h"

#if EDITOR

#include "AssetRef.h"
#include "Engine/Assets/Texture.h"

#include <cstdint>
#include <string>
#include <vector>

// Post-import scan for textures whose width or height isn't a power of two.
// Texture::Import enqueues a row here when stbi_load returns a non-POT image;
// Create() is deferred. The user picks Pad (next POT, transparent border),
// Resize (previous POT via stbir bilinear), or Cancel (asset is discarded)
// per row or in bulk. The fix is applied via Texture::FinalizeDeferredImport.
class TextureImportFixupModal
{
public:

    enum class FixChoice
    {
        None,
        Padded,
        Resized,
        Cancelled,
    };

    struct PendingRow
    {
        AssetRef             mTexture;            // half-imported asset (auto-cleared on unload)
        std::string          mAssetName;          // captured at enqueue so the row still labels after teardown
        std::string          mSourcePath;
        std::vector<uint8_t> mDecodedPixels;      // RGBA8, srcW * srcH * 4
        uint32_t             mSrcWidth = 0;
        uint32_t             mSrcHeight = 0;
        uint32_t             mPadWidth = 0;       // next POT >= srcW
        uint32_t             mPadHeight = 0;
        uint32_t             mResizeWidth = 0;    // prev POT <= srcW (min 1)
        uint32_t             mResizeHeight = 0;
        FixChoice            mResolved = FixChoice::None;
    };

    static TextureImportFixupModal* Get();

    // Called from Texture::Import when stbi_load returns a non-POT image.
    // Transfers ownership of the decoded pixel buffer.
    void Enqueue(Texture* tex,
                 const std::string& sourcePath,
                 std::vector<uint8_t>&& pixels,
                 uint32_t srcW, uint32_t srcH);

    bool HasPending() const { return !mRows.empty(); }

    // Returns true if `asset` was enqueued by Texture::Import and the user
    // hasn't resolved the row yet. Callers (e.g. ActionManager) should skip
    // any auto-save of the asset while this is true, because mPixels is empty
    // and SaveStream would assert. The modal saves the asset itself after
    // ApplyPad / ApplyResize finalize the pixel buffer.
    bool IsAwaitingFixup(const Asset* asset) const;

    // Wipe all queued rows without resolving them. Called from project
    // unload paths so a stale modal doesn't carry across project switches.
    void Reset();

    // Call from EditorImguiDraw every frame.
    void Draw();

private:

    TextureImportFixupModal() = default;

    void ApplyPad(PendingRow& row);
    void ApplyResize(PendingRow& row);
    void ApplyCancel(PendingRow& row);

    std::vector<PendingRow> mRows;
    bool mModalRequested = false;
};

#endif // EDITOR
