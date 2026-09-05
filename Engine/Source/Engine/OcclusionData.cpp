#include "OcclusionData.h"
#include "Stream.h"
#include "Asset.h"

bool OcclusionData::IsValid() const
{
    return mDimX > 0 && mDimY > 0 && mDimZ > 0 &&
        mCellSize > 0.0f &&
        mOccludees.size() > 0 &&
        mWordsPerSet > 0 &&
        mCellToSet.size() == size_t(mDimX) * mDimY * mDimZ;
}

void OcclusionData::Clear()
{
    mMin = glm::vec3(0.0f);
    mCellSize = 4.0f;
    mDimX = 0;
    mDimY = 0;
    mDimZ = 0;
    mWordsPerSet = 0;
    mOccludees.clear();
    mOccluders.clear();
    mCellToSet.clear();
    mSetPool.clear();
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

const uint32_t* OcclusionData::GetSet(int32_t cell) const
{
    if (cell < 0 || size_t(cell) >= mCellToSet.size())
    {
        return nullptr;
    }

    uint32_t setIndex = mCellToSet[cell];
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

size_t OcclusionData::GetMemoryBytes() const
{
    return mOccludees.size() * sizeof(OcclusionEntry) +
        mOccluders.size() * sizeof(OcclusionEntry) +
        mCellToSet.size() * sizeof(uint32_t) +
        mSetPool.size() * sizeof(uint32_t);
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

    stream.WriteUint32((uint32_t)mCellToSet.size());
    for (uint32_t i = 0; i < mCellToSet.size(); ++i)
    {
        stream.WriteUint32(mCellToSet[i]);
    }

    stream.WriteUint32((uint32_t)mSetPool.size());
    for (uint32_t i = 0; i < mSetPool.size(); ++i)
    {
        stream.WriteUint32(mSetPool[i]);
    }
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

    uint32_t numCells = stream.ReadUint32();
    mCellToSet.resize(numCells);
    for (uint32_t i = 0; i < numCells; ++i)
    {
        mCellToSet[i] = stream.ReadUint32();
    }

    uint32_t numWords = stream.ReadUint32();
    mSetPool.resize(numWords);
    for (uint32_t i = 0; i < numWords; ++i)
    {
        mSetPool[i] = stream.ReadUint32();
    }

    if (!IsValid() && numCells > 0)
    {
        Clear();
    }
}
