#if EDITOR

#include "MemorySnapshotProfiler.h"

#include "Engine.h"
#include "World.h"
#include "Stream.h"
#include "Utilities.h"
#include "Log.h"

#include "Nodes/Node.h"
#include "Script.h"
#include "Property.h"
#include "Datum.h"

#include "Asset.h"
#include "Assets/Texture.h"
#include "Assets/StaticMesh.h"
#include "Assets/SkeletalMesh.h"
#include "Assets/SoundWave.h"
#include "Assets/Material.h"
#include "Assets/MaterialBase.h"
#include "Assets/MaterialLite.h"
#include "Assets/MaterialInstance.h"
#include "Assets/Font.h"
#include "Assets/ParticleSystem.h"
#include "Constants.h"

#include "Vertex.h"
#include "Graphics/GraphicsTypes.h"

#include "System/System.h"
#include "Audio/Audio.h"
#include "Audio/AudioConstants.h"
#include "GamePreview/GamePreview.h"

#include <ctime>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ------------------------------------------------------------------------------------------------
// MemorySnapshot member helpers + category names (data model lives in MemorySnapshot.h)
// ------------------------------------------------------------------------------------------------

const char* SnapshotCategoryName(SnapshotCategory cat)
{
    switch (cat)
    {
    case SnapshotCategory::Geometry:  return "Geometry";
    case SnapshotCategory::Textures:  return "Textures";
    case SnapshotCategory::Audio:     return "Audio";
    case SnapshotCategory::Scripts:   return "Scripts";
    case SnapshotCategory::Materials: return "Materials";
    case SnapshotCategory::Fonts:     return "Fonts";
    case SnapshotCategory::Particles: return "Particles";
    case SnapshotCategory::Animation: return "Animation";
    case SnapshotCategory::Nodes:     return "Nodes";
    case SnapshotCategory::Other:     return "Other";
    case SnapshotCategory::FrameBuffer: return "Frame Buffers";
    default:                          return "?";
    }
}

uint64_t MemorySnapshot::GetTotalCpuBytes() const
{
    uint64_t total = 0;
    for (const SnapshotEntry& e : mEntries)
        total += e.mCpuBytes;
    return total;
}

uint64_t MemorySnapshot::GetTotalGpuBytes() const
{
    uint64_t total = 0;
    for (const SnapshotEntry& e : mEntries)
        total += e.mGpuBytes;
    return total;
}

void MemorySnapshot::GetCategoryTotals(uint64_t outCpu[(int)SnapshotCategory::Count],
                                       uint64_t outGpu[(int)SnapshotCategory::Count]) const
{
    for (int32_t i = 0; i < (int)SnapshotCategory::Count; ++i)
    {
        outCpu[i] = 0;
        outGpu[i] = 0;
    }

    for (const SnapshotEntry& e : mEntries)
    {
        outCpu[(int)e.mCategory] += e.mCpuBytes;
        outGpu[(int)e.mCategory] += e.mGpuBytes;
    }
}

// ------------------------------------------------------------------------------------------------
// Per-type sizing
// ------------------------------------------------------------------------------------------------

static float BytesPerPixel(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::LA4:      return 1.0f;
    case PixelFormat::RGB565:   return 2.0f;
    case PixelFormat::RGBA8:    return 4.0f;
    case PixelFormat::CMPR:     return 0.5f;   // block-compressed, ~4 bits/pixel
    case PixelFormat::RGBA5551: return 2.0f;
    case PixelFormat::R8:       return 1.0f;
    case PixelFormat::R32U:     return 4.0f;
    case PixelFormat::R32F:     return 4.0f;
    case PixelFormat::RGBA16F:  return 8.0f;
    case PixelFormat::Depth24Stencil8:
    case PixelFormat::Depth32FStencil8:
    case PixelFormat::Depth32F: return 4.0f;
    case PixelFormat::Depth16:  return 2.0f;
    default:                    return 4.0f;
    }
}

