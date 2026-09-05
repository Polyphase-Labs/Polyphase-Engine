#include "OcclusionData.h"
#include "Stream.h"
#include "Asset.h"

#include <math.h>

static const uint16_t kNoSet16 = 0xFFFFu;

bool OcclusionData::IsValid() const
{
    size_t numCells = size_t(mDimX) * mDimY * mDimZ;
    return mDimX > 0 && mDimY > 0 && mDimZ > 0 &&
        mCellSize > 0.0f &&
        mOccludees.size() > 0 &&
        mWordsPerSet > 0 &&
        (mCellToSet16.size() == numCells || mCellToSet32.size() == numCells);
}

bool OcclusionData::HasDynamicTable() const
{
    size_t numCells = size_t(mDimX) * mDimY * mDimZ;
    return mCoarseFactor > 0 &&
        mCoarseDimX > 0 && mCoarseDimY > 0 && mCoarseDimZ > 0 &&
        mWordsPerCoarseSet > 0 &&
        (mCellToCoarseSet16.size() == numCells || mCellToCoarseSet32.size() == numCells);
}

void OcclusionData::Clear()
{
    mMin = glm::vec3(0.0f);
    mCellSize = 4.0f;
    mDimX = 0;
    mDimY = 0;
    mDimZ = 0;
    mWordsPerSet = 0;
    mBakeAlgorithm = 0;
    mOccludees.clear();
    mOccluders.clear();
    mCellToSet16.clear();
    mCellToSet32.clear();
    mSetPool.clear();

    mCoarseFactor = 0;
    mCoarseDimX = 0;
    mCoarseDimY = 0;
    mCoarseDimZ = 0;
    mWordsPerCoarseSet = 0;
    mCellToCoarseSet16.clear();
    mCellToCoarseSet32.clear();
    mCoarseSetPool.clear();
}

int32_t OcclusionData::FindCell(glm::vec3 point) const
{
    if (mDimX == 0 || mDimY == 0 || mDimZ == 0 || mCellSize <= 0.0f)
    {
        return -1;
    }

    glm::vec3 rel = (point - mMin) / mCellSize;
    if (rel.x < 0.0f || rel.y < 0.0f || rel.z < 0.0f)
    {
        return -1;
    }

    uint32_t x = uint32_t(rel.x);
    uint32_t y = uint32_t(rel.y);
    uint32_t z = uint32_t(rel.z);
    if (x >= mDimX || y >= mDimY || z >= mDimZ)
    {
        return -1;
    }

    return int32_t((z * mDimY + y) * mDimX + x);
}

uint32_t OcclusionData::GetCellSetIndex(int32_t cell) const
{
    if (cell < 0)
        return kNoSet;

    if (!mCellToSet16.empty())
    {
        if (size_t(cell) >= mCellToSet16.size())
            return kNoSet;
        uint16_t v = mCellToSet16[cell];
        return (v == kNoSet16) ? kNoSet : uint32_t(v);
    }

    if (size_t(cell) >= mCellToSet32.size())
        return kNoSet;
    return mCellToSet32[cell];
}

const uint32_t* OcclusionData::GetSet(int32_t cell) const
{
    uint32_t setIndex = GetCellSetIndex(cell);
    if (setIndex == kNoSet)
    {
        return nullptr;
    }

    size_t offset = size_t(setIndex) * mWordsPerSet;
    if (offset + mWordsPerSet > mSetPool.size())
    {
        return nullptr;
    }

    return &mSetPool[offset];
}

bool OcclusionData::IsVisible(const uint32_t* set, uint32_t occludeeIndex) const
{
    if (set == nullptr || occludeeIndex >= mOccludees.size())
    {
        return true;
    }

    return (set[occludeeIndex >> 5] & (1u << (occludeeIndex & 31))) != 0;
}

uint32_t OcclusionData::GetCoarseSetIndex(int32_t cell) const
{
    if (cell < 0)
        return kNoSet;

    if (!mCellToCoarseSet16.empty())
    {
        if (size_t(cell) >= mCellToCoarseSet16.size())
            return kNoSet;
        uint16_t v = mCellToCoarseSet16[cell];
        return (v == kNoSet16) ? kNoSet : uint32_t(v);
    }

    if (size_t(cell) >= mCellToCoarseSet32.size())
        return kNoSet;
    return mCellToCoarseSet32[cell];
}

