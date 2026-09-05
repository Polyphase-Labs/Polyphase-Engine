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
    Primitive3D* mNode = nullptr;
    AABB mBox;
    std::vector<glm::vec3> mTargets;
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
            // Interior points only; corners and face centers are added separately.
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

void GatherOccludeeTargets(Primitive3D* prim, const AABB& box, const BakeSettings& settings, float cellSize, std::vector<glm::vec3>& outTargets)
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
    // every face that is bigger than a couple of cells. Capped per axis so a
    // huge wall stays affordable.
    {
        const float spacing = 2.0f * cellSize;
        const uint32_t kMaxPerAxis = settings.mMeshVertexTargets >= 24 ? 8u : 6u;
        glm::vec3 size = box.GetSize();

        uint32_t counts[3];
        bool anyLarge = false;
        for (int32_t axis = 0; axis < 3; ++axis)
        {
            counts[axis] = glm::clamp((uint32_t)ceilf(size[axis] / spacing), 1u, kMaxPerAxis);
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
    glm::vec3 insetE = e * 0.9f;

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

    auto startTime = std::chrono::steady_clock::now();
    EditorProgress::Begin("Baking Occlusion Culling", "Gathering geometry...", true);

    // Gather occluder triangles and occludees. Hidden subtrees are skipped,
    // matching what the renderer draws.
    std::vector<BakeTri> tris;
    std::vector<Occludee> occludees;
    std::vector<OcclusionEntry> occluderEntries;
    std::vector<OcclusionArea3D*> areas;

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
            occludee.mNode = prim;
            occludee.mBox = box;
            GatherOccludeeTargets(prim, box, settings, cellSize, occludee.mTargets);
            occludees.push_back(std::move(occludee));
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
    uint32_t dimX = glm::max(1u, (uint32_t)ceilf(volumeSize.x / cellSize));
    uint32_t dimY = glm::max(1u, (uint32_t)ceilf(volumeSize.y / cellSize));
    uint32_t dimZ = glm::max(1u, (uint32_t)ceilf(volumeSize.z / cellSize));
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

    const float rayEps = 1e-3f * cellSize;
    const float solidProbeDist = 4.0f * cellSize;

    std::vector<uint32_t> cellWords((size_t)numCells * wordsPerSet, 0u);
    std::vector<uint8_t> cellAllVisible(numCells, 0);

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
                doneCells.fetch_add(1);
                continue;
            }

            uint32_t* words = &cellWords[(size_t)cell * wordsPerSet];
            uint32_t visibleCount = 0;

            for (uint32_t o = 0; o < numOccludees; ++o)
            {
                if (cancel.load())
                    break;

                bool visible = nearBoxes[o].Intersects(cellBox);

                if (!visible)
                {
                    const std::vector<glm::vec3>& targets = occludees[o].mTargets;
                    for (size_t si = 0; si < validSamples.size() && !visible; ++si)
                    {
                        const glm::vec3& s = validSamples[si];
                        for (size_t ti = 0; ti < targets.size(); ++ti)
                        {
                            glm::vec3 delta = targets[ti] - s;
                            float dist = glm::length(delta);
                            if (dist <= rayEps)
                            {
                                visible = true;
                                break;
                            }

                            glm::vec3 dir = delta / dist;
                            if (!bvh.AnyHit(s, dir, rayEps, dist - rayEps))
                            {
                                visible = true;
                                break;
                            }
                        }
                    }
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

    char statusBuf[128];
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

        snprintf(statusBuf, sizeof(statusBuf), "Cell %u / %u (%u occludees, %u triangles)", done, numCells, numOccludees, numTris);
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

    // Deduplicate identical bitsets so large grids stay small.
    OcclusionData data;
    data.mMin = volume.mMin;
    data.mCellSize = cellSize;
    data.mDimX = dimX;
    data.mDimY = dimY;
    data.mDimZ = dimZ;
    data.mWordsPerSet = wordsPerSet;
    data.mCellToSet.assign(numCells, OcclusionData::kNoSet);

    std::unordered_map<uint64_t, std::vector<uint32_t>> setLookup;
    for (uint32_t cell = 0; cell < numCells; ++cell)
    {
        if (cellAllVisible[cell])
            continue;

        const uint32_t* words = &cellWords[(size_t)cell * wordsPerSet];
        uint64_t hash = HashWords(words, wordsPerSet);
        std::vector<uint32_t>& candidates = setLookup[hash];

        uint32_t setIndex = OcclusionData::kNoSet;
        for (uint32_t candidate : candidates)
        {
            if (memcmp(&data.mSetPool[(size_t)candidate * wordsPerSet], words, wordsPerSet * sizeof(uint32_t)) == 0)
            {
                setIndex = candidate;
                break;
            }
        }

        if (setIndex == OcclusionData::kNoSet)
        {
            setIndex = (uint32_t)(data.mSetPool.size() / wordsPerSet);
            data.mSetPool.insert(data.mSetPool.end(), words, words + wordsPerSet);
            candidates.push_back(setIndex);
        }

        data.mCellToSet[cell] = setIndex;
    }

    data.mOccludees.resize(numOccludees);
    for (uint32_t i = 0; i < numOccludees; ++i)
    {
        data.mOccludees[i].mCenter = occludees[i].mBox.GetCenter();
        data.mOccludees[i].mExtents = occludees[i].mBox.GetExtents();
    }
    data.mOccluders = occluderEntries;

    uint32_t uniqueSets = data.GetNumUniqueSets();
    size_t bytes = data.GetMemoryBytes();

    scene->SetOcclusionData(std::move(data));
    scene->SetDirtyFlag();
    world->UpdateRenderSettings();

    EditorProgress::End();

    float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    LogDebug("Occlusion bake complete: %u occludees, %u occluders (%u tris), %u x %u x %u cells (%.1f m), %u unique sets, %u KB, %.1f s",
        numOccludees, (uint32_t)occluderEntries.size(), numTris, dimX, dimY, dimZ, cellSize, uniqueSets, (uint32_t)(bytes / 1024), elapsed);
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