static const char* PixelFormatName(PixelFormat format)
{
    switch (format)
    {
    case PixelFormat::LA4:      return "LA4";
    case PixelFormat::RGB565:   return "RGB565";
    case PixelFormat::RGBA8:    return "RGBA8";
    case PixelFormat::CMPR:     return "CMPR";
    case PixelFormat::RGBA5551: return "RGBA5551";
    case PixelFormat::R8:       return "R8";
    case PixelFormat::R32U:     return "R32U";
    case PixelFormat::R32F:     return "R32F";
    case PixelFormat::RGBA16F:  return "RGBA16F";
    default:                    return "Depth";
    }
}

static std::string FormatCount(uint32_t n)
{
    char buf[32];
    if (n >= 1000000)
        snprintf(buf, sizeof(buf), "%.1fM", n / 1000000.0);
    else if (n >= 1000)
        snprintf(buf, sizeof(buf), "%.1fk", n / 1000.0);
    else
        snprintf(buf, sizeof(buf), "%u", n);
    return buf;
}

// Appends one or more SnapshotEntry rows describing `asset`. SkeletalMesh emits
// a Geometry row plus an Animation row so keyframe cost is broken out separately.
static void ClassifyAndSize(Asset* asset, uint32_t refCount, std::vector<SnapshotEntry>& out)
{
    SnapshotEntry base;
    base.mName = asset->GetName();
    base.mTypeName = asset->GetTypeName();
    base.mRefCount = refCount;

    if (Texture* t = asset->As<Texture>())
    {
        uint32_t w = t->GetWidth();
        uint32_t h = t->GetHeight();
        uint32_t layers = t->GetLayers() > 0 ? t->GetLayers() : 1;
        double bytes = (double)w * (double)h * BytesPerPixel(t->GetFormat()) * (double)layers;
        if (t->IsMipmapped())
            bytes *= 1.3333;

        base.mCategory = SnapshotCategory::Textures;
        base.mGpuBytes = (uint64_t)bytes;
        base.mCpuBytes = (uint64_t)t->GetPixels().size();

        char detail[128];
        snprintf(detail, sizeof(detail), "%ux%u %s%s%s",
                 w, h, PixelFormatName(t->GetFormat()),
                 t->IsMipmapped() ? " +mips" : "",
                 layers > 1 ? " (array)" : "");
        base.mDetail = detail;
        out.push_back(base);
        return;
    }

    if (StaticMesh* sm = asset->As<StaticMesh>())
    {
        uint64_t bytes = (uint64_t)sm->GetNumVertices() * sm->GetVertexSize()
                       + (uint64_t)sm->GetNumIndices() * sizeof(IndexType);
        base.mCategory = SnapshotCategory::Geometry;
        base.mCpuBytes = bytes;
        base.mGpuBytes = bytes;
        base.mDetail = FormatCount(sm->GetNumVertices()) + " verts, "
                     + FormatCount(sm->GetNumFaces()) + " tris";
        out.push_back(base);
        return;
    }

    if (SkeletalMesh* sk = asset->As<SkeletalMesh>())
    {
        uint64_t geo = (uint64_t)sk->GetNumVertices() * sizeof(VertexSkinned)
                     + (uint64_t)sk->GetNumIndices() * sizeof(IndexType);

        SnapshotEntry geoEntry = base;
        geoEntry.mCategory = SnapshotCategory::Geometry;
        geoEntry.mCpuBytes = geo;
        geoEntry.mGpuBytes = geo;
        geoEntry.mDetail = FormatCount(sk->GetNumVertices()) + " verts, "
                         + FormatCount(sk->GetNumBones()) + " bones";
        out.push_back(geoEntry);

        uint64_t animBytes = 0;
        uint32_t numKeys = 0;
        const std::vector<Animation>& anims = sk->GetAnimations();
        for (const Animation& anim : anims)
        {
            for (const Channel& ch : anim.mChannels)
            {
                animBytes += ch.mPositionKeys.size() * sizeof(PositionKey);
                animBytes += ch.mRotationKeys.size() * sizeof(RotationKey);
                animBytes += ch.mScaleKeys.size()    * sizeof(ScaleKey);
                numKeys += (uint32_t)(ch.mPositionKeys.size() + ch.mRotationKeys.size() + ch.mScaleKeys.size());
            }
        }

        if (animBytes > 0)
        {
            SnapshotEntry animEntry = base;
            animEntry.mCategory = SnapshotCategory::Animation;
            animEntry.mCpuBytes = animBytes;
            animEntry.mGpuBytes = 0;
            char detail[128];
            snprintf(detail, sizeof(detail), "%u anims, %s keys",
                     (uint32_t)anims.size(), FormatCount(numKeys).c_str());
            animEntry.mDetail = detail;
            out.push_back(animEntry);
        }
        return;
    }

    if (SoundWave* sw = asset->As<SoundWave>())
    {
        base.mCategory = SnapshotCategory::Audio;
        if (sw->IsStreaming())
        {
            base.mCpuBytes = 0;  // PCM streamed from disk, not resident
            char detail[128];
            snprintf(detail, sizeof(detail), "streamed, %.1fs %uHz %s",
                     sw->GetDuration(), sw->GetSampleRate(),
                     sw->GetNumChannels() >= 2 ? "stereo" : "mono");
            base.mDetail = detail;
        }
        else
        {
            base.mCpuBytes = sw->GetWaveDataSize();
            char detail[128];
            snprintf(detail, sizeof(detail), "%.1fs %uHz %ubit %s",
                     sw->GetDuration(), sw->GetSampleRate(), sw->GetBitsPerSample(),
                     sw->GetNumChannels() >= 2 ? "stereo" : "mono");
            base.mDetail = detail;
        }
        out.push_back(base);
        return;
    }

    if (Font* f = asset->As<Font>())
    {
        base.mCategory = SnapshotCategory::Fonts;
        base.mCpuBytes = f->GetCharacters().size() * sizeof(uint32_t) * 8;  // rough glyph metadata
        char detail[64];
        snprintf(detail, sizeof(detail), "%u glyphs, size %d",
                 (uint32_t)f->GetCharacters().size(), f->GetSize());
        base.mDetail = detail;
        out.push_back(base);
        return;
    }

    if (Material* mat = asset->As<Material>())
    {
        base.mCategory = SnapshotCategory::Materials;

        if (MaterialLite* lite = mat->AsLite())
        {
            const MaterialLiteParams& lp = lite->GetLiteParams();
            uint32_t n = lp.mNumTextures < MATERIAL_LITE_MAX_TEXTURES ? lp.mNumTextures : MATERIAL_LITE_MAX_TEXTURES;
            uint32_t texCount = 0;
            for (uint32_t i = 0; i < n; ++i)
            {
                if (lp.mTextures[i].Get() != nullptr)
                    texCount++;
            }
            // Lite materials carry a small fixed uniform/param block (colors, UV
            // transforms, TEV modes) rather than a shader-parameter vector; the
            // textures themselves are counted separately under Textures.
            base.mCpuBytes = sizeof(MaterialLite) - sizeof(Material);
            char detail[64];
            snprintf(detail, sizeof(detail), "lite, %u textures", texCount);
            base.mDetail = detail;
        }
        else
        {
            uint32_t texCount = 0;
            for (const ShaderParameter& p : mat->GetParameters())
            {
                if (p.mType == ShaderParameterType::Texture)
                    texCount++;
            }
            base.mCpuBytes = mat->GetParameters().size() * sizeof(ShaderParameter);
            char detail[64];
            snprintf(detail, sizeof(detail), "%u params, %u textures",
                     (uint32_t)mat->GetParameters().size(), texCount);
            base.mDetail = detail;
        }
        out.push_back(base);
        return;
    }

    if (asset->As<ParticleSystem>())
    {
        base.mCategory = SnapshotCategory::Particles;
        base.mCpuBytes = 0;  // config-only; the referenced material/texture is counted separately
        base.mDetail = "emitter config";
        out.push_back(base);
        return;
    }

    // Unknown asset type -- record it so it shows up, but with no size estimate.
    base.mCategory = SnapshotCategory::Other;
    base.mDetail = "no size estimate";
    out.push_back(base);
}

