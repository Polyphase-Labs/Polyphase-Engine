#if EDITOR

#include "TriangleBvh.h"

#include "Maths.h"
#include "Vertex.h"
#include "Assets/StaticMesh.h"
#include "Nodes/3D/Primitive3d.h"
#include "Nodes/3D/StaticMesh3d.h"
#include "Nodes/3D/InstancedMesh3d.h"
#include "Nodes/3D/Terrain3d.h"
#include "Nodes/3D/Voxel3d.h"

#include <algorithm>

static const uint32_t kMaxTrisPerLeaf = 4;

static glm::vec3 InvertRayDir(const glm::vec3& dir)
{
    glm::vec3 invDir;
    for (int32_t i = 0; i < 3; ++i)
    {
        invDir[i] = (fabsf(dir[i]) > 1e-12f) ? (1.0f / dir[i]) : ((dir[i] < 0.0f) ? -1e30f : 1e30f);
    }
    return invDir;
}

void TriangleBvh::Build(std::vector<BvhTri>&& tris)
{
    mTris = std::move(tris);
    mNodes.clear();
    if (mTris.empty())
        return;

    std::vector<glm::vec3> centroids(mTris.size());
    std::vector<AABB> boxes(mTris.size());
    for (uint32_t i = 0; i < mTris.size(); ++i)
    {
        const BvhTri& t = mTris[i];
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

void TriangleBvh::Clear()
{
    mTris.clear();
    mTris.shrink_to_fit();
    mNodes.clear();
    mNodes.shrink_to_fit();
}

bool TriangleBvh::AnyHit(const glm::vec3& origin, const glm::vec3& dir, float tMin, float tMax) const
{
    if (mNodes.empty())
        return false;

    glm::vec3 invDir = InvertRayDir(dir);

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
                const BvhTri& t = mTris[node.mFirst + i];
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

bool TriangleBvh::ClosestHit(const glm::vec3& origin, const glm::vec3& dir, float tMax, float& outT, uint32_t& outTri) const
{
    if (mNodes.empty())
        return false;

    glm::vec3 invDir = InvertRayDir(dir);

    float bestT = tMax;
    bool hit = false;

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
                const BvhTri& t = mTris[node.mFirst + i];
                float hitT = 0.0f;
                if (Maths::RayIntersectsTriangle(origin, dir, t.mV0, t.mE1, t.mE2, hitT) && hitT < bestT)
                {
                    bestT = hitT;
                    outTri = node.mFirst + i;
                    hit = true;
                }
            }
        }
        else if (stackSize + 2 <= 64)
        {
            stack[stackSize++] = node.mRight;
            stack[stackSize++] = (uint32_t)(&node - mNodes.data()) + 1;
        }
    }

    if (hit)
    {
        outT = bestT;
    }

    return hit;
}

bool TriangleBvh::ClosestHitIsBackFacing(const glm::vec3& origin, const glm::vec3& dir, float tMax, bool& outHit) const
{
    float hitT = 0.0f;
    uint32_t hitTri = 0;
    outHit = ClosestHit(origin, dir, tMax, hitT, hitTri);
    if (!outHit)
        return false;

    const BvhTri& t = mTris[hitTri];
    return glm::dot(glm::cross(t.mE1, t.mE2), dir) > 0.0f;
}

uint32_t TriangleBvh::BuildRecursive(uint32_t first, uint32_t count, std::vector<glm::vec3>& centroids, std::vector<AABB>& boxes)
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

        std::vector<BvhTri> tmpTris(count);
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

void AppendTriangles(std::vector<BvhTri>& outTris, const glm::mat4& transform, const glm::vec3* positions, size_t positionStride, uint32_t numVerts, const IndexType* indices, uint32_t numIndices)
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

        BvhTri tri;
        tri.mV0 = worldVerts[ia];
        tri.mE1 = worldVerts[ib] - worldVerts[ia];
        tri.mE2 = worldVerts[ic] - worldVerts[ia];

        glm::vec3 n = glm::cross(tri.mE1, tri.mE2);
        if (glm::dot(n, n) <= 1e-12f)
            continue;

        outTris.push_back(tri);
    }
}

void GatherPrimitiveTriangles(Primitive3D* prim, std::vector<BvhTri>& outTris)
{
    if (StaticMesh3D* meshNode = prim->As<StaticMesh3D>())
    {
        StaticMesh* mesh = meshNode->GetStaticMesh();
        if (mesh == nullptr || mesh->GetNumVertices() == 0)
            return;

        // GetVertices() asserts on vertex-color meshes, so pick the accessor first.
        const bool hasColor = mesh->HasVertexColor();
        const VertexColor* colorVerts = hasColor ? mesh->GetColorVertices() : nullptr;
        const Vertex* verts = hasColor ? nullptr : mesh->GetVertices();
        const glm::vec3* positions = colorVerts ? &colorVerts[0].mPosition : (verts ? &verts[0].mPosition : nullptr);
        size_t stride = hasColor ? sizeof(VertexColor) : sizeof(Vertex);

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

#endif
