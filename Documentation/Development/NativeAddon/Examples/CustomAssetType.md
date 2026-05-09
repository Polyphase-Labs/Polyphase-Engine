# Custom Asset Type

Add a new asset class from a native addon — registered with the engine factory, importable from the editor, version-gated for serialization, and exposed in the asset browser. The VideoPlayer addon's `VideoClip` asset is a fully-worked production example.

## What this gives you

- A new asset class that lives alongside built-in assets like `Texture`, `StaticMesh`, `SoundWave`.
- Editor-side import: drop a file with your registered extension into the asset browser and the editor calls your `Import()`.
- Inspector properties via `GatherProperties`.
- Round-tripped serialization (`SaveStream` / `LoadStream`) with version gates so older saves keep loading.

## Files to add

```
Packages/com.example.myaddon/
    Source/
        Assets/
            MyAsset.h
            MyAsset.cpp
        MyAddon.cpp     (existing entry point)
```

## Asset header

```cpp
// Source/Assets/MyAsset.h
#pragma once

#include "Asset.h"

class MyAsset : public Asset
{
public:

    DECLARE_ASSET(MyAsset, Asset);

    MyAsset();
    virtual ~MyAsset();

    virtual void Create() override;
    virtual void Destroy() override;

    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

#if EDITOR
    virtual void Import(const std::string& path, ImportOptions* options) override;
#endif

    float GetSpeed() const  { return mSpeed; }
    void  SetSpeed(float v) { mSpeed = v;   }

protected:

    static const uint32_t kVersionInitial   = 1;
    static const uint32_t kVersionWithColor = 2;
    static const uint32_t kVersionCurrent   = kVersionWithColor;

    float       mSpeed   = 1.0f;
    glm::vec4   mColor   = glm::vec4(1.0f);
    std::vector<uint8_t> mSourceData;   // raw imported bytes (cooked formats live in subclasses)
};
```

`DECLARE_ASSET(MyAsset, Asset)` registers the class with the engine's asset factory at static-init time when the addon DLL loads. You do **not** need to touch any engine factory files.

## Asset implementation

```cpp
// Source/Assets/MyAsset.cpp
#include "Assets/MyAsset.h"

#include "Stream.h"
#include "Property.h"

FORCE_LINK_DEF(MyAsset);
DEFINE_ASSET(MyAsset);

MyAsset::MyAsset() {}
MyAsset::~MyAsset() {}

void MyAsset::Create()  { Asset::Create();  }
void MyAsset::Destroy() { Asset::Destroy(); }

void MyAsset::SaveStream(Stream& stream, Platform platform)
{
    Asset::SaveStream(stream, platform);

    stream.WriteUint32(kVersionCurrent);    // version your own format

    stream.WriteFloat(mSpeed);
    stream.WriteVec4(mColor);

    stream.WriteUint32((uint32_t)mSourceData.size());
    if (!mSourceData.empty())
    {
        stream.WriteBytes(mSourceData.data(), (uint32_t)mSourceData.size());
    }
}

void MyAsset::LoadStream(Stream& stream, Platform platform)
{
    Asset::LoadStream(stream, platform);

    uint32_t version = stream.ReadUint32();

    mSpeed = stream.ReadFloat();

    if (version >= kVersionWithColor)
    {
        mColor = stream.ReadVec4();
    }

    uint32_t numBytes = stream.ReadUint32();
    mSourceData.resize(numBytes);
    if (numBytes > 0)
    {
        stream.ReadBytes(mSourceData.data(), numBytes);
    }
}

void MyAsset::GatherProperties(std::vector<Property>& outProps)
{
    Asset::GatherProperties(outProps);
    outProps.push_back(Property("Speed", &mSpeed, PropertyType::Float));
    outProps.push_back(Property("Color", &mColor, PropertyType::Color));
}

#if EDITOR
void MyAsset::Import(const std::string& path, ImportOptions* options)
{
    // Read the source file into mSourceData. Real cookers parse the format
    // here, populate fields, and either store cooked bytes inline or write a
    // sidecar file (see VideoClip's THP/N3MV pipeline for the sidecar pattern).
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    size_t size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    mSourceData.resize(size);
    if (size > 0) fread(mSourceData.data(), 1, size, fp);
    fclose(fp);

    SetDirtyFlag();   // editor-only — see "Save dirty state" gotcha below
}
#endif
```

`FORCE_LINK_DEF(MyAsset)` plus `FORCE_LINK_CALL(MyAsset)` in the addon's `OnLoad` keep the linker from dropping the translation unit when no other addon code references it.

## Wire the addon entry point

In your existing `MyAddon.cpp`:

```cpp
#include "Plugins/PolyphasePluginAPI.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "Assets/MyAsset.h"

#if EDITOR
#include "AssetManager.h"   // RegisterImportExtension lives here
#endif

static int OnLoad(PolyphaseEngineAPI* api)
{
    FORCE_LINK_CALL(MyAsset);

#if EDITOR
    TypeId t = MyAsset::GetStaticType();
    RegisterImportExtension(".myext", t);
    RegisterImportExtension(".myx",   t);
#endif

    return 0;
}
```

`RegisterImportExtension` teaches the editor's import dispatcher to instantiate `MyAsset` when a file with the matching extension is imported via the asset browser, drag-drop, or **Reimport**. It's editor-only.

## Save dirty state

If your asset is mutated programmatically (e.g. from a custom editor action), call `SetDirtyFlag()` on the asset so the editor's unsaved-changes flow picks it up. `SetDirtyFlag` only exists in `#if EDITOR` builds, so wrap call sites accordingly.

## Verification

1. Place a `foo.myext` file inside the project's `Assets/` directory.
2. In the editor, the asset browser should pick it up automatically and show a `MyAsset` instance.
3. Open the inspector — `Speed` and `Color` should be editable.
4. Edit the values, save the project, restart the editor — the values should round-trip.
5. To test version migration: bump `kVersionCurrent` to a new constant, add a new field gated behind it, and confirm the asset still loads from a save written before the bump.

## See also

- `Engine/Source/Engine/Asset.h` — base class, `ASSET_VERSION_CURRENT`, `Stream` API.
- VideoPlayer's `Source/Assets/VideoClip.cpp` — production-grade asset with `Import()`, sidecar cook output, and inspector cook-time knobs.
- [External Library Integration](ExternalLibrary.md) — pair this with FFmpeg-style import if your asset wraps third-party data.