// Assets that `asset` transitively pulls into memory (meshes -> materials -> textures,
// fonts -> atlas texture). Only loaded, non-null children are returned.
static void GatherChildAssets(Asset* asset, std::vector<Asset*>& out)
{
    auto add = [&out](Asset* a)
    {
        if (a != nullptr && a->IsLoaded())
            out.push_back(a);
    };

    if (StaticMesh* sm = asset->As<StaticMesh>())
    {
        add(sm->GetMaterial());
    }
    else if (SkeletalMesh* sk = asset->As<SkeletalMesh>())
    {
        add(sk->GetMaterial());
        for (uint32_t i = 0; i < sk->GetNumSections(); ++i)
            add(sk->GetSectionMaterial(i));
    }
    else if (Material* mat = asset->As<Material>())
    {
        // MaterialLite keeps its textures in a fixed slot array, not shader params.
        // Read the params directly -- GetTexture() substitutes the engine white
        // texture for unused/out-of-range slots, which we must not count.
        if (MaterialLite* lite = mat->AsLite())
        {
            const MaterialLiteParams& lp = lite->GetLiteParams();
            uint32_t n = lp.mNumTextures < MATERIAL_LITE_MAX_TEXTURES ? lp.mNumTextures : MATERIAL_LITE_MAX_TEXTURES;
            for (uint32_t i = 0; i < n; ++i)
                add(lp.mTextures[i].Get());
        }
        // A MaterialInstance's textures live on its base material.
        if (MaterialInstance* inst = mat->As<MaterialInstance>())
        {
            add(inst->GetBaseMaterial());
        }
        // Base / instance materials expose textures as shader parameters.
        for (ShaderParameter& p : mat->GetParameters())
        {
            if (p.mType == ShaderParameterType::Texture)
                add(p.mTextureValue.Get());
        }
    }
    else if (Font* f = asset->As<Font>())
    {
        add(f->GetTexture());
    }
    else if (ParticleSystem* ps = asset->As<ParticleSystem>())
    {
        add(ps->GetMaterial());
    }
}

