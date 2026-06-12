# Action Prompt UI — Architecture

This is the engine-side view of the prompt rendering system. If you're authoring assets or placing prompts in a scene, start with [UI / Input Action Prompts](UI/InputActionPrompt.md). This page is for engine developers extending or debugging the system.

---

## Pieces

```
PlayerInputSystem ──── FindAction(cat, name) ──► InputAction { bindings[] }
       │
       └── GetLastActiveDevice() ─► InputDeviceDescriptor { kind, gamepadType }
                                            │
   InputPromptMap (Asset) ─────► InputPromptResolver (singleton, LRU cache)
   InputPromptStyle (Asset) ───►        │
                                        ▼
                                 ResolvedPrompt
                                 { kind: Sprite|Glyph|Text,
                                   sprite/font/codepoint, label }
                                        │
                                        ▼
                              InputActionPrompt (Widget)
                              ├── composes private Quad child
                              └── composes private Text child
```

The widget is intentionally stateless — every `PreRender` it calls the resolver, gets back a cached `ResolvedPrompt`, and toggles either its inner `Quad` or `Text` accordingly. The resolver owns the cache and the per-frame invalidation logic.

---

## Subsystems

### `PlayerInputSystem` extensions

`Engine/Source/Input/PlayerInputSystem.{h,cpp}` was extended with:

| Member | Purpose |
|---|---|
| `InputDeviceKind { Keyboard, Mouse, Gamepad }` | active-device taxonomy |
| `InputDeviceDescriptor { kind, gamepadIndex, gamepadType }` | the resolved active device |
| `GetLastActiveDevice()` | current descriptor — read by the resolver |
| `GetDeviceChangeFrame()` | engine frame number of the last device change; used as a cheap cache-validation epoch |
| `SetActionEvaluationEnabled(bool)` | mute gate used by the editor capture modal so a captured key doesn't also fire an action |
| `AreInputActionsActive()` | combined `mEnabled && mActionEvaluationEnabled` |

Device tracking is edge-triggered inside `Update()`. Holding a key doesn't keep the device pinned; only a fresh press/button/axis-promotion swaps it. The axis promote threshold is `0.5`. Only the *kind* or *gamepad type* changing bumps `mDeviceChangeFrame` — the resolver compares a single `uint32_t` to decide whether to flush.

At the tail of `Update()`, `InputPromptResolver::Tick()` is invoked. That's the only coupling between the two systems.

### `InputPath` helpers

`Engine/Source/Input/InputPath.{h,cpp}`:

```cpp
std::string MakeInputPath(const InputActionBinding& binding);
InputActionBinding MakeBinding(InputSourceType, int32_t code,
                               AxisDirection = Positive, int32_t gamepadIndex = 0);
```

`MakeInputPath` is the canonical key. Examples:

| Binding | Path |
|---|---|
| Keyboard + `POLYPHASE_KEY_F` | `Keyboard/F` |
| Mouse + `MOUSE_LEFT` | `Mouse/Left` |
| Gamepad button + `GAMEPAD_A` | `Gamepad.Button/A` |
| Gamepad axis + `GAMEPAD_AXIS_LTRIGGER` positive | `Gamepad.Axis/LTrigger+` |
| Pointer + index 0 | `Pointer/0` |

Names come from `InputMap::GetKeyCodeName / GetGamepadButtonName / GetGamepadAxisName`. There is no separate naming table — adding a new key just means adding a row to `InputMap`'s existing tables and the prompt system picks it up.

### `InputPromptMap` (asset)

`Engine/Source/Engine/Assets/InputPromptMap.{h,cpp}`. A flat `std::vector<InputPromptEntry>` plus a build-time `std::unordered_map<std::string, size_t>` index for fast `Find`.

```cpp
struct InputPromptEntry {
    Platform        mPlatform;       // Count = "Any"
    GamepadType     mGamepadType;    // Count = "Any/N-A"
    std::string     mInputPath;
    InputPromptKind mKind;           // Sprite | Glyph | Text
    TextureRef      mSprite;
    FontRef         mGlyphFont;
    uint32_t        mGlyphCodepoint;
    std::string     mFallbackText;
};
```

Resolution priority (most specific wins):

```
(platform,        gamepadType,        path)
(platform,        Any,                path)
(Any,             gamepadType,        path)
(Any,             Any,                path)
```

The index is rebuilt lazily on read via `mIndexDirty`; the editor inspector marks dirty on every mutation.

Versioned with `ASSET_VERSION_INPUT_PROMPT_MAP = 33`. Old projects without the version load to an empty entry list.

