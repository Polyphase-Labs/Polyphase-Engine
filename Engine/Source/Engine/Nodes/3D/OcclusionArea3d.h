#pragma once

#include "Nodes/3D/Node3d.h"

// Bounds the volume that the occlusion baker divides into cells. Place one (or
// several) over the areas the camera can actually reach. When a scene has no
// OcclusionArea3D the baker falls back to the union of all occludee bounds.
class POLYPHASE_API OcclusionArea3D : public Node3D
{
public:

    DECLARE_NODE(OcclusionArea3D, Node3D);

    OcclusionArea3D();
    ~OcclusionArea3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    glm::vec3 GetExtents() const;
    void SetExtents(glm::vec3 extents);

    AABB GetWorldAABB();

#if EDITOR
    virtual void OnDrawGizmos() override;
#endif

protected:

    glm::vec3 mExtents = { 20.0f, 10.0f, 20.0f };
};
