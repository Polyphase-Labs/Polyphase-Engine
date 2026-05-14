#include "Nodes/3D/Skybox3D.h"

#include "Assets/StaticMesh.h"
#include "Assets/Material.h"
#include "AssetManager.h"

FORCE_LINK_DEF(Skybox3D);
DEFINE_NODE(Skybox3D, StaticMesh3D);

Skybox3D::Skybox3D()
{
    mName = "Skybox";
}

Skybox3D::~Skybox3D()
{

}

const char* Skybox3D::GetTypeName() const
{
    return "Skybox";
}

void Skybox3D::GatherProperties(std::vector<Property>& outProps)
{
    StaticMesh3D::GatherProperties(outProps);

    SCOPED_CATEGORY("Skybox");

    outProps.push_back(Property(DatumType::Bool, "Use Env Map", this, &mUseEnvMap));
}

void Skybox3D::Create()
{
    StaticMesh3D::Create();

    // Install skybox defaults so a freshly-spawned Skybox3D renders correctly without
    // any external configuration. LoadStream runs after Create() and will overwrite
    // these with the saved state when this node is loaded from a scene asset.
    StaticMesh* sphere = LoadAsset<StaticMesh>("SM_Sphere");
    if (sphere != nullptr)
    {
        SetStaticMesh(sphere);
    }

    Material* skyMat = LoadAsset<Material>("M_SkyboxDay");
    if (skyMat != nullptr)
    {
        SetMaterialOverride(skyMat);
    }

    EnableCollision(false);
    EnableOverlaps(false);
    EnablePhysics(false);
    EnableCastShadows(false);
    EnableReceiveShadows(false);
    EnableReceiveSimpleShadows(false);
    SetScale(glm::vec3(500.0f));
    SetTargetScreen(0xFF);
}

void Skybox3D::SaveStream(Stream& stream, Platform platform)
{
    StaticMesh3D::SaveStream(stream, platform);

    stream.WriteBool(mUseEnvMap);
}

void Skybox3D::LoadStream(Stream& stream, Platform platform, uint32_t version)
{
    StaticMesh3D::LoadStream(stream, platform, version);

    mUseEnvMap = stream.ReadBool();
}
