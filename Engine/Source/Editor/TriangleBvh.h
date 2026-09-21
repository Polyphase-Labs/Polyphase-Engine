#pragma once

#if EDITOR

#include "EngineTypes.h"
#include "Graphics/GraphicsTypes.h"

#include <vector>

class Primitive3D;

struct BvhTri
{
    glm::vec3 mV0;
    glm::vec3 mE1;
    glm::vec3 mE2;
};

struct BvhNode
{
    AABB mBox;
    uint32_t mFirst = 0;   // leaf: first triangle index
    uint32_t mCount = 0;   // leaf: triangle count (0 = interior)
    uint32_t mRight = 0;   // interior: right child (left child is this + 1)
};

// Static world-space triangle BVH shared by the occlusion baker and viewport
// surface snapping. Build() reorders the triangles, so indices returned by
// ClosestHit() are only meaningful against GetTri().
class TriangleBvh
{
public:

    void Build(std::vector<BvhTri>&& tris);
    void Clear();

    bool IsEmpty() const { return mNodes.empty(); }
    const BvhTri& GetTri(uint32_t index) const { return mTris[index]; }

    // True if any triangle blocks the segment origin + dir * t, t in (tMin, tMax).
    bool AnyHit(const glm::vec3& origin, const glm::vec3& dir, float tMin, float tMax) const;

    // Closest hit along dir within (0, tMax).
    bool ClosestHit(const glm::vec3& origin, const glm::vec3& dir, float tMax, float& outT, uint32_t& outTri) const;

    // Closest hit along dir; returns whether the hit triangle faces away from the ray.
    bool ClosestHitIsBackFacing(const glm::vec3& origin, const glm::vec3& dir, float tMax, bool& outHit) const;

private:

    uint32_t BuildRecursive(uint32_t first, uint32_t count, std::vector<glm::vec3>& centroids, std::vector<AABB>& boxes);

    std::vector<BvhTri> mTris;
    std::vector<BvhNode> mNodes;
};

void AppendTriangles(std::vector<BvhTri>& outTris, const glm::mat4& transform, const glm::vec3* positions, size_t positionStride, uint32_t numVerts, const IndexType* indices, uint32_t numIndices);

// World-space triangles of a StaticMesh3D / InstancedMesh3D / Terrain3D / Voxel3D.
void GatherPrimitiveTriangles(Primitive3D* prim, std::vector<BvhTri>& outTris);

#endif
