#pragma once

#include "PolyphaseAPI.h"
#include "Maths.h"
#include "EngineTypes.h"

#include <vector>
#include <unordered_map>
#include <stdint.h>

class Stream;

// World-space AABB of an occluder / occludee at bake time. Used to match
// baked entries back to live nodes when a scene is instantiated.
struct OcclusionEntry
{
    glm::vec3 mCenter = {};
    glm::vec3 mExtents = {};
};

// Baked potentially-visible-set for one Scene. The bake volume is split into
// uniform cells; each cell maps to a bitset of occludees that can be seen
// from anywhere inside that cell. Identical bitsets are shared through
// mSetPool so the table stays small on consoles.
//
// Optionally a second, coarser table records which coarse cells (blocks of
// mCoarseFactor^3 cells) are visible from each cell. Moving objects that are
// flagged as occludees are tested against that table by their current bounds.
class POLYPHASE_API OcclusionData
{
public:

    static constexpr uint32_t kNoSet = 0xFFFFFFFFu;   // constexpr: ODR-used (vector::assign takes a reference)

    bool IsValid() const;
    bool HasDynamicTable() const;
    void Clear();

    int32_t FindCell(glm::vec3 point) const;
    uint32_t GetCellSetIndex(int32_t cell) const;
    const uint32_t* GetSet(int32_t cell) const;
    bool IsVisible(const uint32_t* set, uint32_t occludeeIndex) const;

    // Dynamic (coarse) table.
    const uint32_t* GetCoarseSet(int32_t cell) const;
    bool IsCoarseCellVisible(const uint32_t* coarseSet, uint32_t coarseIndex) const;
    uint32_t GetNumCoarseCells() const { return mCoarseDimX * mCoarseDimY * mCoarseDimZ; }
    uint32_t GetNumUniqueCoarseSets() const { return mWordsPerCoarseSet > 0 ? uint32_t(mCoarseSetPool.size() / mWordsPerCoarseSet) : 0; }
    // True if any coarse cell overlapped by the box is visible from `cell`.
    // Boxes that leave the volume or cover more than kMaxDynamicCoarseCells
    // are reported visible.
    bool IsBoxVisibleDynamic(int32_t cell, const AABB& box) const;

    uint32_t GetNumCells() const { return mDimX * mDimY * mDimZ; }
    uint32_t GetNumUniqueSets() const { return mWordsPerSet > 0 ? uint32_t(mSetPool.size() / mWordsPerSet) : 0; }
    AABB GetCellAABB(int32_t cell) const;
    AABB GetVolumeAABB() const;
    size_t GetMemoryBytes() const;
    size_t GetStaticTableBytes() const;
    size_t GetDynamicTableBytes() const;

    // Cell -> set index storage. Uses 16-bit indices when the pool is small
    // enough (the common case), which halves the biggest table on consoles.
    void SetCellSetIndices(const std::vector<uint32_t>& cellToSet);
    void SetCellCoarseSetIndices(const std::vector<uint32_t>& cellToSet);

    void SaveStream(Stream& stream) const;
    void LoadStream(Stream& stream, uint32_t version);

    static const uint32_t kMaxDynamicCoarseCells = 64;

    glm::vec3 mMin = {};
    float mCellSize = 4.0f;
    uint32_t mDimX = 0;
    uint32_t mDimY = 0;
    uint32_t mDimZ = 0;
    uint32_t mWordsPerSet = 0;
    // Sampling scheme that produced this data. The baker only reuses bits
    // from a previous bake made with the same scheme.
    uint32_t mBakeAlgorithm = 0;

    std::vector<OcclusionEntry> mOccludees;
    std::vector<OcclusionEntry> mOccluders;
    std::vector<uint16_t> mCellToSet16;   // used when every index fits (0xFFFF = kNoSet)
    std::vector<uint32_t> mCellToSet32;   // otherwise
    std::vector<uint32_t> mSetPool;

    // Dynamic table (0 factor = not baked).
    uint32_t mCoarseFactor = 0;
    uint32_t mCoarseDimX = 0;
    uint32_t mCoarseDimY = 0;
    uint32_t mCoarseDimZ = 0;
    uint32_t mWordsPerCoarseSet = 0;
    std::vector<uint16_t> mCellToCoarseSet16;
    std::vector<uint32_t> mCellToCoarseSet32;
    std::vector<uint32_t> mCoarseSetPool;

private:

    uint32_t GetCoarseSetIndex(int32_t cell) const;
};

// Tolerant lookup of OcclusionEntry records by world-space AABB. Entries are
// bucketed by position; a query matches the closest entry whose center and
// extents are within 10% of a cell or 1% of the object's size. Cloned (PIE)
// and re-instantiated trees rebuild rotations from euler properties and the
// float noise on a large rotated wall exceeds any exact quantisation.
class POLYPHASE_API OcclusionEntryIndex
{
public:

    void Build(const std::vector<OcclusionEntry>& entries, float cellSize);
    int32_t Find(glm::vec3 center, glm::vec3 extents) const;

private:

    uint64_t Key(glm::vec3 p) const;

    const std::vector<OcclusionEntry>* mEntries = nullptr;
    float mCellSize = 1.0f;
    float mBucketSize = 1.0f;
    std::unordered_map<uint64_t, std::vector<uint32_t>> mBuckets;
};
