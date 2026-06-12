#pragma once

#include "Assets/InputPromptMap.h"

#include <list>
#include <string>
#include <unordered_map>

class InputPromptStyle;
class Texture;
class Font;

struct ResolvedPrompt
{
    InputPromptKind kind = InputPromptKind::Text;
    Texture* sprite = nullptr;
    Font* font = nullptr;
    uint32_t codepoint = 0;
    std::string label;
};

// Owns the resolve-cache that turns an action id + device into a renderable
// prompt. Singleton, created alongside PlayerInputSystem; PlayerInputSystem
// calls Tick() at the end of its Update() so the cache flushes itself on
// device change.
class InputPromptResolver
{
public:

    static void Create();
    static void Destroy();
    static InputPromptResolver* Get();

    // Per-frame tick. Drops the cache when PlayerInputSystem's
    // GetDeviceChangeFrame() has advanced since the last call.
    void Tick();

    // Resolve an action to a renderable prompt. Returned pointer is valid until
    // the next Tick() invalidation OR a fresh Resolve() that evicts via LRU.
    // Callers should re-resolve every PreRender — that's the whole point of the
    // cache.
    //
    // When `deviceOverride` is non-null, uses it instead of
    // PlayerInputSystem::GetLastActiveDevice(); the editor inspector uses this
    // to preview against a specific test device.
    const ResolvedPrompt* Resolve(InputPromptMap* map,
                                  InputPromptStyle* style,
                                  const std::string& actionCategory,
                                  const std::string& actionName,
                                  const struct InputDeviceDescriptor* deviceOverride = nullptr);

    // Pre-resolve every action listed in style->mPrewarmActions ("Category/Name").
    void Prewarm(InputPromptMap* map, InputPromptStyle* style);

    // Force cache flush — InputPromptMap inspector calls this after the artist
    // edits an entry so previews refresh.
    void Invalidate();

private:

    static InputPromptResolver* sInstance;

    InputPromptResolver() = default;

    struct CacheKey
    {
        uint64_t mapUuid = 0;
        uint64_t styleUuid = 0;
        uint32_t deviceEpoch = 0;
        std::string action;     // "Category/Name"
        std::string deviceTag;  // serialized device descriptor

        bool operator==(const CacheKey& o) const
        {
            return mapUuid == o.mapUuid && styleUuid == o.styleUuid &&
                   deviceEpoch == o.deviceEpoch && action == o.action &&
                   deviceTag == o.deviceTag;
        }
    };

    struct CacheKeyHash
    {
        size_t operator()(const CacheKey& k) const
        {
            size_t h = std::hash<uint64_t>()(k.mapUuid);
            h ^= std::hash<uint64_t>()(k.styleUuid) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>()(k.deviceEpoch) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>()(k.action) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>()(k.deviceTag) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    struct CacheEntry
    {
        CacheKey key;
        ResolvedPrompt prompt;
    };

    static constexpr size_t kCacheLimit = 256;

    std::list<CacheEntry> mLru;  // front = most recent
    std::unordered_map<CacheKey, std::list<CacheEntry>::iterator, CacheKeyHash> mLookup;
    uint32_t mLastSeenDeviceFrame = 0;
};
