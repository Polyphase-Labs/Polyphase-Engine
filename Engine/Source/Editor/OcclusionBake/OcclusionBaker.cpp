#if EDITOR

#include "OcclusionBake/OcclusionBaker.h"

#include "Engine.h"
#include "World.h"
#include "Log.h"
#include "Maths.h"
#include "OcclusionData.h"
#include "EditorImgui.h"
#include "Assets/Scene.h"
#include "Assets/StaticMesh.h"
#include "Assets/Material.h"
#include "Nodes/3D/Primitive3d.h"
#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/InstancedMesh3d.h"
#include "Nodes/3D/SkeletalMesh3d.h"
#include "Nodes/3D/Terrain3d.h"
#include "Nodes/3D/Voxel3d.h"
#include "Nodes/3D/TextMesh3d.h"
#include "Nodes/3D/Skybox3D.h"
#include "Nodes/3D/ShadowMesh3d.h"
#include "Nodes/3D/OcclusionArea3d.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{

struct BakeTri
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

struct Occludee
{
    std::string mName;
    AABB mBox;
    std::vector<glm::vec3> mTargets;
    int32_t mReuseIndex = -1;   // index into the previous bake's occludees, or -1
};

struct BakeSettings
{
    uint32_t mCellSamples = 9;
    uint32_t mMeshVertexTargets = 8;
    bool mFaceCenterTargets = true;
};

const uint32_t kMaxTrisPerLeaf = 4;
const uint32_t kWarnCells = 65536;
const uint32_t kMaxCells = 262144;
const float kLargeOccludeeCells = 8.0f;     // warn when an occludee spans more than this many cells
const uint32_t kMaxDynamicSamples = 9;      // cell samples used for the coarse (dynamic) table
const uint32_t kBakeAlgorithm = 2;          // bump when sampling changes so old bits are not reused
const uint32_t kTargetCoarseCells = 512;    // pick the coarse factor so the table stays around this size

class TriangleBvh
{
public:

    void Build(std::vector<BakeTri>&& tris)
    {
        mTris = std::move(tris);
        mNodes.clear();
        if (mTris.empty())
            return;

        std::vector<glm::vec3> centroids(mTris.size());
        std::vector<AABB> boxes(mTris.size());
        for (uint32_t i = 0; i < mTris.size(); ++i)
        {
            const BakeTri& t = mTris[i];
            glm::vec3 v1 = t.mV0 + t.mE1;
            glm::vec3 v2 = t.mV0 + t.mE2;
            AABB box = AABB::MakeInvalid();
            box.Encapsulate(t.mV0);
            box.Encapsulate(v1);
            box.Encapsulate(v2);
            boxes[i] = box;
            centroids[i] = (t.mV0 + v1 + v2) / 3.0f;
        }

        mNodes.reserve(mTris.size() * 2);
        BuildRecursive(0, (uint32_t)mTris.size(), centroids, boxes);
    }

    bool IsEmpty() const { return mNodes.empty(); }

    // True if any triangle blocks the segment origin + dir * t, t in (tMin, tMax).
    bool AnyHit(const glm::vec3& origin, const glm::vec3& dir, float tMin, float tMax) const
    {
        if (mNodes.empty())
            return false;

        glm::vec3 invDir;
        for (int32_t i = 0; i < 3; ++i)
        {
            invDir[i] = (fabsf(dir[i]) > 1e-12f) ? (1.0f / dir[i]) : ((dir[i] < 0.0f) ? -1e30f : 1e30f);
        }

        uint32_t stack[64];
        uint32_t stackSize = 0;
        stack[stackSize++] = 0;

        while (stackSize > 0)
        {
            const BvhNode& node = mNodes[stack[--stackSize]];
            if (!Maths::RayIntersectsAABB(origin, invDir, node.mBox.mMin, node.mBox.mMax, tMin, tMax))
                continue;

            if (node.mCount > 0)
            {
                for (uint32_t i = 0; i < node.mCount; ++i)
                {
                    const BakeTri& t = mTris[node.mFirst + i];
                    float hitT = 0.0f;
                    if (Maths::RayIntersectsTriangle(origin, dir, t.mV0, t.mE1, t.mE2, hitT) &&
                        hitT > tMin && hitT < tMax)
                    {
                        return true;
                    }
                }
            }
            else if (stackSize + 2 <= 64)
            {
                stack[stackSize++] = node.mRight;
                stack[stackSize++] = (uint32_t)(&node - mNodes.data()) + 1;
            }
        }

        return false;
    }

    // Closest hit along dir; returns whether the hit triangle faces away from the ray.
    bool ClosestHitIsBackFacing(const glm::vec3& origin, const glm::vec3& dir, float tMax, bool& outHit) const
    {
        outHit = false;
        if (mNodes.empty())
            return false;

        glm::vec3 invDir;
        for (int32_t i = 0; i < 3; ++i)
        {
            invDir[i] = (fabsf(dir[i]) > 1e-12f) ? (1.0f / dir[i]) : ((dir[i] < 0.0f) ? -1e30f : 1e30f);
        }

        float bestT = tMax;
        bool bestBackFacing = false;

        uint32_t stack[64];
        uint32_t stackSize = 0;
        stack[stackSize++] = 0;

        while (stackSize > 0)
        {
            const BvhNode& node = mNodes[stack[--stackSize]];
            if (!Maths::RayIntersectsAABB(origin, invDir, node.mBox.mMin, node.mBox.mMax, 0.0f, bestT))
                continue;

            if (node.mCount > 0)
            {
                for (uint32_t i = 0; i < node.mCount; ++i)
                {
                    const BakeTri& t = mTris[node.mFirst + i];
                    float hitT = 0.0f;
                    if (Maths::RayIntersectsTriangle(origin, dir, t.mV0, t.mE1, t.mE2, hitT) && hitT < bestT)
                    {
                        bestT = hitT;
                        outHit = true;
                        bestBackFacing = glm::dot(glm::cross(t.mE1, t.mE2), dir) > 0.0f;
                    }
                }
            }
            else if (stackSize + 2 <= 64)
            {
                stack[stackSize++] = node.mRight;
                stack[stackSize++] = (uint32_t)(&node - mNodes.data()) + 1;
            }
        }

        return bestBackFacing;
    }

private:

