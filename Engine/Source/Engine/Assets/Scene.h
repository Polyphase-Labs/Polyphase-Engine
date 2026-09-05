#pragma once

#include "Asset.h"
#include "Property.h"
#include "Factory.h"
#include "AssetRef.h"
#include "NodePath.h"
#include "Nodes/Node.h"
#include "OcclusionData.h"

class World;

struct SubSceneOverride
{
    std::string mPath;
    std::vector<Property> mProperties;
    bool mOverrideColors = false;
    bool mBakedLighting = false;
    std::vector<uint32_t> mInstanceColors;
};

struct SceneNodeDef
{
    TypeId mType = INVALID_TYPE_ID;
    int32_t mParentIndex = -1;
    SceneRef mScene;
    std::string mName;
    std::vector<Property> mProperties;
    std::vector<uint8_t> mExtraData;
    std::vector<SubSceneOverride> mSubSceneOverrides;
    // Legacy bone attachment. Was int8_t, which silently corrupted any rig with
    // more than 127 bones (routine once fingers and twist bones are present).
    // Widened at ASSET_VERSION_BONE_SOCKETS. New content carries the attachment
    // in Node3D's "Attach Socket" string property instead; this is kept so old
    // scenes still load and so index-only attachments keep working.
    int32_t mParentBone = -1;
    bool mExposeVariable = false;
    uint64_t mPersistentUuid = 0;
};

class POLYPHASE_API Scene : public Asset
{
public:

    DECLARE_ASSET(Scene, Asset);

    Scene();
    ~Scene();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;

    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    void Capture(Node* root, Platform platform = Platform::Count);
    NodePtr Instantiate();

    template<typename T>
    SharedPtr<T> Instantiate()
    {
        return Cast<T>(Instantiate());
    }

    void ApplyRenderSettings(World* world);

    // The scene's own ambient colour (the value ApplyRenderSettings hands the
    // world when mSetAmbientLightColor is on). Returns whether it is set.
    bool GetAmbientLightColor(glm::vec4& outColor) const { outColor = mAmbientLightColor; return mSetAmbientLightColor; }

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    void AddNodeDef(Node* node, Platform platform, std::vector<Node*>& nodeList);
    int32_t FindNodeIndex(Node* node, const std::vector<Node*>& nodeList);

    bool CheckForNodeProps(std::vector<Property>& props);

    static int32_t sInstantiationCount;
    static std::vector<PendingNodePath> sPendingNodePaths;

    std::vector<SceneNodeDef> mNodeDefs;

    // World render properties (used when this scene is the world root).
    bool mSetAmbientLightColor = false;
    bool mSetShadowColor = false;
    bool mSetFog = false;

    glm::vec4 mAmbientLightColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    glm::vec4 mShadowColor = { 0.0f, 0.0f, 0.0f, 0.8f };

    bool mFogEnabled = false;
    glm::vec4 mFogColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    FogDensityFunc mFogDensityFunc = FogDensityFunc::Linear;
    float mFogNear = 0.0f;
    float mFogFar = 100.0f;

    uint8_t mIconOverride = 0;  // 0 = default, 1+ = icon index
    std::string mMenuOverride;  // Empty = "Scene" menu, or category like "3D", "UI", "Gameplay"

    // Baked occlusion culling (see OcclusionData / OcclusionBaker).
    bool mOcclusionCullingEnabled = false;
    float mOcclusionCellSize = 4.0f;
    uint8_t mOcclusionBakeQuality = 1; // 0 = Low, 1 = Medium, 2 = High
    bool mOcclusionDynamic = true;              // also bake the coarse table for moving occludees
    int32_t mOcclusionConsoleBudgetKB = 512;    // strip the data from console cooks above this (0 = unlimited)
    OcclusionData mOcclusionData;

public:
    uint8_t GetIconOverride() const { return mIconOverride; }
    const std::string& GetMenuOverride() const { return mMenuOverride; }
    const std::vector<Property>* GetRootNodeProperties() const { return mNodeDefs.empty() ? nullptr : &mNodeDefs[0].mProperties; }

    bool IsOcclusionCullingEnabled() const { return mOcclusionCullingEnabled; }
    float GetOcclusionCellSize() const { return mOcclusionCellSize; }
    uint8_t GetOcclusionBakeQuality() const { return mOcclusionBakeQuality; }
    bool IsOcclusionDynamicEnabled() const { return mOcclusionDynamic; }
    uint32_t GetOcclusionConsoleBudgetKB() const { return mOcclusionConsoleBudgetKB > 0 ? (uint32_t)mOcclusionConsoleBudgetKB : 0u; }
    const OcclusionData& GetOcclusionData() const { return mOcclusionData; }
    void SetOcclusionData(OcclusionData&& data) { mOcclusionData = std::move(data); }
    void ClearOcclusionData() { mOcclusionData.Clear(); }
};
