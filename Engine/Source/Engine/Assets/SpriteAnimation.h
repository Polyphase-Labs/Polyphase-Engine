#pragma once

#include "Asset.h"
#include "AssetRef.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>

class Texture;

// Determines how a SpriteAnimation lays out its frames:
//   Discrete  — one Texture asset per frame; frames may be different sizes.
//   AtlasGrid — single atlas Texture sliced into a regular grid; each frame is
//               a cell index into that grid. More memory-efficient and avoids
//               per-frame texture binds at runtime.
enum class SpriteFrameSourceMode : uint8_t
{
    Discrete = 0,
    AtlasGrid = 1,

    Count
};

// A reusable, named sprite animation clip. Authored once, played by name from
// any number of SpriteAnimator nodes. The asset transparently supports two
// frame-source modes (discrete textures or one atlas + grid metadata) and the
// SpriteAnimator's binding API works the same either way.
class POLYPHASE_API SpriteAnimation : public Asset
{
public:

    DECLARE_ASSET(SpriteAnimation, Asset);

    SpriteAnimation();
    virtual ~SpriteAnimation();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual bool Import(const std::string& path, ImportOptions* options) override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;
    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    // Common
    SpriteFrameSourceMode GetMode() const { return mMode; }
    void SetMode(SpriteFrameSourceMode mode) { mMode = mode; }

    const std::string& GetAnimationName() const { return mAnimationName; }
    void SetAnimationName(const std::string& name) { mAnimationName = name; }

    float GetFps() const { return mFps; }
    void SetFps(float fps) { mFps = fps; }

    bool GetLoop() const { return mLoop; }
    void SetLoop(bool loop) { mLoop = loop; }

    // Discrete-mode authoring
    const std::vector<TextureRef>& GetFrames() const { return mFrames; }
    std::vector<TextureRef>& GetFramesMutable() { return mFrames; }
    void AddFrame(Texture* tex);
    void ClearFrames();

    // Atlas-mode authoring
    Texture* GetAtlasTexture() const;
    void SetAtlasTexture(Texture* tex);
    int32_t GetAtlasCols() const { return mAtlasCols; }
    int32_t GetAtlasRows() const { return mAtlasRows; }
    void SetAtlasGrid(int32_t cols, int32_t rows) { mAtlasCols = cols; mAtlasRows = rows; }
    void SetAtlasMargin(int32_t x, int32_t y) { mAtlasMarginX = x; mAtlasMarginY = y; }
    void SetAtlasSpacing(int32_t x, int32_t y) { mAtlasSpacingX = x; mAtlasSpacingY = y; }
    const std::vector<int32_t>& GetAtlasFrameIndices() const { return mAtlasFrameIndices; }
    std::vector<int32_t>& GetAtlasFrameIndicesMutable() { return mAtlasFrameIndices; }

    // Unified frame query — works for either mode.
    int32_t GetFrameCount() const;
    Texture* GetFrameTexture(int32_t frameIndex) const;
    // Discrete: returns (0,0)-(1,1). AtlasGrid: shared math with TileSet via TextureAtlasUtil.
    bool GetFrameUV(int32_t frameIndex, glm::vec2& outUV0, glm::vec2& outUV1) const;

protected:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    // Common
    std::string mAnimationName;
    SpriteFrameSourceMode mMode = SpriteFrameSourceMode::Discrete;
    float mFps = 12.0f;
    bool mLoop = true;

    // Discrete mode
    std::vector<TextureRef> mFrames;

    // AtlasGrid mode
    AssetRef mAtlasTexture;
    int32_t mAtlasCols = 1;
    int32_t mAtlasRows = 1;
    int32_t mAtlasMarginX = 0;
    int32_t mAtlasMarginY = 0;
    int32_t mAtlasSpacingX = 0;
    int32_t mAtlasSpacingY = 0;
    std::vector<int32_t> mAtlasFrameIndices;
};