    uint32_t BuildRecursive(uint32_t first, uint32_t count, std::vector<glm::vec3>& centroids, std::vector<AABB>& boxes)
    {
        uint32_t nodeIndex = (uint32_t)mNodes.size();
        mNodes.push_back(BvhNode());

        AABB box = AABB::MakeInvalid();
        for (uint32_t i = 0; i < count; ++i)
        {
            box.Encapsulate(boxes[first + i]);
        }
        mNodes[nodeIndex].mBox = box;

        if (count <= kMaxTrisPerLeaf)
        {
            mNodes[nodeIndex].mFirst = first;
            mNodes[nodeIndex].mCount = count;
            return nodeIndex;
        }

        AABB centroidBox = AABB::MakeInvalid();
        for (uint32_t i = 0; i < count; ++i)
        {
            centroidBox.Encapsulate(centroids[first + i]);
        }
        glm::vec3 size = centroidBox.GetSize();
        int32_t axis = 0;
        if (size.y > size.x) axis = 1;
        if (size.z > size[axis]) axis = 2;

        uint32_t mid = first + count / 2;
        if (size[axis] > 1e-6f)
        {
            // Median split on the chosen axis. Sort the parallel arrays together.
            std::vector<uint32_t> order(count);
            for (uint32_t i = 0; i < count; ++i) order[i] = first + i;
            std::nth_element(order.begin(), order.begin() + (count / 2), order.end(),
                [&](uint32_t a, uint32_t b) { return centroids[a][axis] < centroids[b][axis]; });

            std::vector<BakeTri> tmpTris(count);
            std::vector<glm::vec3> tmpCentroids(count);
            std::vector<AABB> tmpBoxes(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                tmpTris[i] = mTris[order[i]];
                tmpCentroids[i] = centroids[order[i]];
                tmpBoxes[i] = boxes[order[i]];
            }
            for (uint32_t i = 0; i < count; ++i)
            {
                mTris[first + i] = tmpTris[i];
                centroids[first + i] = tmpCentroids[i];
                boxes[first + i] = tmpBoxes[i];
            }
        }

        BuildRecursive(first, mid - first, centroids, boxes);
        uint32_t right = BuildRecursive(mid, first + count - mid, centroids, boxes);
        mNodes[nodeIndex].mRight = right;
        return nodeIndex;
    }

    std::vector<BakeTri> mTris;
    std::vector<BvhNode> mNodes;
};

bool IsEligibleOccludeeType(Node* node)
{
    if (node->As<Skybox3D>() != nullptr || node->As<ShadowMesh3D>() != nullptr)
        return false;

    return node->As<StaticMesh3D>() != nullptr ||
        node->As<SkeletalMesh3D>() != nullptr ||
        node->As<Terrain3D>() != nullptr ||
        node->As<Voxel3D>() != nullptr ||
        node->As<TextMesh3D>() != nullptr;
}

bool MaterialCanOcclude(Material* material)
{
    if (material == nullptr)
        return true;

    BlendMode mode = material->GetBlendMode();
    return mode == BlendMode::Opaque;
}

void AppendTriangles(std::vector<BakeTri>& outTris, const glm::mat4& transform, const glm::vec3* positions, size_t positionStride, uint32_t numVerts, const IndexType* indices, uint32_t numIndices)
{
    if (positions == nullptr || indices == nullptr || numVerts == 0 || numIndices < 3)
        return;

    std::vector<glm::vec3> worldVerts(numVerts);
    for (uint32_t i = 0; i < numVerts; ++i)
    {
        const glm::vec3* p = (const glm::vec3*)((const uint8_t*)positions + i * positionStride);
        worldVerts[i] = glm::vec3(transform * glm::vec4(*p, 1.0f));
    }

    for (uint32_t i = 0; i + 2 < numIndices; i += 3)
    {
        uint32_t ia = (uint32_t)indices[i + 0];
        uint32_t ib = (uint32_t)indices[i + 1];
        uint32_t ic = (uint32_t)indices[i + 2];
        if (ia >= numVerts || ib >= numVerts || ic >= numVerts)
            continue;

        BakeTri tri;
        tri.mV0 = worldVerts[ia];
        tri.mE1 = worldVerts[ib] - worldVerts[ia];
        tri.mE2 = worldVerts[ic] - worldVerts[ia];

        glm::vec3 n = glm::cross(tri.mE1, tri.mE2);
        if (glm::dot(n, n) <= 1e-12f)
            continue;

        outTris.push_back(tri);
    }
}

