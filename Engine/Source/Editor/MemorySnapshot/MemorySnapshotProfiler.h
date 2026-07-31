#pragma once

#if EDITOR

#include <string>
#include "MemorySnapshot.h"

// Builds a MemorySnapshot of the running game (the Game/3DS preview surface, or
// live Play-In-Editor) -- never the editor viewport. Memory is attributed to the
// assets referenced by the game world's nodes, so editor-only resources (gizmo
// icons, editor fonts, asset-browser thumbnails, grid) are naturally excluded.
// Safe to call whether or not PIE is active; the result records which mode it was
// captured in (MemorySnapshot::mWasPlayingInEditor). Must run on the main thread
// (reads the render target for the thumbnail).
MemorySnapshot CaptureSnapshot();

// The <project>/Debug/ folder snapshots are written to (with a trailing slash).
// Deliberately a sibling of Assets/ so the AssetManager never indexes snapshots.
// Returns "" when no project is open.
std::string GetDebugSnapshotDir();

// Auto-named path in the Debug/ folder: DebugSnapshot_YYYY-MM-DD_HH-MM-SS.oct.
// Returns "" when no project is open.
std::string MakeDefaultSnapshotPath();

// Loose-file (bare Stream) serialization. path is any writable location; the
// primary caller passes MakeDefaultSnapshotPath(). Creates Debug/ if needed.
bool SaveSnapshot(const MemorySnapshot& snapshot, const std::string& path);
bool LoadSnapshot(MemorySnapshot& outSnapshot, const std::string& path);

#endif