const uint32_t* OcclusionData::GetCoarseSet(int32_t cell) const
{
    uint32_t setIndex = GetCoarseSetIndex(cell);
    if (setIndex == kNoSet)
    {
        return nullptr;
    }

    size_t offset = size_t(setIndex) * mWordsPerCoarseSet;
    if (offset + mWordsPerCoarseSet > mCoarseSetPool.size())
    {
        return nullptr;
    }

    return &mCoarseSetPool[offset];
}

bool OcclusionData::IsCoarseCellVisible(const uint32_t* coarseSet, uint32_t coarseIndex) const
{
    if (coarseSet == nullptr || coarseIndex >= GetNumCoarseCells())
    {
        return true;
    }

    return (coarseSet[coarseIndex >> 5] & (1u << (coarseIndex & 31))) != 0;
}

bool OcclusionData::IsBoxVisibleDynamic(int32_t cell, const AABB& box) const
{
    if (!HasDynamicTable())
        return true;

    const uint32_t* coarseSet = GetCoarseSet(cell);
    if (coarseSet == nullptr)
        return true;

    AABB volume = GetVolumeAABB();
    if (!volume.Contains(box))
    {
        // Partly outside the baked volume: nothing is known about that part.
        return true;
    }

    float coarseSize = mCellSize * float(mCoarseFactor);
    glm::vec3 relMin = (box.mMin - mMin) / coarseSize;
    glm::vec3 relMax = (box.mMax - mMin) / coarseSize;

    int32_t x0 = glm::clamp<int32_t>((int32_t)floorf(relMin.x), 0, (int32_t)mCoarseDimX - 1);
    int32_t y0 = glm::clamp<int32_t>((int32_t)floorf(relMin.y), 0, (int32_t)mCoarseDimY - 1);
    int32_t z0 = glm::clamp<int32_t>((int32_t)floorf(relMin.z), 0, (int32_t)mCoarseDimZ - 1);
    int32_t x1 = glm::clamp<int32_t>((int32_t)floorf(relMax.x), 0, (int32_t)mCoarseDimX - 1);
    int32_t y1 = glm::clamp<int32_t>((int32_t)floorf(relMax.y), 0, (int32_t)mCoarseDimY - 1);
    int32_t z1 = glm::clamp<int32_t>((int32_t)floorf(relMax.z), 0, (int32_t)mCoarseDimZ - 1);

    uint32_t count = uint32_t(x1 - x0 + 1) * uint32_t(y1 - y0 + 1) * uint32_t(z1 - z0 + 1);
    if (count > kMaxDynamicCoarseCells)
    {
        return true;
    }

    for (int32_t z = z0; z <= z1; ++z)
    for (int32_t y = y0; y <= y1; ++y)
    for (int32_t x = x0; x <= x1; ++x)
    {
        uint32_t index = (uint32_t(z) * mCoarseDimY + uint32_t(y)) * mCoarseDimX + uint32_t(x);
        if (IsCoarseCellVisible(coarseSet, index))
        {
            return true;
        }
    }

    return false;
}

AABB OcclusionData::GetCellAABB(int32_t cell) const
{
    if (cell < 0 || mDimX == 0 || mDimY == 0)
    {
        return AABB::MakeInvalid();
    }

    uint32_t c = uint32_t(cell);
    uint32_t x = c % mDimX;
    uint32_t y = (c / mDimX) % mDimY;
    uint32_t z = c / (mDimX * mDimY);

    glm::vec3 cellMin = mMin + glm::vec3(float(x), float(y), float(z)) * mCellSize;
    return AABB(cellMin, cellMin + glm::vec3(mCellSize));
}

AABB OcclusionData::GetVolumeAABB() const
{
    return AABB(mMin, mMin + glm::vec3(float(mDimX), float(mDimY), float(mDimZ)) * mCellSize);
}

size_t OcclusionData::GetStaticTableBytes() const
{
    return mOccludees.size() * sizeof(OcclusionEntry) +
        mOccluders.size() * sizeof(OcclusionEntry) +
        mCellToSet16.size() * sizeof(uint16_t) +
        mCellToSet32.size() * sizeof(uint32_t) +
        mSetPool.size() * sizeof(uint32_t);
}

size_t OcclusionData::GetDynamicTableBytes() const
{
    return mCellToCoarseSet16.size() * sizeof(uint16_t) +
        mCellToCoarseSet32.size() * sizeof(uint32_t) +
        mCoarseSetPool.size() * sizeof(uint32_t);
}

