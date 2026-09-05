#include "Nodes/3D/OcclusionArea3d.h"
#include "Gizmos.h"

FORCE_LINK_DEF(OcclusionArea3D);
DEFINE_NODE(OcclusionArea3D, Node3D);

OcclusionArea3D::OcclusionArea3D()
{
    mName = "Occlusion Area";
}

OcclusionArea3D::~OcclusionArea3D()
{
}

const char* OcclusionArea3D::GetTypeName() const
{
    return "OcclusionArea3D";
}

void OcclusionArea3D::GatherProperties(std::vector<Property>& outProps)
{
    Node3D::GatherProperties(outProps);

    SCOPED_CATEGORY("Occlusion Area");
    outProps.push_back(Property(DatumType::Vector, "Extents", this, &mExtents));
}

glm::vec3 OcclusionArea3D::GetExtents() const
{
    return mExtents;
}

void OcclusionArea3D::SetExtents(glm::vec3 extents)
{
    mExtents = extents;
}

AABB OcclusionArea3D::GetWorldAABB()
{
    AABB local(-mExtents * 0.5f, mExtents * 0.5f);
    return local.Transform(GetTransform());
}

#if EDITOR
void OcclusionArea3D::OnDrawGizmos()
{
    Gizmos::SetMatrix(GetTransform());
    Gizmos::SetColor({ 0.2f, 0.8f, 1.0f, 1.0f });
    Gizmos::DrawWireCube(glm::vec3(0.0f), mExtents);
}
#endif