// ------------------------------------------------------------------------------------------------
// Capture
// ------------------------------------------------------------------------------------------------

static void DownscaleThumbnail(const std::vector<uint8_t>& srcRgba, uint32_t srcW, uint32_t srcH,
                               std::vector<uint8_t>& outRgba, uint32_t& outW, uint32_t& outH)
{
    const uint32_t kMaxDim = 256;
    if (srcW == 0 || srcH == 0 || srcRgba.size() < (size_t)srcW * srcH * 4)
    {
        outW = 0;
        outH = 0;
        outRgba.clear();
        return;
    }

    uint32_t dstW = srcW;
    uint32_t dstH = srcH;
    if (dstW > kMaxDim || dstH > kMaxDim)
    {
        float scale = (srcW >= srcH) ? (float)kMaxDim / srcW : (float)kMaxDim / srcH;
        dstW = (uint32_t)(srcW * scale);
        dstH = (uint32_t)(srcH * scale);
        if (dstW == 0) dstW = 1;
        if (dstH == 0) dstH = 1;
    }

    outW = dstW;
    outH = dstH;
    outRgba.resize((size_t)dstW * dstH * 4);
    for (uint32_t y = 0; y < dstH; ++y)
    {
        uint32_t sy = (uint32_t)((uint64_t)y * srcH / dstH);
        for (uint32_t x = 0; x < dstW; ++x)
        {
            uint32_t sx = (uint32_t)((uint64_t)x * srcW / dstW);
            const uint8_t* s = &srcRgba[((size_t)sy * srcW + sx) * 4];
            uint8_t* d = &outRgba[((size_t)y * dstW + x) * 4];
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
    }
}

void GatherGameReferencedAssets(std::unordered_map<Asset*, uint32_t>& outRefs,
                                std::unordered_map<std::string, uint32_t>* outScriptFiles,
                                uint32_t* outNodeCount,
                                int32_t* outWorldCount)
{
    // Gather every node in the game world(s). When not playing, world 0 holds the
    // edit scene; during PIE it holds the cloned play tree.
    std::vector<Node*> nodes;
    int32_t numWorlds = GetNumWorlds();
    for (int32_t i = 0; i < numWorlds; ++i)
    {
        World* world = GetWorld(i);
        if (world != nullptr)
            world->GatherNodes(nodes);
    }

    // Direct references: any asset-typed node/script property.
    std::vector<Property> props;
    for (Node* node : nodes)
    {
        props.clear();
        node->GatherProperties(props);

        Script* script = node->GetScript();
        if (script != nullptr)
        {
            script->AppendScriptProperties(props);
            if (outScriptFiles != nullptr)
            {
                const std::string& file = script->GetFile();
                if (!file.empty())
                    (*outScriptFiles)[file]++;
            }
        }

        for (const Property& prop : props)
        {
            if (!IsAssetDatumType(prop.GetType()))
                continue;

            for (uint32_t idx = 0; idx < prop.GetCount(); ++idx)
            {
                Asset* a = prop.GetAsset(idx);
                if (a != nullptr && a->IsLoaded())
                    outRefs[a]++;
            }
        }
    }

    // Transitively pull in child assets (mesh -> material -> texture incl.
    // MaterialLite slots, font -> atlas, particle -> material).
    std::vector<Asset*> work;
    work.reserve(outRefs.size());
    for (const auto& kv : outRefs)
        work.push_back(kv.first);

    std::unordered_set<Asset*> expanded;
    std::vector<Asset*> children;
    while (!work.empty())
    {
        Asset* a = work.back();
        work.pop_back();
        if (expanded.count(a) != 0)
            continue;
        expanded.insert(a);

        children.clear();
        GatherChildAssets(a, children);
        for (Asset* c : children)
        {
            auto it = outRefs.find(c);
            if (it == outRefs.end())
                outRefs[c] = 1;
            else
                it->second++;
            work.push_back(c);
        }
    }

    if (outNodeCount != nullptr)
        *outNodeCount = (uint32_t)nodes.size();
    if (outWorldCount != nullptr)
        *outWorldCount = numWorlds;
}

void BuildGameMemoryEntries(std::vector<SnapshotEntry>& out,
                            uint32_t* outNodeCount,
                            int32_t* outWorldCount)
{
    std::unordered_map<Asset*, uint32_t> refs;
    std::unordered_map<std::string, uint32_t> scriptFiles;
    GatherGameReferencedAssets(refs, &scriptFiles, outNodeCount, outWorldCount);

    // Per-asset entries (format-accurate textures, streaming-aware audio, etc.).
    for (const auto& kv : refs)
        ClassifyAndSize(kv.first, kv.second, out);

    // Lua script source sizes from disk.
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    for (const auto& kv : scriptFiles)
    {
        std::string file = kv.first;
        if (file.size() < 4 || file.substr(file.size() - 4) != ".lua")
            file += ".lua";
        std::string path = projDir + "Scripts/" + file;

        SnapshotEntry entry;
        entry.mName = kv.first;
        entry.mTypeName = "Script";
        entry.mCategory = SnapshotCategory::Scripts;
        entry.mRefCount = kv.second;

        if (SYS_DoesFileExist(path.c_str(), false))
        {
            Stream s;
            if (s.ReadFile(path.c_str(), false))
            {
                entry.mCpuBytes = s.GetSize();
                entry.mDetail = "Lua source";
            }
        }
        else
        {
            entry.mDetail = "source not on disk";
        }
        out.push_back(entry);
    }

    // Frame buffers: real runtime GPU memory for the render surface (double-
    // buffered RGBA8 color + 16-bit depth), sized from the Game Preview.
    {
        uint32_t w = 640;
        uint32_t h = 480;
        GamePreview* preview = GetGamePreview();
        if (preview != nullptr && preview->IsEnabled() &&
            preview->GetCurrentWidth() > 0 && preview->GetCurrentHeight() > 0)
        {
            w = preview->GetCurrentWidth();
            h = preview->GetCurrentHeight();
        }
        uint64_t color = (uint64_t)w * h * 4 * 2;
        uint64_t depth = (uint64_t)w * h * 2;

        SnapshotEntry entry;
        entry.mName = "Frame Buffers";
        entry.mTypeName = "RenderTarget";
        entry.mCategory = SnapshotCategory::FrameBuffer;
        entry.mRefCount = 1;
        entry.mGpuBytes = color + depth;
        char detail[96];
        snprintf(detail, sizeof(detail), "%ux%u color x2 + depth", w, h);
        entry.mDetail = detail;
        out.push_back(entry);
    }
}

MemorySnapshot CaptureSnapshot()
{
    MemorySnapshot snapshot;
    snapshot.mProjectName = GetEngineState()->mProjectName;
    snapshot.mWasPlayingInEditor = IsPlayingInEditor();

    // Date string (avoid ':' -- illegal in Windows filenames).
    {
        time_t now = time(nullptr);
        struct tm timeInfo;
#if PLATFORM_WINDOWS
        localtime_s(&timeInfo, &now);
#else
        localtime_r(&now, &timeInfo);
#endif
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeInfo);
        snapshot.mDateString = buf;
    }

    // Whole-process figures (MB -> bytes). Includes editor overhead; shown separately.
    snapshot.mSystemRamUsed  = (uint64_t)(SYS_GetRAMUsage()  * 1024.0 * 1024.0);
    snapshot.mSystemVramUsed = (uint64_t)(SYS_GetVRAMUsage() * 1024.0 * 1024.0);
    snapshot.mSystemTotalRam = (uint64_t)(SYS_GetTotalRAM()  * 1024.0 * 1024.0);

    // Active audio voices.
    for (uint32_t v = 0; v < AUDIO_MAX_VOICES; ++v)
    {
        if (AUD_IsPlaying(v))
            snapshot.mActiveAudioVoices++;
    }

    // Build the game-scoped memory entries (assets + scripts + frame buffers).
    // Shared with the Profiling window so both report identical numbers.
    uint32_t nodeCount = 0;
    int32_t numWorlds = 0;
    BuildGameMemoryEntries(snapshot.mEntries, &nodeCount, &numWorlds);

    // Informational node-count row.
    {
        SnapshotEntry nodeEntry;
        nodeEntry.mName = "Scene Nodes";
        nodeEntry.mTypeName = "Node";
        nodeEntry.mCategory = SnapshotCategory::Nodes;
        nodeEntry.mRefCount = nodeCount;
        char detail[96];
        snprintf(detail, sizeof(detail), "%u nodes across %d world%s",
                 nodeCount, numWorlds, numWorlds == 1 ? "" : "s");
        nodeEntry.mDetail = detail;
        snapshot.mEntries.push_back(nodeEntry);
    }

    // Frame thumbnail from the Game Preview color target (if it is rendering).
    GamePreview* preview = GetGamePreview();
    if (preview != nullptr && preview->IsEnabled())
    {
        std::vector<uint8_t> rgba;
        uint32_t w = 0;
        uint32_t h = 0;
        if (preview->CaptureScreenshotToMemory(rgba, w, h))
        {
            DownscaleThumbnail(rgba, w, h, snapshot.mThumbRgba, snapshot.mThumbW, snapshot.mThumbH);
        }
    }

    LogDebug("Captured memory snapshot: %u entries, %.2f MB CPU / %.2f MB GPU (%s)",
             (uint32_t)snapshot.mEntries.size(),
             snapshot.GetTotalCpuBytes() / (1024.0 * 1024.0),
             snapshot.GetTotalGpuBytes() / (1024.0 * 1024.0),
             snapshot.mWasPlayingInEditor ? "playing" : "edit scene");

    return snapshot;
}