size_t OcclusionData::GetMemoryBytes() const
{
    return GetStaticTableBytes() + GetDynamicTableBytes();
}

static void PackCellIndices(const std::vector<uint32_t>& src, std::vector<uint16_t>& out16, std::vector<uint32_t>& out32)
{
    out16.clear();
    out32.clear();

    bool fits16 = true;
    for (uint32_t v : src)
    {
        if (v != OcclusionData::kNoSet && v >= kNoSet16)
        {
            fits16 = false;
            break;
        }
    }

    if (fits16)
    {
        out16.resize(src.size());
        for (size_t i = 0; i < src.size(); ++i)
        {
            out16[i] = (src[i] == OcclusionData::kNoSet) ? kNoSet16 : uint16_t(src[i]);
        }
    }
    else
    {
        out32 = src;
    }
}

void OcclusionData::SetCellSetIndices(const std::vector<uint32_t>& cellToSet)
{
    PackCellIndices(cellToSet, mCellToSet16, mCellToSet32);
}

void OcclusionData::SetCellCoarseSetIndices(const std::vector<uint32_t>& cellToSet)
{
    PackCellIndices(cellToSet, mCellToCoarseSet16, mCellToCoarseSet32);
}

static void WriteCellIndices(Stream& stream, const std::vector<uint16_t>& v16, const std::vector<uint32_t>& v32)
{
    if (!v16.empty() || v32.empty())
    {
        stream.WriteUint8(2);
        stream.WriteUint32((uint32_t)v16.size());
        for (uint32_t i = 0; i < v16.size(); ++i)
        {
            stream.WriteUint16(v16[i]);
        }
    }
    else
    {
        stream.WriteUint8(4);
        stream.WriteUint32((uint32_t)v32.size());
        for (uint32_t i = 0; i < v32.size(); ++i)
        {
            stream.WriteUint32(v32[i]);
        }
    }
}

static void ReadCellIndices(Stream& stream, std::vector<uint16_t>& v16, std::vector<uint32_t>& v32)
{
    v16.clear();
    v32.clear();

    uint8_t width = stream.ReadUint8();
    uint32_t count = stream.ReadUint32();
    if (width == 2)
    {
        v16.resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            v16[i] = stream.ReadUint16();
        }
    }
    else
    {
        v32.resize(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            v32[i] = stream.ReadUint32();
        }
    }
}

static void WriteWords(Stream& stream, const std::vector<uint32_t>& words)
{
    stream.WriteUint32((uint32_t)words.size());
    for (uint32_t i = 0; i < words.size(); ++i)
    {
        stream.WriteUint32(words[i]);
    }
}

static void ReadWords(Stream& stream, std::vector<uint32_t>& words)
{
    uint32_t count = stream.ReadUint32();
    words.resize(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        words[i] = stream.ReadUint32();
    }
}

void OcclusionData::SaveStream(Stream& stream) const
{
    stream.WriteVec3(mMin);
    stream.WriteFloat(mCellSize);
    stream.WriteUint32(mDimX);
    stream.WriteUint32(mDimY);
    stream.WriteUint32(mDimZ);
    stream.WriteUint32(mWordsPerSet);

    stream.WriteUint32((uint32_t)mOccludees.size());
    for (uint32_t i = 0; i < mOccludees.size(); ++i)
    {
        stream.WriteVec3(mOccludees[i].mCenter);
        stream.WriteVec3(mOccludees[i].mExtents);
    }

    stream.WriteUint32((uint32_t)mOccluders.size());
    for (uint32_t i = 0; i < mOccluders.size(); ++i)
    {
        stream.WriteVec3(mOccluders[i].mCenter);
        stream.WriteVec3(mOccluders[i].mExtents);
    }

    // ASSET_VERSION_SCENE_OCCLUSION wrote the cell table as plain uint32.
    // Since ASSET_VERSION_SCENE_OCCLUSION_DYNAMIC it carries a width byte.
    WriteCellIndices(stream, mCellToSet16, mCellToSet32);
    WriteWords(stream, mSetPool);

    stream.WriteUint32(mCoarseFactor);
    stream.WriteUint32(mCoarseDimX);
    stream.WriteUint32(mCoarseDimY);
    stream.WriteUint32(mCoarseDimZ);
    stream.WriteUint32(mWordsPerCoarseSet);
    WriteCellIndices(stream, mCellToCoarseSet16, mCellToCoarseSet32);
    WriteWords(stream, mCoarseSetPool);

    stream.WriteUint32(mBakeAlgorithm);
}