void GatherOccluderTriangles(Primitive3D* prim, std::vector<BakeTri>& outTris)
{
    if (StaticMesh3D* meshNode = prim->As<StaticMesh3D>())
    {
        StaticMesh* mesh = meshNode->GetStaticMesh();
        if (mesh == nullptr || !MaterialCanOcclude(meshNode->GetMaterial()))
            return;

        const Vertex* verts = mesh->GetVertices();
        const VertexColor* colorVerts = mesh->HasVertexColor() ? mesh->GetColorVertices() : nullptr;
        const glm::vec3* positions = colorVerts ? &colorVerts[0].mPosition : (verts ? &verts[0].mPosition : nullptr);
        size_t stride = colorVerts ? sizeof(VertexColor) : sizeof(Vertex);

        if (InstancedMesh3D* instNode = prim->As<InstancedMesh3D>())
        {
            if (instNode->IsUnrolled())
                return;

            const glm::mat4 nodeTransform = instNode->GetTransform();
            for (uint32_t i = 0; i < instNode->GetNumInstances(); ++i)
            {
                glm::mat4 transform = nodeTransform * instNode->CalculateInstanceTransform((int32_t)i);
                AppendTriangles(outTris, transform, positions, stride, mesh->GetNumVertices(), mesh->GetIndices(), mesh->GetNumIndices());
            }
        }
        else
        {
            AppendTriangles(outTris, meshNode->GetTransform(), positions, stride, mesh->GetNumVertices(), mesh->GetIndices(), mesh->GetNumIndices());
        }
    }
    else if (Terrain3D* terrain = prim->As<Terrain3D>())
    {
        const std::vector<VertexColor>& verts = terrain->GetVertices();
        const std::vector<IndexType>& indices = terrain->GetIndices();
        if (!verts.empty() && !indices.empty())
        {
            AppendTriangles(outTris, terrain->GetTransform(), &verts[0].mPosition, sizeof(VertexColor),
                (uint32_t)verts.size(), indices.data(), (uint32_t)indices.size());
        }
    }
    else if (Voxel3D* voxel = prim->As<Voxel3D>())
    {
        const std::vector<VertexColor>& verts = voxel->GetVertices();
        const std::vector<IndexType>& indices = voxel->GetIndices();
        if (!verts.empty() && !indices.empty())
        {
            AppendTriangles(outTris, voxel->GetTransform(), &verts[0].mPosition, sizeof(VertexColor),
                (uint32_t)verts.size(), indices.data(), (uint32_t)indices.size());
        }
    }
}

// Grid of points across one face of the box. u/v are the in-plane axes, n is
// the face normal axis, sign picks which of the two faces.
void AddFaceGridTargets(glm::vec3 c, glm::vec3 e, int32_t n, float sign, uint32_t nu, uint32_t nv, std::vector<glm::vec3>& outTargets)
{
    int32_t u = (n + 1) % 3;
    int32_t v = (n + 2) % 3;

    for (uint32_t iu = 0; iu < nu; ++iu)
    {
        for (uint32_t iv = 0; iv < nv; ++iv)
        {
            float fu = (nu == 1) ? 0.0f : (-1.0f + 2.0f * (iu + 0.5f) / nu);
            float fv = (nv == 1) ? 0.0f : (-1.0f + 2.0f * (iv + 0.5f) / nv);

            glm::vec3 p = c;
            p[n] += sign * e[n];
            p[u] += fu * e[u];
            p[v] += fv * e[v];
            outTargets.push_back(p);
        }
    }
}

// Box-only targets: center, corners, face centers, and a face grid for boxes
// larger than a couple of cells.
void GatherBoxTargets(const AABB& box, const BakeSettings& settings, float cellSize, std::vector<glm::vec3>& outTargets)
{
    glm::vec3 c = box.GetCenter();
    glm::vec3 e = box.GetExtents();

    outTargets.push_back(c);
    for (int32_t i = 0; i < 8; ++i)
    {
        glm::vec3 corner = c + glm::vec3((i & 1) ? e.x : -e.x, (i & 2) ? e.y : -e.y, (i & 4) ? e.z : -e.z);
        outTargets.push_back(corner);
    }

    if (settings.mFaceCenterTargets)
    {
        outTargets.push_back(c + glm::vec3(e.x, 0.0f, 0.0f));
        outTargets.push_back(c - glm::vec3(e.x, 0.0f, 0.0f));
        outTargets.push_back(c + glm::vec3(0.0f, e.y, 0.0f));
        outTargets.push_back(c - glm::vec3(0.0f, e.y, 0.0f));
        outTargets.push_back(c + glm::vec3(0.0f, 0.0f, e.z));
        outTargets.push_back(c - glm::vec3(0.0f, 0.0f, e.z));
    }

    // Large objects (walls, floors) are only ever partially hidden. Corners and
    // face centers alone miss the case where an occluder covers those points
    // but the middle of a face is still in view, so spread extra targets over
    // every face that is bigger than a couple of cells.
    const float spacing = 2.0f * cellSize;
    const uint32_t kMaxPerAxis = settings.mMeshVertexTargets >= 24 ? 8u : 6u;
    glm::vec3 size = box.GetSize();

    uint32_t counts[3];
    bool anyLarge = false;
    for (int32_t axis = 0; axis < 3; ++axis)
    {
        counts[axis] = glm::clamp<uint32_t>((uint32_t)ceilf(size[axis] / spacing), 1, kMaxPerAxis);
        anyLarge = anyLarge || (size[axis] > spacing);
    }

    if (anyLarge)
    {
        for (int32_t n = 0; n < 3; ++n)
        {
            uint32_t nu = counts[(n + 1) % 3];
            uint32_t nv = counts[(n + 2) % 3];
            if (nu * nv <= 1)
                continue;

            AddFaceGridTargets(c, e, n, 1.0f, nu, nv, outTargets);
            AddFaceGridTargets(c, e, n, -1.0f, nu, nv, outTargets);
        }
    }
}

