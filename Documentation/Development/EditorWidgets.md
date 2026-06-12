# Editor Widgets

Shared, theme-aware ImGui widgets used across the Polyphase editor. Lives in `Engine/Source/Editor/EditorWidgets.{h,cpp}`. Everything in this header is `#if EDITOR`-gated — runtime code never sees it.

When you need any of the affordances below in an editor panel, **use the widget**. Do not hand-roll the equivalent — the widgets keep behavior, theming, drag-drop handling, and asset-system integration consistent across the whole editor.

---

## `Polyphase::Checkbox(label, bool*)`

Drop-in replacement for `ImGui::Checkbox` that constrains the *visual* box size (`gCheckboxSize`, default 16px) without changing the row's *layout* height. The label baseline lines up with adjacent buttons / inputs / combos no matter what the theme's `FramePadding` is.

```cpp
bool active = node->IsActive();
if (Polyphase::Checkbox("Active", &active))
{
    node->SetActive(active);
}
```

Themes can override `Polyphase::gCheckboxSize` at theme-apply time — see `Preferences/Appearance/Theme/EditorTheme.cpp` and the CSS parser.

---

## `Polyphase::AssetRefPicker(...)` — the unified asset picker

One widget for every "drop an asset here / pick / clear" affordance in the editor. Renders, left-to-right:

```
[Browse]  [autocomplete InputText]  [X clear]  [Inspect]  [Reveal]
```

It accepts a drag-drop payload from the Asset Panel, filters by asset type (with Material polymorphism — `Material` accepts `MaterialBase` / `MaterialInstance` / `MaterialLite`), and undo-wraps the assignment through `ActionManager` when given an undo context.

### Signature

```cpp
namespace Polyphase
{
    enum AssetPickerFlags_ : uint32_t
    {
        AssetPickerFlags_None           = 0,
        AssetPickerFlags_NoAutocomplete = 1 << 0, // labelled button instead of InputText — compact table cells
        AssetPickerFlags_NoBrowse       = 1 << 1, // hide the Download/Upload/Info button + hover-delete shortcut
        AssetPickerFlags_NoInspect      = 1 << 2,
        AssetPickerFlags_NoReveal       = 1 << 3,
        AssetPickerFlags_NoDragDrop     = 1 << 4,
        AssetPickerFlags_NoColorTint    = 1 << 5,
    };
    using AssetPickerFlags = uint32_t;

    struct AssetPickerUndoCtx
    {
        Object*           mOwner     = nullptr;
        PropertyOwnerType mOwnerType = PropertyOwnerType::Count;
        const char*       mPropName  = nullptr;
        uint32_t          mIndex     = 0;
    };

    bool AssetRefPicker(const char*               label,
                        AssetRef&                 ref,
                        TypeId                    typeFilter = 0,
                        AssetPickerFlags          flags      = 0,
                        const AssetPickerUndoCtx* undo       = nullptr);
}
```

Returns `true` on the frame the `AssetRef` changes — handy for triggering a `MarkDirty()` callback.

### Typical patterns

**Bare slot in a custom panel** (no Property, no undo):

```cpp
Polyphase::AssetRefPicker("Brush Mask", mgr->mOptions.mBrushMask,
                          Texture::GetStaticType());
```

**Compact cell in a table** (drop + X only, no buttons or autocomplete):

```cpp
const Polyphase::AssetPickerFlags kCompact =
    Polyphase::AssetPickerFlags_NoAutocomplete |
    Polyphase::AssetPickerFlags_NoBrowse       |
    Polyphase::AssetPickerFlags_NoInspect      |
    Polyphase::AssetPickerFlags_NoReveal;

ImGui::SetNextItemWidth(-32.0f);
if (Polyphase::AssetRefPicker("##spr", row.mSprite,
                              Texture::GetStaticType(), kCompact))
{
    MarkDirty(map);
}
```

**Property-backed (rare — the inspector does this for you)**: route through `DrawAssetProperty` in `EditorImgui.cpp`. It builds an `AssetPickerUndoCtx` from the `Property`+`owner` and delegates to the widget.

### Width

The widget honors `ImGui::SetNextItemWidth(...)` / `ImGui::PushItemWidth(...)` for the central element (InputText in normal mode, labelled button in `NoAutocomplete` mode). Buttons after it are fixed-size.

### Undo wrapping

- `undo == nullptr` → the widget mutates the `AssetRef` directly. Use this for non-Property panels (paint brush mask, table-cell pickers, etc.).
- `undo != nullptr` with `mOwnerType ∈ {Node, Asset}` → assignment routes through `ActionManager::EXE_EditPropertyOnSelection`, so `Ctrl+Z` restores the previous reference. The inspector path uses this.
- `undo != nullptr` with `mOwnerType == Count` → direct mutation, no undo. Used by the synth-Property paths (e.g., the Material shader-parameter texture slot before its migration).

---

## Anti-patterns — don't roll your own

If you find yourself writing this:

```cpp
// DON'T do this anymore.
ImGui::Button("Drop Texture Here##slot", ImVec2(-32.0f, 0.0f));
if (ImGui::BeginDragDropTarget())
{
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
    {
        AssetStub* stub = *(AssetStub**)p->Data;
        if (stub && stub->mType == Texture::GetStaticType())
        {
            ref = LoadAsset(stub->mName);
        }
    }
    ImGui::EndDragDropTarget();
}
ImGui::SameLine();
if (ImGui::SmallButton("X##clr")) ref = nullptr;
```

…stop. Use `Polyphase::AssetRefPicker` instead. You'll get the same drag-drop **plus** an X clear button that's actually consistent with every other asset slot in the editor, plus Inspect / Reveal / autocomplete / Material polymorphism / asset-color tint / undo wrapping when available — all the affordances users already know from the property inspector.

The same applies to material-instance texture parameters, paint settings, custom panel asset slots in addons, etc. — anywhere an `AssetRef`-shaped value needs to be picked, dropped, or cleared.

---

## Where the implementation lives

| File | Role |
|------|------|
| `Engine/Source/Editor/EditorWidgets.h` | Public widget API: `Checkbox`, `AssetRefPicker`, flags, undo context. |
| `Engine/Source/Editor/EditorWidgets.cpp` | Implementations. |
| `Engine/Source/Editor/EditorWidgetsInternal.h` | Private bridge: declares `PolyphaseEditorInternal::DrawAutocompleteDropdown` and `AssignAssetToProperty` (defined in `EditorImgui.cpp`) so the widget can reuse them without duplicating ~300 lines of dropdown logic. Do not include from outside the Editor target. |
| `Engine/Source/Editor/EditorImgui.cpp::DrawAssetProperty` | Thin Property→widget adapter. The inspector's asset rows route through this. |
