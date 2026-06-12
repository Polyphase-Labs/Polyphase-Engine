#include "InputPromptResolver.h"
#include "InputPath.h"
#include "PlayerInputSystem.h"
#include "InputMap.h"

#include "Assets/InputPromptStyle.h"
#include "Assets/Texture.h"
#include "Assets/Font.h"
#include "EngineTypes.h"
#include "Engine.h"
#include "Utilities.h"

InputPromptResolver* InputPromptResolver::sInstance = nullptr;

void InputPromptResolver::Create()
{
    Destroy();
    sInstance = new InputPromptResolver();
}

void InputPromptResolver::Destroy()
{
    if (sInstance)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

InputPromptResolver* InputPromptResolver::Get()
{
    return sInstance;
}

void InputPromptResolver::Tick()
{
    PlayerInputSystem* pis = PlayerInputSystem::Get();
    if (!pis)
        return;

    uint32_t curFrame = pis->GetDeviceChangeFrame();
    if (curFrame != mLastSeenDeviceFrame)
    {
        mLastSeenDeviceFrame = curFrame;
        Invalidate();
    }
}

void InputPromptResolver::Invalidate()
{
    mLru.clear();
    mLookup.clear();
}

static std::string DeviceTag(const InputDeviceDescriptor& d)
{
    return std::to_string((int)d.kind) + ":" + std::to_string((int)d.gamepadType) +
           ":" + std::to_string(d.gamepadIndex);
}

// Pick the best binding for the active device kind. Falls back to the first
// binding if none matches the device — that way text fallback still renders
// something useful instead of going blank.
static const InputActionBinding* PickBindingForDevice(const InputAction& action,
                                                      const InputDeviceDescriptor& dev)
{
    const InputActionBinding* fallback = action.bindings.empty() ? nullptr : &action.bindings[0];
    for (const InputActionBinding& b : action.bindings)
    {
        switch (dev.kind)
        {
        case InputDeviceKind::Keyboard:
            if (b.sourceType == InputSourceType::Keyboard) return &b;
            break;
        case InputDeviceKind::Mouse:
            if (b.sourceType == InputSourceType::MouseButton) return &b;
            break;
        case InputDeviceKind::Gamepad:
            if (b.sourceType == InputSourceType::GamepadButton ||
                b.sourceType == InputSourceType::GamepadAxis)
                return &b;
            break;
        default:
            break;
        }
    }
    return fallback;
}

static std::string DefaultLabel(const InputActionBinding& binding)
{
    switch (binding.sourceType)
    {
    case InputSourceType::Keyboard:
    {
        const char* n = InputMap::GetKeyCodeName(binding.code);
        return n ? n : "?";
    }
    case InputSourceType::MouseButton:
        switch (binding.code)
        {
        case MOUSE_LEFT:   return "LMB";
        case MOUSE_RIGHT:  return "RMB";
        case MOUSE_MIDDLE: return "MMB";
        case MOUSE_X1:     return "M4";
        case MOUSE_X2:     return "M5";
        default:           return "Mouse";
        }
    case InputSourceType::GamepadButton:
        if (binding.code >= 0 && binding.code < GAMEPAD_BUTTON_COUNT)
            return InputMap::GetGamepadButtonName((GamepadButtonCode)binding.code);
        return "?";
    case InputSourceType::GamepadAxis:
        if (binding.code >= 0 && binding.code < GAMEPAD_AXIS_COUNT)
            return InputMap::GetGamepadAxisName((GamepadAxisCode)binding.code);
        return "?";
    case InputSourceType::Pointer:
        return "Touch";
    }
    return "?";
}

const ResolvedPrompt* InputPromptResolver::Resolve(InputPromptMap* map,
                                                   InputPromptStyle* style,
                                                   const std::string& actionCategory,
                                                   const std::string& actionName,
                                                   const InputDeviceDescriptor* deviceOverride)
{
    PlayerInputSystem* pis = PlayerInputSystem::Get();
    if (!pis)
        return nullptr;

    const InputDeviceDescriptor& device = deviceOverride
        ? *deviceOverride
        : pis->GetLastActiveDevice();

    CacheKey key;
    key.mapUuid = map ? map->GetUuid() : 0;
    key.styleUuid = style ? style->GetUuid() : 0;
    key.deviceEpoch = pis->GetDeviceChangeFrame();
    key.action = actionCategory + "/" + actionName;
    key.deviceTag = DeviceTag(device);

    auto it = mLookup.find(key);
    if (it != mLookup.end())
    {
        // Validate cached entry against the asset's current state. This catches
        // the case where a Font asset was re-imported (so it now contains the
        // codepoint) without the map being edited — the resolver cache key
        // wouldn't otherwise change and we'd serve a stale "fallback to text"
        // result forever. Cheap O(N-glyphs) walk; only fires on cache hit.
        const ResolvedPrompt& cached = it->second->prompt;
        bool stillValid = true;
        if (cached.kind == InputPromptKind::Glyph)
        {
            stillValid = false;
            if (cached.font && cached.codepoint != 0)
            {
                for (const Character& c : cached.font->GetCharacters())
                {
                    if ((uint32_t)c.mCodePoint == cached.codepoint)
                    {
                        stillValid = true;
                        break;
                    }
                }
            }
        }
        else if (cached.kind == InputPromptKind::Sprite)
        {
            stillValid = (cached.sprite != nullptr);
        }

        if (stillValid)
        {
            mLru.splice(mLru.begin(), mLru, it->second);
            return &it->second->prompt;
        }
        // Stale — evict and fall through to re-resolve.
        mLru.erase(it->second);
        mLookup.erase(it);
    }

    // ---------------- resolve cold -----------------

    ResolvedPrompt resolved;

    const InputAction* action = pis->FindAction(actionCategory, actionName);
    const InputActionBinding* binding = nullptr;
    if (action)
        binding = PickBindingForDevice(*action, device);

    // Label is always populated so consumers have something to show.
    resolved.label = binding ? DefaultLabel(*binding) : actionName;

    if (binding && map)
    {
        std::string path = MakeInputPath(*binding);
        const InputPromptEntry* entry =
            map->Find(GetPlatform(), device.gamepadType, path);

        // Style priority: walk the chain and pick the first kind the entry
        // actually provides. The entry's own mKind is treated as a hint —
        // if the chosen kind has no asset, fall through to the next.
        const std::array<InputPromptKind, InputPromptStyle::kPriorityCount> defaultPriority = {
            InputPromptKind::Sprite, InputPromptKind::Glyph, InputPromptKind::Text
        };
        const auto& priority = style ? style->GetPriority() : defaultPriority;

        if (entry)
        {
            for (InputPromptKind want : priority)
            {
                if (want == InputPromptKind::Sprite &&
                    entry->mKind == InputPromptKind::Sprite &&
                    entry->mSprite.Get() != nullptr)
                {
                    resolved.kind = InputPromptKind::Sprite;
                    resolved.sprite = entry->mSprite.Get<Texture>();
                    if (!entry->mFallbackText.empty()) resolved.label = entry->mFallbackText;
                    break;
                }
                if (want == InputPromptKind::Glyph &&
                    entry->mKind == InputPromptKind::Glyph &&
                    entry->mGlyphFont.Get() != nullptr &&
                    entry->mGlyphCodepoint != 0)
                {
                    Font* gFont = entry->mGlyphFont.Get<Font>();
                    bool inFont = false;
                    for (const Character& c : gFont->GetCharacters())
                    {
                        if ((uint32_t)c.mCodePoint == entry->mGlyphCodepoint)
                        {
                            inFont = true;
                            break;
                        }
                    }
                    if (inFont)
                    {
                        resolved.kind = InputPromptKind::Glyph;
                        resolved.font = gFont;
                        resolved.codepoint = entry->mGlyphCodepoint;
                        if (!entry->mFallbackText.empty()) resolved.label = entry->mFallbackText;
                        break;
                    }
                    // Codepoint missing — let the priority chain fall through
                    // (to Glyph-skip, then Text) so the user sees the label
                    // until the font is updated. Cache validator above will
                    // re-resolve when the font's character table changes.
                }
                if (want == InputPromptKind::Text)
                {
                    resolved.kind = InputPromptKind::Text;
                    if (!entry->mFallbackText.empty()) resolved.label = entry->mFallbackText;
                    break;
                }
            }
        }
    }

    // Insert into LRU.
    mLru.push_front({ key, resolved });
    mLookup[key] = mLru.begin();
    if (mLru.size() > kCacheLimit)
    {
        mLookup.erase(mLru.back().key);
        mLru.pop_back();
    }
    return &mLru.front().prompt;
}

void InputPromptResolver::Prewarm(InputPromptMap* map, InputPromptStyle* style)
{
    if (!style) return;
    for (const std::string& spec : style->GetPrewarmActions())
    {
        size_t slash = spec.find('/');
        if (slash == std::string::npos) continue;
        std::string cat = spec.substr(0, slash);
        std::string name = spec.substr(slash + 1);
        Resolve(map, style, cat, name);
    }
}
