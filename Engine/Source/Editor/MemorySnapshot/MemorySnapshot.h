#pragma once

#if EDITOR

#include <cstdint>
#include <string>
#include <vector>

// On-disk snapshot format version. Independent from ASSET_VERSION_CURRENT --
// snapshots are loose Debug/ artifacts, not registered assets. Bump when the
// serialized layout in MemorySnapshotProfiler changes and gate the load path.
static const uint32_t kSnapshotVersion = 1;

// Four-byte magic at the head of a DebugSnapshot_*.oct file ("PSNP").
static const uint32_t kSnapshotMagic = 0x504e5350;

enum class SnapshotCategory : uint8_t
{
    Geometry,   // StaticMesh / SkeletalMesh vertex + index data
    Textures,   // Texture pixel data (referenced directly or via materials/fonts)
    Audio,      // SoundWave PCM
    Scripts,    // Lua source referenced by node Script components
    Materials,  // Material shader parameters
    Fonts,      // Font glyph metadata (atlas texture counts under Textures)
    Particles,  // ParticleSystem config
    Animation,  // SkeletalMesh animation keyframe data
    Nodes,      // Runtime node overhead in the game world
    Other,      // Anything without a cheap size estimate

    Count
};

const char* SnapshotCategoryName(SnapshotCategory cat);

struct SnapshotEntry
{
    std::string mName;
    std::string mTypeName;
    SnapshotCategory mCategory = SnapshotCategory::Other;
    uint64_t mCpuBytes = 0;   // system RAM
    uint64_t mGpuBytes = 0;   // VRAM (0 on unified-memory backends / non-Vulkan)
    uint32_t mRefCount = 0;   // how many game nodes reference this
    std::string mDetail;      // e.g. "1024x1024 RGBA8 +mips", "12k verts", "3.2s 44.1kHz stereo"
};

struct MemorySnapshot
{
    uint32_t mVersion = kSnapshotVersion;
    std::string mProjectName;
    std::string mDateString;
    bool mWasPlayingInEditor = false;     // captured live during PIE vs the edit-time scene
    uint32_t mActiveAudioVoices = 0;      // voices playing at capture time

    // Whole-process figures for context. Explicitly NOT game-scoped -- they
    // include editor overhead and are shown separately from the per-asset sum.
    uint64_t mSystemRamUsed = 0;
    uint64_t mSystemVramUsed = 0;
    uint64_t mSystemTotalRam = 0;

    std::vector<SnapshotEntry> mEntries;

    // Optional downscaled frame thumbnail (RGBA8, row-major, mThumbW*mThumbH*4 bytes).
    uint32_t mThumbW = 0;
    uint32_t mThumbH = 0;
    std::vector<uint8_t> mThumbRgba;

    // Convenience: total CPU/GPU bytes across all entries.
    uint64_t GetTotalCpuBytes() const;
    uint64_t GetTotalGpuBytes() const;
    void GetCategoryTotals(uint64_t outCpu[(int)SnapshotCategory::Count],
                           uint64_t outGpu[(int)SnapshotCategory::Count]) const;
};

#endif
