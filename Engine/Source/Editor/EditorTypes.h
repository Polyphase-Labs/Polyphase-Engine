#pragma once

#if EDITOR

#include "EngineTypes.h"

struct AssetStub;

enum class PolyphaseMeshType { Node3D, StaticMesh, InstancedMesh };

struct PolyphaseNodeExtras
{
    PolyphaseMeshType mMeshType = PolyphaseMeshType::StaticMesh;
    std::string mAssetName;
    uint64_t mAssetUuid = 0;
    std::string mScriptPath;
    std::string mScriptPropsJson;
    std::string mScriptPropsTypesJson;
    std::string mMaterialType;
    bool mMainCamera = false;
};

struct SceneImportOptions
{
    std::string mFilePath;
    std::string mSceneName;
    std::string mPrefix;
    bool mImportMeshes = true;
    bool mImportMaterials = true;
    bool mImportTextures = true;
    bool mImportLights = true;
    bool mImportCameras = true;
    bool mEnableCollision = true;
    bool mApplyGltfExtras = true;
    ShadingModel mDefaultShadingModel = ShadingModel::Lit;
    VertexColorMode mDefaultVertexColorMode = VertexColorMode::None;
    bool mReimportTextures = false;
    AssetStub* mReimportSceneStub = nullptr;  // Non-null = reimport mode
};

enum class MeshImportMode : uint8_t
{
    AsScene = 0,
    AsMultipleObjects = 1,
    AsSingleObject = 2,
    Count
};

struct MeshImportOptions
{
    MeshImportMode mMode = MeshImportMode::AsMultipleObjects;
};

struct CameraImportOptions
{
    std::string mFilePath;
    std::string mCameraName;
    bool mIsMainCamera = false;
    bool mOverrideCameraName = false;
};

#endif
