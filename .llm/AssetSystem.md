# Asset System

## Overview

Assets are serializable game data (textures, meshes, materials, scenes, etc.). The system provides versioned binary serialization, UUID-based identification, async loading, reference counting, and a factory-based type registry.

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Engine/Asset.h/.cpp` | Base asset class |
| `Engine/Source/Engine/AssetManager.h/.cpp` | Registry, loading, discovery |
| `Engine/Source/Engine/AssetRef.h` | Reference wrapper (typedef aliases) |
| `Engine/Source/Engine/Stream.h/.cpp` | Binary serialization |
| `Engine/Source/Engine/Assets/*.h/.cpp` | Asset type implementations |

## Asset Base Class

```cpp
class Asset : public Object {
    DECLARE_ASSET(Asset, Object);

    // Serialization
    virtual void LoadStream(Stream& stream, Platform platform);
    virtual void SaveStream(Stream& stream, Platform platform);
    virtual bool Import(const std::string& path, ImportOptions* options = nullptr);

    // Lifecycle
    virtual void Create();
    virtual void Destroy();
    virtual void Copy(Asset* srcAsset);

    // File I/O
    void LoadFile(const char* path, AsyncLoadRequest* request = nullptr);
    void SaveFile(const char* path, Platform platform);

    // Identity
    const std::string& GetName() const;
    uint64_t GetUuid() const;
    void EnsureUuid();       // Generate if not assigned
    bool IsLoaded() const;

    // Editor
    void SetDirtyFlag();     // Mark as modified (EDITOR only)
    bool GetDirtyFlag();

    // Properties
    virtual void GatherProperties(std::vector<Property>& outProps);
};
```

Macros: `DECLARE_ASSET(Class, Parent)` / `DEFINE_ASSET(Class)`.

## Asset Types

All in `Engine/Source/Engine/Assets/`:

| Type | File | Description |
|------|------|-------------|
| `Texture` | `Texture.h/.cpp` | Images with mipmaps, formats, filter/wrap modes |
| `StaticMesh` | `StaticMesh.h/.cpp` | Static geometry with baked lighting support |
| `SkeletalMesh` | `SkeletalMesh.h/.cpp` | Skinned mesh with skeleton/bones, multi-section material slots |
| `SkeletalAnimationAsset` | `SkeletalAnimationAsset.h/.cpp` | Standalone bone-animation clip; bone-name keyed; supports tier-1 and tier-2 retarget bakes |
| `HumanoidAvatarAsset` | `HumanoidAvatarAsset.h/.cpp` | Mecanim-style humanoid bone slot mapping; pair source + target for retargeting |
| `Material` | `Material.h/.cpp` | Full material with shader parameters |
| `MaterialBase` | `MaterialBase.h/.cpp` | Abstract material base |
| `MaterialLite` | `MaterialLite.h/.cpp` | Lightweight runtime material |
| `MaterialInstance` | `MaterialInstance.h/.cpp` | Material with overrides from a base |
| `ParticleSystem` | `ParticleSystem.h/.cpp` | Particle emitter configuration |
| `Font` | `Font.h/.cpp` | TTF/OTF fonts with glyph atlases |
| `SoundWave` | `SoundWave.h/.cpp` | Audio data (Vorbis compressed) |
| `Scene` | `Scene.h/.cpp` | Serialized node hierarchy |
| `Timeline` | `Timeline.h/.cpp` | Keyframe animation sequences |
| `TransformAnimationAsset` | `TransformAnimationAsset.h/.cpp` | Non-bone transform keyframe clip |
| `NodeGraphAsset` | `NodeGraphAsset.h/.cpp` | Visual scripting graphs |
| `DataAsset` | `DataAsset.h/.cpp` | Lua-defined data containers (ScriptableObject equivalent) |

## AssetManager

**Singleton:** `AssetManager::Get()`

Key operations:

```cpp
// Discovery
void Initialize();
void Discover(directoryName, directoryPath);
void DiscoverEmbeddedAssets(assets, count);

// Loading (UUID-based, preferred)
Asset* LoadAssetByUuid(uint64_t uuid);
void AsyncLoadAssetByUuid(uint64_t uuid, AssetRef* targetRef);
AssetStub* GetAssetStubByUuid(uint64_t uuid);

// Loading (name-based, backward compatible)
Asset* LoadAsset(const std::string& name);
Asset* GetAsset(const std::string& name);
void AsyncLoadAsset(const std::string& name, AssetRef* targetRef);

// Loading (path-based)
Asset* LoadAssetByPath(const std::string& path);  // "Assets/Models/SM_Plane"

// Utilities
bool DoesAssetExist(name);
void SaveAsset(name);
bool RenameAsset(asset, newName);
void Purge(purgeEngineAssets);  // Unload unreferenced
void RefSweep();                // Garbage collect
```

Global helpers: `FetchAsset(name)`, `LoadAsset(name)`, `FetchAssetByUuid(uuid)`, `LoadAssetByUuid(uuid)`.

Template helpers: `FetchAsset<Texture>(name)`, `LoadAsset<Material>(name)`.

## AssetRef (Reference Pattern)

**File:** `Engine/Source/Engine/AssetRef.h`

Lightweight wrapper around `Asset*` with async load support:

```cpp
AssetRef ref;
ref = LoadAsset("MyTexture");
Texture* tex = ref.Get<Texture>();
```

Typedefs: `TextureRef`, `StaticMeshRef`, `MaterialRef`, `SkeletalMeshRef`, `SkeletalAnimationRef`, `HumanoidAvatarRef`, `ParticleSystemRef`, `SoundWaveRef`, `FontRef`, `SceneRef`, `TimelineRef`, `TransformAnimationRef`, `DataAssetRef`, `SpriteAnimationRef`.

## AssetStub

Metadata entry in the asset registry:
```cpp
struct AssetStub {
    Asset* mAsset;           // nullptr until loaded
    std::string mPath;       // File path
    TypeId mType;            // Asset class type
    uint64_t mUuid;          // Primary identifier (v12+)
    bool mEngineAsset;       // Built-in engine asset?
    // EDITOR: mName, mDirectory
};
```

## Stream (Serialization)

**File:** `Engine/Source/Engine/Stream.h/.cpp`

Binary stream with methods for all types:
- Primitives: `ReadInt32()`, `WriteFloat()`, `ReadBool()`, etc.
- Math: `ReadVec3()`, `WriteQuat()`, `ReadMatrix()`, etc.
- Strings: `ReadString()`, `WriteString()` (4-byte length prefix)
- Assets: `ReadAsset(AssetRef&)`, `WriteAsset(AssetRef&)` (UUID-based)
- File I/O: `ReadFile(path)`, `WriteFile(path)`
- Async: `SetAsyncRequest()`, `SetAssetVersion()`

### `Stream::ReadFile` is the universal content chokepoint

Every asset, script and registry read on **every** platform funnels through here,
including targets whose `SYS_AcquireFileData` lives in an out-of-tree build-target
addon. `ReadFile` therefore does two things before returning:

1. Consults `ContentPak` — a mounted `Content.pak` serves the entry; a miss falls
   through to loose/embedded files.
2. Unwraps a `ContentObfuscation` container if one is present (Static builds).
   Plain files miss the magic and pass through untouched, so obfuscated and clear
   content coexist and the editor keeps reading its own project tree.

**Put content-wide behaviour here, not in per-platform `SYS_AcquireFileData`** —
there are five in-repo backends plus every addon backend, and editing them all
defeats the point. The seekable streaming API (`SYS_FileOpenRead`/`Read`/`Seek`
in `System/SystemUtils.cpp`) is the matching chokepoint for chunked reads and is
likewise implemented once for all platforms.

Callers that bypass `Stream` (raw `fopen`, `luaL_dofile`) silently miss both
layers. See `Documentation/Development/StaticContent.md`.

## Asset Header

```cpp
struct AssetHeader {
    uint32_t mMagic = 0x4f435421;          // "OCT!"
    uint32_t mVersion = ASSET_VERSION_CURRENT;
    TypeId mType;
    uint8_t mEmbedded;
    uint64_t mUuid;                         // v12+
};
```

## Version History

| Version | Constant | Changes |
|---------|----------|---------|
| 1 | `ASSET_VERSION_BASE` | Initial format |
| 2 | `ASSET_VERSION_SCENE_EXTRA_DATA` | Scene extra data |
| 3 | `ASSET_VERSION_PARTICLE_RADIAL_SPAWN` | Particle radial spawning |
| 4 | `ASSET_VERSION_PROPERTY_EXTRA` | Extended property data |
| 5 | `ASSET_VERSION_SCENE_SUB_SCENE_OVERRIDE` | Sub-scene overrides |
| 6 | `ASSET_VERSION_FONT_TTF_FLAG` | TTF support flag |
| 7 | `ASSET_VERSION_TEXTURE_COOKED_PROPERTIES` | Texture cook properties |
| 8 | `ASSET_VERSION_TEXTURE_LOW_QUALITY` | Low-quality textures |
| 9 | `ASSET_VERSION_MATERIAL_LITE_TEXTURE_COUNT` | MaterialLite textures |
| 10 | `ASSET_VERSION_STATIC_MESH_3D_HAS_BAKED_LIGHTING` | Baked lighting |
| 11 | `ASSET_VERSION_SCENE_SUBSCENE_INSTANCE_COLORS` | Sub-scene colors |
| 12 | `ASSET_VERSION_UUID_SUPPORT` | UUID-based asset IDs |
| 13 | `ASSET_VERSION_UUID_WITH_NAME_FALLBACK` | UUID + name fallback |
| 14 | `ASSET_VERSION_NODE_PERSISTENT_UUID` | Persistent node UUIDs |
| 15 | `ASSET_VERSION_MATERIAL_LITE_NODE_GRAPH` | MaterialLite node graphs |
| 16 | `ASSET_VERSION_NODE_GRAPH_FUNCTIONS` | Named function subgraphs |
| 17 | `ASSET_VERSION_NODE_GRAPH_VARIABLES` | Local named variables, copy/paste/export |
| 18 | `ASSET_VERSION_SCENE_ICON_OVERRIDE` | Scene icon override |
| 19 | `ASSET_VERSION_SCENE_MENU_OVERRIDE` | Scene menu override |
| 20 | `ASSET_VERSION_VOXEL3D` | Voxel3D asset support |
| 21 | `ASSET_VERSION_VOXEL3D_ATLAS` | Voxel3D atlas |
| 22 | `ASSET_VERSION_VOXEL3D_ATLAS_INT32` | Voxel3D atlas widened |
| 23 | `ASSET_VERSION_TERRAIN3D` | Terrain3D asset |
| 24 | `ASSET_VERSION_TERRAIN3D_MATSLOTS` | Terrain3D material slots |
| 25 | `ASSET_VERSION_TERRAIN3D_ATLAS` | Terrain3D atlas |
| 26 | `ASSET_VERSION_TERRAIN3D_BAKE` | Terrain3D bake |
| 27 | `ASSET_VERSION_TERRAIN3D_BAKEDMAP` | Terrain3D baked map |
| 28 | `ASSET_VERSION_TILESET_BASE` | TileSet base |
| 29 | `ASSET_VERSION_TILEMAP_BASE` | TileMap base |
| 30 | `ASSET_VERSION_TILESET_METADATA` | TileSet metadata |
| 31 | `ASSET_VERSION_TILESET_AUTOTILE` | TileSet autotile |
| 32 | `ASSET_VERSION_TRANSFORM_KEYFRAME_SIGNAL` | TransformKeyframe signal |
| 33 | `ASSET_VERSION_INPUT_PROMPT_MAP` | InputPromptMap asset |
| 34 | `ASSET_VERSION_INPUT_PROMPT_STYLE` | InputPromptStyle asset |
| 35 | `ASSET_VERSION_MATERIAL_LITE_UV_SOURCE` | MaterialLite UV source |
| 36 | `ASSET_VERSION_SKELETAL_MESH_SECTIONS` | SkeletalMesh per-section material slots |

**Current:** `ASSET_VERSION_CURRENT = 36`

## Async Loading

Assets can be loaded asynchronously via `AsyncLoadAsset()`. The `AsyncLoadRequest` tracks the load state. `AssetRef::GetLoadRequest()` checks pending loads. Load states: `Unloaded → AwaitBegin → AwaitEnd → Loaded`.