void OcclusionData::LoadStream(Stream& stream, uint32_t version)
{
    Clear();

    mMin = stream.ReadVec3();
    mCellSize = stream.ReadFloat();
    mDimX = stream.ReadUint32();
    mDimY = stream.ReadUint32();
    mDimZ = stream.ReadUint32();
    mWordsPerSet = stream.ReadUint32();

    uint32_t numOccludees = stream.ReadUint32();
    mOccludees.resize(numOccludees);
    for (uint32_t i = 0; i < numOccludees; ++i)
    {
        mOccludees[i].mCenter = stream.ReadVec3();
        mOccludees[i].mExtents = stream.ReadVec3();
    }

    uint32_t numOccluders = stream.ReadUint32();
    mOccluders.resize(numOccluders);
    for (uint32_t i = 0; i < numOccluders; ++i)
    {
        mOccluders[i].mCenter = stream.ReadVec3();
        mOccluders[i].mExtents = stream.ReadVec3();
    }

    if (version >= ASSET_VERSION_SCENE_OCCLUSION_DYNAMIC)
    {
        ReadCellIndices(stream, mCellToSet16, mCellToSet32);
        ReadWords(stream, mSetPool);

        mCoarseFactor = stream.ReadUint32();
        mCoarseDimX = stream.ReadUint32();
        mCoarseDimY = stream.ReadUint32();
        mCoarseDimZ = stream.ReadUint32();
        mWordsPerCoarseSet = stream.ReadUint32();
        ReadCellIndices(stream, mCellToCoarseSet16, mCellToCoarseSet32);
        ReadWords(stream, mCoarseSetPool);

        mBakeAlgorithm = (version >= ASSET_VERSION_SCENE_OCCLUSION_ALGO) ? stream.ReadUint32() : 1u;
    }
    else
    {
        std::vector<uint32_t> cellToSet;
        ReadWords(stream, cellToSet);
        SetCellSetIndices(cellToSet);
        ReadWords(stream, mSetPool);
    }

    size_t numCells = size_t(mDimX) * mDimY * mDimZ;
    if (!IsValid() && numCells > 0)
    {
        Clear();
    }
}

// ---------------------------------------------------------------------------

void OcclusionEntryIndex::Build(const std::vector<OcclusionEntry>& entries, float cellSize)
{
    mEntries = &entries;
    mCellSize = cellSize;
    mBucketSize = glm::max(cellSize, 1.0f);
    mBuckets.clear();
    for (uint32_t i = 0; i < entries.size(); ++i)
    {
        mBuckets[Key(entries[i].mCenter)].push_back(i);
    }
}

int32_t OcclusionEntryIndex::Find(glm::vec3 center, glm::vec3 extents) const
{
    if (mEntries == nullptr)
        return -1;

    float maxExtent = glm::max(extents.x, glm::max(extents.y, extents.z));
    float tolerance = glm::max(0.1f * mCellSize, 0.01f * maxExtent);

    int32_t best = -1;
    float bestErr = tolerance;

    for (int32_t dz = -1; dz <= 1; ++dz)
    for (int32_t dy = -1; dy <= 1; ++dy)
    for (int32_t dx = -1; dx <= 1; ++dx)
    {
        glm::vec3 probe = center + glm::vec3(float(dx), float(dy), float(dz)) * mBucketSize;
        auto it = mBuckets.find(Key(probe));
        if (it == mBuckets.end())
            continue;

        for (uint32_t index : it->second)
        {
            const OcclusionEntry& e = (*mEntries)[index];
            glm::vec3 dc = glm::abs(e.mCenter - center);
            glm::vec3 de = glm::abs(e.mExtents - extents);
            float err = glm::max(glm::max(dc.x, glm::max(dc.y, dc.z)), glm::max(de.x, glm::max(de.y, de.z)));
            if (err <= bestErr)
            {
                bestErr = err;
                best = (int32_t)index;
            }
        }
    }

    return best;
}

uint64_t OcclusionEntryIndex::Key(glm::vec3 p) const
{
    int64_t v[3] =
    {
        (int64_t)floorf(p.x / mBucketSize),
        (int64_t)floorf(p.y / mBucketSize),
        (int64_t)floorf(p.z / mBucketSize)
    };

    uint64_t hash = 1469598103934665603ULL;
    for (int32_t i = 0; i < 3; ++i)
    {
        hash ^= (uint64_t)v[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
