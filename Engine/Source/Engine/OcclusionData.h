#pragma once

#include "PolyphaseAPI.h"
#include "Maths.h"
#include "EngineTypes.h"

#include <vector>
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
class POLYPHASE_API OcclusionData
{
public:

    static const uint32_t kNoSet = 0xFFFFFFFFu;

    bool IsValid() const;
    void Clear();

    int32_t FindCell(glm::vec3 point) const;
    const uint32_t* GetSet(int32_t cell) const;
    bool IsVisible(const uint32_t* set, uint32_t occludeeIndex) const;

    uint32_t GetNumCells() const { return mDimX * mDimY * mDimZ; }
    uint32_t GetNumUniqueSets() const { return mWordsPerSet > 0 ? uint32_t(mSetPool.size() / mWordsPerSet) : 0; }
    AABB GetCellAABB(int32_t cell) const;
    AABB GetVolumeAABB() const;
    size_t GetMemoryBytes() const;

    void SaveStream(Stream& stream) const;
    void LoadStream(Stream& stream, uint32_t version);

    glm::vec3 mMin = {};
    float mCellSize = 4.0f;
    uint32_t mDimX = 0;
    uint32_t mDimY = 0;
    uint32_t mDimZ = 0;
    uint32_t mWordsPerSet = 0;

    std::vector<OcclusionEntry> mOccludees;
    std::vector<OcclusionEntry> mOccluders;
    std::vector<uint32_t> mCellToSet;
    std::vector<uint32_t> mSetPool;
};
