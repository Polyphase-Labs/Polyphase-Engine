#pragma once

#include "PointLight3d.h"

class POLYPHASE_API SpotLight3D : public PointLight3D
{
public:

    DECLARE_NODE(SpotLight3D, PointLight3D);

    SpotLight3D();
    ~SpotLight3D();

    virtual const char* GetTypeName() const override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual void GatherProxyDraws(std::vector<DebugDraw>& inoutDraws) override;

    virtual bool IsSpotLight3D() const override;

    void SetInnerAngle(float angle);
    float GetInnerAngle() const;

    void SetOuterAngle(float angle);
    float GetOuterAngle() const;

    glm::vec3 GetDirection();
    void SetDirection(const glm::vec3& dir);

protected:

    float mInnerAngle = 30.0f;
    float mOuterAngle = 45.0f;
};