### `InputPromptStyle` (asset)

`Engine/Source/Engine/Assets/InputPromptStyle.{h,cpp}`. Plain data:

- `mIconSize` (px) — widget auto-size and text size for fallback/glyph
- `mSpacing` (px) — reserved for future side-by-side prompt strings
- `mTint` — multiplied into the rendered prompt color
- `mPriority[3]` — permutation of `{Sprite, Glyph, Text}`
- `mPrewarmActions` — list of `Category/Name` strings resolved on asset load

Versioned with `ASSET_VERSION_INPUT_PROMPT_STYLE = 34`.

### `InputPromptResolver` (service)

`Engine/Source/Input/InputPromptResolver.{h,cpp}`. Singleton; lifecycle is bracketed alongside `PlayerInputSystem` in both `Engine.cpp` (game build) and `EditorMain.cpp` (editor build).

Cache key:

```
(map UUID, style UUID, device-change-frame epoch, "category/name", deviceTag)
```

The LRU bound is `kCacheLimit = 256`. When `PlayerInputSystem::GetDeviceChangeFrame()` advances, `Tick()` flushes the entire cache — re-resolves happen lazily on the next `Resolve()` call. Per-frame cost in the no-change steady state is one map lookup per visible prompt.

Resolution algorithm:

1. `PlayerInputSystem::FindAction(cat, name)` → `InputAction` (or null).
2. Pick the binding whose `sourceType` matches the active `InputDeviceKind`. Fallback = `bindings[0]` if no match.
3. `MakeInputPath(binding)` → path key.
4. `InputPromptMap::Find(platform, gamepadType, path)` walks the 4-key priority chain.
5. Walk `style->mPriority` — the first kind whose data is populated wins. The entry's own `mKind` is treated as a hint: if it's `Sprite` but the texture is null, we fall through to the next priority slot.
6. The `label` is always populated (from `InputMap::GetKeyCodeName`/`GetGamepadButtonName`/etc.) so the consumer can always render *something*.

`Resolve()` accepts an optional `InputDeviceDescriptor* deviceOverride` — used by the editor's `InputPromptStyleInspector` live-preview to show "what this would look like on a DualSense" without faking input.

### `InputActionPrompt` (widget)

`Engine/Source/Engine/Nodes/Widgets/InputActionPrompt.{h,cpp}`. Composition pattern mirrors `Button`:

```
InputActionPrompt
├── Quad (private child, hidden in tree, transient)
└── Text (private child, hidden in tree, transient)
```

In `PreRender`, the widget asks the resolver, sets one child visible and the other hidden, and either:

- **Sprite** → `Quad::SetTexture(resolved->sprite)`, tint = style tint
- **Glyph** → `Text::SetFont(resolved->font)`, `SetText(<UTF-8 of codepoint>)`, size = style icon size
- **Text** → `Text::SetText(resolved->label)`, size = style icon size