void GatherOccludeeTargets(Primitive3D* prim, const AABB& box, const BakeSettings& settings, float cellSize, std::vector<glm::vec3>& outTargets)
{
    GatherBoxTargets(box, settings, cellSize, outTargets);

    StaticMesh3D* meshNode = prim->As<StaticMesh3D>();
    if (settings.mMeshVertexTargets > 0 && meshNode != nullptr && meshNode->As<InstancedMesh3D>() == nullptr)
    {
        StaticMesh* mesh = meshNode->GetStaticMesh();
        if (mesh != nullptr && mesh->GetNumVertices() > 0)
        {
            const Vertex* verts = mesh->GetVertices();
            const VertexColor* colorVerts = mesh->HasVertexColor() ? mesh->GetColorVertices() : nullptr;
            uint32_t numVerts = mesh->GetNumVertices();
            uint32_t stride = numVerts / settings.mMeshVertexTargets;
            if (stride == 0) stride = 1;
            const glm::mat4 transform = meshNode->GetTransform();

            size_t maxTargets = outTargets.size() + settings.mMeshVertexTargets;
            for (uint32_t i = 0; i < numVerts && outTargets.size() < maxTargets; i += stride)
            {
                glm::vec3 local = colorVerts ? colorVerts[i].mPosition : verts[i].mPosition;
                outTargets.push_back(glm::vec3(transform * glm::vec4(local, 1.0f)));
            }
        }
    }
}

