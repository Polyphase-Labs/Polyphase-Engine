#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <unordered_map>
#include "MemorySnapshot.h"

class Asset;

// Walks the game world(s) and returns the set of assets the game actually
// references -- direct node/script property refs plus transitive children
// (mesh -> material -> texture, incl. MaterialLite texture slots; font -> atlas).
// Editor-only assets (gizmos, grid, thumbnails, engine editor fonts) are excluded
// because game nodes never reference them. This is the accurate, game-scoped
// resource set shared by the Memory Snapshot tool and the Profiling window.
// outRefs maps each referenced Asset* to how many times it was referenced.
// Optional out-params report the Lua scripts referenced and the node/world counts.
void GatherGameReferencedAssets(std::unordered_map<Asset*, uint32_t>& outRefs,
                                std::unordered_map<std::string, uint32_t>* outScriptFiles = nullptr,
                                uint32_t* outNodeCount = nullptr,
                                int32_t* outWorldCount = nullptr);

// Builds the full game-scoped memory report: one entry per referenced asset
// (sized by actual pixel format, streaming-aware audio), plus Lua script source
// entries and a Frame Buffers entry (color x2 + depth for the render surface).
// This is the single source of truth shared by the Memory Snapshot tool and the
// Profiling window, so both report identical numbers. Appends to outEntries.
void BuildGameMemoryEntries(std::vector<SnapshotEntry>& outEntries,
                            uint32_t* outNodeCount = nullptr,
                            int32_t* outWorldCount = nullptr);

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