The widget never builds its own vertices; it leans entirely on `Quad` and `Text` rendering. This means glyph fonts must have their codepoints mapped into the ASCII visible range until `Text` grows Unicode support — see [Out of scope](#out-of-scope) below.

### `[action:Category.Name]` text substitution

`Text::UpdateVertexData` does a single linear-time scan over `mText` for the literal `[action:` token before its existing per-character loop. Matches are replaced with the resolver's `label` field in a transient buffer; `mText` itself is untouched.

This is the **cheap path** — no inline sprite/glyph rendering, just text replacement. The Text widget's draw call count, vertex buffer layout, GFX backend, and word-wrap logic are all unchanged. Cost when no tag is present: one `std::string::find` returning `npos`.

### Editor inspector

`Engine/Source/Editor/InputPromptEditor/`:

- `InputPromptMapInspector.{h,cpp}` — entry table, drag-drop targets, capture button wiring, validation strip
- `InputPromptStyleInspector.{h,cpp}` — priority chips, prewarm list, live preview
- `InputCaptureModal.{h,cpp}` — shared 5-second "press any input" modal. Mirrors `EditorHotkeysWindow::DrawCaptureOverlay` (`Editor/Hotkeys/EditorHotkeysWindow.cpp:412`); the only difference is it polls gamepad button/axis edges in addition to keys.

Dispatch is one `else if` branch each in `Editor/EditorImgui.cpp` around lines 8800, alongside the existing `Material` / `InstancedMesh3D` branches. The default `GatherProperties` UI draws above; the custom inspector body draws below.

---

## Lifecycle

| Phase | Who calls | What |
|---|---|---|
| `Initialize` (game) | `Engine.cpp` | `PlayerInputSystem::Create(); InputPromptResolver::Create();` |
| `Initialize` (editor) | `EditorMain.cpp` | same pair after `EditorHotkeyMap::Create` |
| `Update` (per frame) | `PlayerInputSystem::Update` | edge-detects device, then `InputPromptResolver::Get()->Tick()` |
| `PreRender` (per widget) | `InputActionPrompt::PreRender` | calls `Resolve()`, sets child visibility |
| `Shutdown` | `Engine.cpp` / `EditorMain.cpp` | `InputPromptResolver::Destroy(); PlayerInputSystem::Destroy();` |

---

## Extending the system

**Adding a new input source type (e.g. SteamDeck back paddles).**
1. Add the enum to `InputSourceType` and the polling implementation in `PlayerInputSystem::PollBindingDown/Value`.
2. Add a name table entry to `InputMap` (`GetKeyCodeName` family).
3. Add a `MakeInputPath` switch arm in `InputPath.cpp`.
4. Add a poll loop arm in `InputCaptureModal::Draw` so artists can capture it.

No changes are needed in the resolver, the assets, or the widget — they're all driven off the path string.

**Adding a new prompt kind (e.g. animated sprite).**
1. Extend the `InputPromptKind` enum.
2. Add storage to `InputPromptEntry` (e.g. a `SpriteAnimationRef`).
3. Add the new variant to `Resolve()`'s priority walk.
4. Add a `ResolvedPrompt` field for the runtime payload.
5. Handle the new kind in `InputActionPrompt::PreRender`.
6. Add the editor inspector arms for the new asset/data cell.

**Adding a device-changed signal for non-resolver consumers.**
Currently only the resolver consumes `GetDeviceChangeFrame()`. If a HUD wants to react to device changes (e.g. fade an entire tutorial panel), polling that frame counter is still the right answer — no SignalBus event is fired today. Add one if/when a second consumer appears; the cost is one publish per change in `UpdateLastActiveDevice`.

---

## Out of scope

- **Inline sprite/glyph rendering in `Text`.** Substituting the *label* is shipped. Rendering inline sprites mid-string requires `Text::UpdateVertexData` to track per-glyph texture and `DrawTextWidget` to issue sub-batches across textures. Tracked as a separate phase.
- **Unicode in `Text`.** The widget still only renders ASCII codepoints (`' '` … `'~'`). For glyph fonts that ship gamepad icons in the PUA range (`U+E000`+), authors must map them down to ASCII. Once `Text` grows real Unicode the widget's glyph path picks it up for free — the codepoint is already passed through verbatim.
- **Dedicated `InputPromptMapEditor` window.** The custom Inspector covers single-asset authoring. A standalone window with bulk import / asset-pack tooling can be lifted out later — the draw functions live in their own files and don't depend on Inspector context beyond the `obj` pointer.
- **Persistent prewarm cache across project loads.** `mPrewarmActions` triggers a lazy warm on first `Resolve` after asset load; that's sufficient for hot-path actions. A pre-baked cache file would only matter for projects with thousands of actions.

---

## File index

| File | Role |
|---|---|
| `Engine/Source/Input/PlayerInputSystem.{h,cpp}` | device tracker, capture-mute gate, resolver Tick hook |
| `Engine/Source/Input/InputPath.{h,cpp}` | canonical path-key synthesis |
| `Engine/Source/Input/InputPromptResolver.{h,cpp}` | LRU-cached resolution service |
| `Engine/Source/Engine/Assets/InputPromptMap.{h,cpp}` | per-(platform, device, path) entry asset |
| `Engine/Source/Engine/Assets/InputPromptStyle.{h,cpp}` | layout + fallback priority asset |
| `Engine/Source/Engine/Nodes/Widgets/InputActionPrompt.{h,cpp}` | widget composing Quad + Text |
| `Engine/Source/Engine/Nodes/Widgets/Text.cpp` | `[action:X.Y]` substitution pass |
| `Engine/Source/Editor/InputPromptEditor/*.{h,cpp}` | editor inspectors + capture modal (EDITOR-only) |
| `Engine/Source/LuaBindings/InputActionPrompt_Lua.{h,cpp}` | Lua surface |

Asset versions: `ASSET_VERSION_INPUT_PROMPT_MAP = 33`, `ASSET_VERSION_INPUT_PROMPT_STYLE = 34`, `ASSET_VERSION_CURRENT = 34`.
