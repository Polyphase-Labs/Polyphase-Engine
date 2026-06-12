#pragma once

#include "Asset.h"
#include "AssetRef.h"
#include "EngineTypes.h"
#include "Input/InputTypes.h"

#include <string>
#include <vector>
#include <unordered_map>

class Texture;
class Font;

enum class InputPromptKind : uint8_t
{
    Sprite,
    Glyph,
    Text,

    Count
};

struct InputPromptEntry
{
    // Platform::Count is the "Any" sentinel — entry applies to every platform
    // unless overridden by a more specific row.
    Platform mPlatform = Platform::Count;

    // For Keyboard/Mouse paths gamepad type is ignored. For Gamepad paths,
    // GamepadType::Count is the "Any controller" sentinel.
    GamepadType mGamepadType = GamepadType::Count;

    // Canonical key from MakeInputPath() — e.g. "Keyboard/F", "Gamepad.Button/A".
    std::string mInputPath;

    InputPromptKind mKind = InputPromptKind::Text;

    TextureRef mSprite;       // valid when mKind == Sprite
    FontRef    mGlyphFont;    // valid when mKind == Glyph
    uint32_t   mGlyphCodepoint = 0;  // valid when mKind == Glyph

    // Always populated — used directly when mKind == Text, and as the final
    // fallback when Sprite/Glyph resolution fails.
    std::string mFallbackText;
};

class POLYPHASE_API InputPromptMap : public Asset
{
public:

    DECLARE_ASSET(InputPromptMap, Asset);

    InputPromptMap();
    virtual ~InputPromptMap();

    virtual void LoadStream(Stream& stream, Platform platform) override;
    virtual void SaveStream(Stream& stream, Platform platform) override;
    virtual void Create() override;
    virtual void Destroy() override;
    virtual void GatherProperties(std::vector<Property>& outProps) override;

    virtual glm::vec4 GetTypeColor() override;
    virtual const char* GetTypeName() override;

    std::vector<InputPromptEntry>& GetEntries();
    const std::vector<InputPromptEntry>& GetEntries() const;

    // Best-match lookup. Returns nullptr when no entry covers the request.
    // Match priority: exact (platform, deviceType, path) > path+anyDevice >
    //                 path+anyPlatform > path-only (any/any).
    const InputPromptEntry* Find(Platform platform,
                                 GamepadType deviceType,
                                 const std::string& inputPath) const;

    // Rebuild the lookup index from mEntries. Call after editor mutations.
    void RebuildIndex();

private:

    static bool HandlePropChange(Datum* datum, uint32_t index, const void* newValue);

    std::vector<InputPromptEntry> mEntries;

    // Cached fast-path index — same data shape used at Find time.
    // Key: "<platform>|<gamepadType>|<inputPath>".
    std::unordered_map<std::string, size_t> mIndex;
    bool mIndexDirty = true;
};
