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
    std::string mMaterialPath;   // existing project Material to use as override (name or path)
    bool mMainCamera = false;
    // Per-node overrides. -1 = unset (use SceneImportOptions default / leave as-is).
    int mCollision = -1;   // 0 = no collision, 1 = collision
    int mHidden    = -1;   // 0 = visible,     1 = hidden
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
    // Share materials/textures across scene imports instead of namespacing them
    // per-scene. When true, M_/T_ assets get a scene-independent name (dropping
    // the per-scene stem) and land in a single shared folder; an existing asset
    // of the same name is reused instead of duplicated. Ideal for tiled/streamed
    // worlds where many sector scenes reference the same ground material/texture.
    bool mShareAssets = false;
    std::string mSharedAssetDir = "_Shared";  // folder (sibling of scene folders)
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
    MeshImportMode  mMode = MeshImportMode::AsMultipleObjects;
    bool            mPlaceInSubdirectory = true;
    bool            mImportMaterials = true;
    bool            mImportTextures = true;
    std::string     mPrefix;
    ShadingModel    mDefaultShadingModel = ShadingModel::Lit;
    VertexColorMode mDefaultVertexColorMode = VertexColorMode::None;
};

struct CameraImportOptions
{
    std::string mFilePath;
    std::string mCameraName;
    bool mIsMainCamera = false;
    bool mOverrideCameraName = false;
};

#endif