// ------------------------------------------------------------------------------------------------
// Paths + serialization
// ------------------------------------------------------------------------------------------------

std::string GetDebugSnapshotDir()
{
    const std::string& projDir = GetEngineState()->mProjectDirectory;
    if (projDir.empty())
        return "";
    return projDir + "Debug/";
}

std::string MakeDefaultSnapshotPath()
{
    std::string dir = GetDebugSnapshotDir();
    if (dir.empty())
        return "";

    time_t now = time(nullptr);
    struct tm timeInfo;
#if PLATFORM_WINDOWS
    localtime_s(&timeInfo, &now);
#else
    localtime_r(&now, &timeInfo);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeInfo);

    return dir + "DebugSnapshot_" + buf + ".oct";
}

bool SaveSnapshot(const MemorySnapshot& snapshot, const std::string& path)
{
    if (path.empty())
        return false;

    // Ensure the Debug/ folder (or whatever parent) exists.
    std::string dir = GetDebugSnapshotDir();
    if (!dir.empty() && !DoesDirExist(dir.c_str()))
        SYS_CreateDirectory(dir.c_str());

    Stream stream;
    stream.WriteUint32(kSnapshotMagic);
    stream.WriteUint32(kSnapshotVersion);
    stream.WriteString(snapshot.mProjectName);
    stream.WriteString(snapshot.mDateString);
    stream.WriteBool(snapshot.mWasPlayingInEditor);
    stream.WriteUint32(snapshot.mActiveAudioVoices);
    stream.WriteUint64(snapshot.mSystemRamUsed);
    stream.WriteUint64(snapshot.mSystemVramUsed);
    stream.WriteUint64(snapshot.mSystemTotalRam);

    stream.WriteUint32((uint32_t)snapshot.mEntries.size());
    for (const SnapshotEntry& e : snapshot.mEntries)
    {
        stream.WriteString(e.mName);
        stream.WriteString(e.mTypeName);
        stream.WriteUint8((uint8_t)e.mCategory);
        stream.WriteUint64(e.mCpuBytes);
        stream.WriteUint64(e.mGpuBytes);
        stream.WriteUint32(e.mRefCount);
        stream.WriteString(e.mDetail);
    }

    stream.WriteUint32(snapshot.mThumbW);
    stream.WriteUint32(snapshot.mThumbH);
    stream.WriteUint32((uint32_t)snapshot.mThumbRgba.size());
    if (!snapshot.mThumbRgba.empty())
        stream.WriteBytes(snapshot.mThumbRgba.data(), (uint32_t)snapshot.mThumbRgba.size());

    if (!stream.WriteFile(path.c_str()))
    {
        LogError("Failed to write snapshot to '%s'", path.c_str());
        return false;
    }

    LogDebug("Saved memory snapshot: %s", path.c_str());
    return true;
}

