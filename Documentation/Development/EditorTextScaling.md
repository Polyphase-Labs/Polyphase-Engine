# Editor Text Scaling internals

How the editor's **Text Scale** and **Interface Scale** settings work under the hood, and what to
watch for when you add editor UI. For the user-facing description see
[Text Scale & Interface Scale](../Info/InterfaceScaling.md).

## Two different mechanisms

| | Interface Scale | Text Scale |
|---|---|---|
| Stored in | `EngineConfig::mEditorInterfaceScale` → project `Config.ini` | `AppearanceModule::mTextScale` → `Appearance.json` (per user) |
| How it is applied | Every frame `EditorImguiDraw()` sets `io.DisplaySize = window / scale` and `io.DisplayFramebufferScale = scale`. ImGui lays out a smaller logical screen and the Vulkan backend magnifies the result | The font atlas is **re-baked** at `15px × scale` and re-uploaded |
| Cost | None | One `vkDeviceWaitIdle` + atlas rebuild when the value changes |
| Result | Everything grows; glyphs are stretched bitmaps, so text softens at large values | Only text and icons grow; glyphs are always sharp |

Interface Scale also sets the custom `io.PolyphaseInterfaceScale`, which ImGui uses to convert
mouse coordinates. Any editor code that converts between device pixels and ImGui coordinates must
divide by `mEditorInterfaceScale` (see `Viewport_WorldToScreen` in `EditorUIHookManager.cpp` for the
canonical conversion).

Helpers in `EditorImgui.h`:

```cpp
float GetDefaultEditorInterfaceScale();   // 1.0, or SYS_GetDisplayScale() on macOS (Retina)
void  ApplyEditorInterfaceScale(float);   // clamps 0.5 - 3.0, writes Config.ini
void  RequestEditorFontRebuild(float);    // clamps 0.75 - 2.0, applied next frame
float GetEditorTextScale();               // the scale the live fonts were baked at
```

Use `ApplyEditorInterfaceScale()` rather than writing the config field yourself; both the
Preferences page and `View → Interface Scale` go through it. `SYS_GetDisplayScale()` only exists on
macOS, which is why the default is wrapped in a helper.

## Why Text Scale rebuilds the font atlas

The vendored ImGui is **1.89.9**, which predates dynamic fonts: glyphs are rasterised once, at a
fixed pixel size, into a texture atlas. The only runtime knob, `io.FontGlobalScale`, stretches those
bitmaps and looks blurry above about 1.25×. So a text scale change re-bakes the atlas instead.

### The rebuild sequence

`RequestEditorFontRebuild(scale)` only stores the requested value. The work happens in
`ProcessPendingFontRebuild()`, called from `EditorImguiDraw()` right after
`EditorImageCache::RetirePending()` and **before `ImGui::NewFrame()`**:

```
io.Fonts->Clear()
LoadEditorFonts(scale)          // main font 15px, merged icons 14px, terminal font 15px, all × scale
io.Fonts->Build()
VulkanContext::RebuildImguiFontTexture()
    DeviceWaitIdle()
    ImGui_ImplVulkan_DestroyFontsTexture()     // backported, see below
    ImGui_ImplVulkan_CreateFontsTexture(cb)    // one-shot command buffer
    DeviceWaitIdle()
    ImGui_ImplVulkan_DestroyFontUploadObjects()
ScriptEditorWindow::OnEditorFontsRebuilt(newScale / oldScale)
```

That point in the frame is the only safe one. No ImGui frame is open, and after the wait the GPU is
finished with the previous frame's draw data, which still references the old font descriptor set.
Freeing an ImGui Vulkan descriptor set in the middle of a frame crashes some drivers (RADV on
Linux), which is the same reason `EditorImageCache` defers its texture frees to this spot.

The rebuild is skipped when the requested scale equals the live one. That matters because
`AppearanceModule::LoadSettings()` requests a rebuild and it also runs when the user presses
**Cancel** in Preferences.

### Startup order

The editor fonts are loaded in `EditorImguiInit()`, which runs **before** `PreferencesManager`
exists. `AppearanceModule::LoadSavedTextScale()` therefore reads `textScale` straight from
`Appearance.json`, the same trick `ThemeModule::LoadSavedFontPreference()` uses for the font name.
The fonts come up at the right size on the first frame and no rebuild is needed.

### The backend backport

`ImGui_ImplVulkan_DestroyFontsTexture()` does not exist in 1.89.9. It was added to
`External/Imgui/backends/imgui_impl_vulkan.h/.cpp` as a small backport of the 1.90 API (marked
`// Polyphase:`): it removes the font descriptor set and destroys the font image, view and memory.
The caller must guarantee the device is idle. If ImGui is ever upgraded to 1.90+, drop the backport
and use the upstream function.

## Rules for editor UI code

**Never cache an `ImFont*` across frames.** `io.Fonts->Clear()` frees every font. The current
holders are handled explicitly:

- `sTerminalFont` is reassigned by `LoadEditorFonts()`; the Terminal panel re-fetches it each frame
  through `GetEditorTerminalFont()`.
- Zep (the built-in script editor) caches an `ImFont*` per text type inside its display.
  `ScriptEditorWindow::OnEditorFontsRebuilt()` replaces them and scales their pixel height.

If you add another long-lived font pointer, refresh it from the same place.

**Avoid hard-coded pixel sizes for anything that contains text.** A `SetNextItemWidth(80)` that fits
"Scene" at 1.0× clips it at 1.5×. Prefer sizes derived from the font:

```cpp
ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);        // scales with the text
ImGui::SetNextItemWidth(80.0f * GetEditorTextScale());       // or scale a tuned constant
ImGui::CalcTextSize("Longest Label").x + padding             // or measure the content
```

The main toolbar combos and the fixed-size Preferences window already do this. Layout that uses
`ImGui::GetFontSize()`, `GetFrameHeight()` or `CalcTextSize()` adapts on its own.

**Adding a font** means adding it inside `LoadEditorFonts()` with its size multiplied by
`textScale`, so it is rebuilt with the others. A font added anywhere else disappears on the first
rebuild.

## Testing checklist

1. Apply 1.5: text and icons are sharp (not stretched), the Terminal panel and script editor scale
   too, no Vulkan validation errors.
2. Apply several different values in a row, then Reset: no crash or leak (exercises the descriptor
   free path repeatedly).
3. Restart: the editor comes up at the saved scale on the first frame.
4. Preferences → Cancel does not trigger a rebuild.
5. Interface Scale Reset gives exactly `1.00` on Windows / Linux and the display scale on a Retina
   Mac, from both Preferences and the View menu.
6. Repeat 1 and 2 on macOS (MoltenVK) and Linux.
