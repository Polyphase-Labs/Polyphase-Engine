#include "Nodes/3D/SpotLight3d.h"
#include "Renderer.h"
#include "Assets/StaticMesh.h"
#include "Engine.h"
#include "AssetManager.h"
#include "Maths.h"

#if EDITOR
#include "EditorState.h"
#endif

#undef min
#undef max

FORCE_LINK_DEF(SpotLight3D);
DEFINE_NODE(SpotLight3D, PointLight3D);

SpotLight3D::SpotLight3D()
{
    mName = "Spot Light";
}

SpotLight3D::~SpotLight3D()
{
}

const char* SpotLight3D::GetTypeName() const
{
    return "SpotLight";
}

void SpotLight3D::GatherProperties(std::vector<Property>& outProps)
{
    PointLight3D::GatherProperties(outProps);

    SCOPED_CATEGORY("Light");

    outProps.push_back(Property(DatumType::Float, "Inner Angle", this, &mInnerAngle));
    outProps.push_back(Property(DatumType::Float, "Outer Angle", this, &mOuterAngle));
}

void SpotLight3D::GatherProxyDraws(std::vector<DebugDraw>& inoutDraws)
{
#if DEBUG_DRAW_ENABLED

    glm::vec4 color = glm::vec4(0.8f, 0.8f, 0.3f, 1.0f);

    if (mDomain == LightingDomain::Static)
    {
        color = glm::vec4(0.8f, 0.5f, 0.3f, 1.0f);
    }
    else if (mDomain == LightingDomain::Dynamic)
    {
        color = glm::vec4(0.8f, 0.8f, 0.6f, 1.0f);
    }

    {
        DebugDraw debugDraw;
        debugDraw.mMesh = LoadAsset<StaticMesh>("SM_Sphere");
        debugDraw.mNode = this;
        debugDraw.mColor = color;
        debugDraw.mTransform = glm::scale(mTransform, { 0.2f, 0.2f, 0.2f });
        inoutDraws.push_back(debugDraw);
    }

    {
        // Pointer Cone
        DebugDraw debugDraw;
        debugDraw.mMesh = LoadAsset<StaticMesh>("SM_Cone");
        debugDraw.mNode = this;
        debugDraw.mColor = color;
        float scale = 0.3f;
        glm::mat4 trans = MakeTransform({ 0.0f, 0.0f, -2.0f * scale }, { -90.0f, 0.0f, 0.0f }, { scale, scale, scale });
        debugDraw.mTransform = mTransform * trans;
        inoutDraws.push_back(debugDraw);
    }

#if EDITOR
    if (GetEditorState()->GetSelectedNode() == this)
    {
        // Preview cone covering the lit region (apex at the light, base at mRadius forward).
        DebugDraw debugDraw;
        debugDraw.mMesh = LoadAsset<StaticMesh>("SM_Cone");
        debugDraw.mNode = this;
        debugDraw.mColor = color;
        float outerRadians = glm::radians(glm::clamp(mOuterAngle, 1.0f, 89.0f));
        float baseRadius = mRadius * tanf(outerRadians);
        float halfLength = mRadius * 0.5f;
        glm::mat4 trans = MakeTransform({ 0.0f, 0.0f, -halfLength }, { 90.0f, 0.0f, 0.0f }, { baseRadius, halfLength, baseRadius });
        debugDraw.mTransform = mTransform * trans;
        inoutDraws.push_back(debugDraw);
    }
#endif // EDITOR

#endif // DEBUG_DRAW_ENABLED
}

bool SpotLight3D::IsSpotLight3D() const
{
    return true;
}

void SpotLight3D::SetInnerAngle(float angle)
{
    mInnerAngle = glm::clamp(angle, 0.0f, 89.9f);
}

float SpotLight3D::GetInnerAngle() const
{
    return mInnerAngle;
}

void SpotLight3D::SetOuterAngle(float angle)
{
    mOuterAngle = glm::clamp(angle, 0.1f, 89.9f);
}

float SpotLight3D::GetOuterAngle() const
{
    return mOuterAngle;
}

glm::vec3 SpotLight3D::GetDirection()
{
    return GetForwardVector();
}

void SpotLight3D::SetDirection(const glm::vec3& dir)
{
    LookAt(GetWorldPosition() + dir, { 0.0f, 1.0f, 0.0f });
}