bool LoadSnapshot(MemorySnapshot& outSnapshot, const std::string& path)
{
    if (path.empty())
        return false;

    Stream stream;
    if (!stream.ReadFile(path.c_str(), false))
    {
        LogError("Failed to read snapshot '%s'", path.c_str());
        return false;
    }

    if (stream.GetSize() < 8)
        return false;

    uint32_t magic = stream.ReadUint32();
    if (magic != kSnapshotMagic)
    {
        LogError("'%s' is not a valid memory snapshot", path.c_str());
        return false;
    }

    outSnapshot = MemorySnapshot();
    outSnapshot.mVersion = stream.ReadUint32();
    stream.ReadString(outSnapshot.mProjectName);
    stream.ReadString(outSnapshot.mDateString);
    outSnapshot.mWasPlayingInEditor = stream.ReadBool();
    outSnapshot.mActiveAudioVoices = stream.ReadUint32();
    outSnapshot.mSystemRamUsed = stream.ReadUint64();
    outSnapshot.mSystemVramUsed = stream.ReadUint64();
    outSnapshot.mSystemTotalRam = stream.ReadUint64();

    uint32_t numEntries = stream.ReadUint32();
    outSnapshot.mEntries.resize(numEntries);
    for (uint32_t i = 0; i < numEntries; ++i)
    {
        SnapshotEntry& e = outSnapshot.mEntries[i];
        stream.ReadString(e.mName);
        stream.ReadString(e.mTypeName);
        e.mCategory = (SnapshotCategory)stream.ReadUint8();
        e.mCpuBytes = stream.ReadUint64();
        e.mGpuBytes = stream.ReadUint64();
        e.mRefCount = stream.ReadUint32();
        stream.ReadString(e.mDetail);
    }

    outSnapshot.mThumbW = stream.ReadUint32();
    outSnapshot.mThumbH = stream.ReadUint32();
    uint32_t thumbBytes = stream.ReadUint32();
    if (thumbBytes > 0)
    {
        outSnapshot.mThumbRgba.resize(thumbBytes);
        stream.ReadBytes(outSnapshot.mThumbRgba.data(), thumbBytes);
    }

    LogDebug("Loaded memory snapshot: %s (%u entries)", path.c_str(), numEntries);
    return true;
}

#endif
