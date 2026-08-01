#pragma once

#if EDITOR

#include <cstdint>
#include <string>

#include "imgui.h"
#include "MemorySnapshot.h"

// "Memory Snapshot" editor panel. Captures the running game's per-resource
// memory footprint (Game / 3DS preview surface, or live PIE -- never the editor
// viewport), shows it as a category tree + per-asset table with RAM-budget bars,
// and saves/loads snapshots as loose <project>/Debug/DebugSnapshot_*.oct files.
class MemorySnapshotWindow
{
public:
    void Open();
    void Close();
    void Draw();
    bool IsOpen() const { return mIsOpen; }

    // Capture a snapshot now (also used by the Profiling window's button).
    void CaptureNow();

    // Release the cached thumbnail texture. Call from EditorImguiPreShutdown.
    void Shutdown();

private:
    void DrawToolbar();
    void DrawHeaderStrip();
    void DrawCategoryTree();
    void DrawEntryTable();

    void SaveCurrent();
    void LoadFromDialog();
    void RefreshThumbnailTexture();
    uint64_t GetSelectedBudgetBytes() const;

    bool mIsOpen = false;

    MemorySnapshot mSnapshot;
    bool mHasSnapshot = false;

    // -1 = show all categories; otherwise a SnapshotCategory index.
    int32_t mCategoryFilter = -1;

    int32_t mBudgetPresetIndex = 6;   // default 64 MB (see sBudgetPresets)
    int32_t mCustomBudgetMB = 64;

    // Cached thumbnail (written to a temp PNG and loaded via EditorImageCache).
    ImTextureID mThumbTexId = 0;
    bool mThumbDirty = false;
};

MemorySnapshotWindow* GetMemorySnapshotWindow();

#endif
