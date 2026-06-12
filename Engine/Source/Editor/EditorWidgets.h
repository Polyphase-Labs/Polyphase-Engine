#pragma once

#if EDITOR

#include "EngineTypes.h"   // PropertyOwnerType, TypeId
#include "imgui.h"

#include <cstdint>

class AssetRef;
class Object;

namespace Polyphase
{
    // Default visual size (in pixels) for editor checkboxes. Matches the Scene
    // Inspector "Active" toggle button so checkboxes feel consistent across the
    // editor regardless of the active theme's FramePadding. Themes may override
    // this at theme-apply time (see EditorTheme.cpp / CssThemeParser.cpp).
    extern float gCheckboxSize;

    // Drop-in replacement for ImGui::Checkbox that constrains the frame height
    // to gCheckboxSize for the duration of the call. All theming (checkmark
    // color, hover, active, rounding) is preserved.
    bool Checkbox(const char* label, bool* v);

    // ---------------------------------------------------------------------
    // Unified asset-reference picker
    // ---------------------------------------------------------------------
    //
    // One widget for every "drop / pick / clear an Asset here" affordance in
    // the editor. Renders, left-to-right:
    //
    //   [Browse]  [autocomplete InputText]  [X clear]  [Inspect]  [Reveal]
    //
    // The hand-rolled drop targets that used to live in InputPromptMap,
    // PaintManager, MaterialEditor, etc., each lost or kept different subsets
    // of those buttons — this widget unifies them. Each affordance can be
    // suppressed via a flag for compact table cells.
    //
    // `ref` is read AND written by the widget. When `undo != nullptr` and the
    // owner is a Node or Asset, the assignment is routed through
    // ActionManager so Ctrl+Z restores the previous reference; otherwise the
    // AssetRef is mutated directly. Callers that have no Property/owner
    // (custom panels) simply pass `undo = nullptr`.
    //
    // `typeFilter` is a TypeId — pass `Texture::GetStaticType()` to constrain
    // the picker to textures, `0` to allow any asset. Material polymorphism
    // (Material accepts MaterialBase / MaterialInstance / MaterialLite) is
    // handled internally.
    //
    // Returns true on the frame the AssetRef changed.
    enum AssetPickerFlags_ : uint32_t
    {
        AssetPickerFlags_None           = 0,
        AssetPickerFlags_NoAutocomplete = 1 << 0, // hide InputText + dropdown — use a labelled button slot instead
        AssetPickerFlags_NoBrowse       = 1 << 1, // hide Download/Upload/Info modifier button + hover-delete shortcut
        AssetPickerFlags_NoInspect      = 1 << 2,
        AssetPickerFlags_NoReveal       = 1 << 3,
        AssetPickerFlags_NoDragDrop     = 1 << 4,
        AssetPickerFlags_NoColorTint    = 1 << 5,
    };
    using AssetPickerFlags = uint32_t;

    // Optional undo wrapping. When supplied, the widget routes the assignment
    // through ActionManager::EXE_EditPropertyOnSelection — mirrors what the
    // inspector's DrawAssetProperty does for Node / Asset owners. Hand-rolled
    // call sites (no Property context) pass nullptr and the widget mutates
    // the AssetRef directly.
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

#endif