uint32_t Lcg(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

float LcgUnit(uint32_t& state)
{
    return (Lcg(state) >> 8) * (1.0f / 16777216.0f);
}

void GenerateCellSamples(const AABB& cell, uint32_t cellIndex, uint32_t numSamples, std::vector<glm::vec3>& outSamples)
{
    outSamples.clear();
    glm::vec3 c = cell.GetCenter();
    glm::vec3 e = cell.GetExtents();
    // Samples must reach the cell faces: the camera can sit right at the top
    // of a cell and see over a low wall that a sample inset from the face
    // would not clear. 2% keeps them off shared cell boundaries.
    glm::vec3 insetE = e * 0.98f;

    outSamples.push_back(c);

    uint32_t rng = cellIndex * 2654435761u + 12345u;

    if (numSamples <= 4)
    {
        for (uint32_t i = 1; i < numSamples; ++i)
        {
            glm::vec3 r(LcgUnit(rng) * 2.0f - 1.0f, LcgUnit(rng) * 2.0f - 1.0f, LcgUnit(rng) * 2.0f - 1.0f);
            outSamples.push_back(c + r * insetE);
        }
        return;
    }

    for (int32_t i = 0; i < 8; ++i)
    {
        outSamples.push_back(c + glm::vec3((i & 1) ? insetE.x : -insetE.x, (i & 2) ? insetE.y : -insetE.y, (i & 4) ? insetE.z : -insetE.z));
    }

    while (outSamples.size() < numSamples)
    {
        glm::vec3 r(LcgUnit(rng) * 2.0f - 1.0f, LcgUnit(rng) * 2.0f - 1.0f, LcgUnit(rng) * 2.0f - 1.0f);
        outSamples.push_back(c + r * insetE);
    }
}

bool IsPointInsideSolid(const TriangleBvh& bvh, glm::vec3 point, float maxDist)
{
    static const glm::vec3 kDirs[6] =
    {
        { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
    };

    int32_t backFacing = 0;
    for (int32_t i = 0; i < 6; ++i)
    {
        bool hit = false;
        if (bvh.ClosestHitIsBackFacing(point, kDirs[i], maxDist, hit) && hit)
        {
            ++backFacing;
        }
    }

    return backFacing >= 4;
}

// True if any sample -> target segment is unblocked.
bool AnyLineOfSight(const TriangleBvh& bvh, const std::vector<glm::vec3>& samples, size_t maxSamples,
    const std::vector<glm::vec3>& targets, float rayEps)
{
    size_t numSamples = glm::min(samples.size(), maxSamples);
    for (size_t si = 0; si < numSamples; ++si)
    {
        const glm::vec3& s = samples[si];
        for (size_t ti = 0; ti < targets.size(); ++ti)
        {
            glm::vec3 delta = targets[ti] - s;
            float dist = glm::length(delta);
            if (dist <= rayEps)
            {
                return true;
            }

            glm::vec3 dir = delta / dist;
            if (!bvh.AnyHit(s, dir, rayEps, dist - rayEps))
            {
                return true;
            }
        }
    }
    return false;
}

uint64_t HashWords(const uint32_t* words, uint32_t count)
{
    uint64_t hash = 1469598103934665603ULL;
    for (uint32_t i = 0; i < count; ++i)
    {
        hash ^= words[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Deduplicate per-cell bitsets into a pool. Cells flagged all-visible map to kNoSet.
void PackSets(uint32_t numCells, uint32_t wordsPerSet, const std::vector<uint32_t>& cellWords,
    const std::vector<uint8_t>& cellAllVisible, std::vector<uint32_t>& outCellToSet, std::vector<uint32_t>& outPool)
{
    outCellToSet.assign(numCells, OcclusionData::kNoSet);
    outPool.clear();

    std::unordered_map<uint64_t, std::vector<uint32_t>> lookup;
    for (uint32_t cell = 0; cell < numCells; ++cell)
    {
        if (cellAllVisible[cell])
            continue;

        const uint32_t* words = &cellWords[(size_t)cell * wordsPerSet];
        uint64_t hash = HashWords(words, wordsPerSet);
        std::vector<uint32_t>& candidates = lookup[hash];

        uint32_t setIndex = OcclusionData::kNoSet;
        for (uint32_t candidate : candidates)
        {
            if (memcmp(&outPool[(size_t)candidate * wordsPerSet], words, wordsPerSet * sizeof(uint32_t)) == 0)
            {
                setIndex = candidate;
                break;
            }
        }

        if (setIndex == OcclusionData::kNoSet)
        {
            setIndex = (uint32_t)(outPool.size() / wordsPerSet);
            outPool.insert(outPool.end(), words, words + wordsPerSet);
            candidates.push_back(setIndex);
        }

        outCellToSet[cell] = setIndex;
    }
}

BakeSettings SettingsForQuality(uint8_t quality)
{
    BakeSettings settings;
    switch (quality)
    {
    case 0:
        settings.mCellSamples = 4;
        settings.mMeshVertexTargets = 0;
        settings.mFaceCenterTargets = false;
        break;
    case 2:
        settings.mCellSamples = 17;
        settings.mMeshVertexTargets = 24;
        settings.mFaceCenterTargets = true;
        break;
    default:
        settings.mCellSamples = 9;
        settings.mMeshVertexTargets = 8;
        settings.mFaceCenterTargets = true;
        break;
    }
    return settings;
}

bool NearlyEqual(glm::vec3 a, glm::vec3 b, float eps)
{
    glm::vec3 d = glm::abs(a - b);
    return d.x <= eps && d.y <= eps && d.z <= eps;
}

} // namespace

void OcclusionBaker::BakeCurrentScene()
{
    World* world = GetWorld(0);
    Node* root = world ? world->GetRootNode() : nullptr;
    Scene* scene = root ? root->GetScene() : nullptr;

    if (IsPlayingInEditor())
    {
        LogError("Occlusion bake: stop Play In Editor first.");
        return;
    }

    if (scene == nullptr)
    {
        LogError("Occlusion bake: the current scene must be saved before baking.");
        return;
    }

    const float cellSize = glm::max(scene->GetOcclusionCellSize(), 0.25f);
    const BakeSettings settings = SettingsForQuality(scene->GetOcclusionBakeQuality());
    const bool wantDynamic = scene->IsOcclusionDynamicEnabled();

    auto startTime = std::chrono::steady_clock::now();
    EditorProgress::Begin("Baking Occlusion Culling", "Gathering geometry...", true);

    // Gather occluder triangles and occludees. Hidden subtrees are skipped,
    // matching what the renderer draws.
    std::vector<BakeTri> tris;
    std::vector<Occludee> occludees;
    std::vector<OcclusionEntry> occluderEntries;
    std::vector<OcclusionArea3D*> areas;
    std::vector<std::string> largeOccludees;

    auto gather = [&](Node* node) -> bool
    {
        if (!node->IsVisible())
            return false;

        if (OcclusionArea3D* area = node->As<OcclusionArea3D>())
        {
            areas.push_back(area);
            return true;
        }

        if (!node->IsPrimitive3D() || !IsEligibleOccludeeType(node))
            return true;

        Primitive3D* prim = static_cast<Primitive3D*>(node);
        prim->GetTransform();
        AABB box = prim->GetAABB();
        bool validBox = box.IsValid() && !box.IsLarge();

        if (prim->IsOccluder() && validBox)
        {
            size_t before = tris.size();
            GatherOccluderTriangles(prim, tris);
            if (tris.size() > before)
            {
                OcclusionEntry entry;
                entry.mCenter = box.GetCenter();
                entry.mExtents = box.GetExtents();
                occluderEntries.push_back(entry);
            }
        }

        if (prim->IsOccludee() && validBox)
        {
            Occludee occludee;
            occludee.mName = prim->GetName();
            occludee.mBox = box;
            GatherOccludeeTargets(prim, box, settings, cellSize, occludee.mTargets);
            occludees.push_back(std::move(occludee));

            glm::vec3 size = box.GetSize();
            float maxSize = glm::max(size.x, glm::max(size.y, size.z));
            if (maxSize > kLargeOccludeeCells * cellSize)
            {
                largeOccludees.push_back(prim->GetName());
            }

            // Consoles unroll InstancedMesh3D into one StaticMesh3D per cell on
            // load. Bake one occludee per unroll cell as well so those children
            // cull individually instead of inheriting the whole node's bit.
            if (InstancedMesh3D* instNode = prim->As<InstancedMesh3D>())
            {
                std::vector<AABB> buckets;
                instNode->ComputeUnrollBuckets(buckets);
                if (buckets.size() > 1)
                {
                    const glm::mat4 nodeTransform = instNode->GetTransform();
                    for (uint32_t b = 0; b < buckets.size(); ++b)
                    {
                        if (!buckets[b].IsValid())
                            continue;

                        Occludee bucket;
                        bucket.mName = prim->GetName() + " [cell]";
                        bucket.mBox = buckets[b].Transform(nodeTransform);
                        GatherBoxTargets(bucket.mBox, settings, cellSize, bucket.mTargets);
                        occludees.push_back(std::move(bucket));
                    }
                }
            }
        }

        return true;
    };
    root->Traverse(gather);

    if (occludees.empty())
    {
        EditorProgress::End();
        LogWarning("Occlusion bake: no nodes have 'Occludee Static' enabled. Nothing to bake.");
        return;
    }

    if (tris.empty())
    {
        EditorProgress::End();
        LogWarning("Occlusion bake: no opaque geometry has 'Occluder Static' enabled. Nothing to bake.");
        return;
    }

    if (!largeOccludees.empty())
    {
        std::string names;
        for (size_t i = 0; i < largeOccludees.size() && i < 8; ++i)
        {
            if (i > 0) names += ", ";
            names += largeOccludees[i];
        }
        if (largeOccludees.size() > 8)
        {
            names += " (+" + std::to_string(largeOccludees.size() - 8) + " more)";
        }
        LogWarning("Occlusion bake: %u occludee(s) span more than %.0f cells and will rarely be culled: %s. Split them into segments, or untick 'Occludee Static' and keep 'Occluder Static'.",
            (uint32_t)largeOccludees.size(), kLargeOccludeeCells, names.c_str());
    }

    // Bake volume.
    AABB volume = AABB::MakeInvalid();
    if (!areas.empty())
    {
        for (OcclusionArea3D* area : areas)
        {
            volume.Encapsulate(area->GetWorldAABB());
        }
    }
    else
    {
        for (const Occludee& o : occludees)
        {
            volume.Encapsulate(o.mBox);
        }
        volume.Expand(2.0f * cellSize);
    }

    if (!volume.IsValid())
    {
        EditorProgress::End();
        LogError("Occlusion bake: invalid bake volume.");
        return;
    }

    glm::vec3 volumeSize = volume.GetSize();
    uint32_t dimX = glm::max<uint32_t>(1, (uint32_t)ceilf(volumeSize.x / cellSize));
    uint32_t dimY = glm::max<uint32_t>(1, (uint32_t)ceilf(volumeSize.y / cellSize));
    uint32_t dimZ = glm::max<uint32_t>(1, (uint32_t)ceilf(volumeSize.z / cellSize));
    uint64_t numCells64 = (uint64_t)dimX * dimY * dimZ;

    if (numCells64 > kMaxCells)
    {
        EditorProgress::End();
        LogError("Occlusion bake: %u x %u x %u = %llu cells exceeds the %u cell limit. Increase the cell size or add OcclusionArea3D nodes to limit the volume.",
            dimX, dimY, dimZ, (unsigned long long)numCells64, kMaxCells);
        return;
    }
    if (numCells64 > kWarnCells)
    {
        LogWarning("Occlusion bake: %llu cells. This will take a while and use a lot of memory on consoles. Consider a larger cell size or OcclusionArea3D nodes.",
            (unsigned long long)numCells64);
    }

    const uint32_t numCells = (uint32_t)numCells64;
    const uint32_t numOccludees = (uint32_t)occludees.size();
    const uint32_t wordsPerSet = (numOccludees + 31) / 32;

    // Coarse grid for dynamic occludees.
    uint32_t coarseFactor = 0;
    uint32_t coarseDimX = 0, coarseDimY = 0, coarseDimZ = 0;
    uint32_t numCoarse = 0;
    uint32_t wordsPerCoarseSet = 0;
    if (wantDynamic)
    {
        float f = cbrtf((float)numCells / (float)kTargetCoarseCells);
        coarseFactor = glm::clamp<uint32_t>((uint32_t)ceilf(f), 4, 16);
        coarseDimX = (dimX + coarseFactor - 1) / coarseFactor;
        coarseDimY = (dimY + coarseFactor - 1) / coarseFactor;
        coarseDimZ = (dimZ + coarseFactor - 1) / coarseFactor;
        numCoarse = coarseDimX * coarseDimY * coarseDimZ;
        wordsPerCoarseSet = (numCoarse + 31) / 32;
    }

    // Incremental bake: when the grid and every occluder are unchanged, the
    // visibility of occludees that already existed cannot have changed, so
    // their bits (and the dynamic table) are copied from the previous bake.
    const OcclusionData& old = scene->GetOcclusionData();
    bool reuseStatic = false;
    bool reuseDynamic = false;
    uint32_t numReused = 0;
    if (old.IsValid() &&
        old.mDimX == dimX && old.mDimY == dimY && old.mDimZ == dimZ &&
        fabsf(old.mCellSize - cellSize) < 1e-4f &&
        NearlyEqual(old.mMin, volume.mMin, 1e-3f) &&
        old.mOccluders.size() == occluderEntries.size() &&
        old.mBakeAlgorithm == kBakeAlgorithm)
    {
        OcclusionEntryIndex occluderIndex;
        occluderIndex.Build(old.mOccluders, cellSize);
        std::vector<uint8_t> matched(old.mOccluders.size(), 0);
        bool allMatched = true;
        for (const OcclusionEntry& e : occluderEntries)
        {
            int32_t idx = occluderIndex.Find(e.mCenter, e.mExtents);
            if (idx < 0 || matched[idx])
            {
                allMatched = false;
                break;
            }
            matched[idx] = 1;
        }

        if (allMatched)
        {
            reuseStatic = true;
            OcclusionEntryIndex occludeeIndex;
            occludeeIndex.Build(old.mOccludees, cellSize);
            for (Occludee& o : occludees)
            {
                o.mReuseIndex = occludeeIndex.Find(o.mBox.GetCenter(), o.mBox.GetExtents());
                if (o.mReuseIndex >= 0)
                    ++numReused;
            }

            reuseDynamic = wantDynamic && old.HasDynamicTable() && old.mCoarseFactor == coarseFactor;
        }
    }

    EditorProgress::SetStatus("Building BVH...");
    TriangleBvh bvh;
    const uint32_t numTris = (uint32_t)tris.size();
    bvh.Build(std::move(tris));

    // Precompute occludee boxes expanded by a cell so anything touching or
    // inside the camera's cell is always visible.
    std::vector<AABB> nearBoxes(numOccludees);
    for (uint32_t i = 0; i < numOccludees; ++i)
    {
        nearBoxes[i] = occludees[i].mBox;
        nearBoxes[i].Expand(cellSize);
    }

    // Coarse cell boxes + targets.
    std::vector<AABB> coarseNearBoxes(numCoarse);
    std::vector<std::vector<glm::vec3>> coarseTargets(numCoarse);
    if (wantDynamic && !reuseDynamic)
    {
        const float coarseSize = cellSize * float(coarseFactor);
        const float inset = 0.25f * cellSize;
        for (uint32_t i = 0; i < numCoarse; ++i)
        {
            uint32_t x = i % coarseDimX;
            uint32_t y = (i / coarseDimX) % coarseDimY;
            uint32_t z = i / (coarseDimX * coarseDimY);
            glm::vec3 bmin = volume.mMin + glm::vec3(float(x), float(y), float(z)) * coarseSize;
            glm::vec3 bmax = glm::min(bmin + glm::vec3(coarseSize), volume.mMax);
            AABB box(bmin, bmax);

            coarseNearBoxes[i] = box;
            coarseNearBoxes[i].Expand(cellSize);

            AABB target = box;
            target.Expand(-inset);
            if (!target.IsValid()) target = box;
            glm::vec3 c = target.GetCenter();
            glm::vec3 e = target.GetExtents();
            coarseTargets[i].push_back(c);
            for (int32_t k = 0; k < 8; ++k)
            {
                coarseTargets[i].push_back(c + glm::vec3((k & 1) ? e.x : -e.x, (k & 2) ? e.y : -e.y, (k & 4) ? e.z : -e.z));
            }
        }
    }

    const float rayEps = 1e-3f * cellSize;
    const float solidProbeDist = 4.0f * cellSize;

    std::vector<uint32_t> cellWords((size_t)numCells * wordsPerSet, 0u);
    std::vector<uint8_t> cellAllVisible(numCells, 0);
    std::vector<uint32_t> coarseWords;
    std::vector<uint8_t> coarseAllVisible;
    if (wantDynamic && !reuseDynamic)
    {
        coarseWords.assign((size_t)numCells * wordsPerCoarseSet, 0u);
        coarseAllVisible.assign(numCells, 0);
    }

    std::atomic<uint32_t> nextCell(0);
    std::atomic<uint32_t> doneCells(0);
    std::atomic<bool> cancel(false);

    auto worker = [&]()
    {
        std::vector<glm::vec3> samples;
        std::vector<glm::vec3> validSamples;

        while (true)
        {
            uint32_t cell = nextCell.fetch_add(1);
            if (cell >= numCells || cancel.load())
                break;

            uint32_t x = cell % dimX;
            uint32_t y = (cell / dimX) % dimY;
            uint32_t z = cell / (dimX * dimY);
            glm::vec3 cellMin = volume.mMin + glm::vec3(float(x), float(y), float(z)) * cellSize;
            AABB cellBox(cellMin, cellMin + glm::vec3(cellSize));

            const uint32_t* oldSet = reuseStatic ? old.GetSet((int32_t)cell) : nullptr;
            const bool oldAllVisible = reuseStatic && old.GetCellSetIndex((int32_t)cell) == OcclusionData::kNoSet;

            GenerateCellSamples(cellBox, cell, settings.mCellSamples, samples);

            validSamples.clear();
            for (const glm::vec3& s : samples)
            {
                if (!IsPointInsideSolid(bvh, s, solidProbeDist))
                {
                    validSamples.push_back(s);
                }
            }

            if (validSamples.empty())
            {
                // Entire cell is inside geometry. Be conservative in case the
                // camera clips through a wall.
                cellAllVisible[cell] = 1;
                if (!coarseAllVisible.empty())
                    coarseAllVisible[cell] = 1;
                doneCells.fetch_add(1);
                continue;
            }

            uint32_t* words = &cellWords[(size_t)cell * wordsPerSet];
            uint32_t visibleCount = 0;

            for (uint32_t o = 0; o < numOccludees; ++o)
            {
                if (cancel.load())
                    break;

                bool visible = false;
                const Occludee& occludee = occludees[o];

                if (reuseStatic && occludee.mReuseIndex >= 0)
                {
                    visible = oldAllVisible || old.IsVisible(oldSet, (uint32_t)occludee.mReuseIndex);
                }
                else
                {
                    visible = nearBoxes[o].Intersects(cellBox) ||
                        AnyLineOfSight(bvh, validSamples, validSamples.size(), occludee.mTargets, rayEps);
                }

                if (visible)
                {
                    words[o >> 5] |= (1u << (o & 31));
                    ++visibleCount;
                }
            }

            if (visibleCount == numOccludees)
            {
                cellAllVisible[cell] = 1;
            }

            if (!coarseWords.empty())
            {
                uint32_t* cwords = &coarseWords[(size_t)cell * wordsPerCoarseSet];
                uint32_t coarseVisible = 0;
                for (uint32_t ci = 0; ci < numCoarse; ++ci)
                {
                    if (cancel.load())
                        break;

                    bool visible = coarseNearBoxes[ci].Intersects(cellBox) ||
                        AnyLineOfSight(bvh, validSamples, kMaxDynamicSamples, coarseTargets[ci], rayEps);
                    if (visible)
                    {
                        cwords[ci >> 5] |= (1u << (ci & 31));
                        ++coarseVisible;
                    }
                }
                if (coarseVisible == numCoarse)
                {
                    coarseAllVisible[cell] = 1;
                }
            }

            doneCells.fetch_add(1);
        }
    };

    uint32_t numThreads = std::thread::hardware_concurrency();
    numThreads = (numThreads > 1) ? (numThreads - 1) : 1;
    numThreads = glm::min(numThreads, numCells);

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (uint32_t i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(worker);
    }

    char statusBuf[160];
    while (true)
    {
        uint32_t done = doneCells.load();
        if (done >= numCells)
            break;

        if (EditorProgress::WasCancelled())
        {
            cancel.store(true);
            break;
        }

        snprintf(statusBuf, sizeof(statusBuf), "Cell %u / %u (%u occludees%s, %u triangles%s)",
            done, numCells, numOccludees,
            reuseStatic ? ", incremental" : "",
            numTris,
            (wantDynamic && !reuseDynamic) ? ", + dynamic table" : "");
        EditorProgress::Step(statusBuf, (int)done, (int)numCells);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (std::thread& t : threads)
    {
        t.join();
    }

    if (cancel.load())
    {
        EditorProgress::End();
        LogWarning("Occlusion bake cancelled. Existing occlusion data left unchanged.");
        return;
    }

    EditorProgress::SetStatus("Packing visibility sets...");

    OcclusionData data;
    data.mMin = volume.mMin;
    data.mCellSize = cellSize;
    data.mDimX = dimX;
    data.mDimY = dimY;
    data.mDimZ = dimZ;
    data.mWordsPerSet = wordsPerSet;
    data.mBakeAlgorithm = kBakeAlgorithm;

    {
        std::vector<uint32_t> cellToSet;
        PackSets(numCells, wordsPerSet, cellWords, cellAllVisible, cellToSet, data.mSetPool);
        data.SetCellSetIndices(cellToSet);
    }

    if (wantDynamic)
    {
        data.mCoarseFactor = coarseFactor;
        data.mCoarseDimX = coarseDimX;
        data.mCoarseDimY = coarseDimY;
        data.mCoarseDimZ = coarseDimZ;
        data.mWordsPerCoarseSet = wordsPerCoarseSet;

        if (reuseDynamic)
        {
            data.mCellToCoarseSet16 = old.mCellToCoarseSet16;
            data.mCellToCoarseSet32 = old.mCellToCoarseSet32;
            data.mCoarseSetPool = old.mCoarseSetPool;
        }
        else
        {
            std::vector<uint32_t> cellToCoarse;
            PackSets(numCells, wordsPerCoarseSet, coarseWords, coarseAllVisible, cellToCoarse, data.mCoarseSetPool);
            data.SetCellCoarseSetIndices(cellToCoarse);
        }
    }

    data.mOccludees.resize(numOccludees);
    for (uint32_t i = 0; i < numOccludees; ++i)
    {
        data.mOccludees[i].mCenter = occludees[i].mBox.GetCenter();
        data.mOccludees[i].mExtents = occludees[i].mBox.GetExtents();
    }
    data.mOccluders = occluderEntries;

    uint32_t uniqueSets = data.GetNumUniqueSets();
    uint32_t uniqueCoarse = data.GetNumUniqueCoarseSets();
    size_t staticBytes = data.GetStaticTableBytes();
    size_t dynamicBytes = data.GetDynamicTableBytes();

    scene->SetOcclusionData(std::move(data));
    scene->SetDirtyFlag();
    world->UpdateRenderSettings();

    EditorProgress::End();

    float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    LogDebug("Occlusion bake complete: %u occludees (%u reused), %u occluders (%u tris), %u x %u x %u cells (%.1f m), %u unique sets, static %u KB, dynamic %u KB (%u coarse cells, %u sets%s), %.1f s",
        numOccludees, numReused, (uint32_t)occluderEntries.size(), numTris, dimX, dimY, dimZ, cellSize, uniqueSets,
        (uint32_t)(staticBytes / 1024), (uint32_t)(dynamicBytes / 1024), numCoarse, uniqueCoarse,
        reuseDynamic ? ", reused" : "", elapsed);

    uint32_t budgetKB = scene->GetOcclusionConsoleBudgetKB();
    if (budgetKB > 0 && (staticBytes + dynamicBytes) > size_t(budgetKB) * 1024)
    {
        LogWarning("Occlusion data (%u KB) exceeds this scene's console budget (%u KB); it will be stripped from console builds. Raise 'Occlusion Console Budget KB', use a larger cell size, or disable the dynamic table.",
            (uint32_t)((staticBytes + dynamicBytes) / 1024), budgetKB);
    }
}

void OcclusionBaker::ClearCurrentScene()
{
    World* world = GetWorld(0);
    Node* root = world ? world->GetRootNode() : nullptr;
    Scene* scene = root ? root->GetScene() : nullptr;

    if (scene == nullptr)
    {
        LogWarning("Clear occlusion data: no scene loaded.");
        return;
    }

    scene->ClearOcclusionData();
    scene->SetDirtyFlag();
    world->UpdateRenderSettings();
    LogDebug("Occlusion data cleared for scene %s.", scene->GetName().c_str());
}

#endif
