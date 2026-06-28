#if EDITOR

#define IMGUI_DEFINE_MATH_OPERATORS
#include "EditorWidgets.h"
#include "EditorWidgetsInternal.h"

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include "AssetManager.h"
#include "AssetRef.h"
#include "ActionManager.h"
#include "EditorConstants.h"
#include "EditorIcons.h"
#include "EditorState.h"
#include "InputDevices.h"
#include "Hotkeys/EditorHotkeyMap.h"
#include "Hotkeys/EditorAction.h"
#include "Property.h"

#include "Assets/Material.h"
#include "Assets/MaterialBase.h"
#include "Assets/MaterialInstance.h"
#include "Assets/MaterialLite.h"

#include "glm/glm.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace Polyphase
{
    float gCheckboxSize = 16.0f;

    // Re-implementation of ImGui::Checkbox that decouples the *visual* box size
    // (gCheckboxSize) from the *layout* height (theme frame height). The box is
    // drawn smaller and centered vertically within a row that is the standard
    // frame height, so the checkbox sits flush with adjacent buttons / inputs /
    // combos and the label baseline lines up with their text.
    bool Checkbox(const char* label, bool* v)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *ImGui::GetCurrentContext();
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const float layout_h = ImGui::GetFrameHeight();
        const float square_sz = ImMin(gCheckboxSize, layout_h);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(
            pos,
            pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
                         layout_h));
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed)
        {
            *v = !(*v);
            ImGui::MarkItemEdited(id);
        }

        const float box_y_offset = (layout_h - square_sz) * 0.5f;
        const ImRect check_bb(
            ImVec2(pos.x, pos.y + box_y_offset),
            ImVec2(pos.x + square_sz, pos.y + box_y_offset + square_sz));

        ImGui::RenderNavHighlight(total_bb, id);
        ImGui::RenderFrame(
            check_bb.Min, check_bb.Max,
            ImGui::GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive
                                                 : hovered ? ImGuiCol_FrameBgHovered
                                                           : ImGuiCol_FrameBg),
            true, style.FrameRounding);

        ImU32 check_col = ImGui::GetColorU32(ImGuiCol_CheckMark);
        bool mixed_value = (g.LastItemData.InFlags & ImGuiItemFlags_MixedValue) != 0;
        if (mixed_value)
        {
            ImVec2 pad(ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)),
                       ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)));
            window->DrawList->AddRectFilled(check_bb.Min + pad, check_bb.Max - pad, check_col, style.FrameRounding);
        }
        else if (*v)
        {
            const float pad = ImMax(1.0f, IM_FLOOR(square_sz / 6.0f));
            ImGui::RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), check_col, square_sz - pad * 2.0f);
        }

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y);
        if (g.LogEnabled)
            ImGui::LogRenderedText(&label_pos, mixed_value ? "[~]" : *v ? "[x]" : "[ ]");
        if (label_size.x > 0.0f)
            ImGui::RenderText(label_pos, label);

        return pressed;
    }

    // ---------------------------------------------------------------------
    // AssetRefPicker — unified asset-reference picker widget
    // ---------------------------------------------------------------------
    //
    // This is the canonical "drop / pick / clear an Asset here" widget. Both
    // the inspector's DrawAssetProperty (Property-backed) and every
    // hand-rolled asset slot (InputPromptMap, PaintManager brush mask,
    // Material shader-parameter texture, paint-instance mesh) call into
    // this function. Behavioral parity with the pre-refactor DrawAssetProperty
    // body is required — Inspect / Reveal / Browse / autocomplete /
    // hover-delete / Material polymorphism all live here now.

    namespace
    {
        // Case-insensitive substring filter used by the autocomplete dropdown.
        // Lambda is intentionally a stateless free function so it converts to
        // PolyphaseEditorInternal::AutocompleteFilter (a function pointer).
        bool AssetAutocompleteFilter(const std::string& suggestion, const std::string& input)
        {
            if (input.empty())
                return true;
            std::string lowerSuggestion = suggestion;
            std::string lowerInput      = input;
            std::transform(lowerSuggestion.begin(), lowerSuggestion.end(), lowerSuggestion.begin(), ::tolower);
            std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
            return lowerSuggestion.find(lowerInput) != std::string::npos;
        }

        // Apply an assignment to the AssetRef respecting the optional undo
        // context. Mirrors the inspector's old branch:
        //   - PropertyOwnerType ∈ {Node, Asset} → undo-wrapped via ActionManager
        //   - everything else                  → direct AssetRef mutation
        void ApplyAssetAssignment(AssetRef& ref,
                                  Asset* newAsset,
                                  const AssetPickerUndoCtx* undo)
        {
            if (undo != nullptr &&
                undo->mPropName != nullptr &&
                (undo->mOwnerType == PropertyOwnerType::Node ||
                 undo->mOwnerType == PropertyOwnerType::Asset))
            {
                ActionManager::Get()->EXE_EditPropertyOnSelection(
                    undo->mOwner, undo->mOwnerType, undo->mPropName, undo->mIndex, newAsset);
            }
            else
            {
                ref = newAsset;
            }
        }

        // Type-filter check shared with AssignAssetToProperty. Pulled inline
        // because the hand-rolled path doesn't have a Property to consult.
        bool AssetMatchesFilter(Asset* asset, TypeId filter)
        {
            if (asset == nullptr || filter == 0)
                return true;
            TypeId t = asset->GetType();
            if (t == filter)
                return true;
            // HACK: Material is an abstract base — accept its concrete
            // subclasses too. Mirrors AssignAssetToProperty's compatibility.
            if (filter == Material::GetStaticType() &&
                (t == MaterialBase::GetStaticType() ||
                 t == MaterialInstance::GetStaticType() ||
                 t == MaterialLite::GetStaticType()))
            {
                return true;
            }
            return false;
        }
    }

    bool AssetRefPicker(const char*               label,
                        AssetRef&                 ref,
                        TypeId                    typeFilter,
                        AssetPickerFlags          flags,
                        const AssetPickerUndoCtx* undo)
    {
        Asset*       asset    = ref.Get();
        Asset* const oldAsset = asset;

        const bool noAutocomplete = (flags & AssetPickerFlags_NoAutocomplete) != 0;
        const bool noBrowse       = (flags & AssetPickerFlags_NoBrowse)       != 0;
        const bool noInspect      = (flags & AssetPickerFlags_NoInspect)      != 0;
        const bool noReveal       = (flags & AssetPickerFlags_NoReveal)       != 0;
        const bool noDragDrop     = (flags & AssetPickerFlags_NoDragDrop)     != 0;
        const bool noColorTint    = (flags & AssetPickerFlags_NoColorTint)    != 0;

        const bool useAssetColor = (!noColorTint && typeFilter != 0);
        if (useAssetColor)
        {
            glm::vec4 assetColor = AssetManager::Get()->GetEditorAssetColor(typeFilter);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(assetColor.r, assetColor.g, assetColor.b, assetColor.a));
        }

        // Push the label as the ImGui ID so every cell in a vector property
        // (or every row in a table) gets a unique identity. Replaces the
        // pre-refactor owner-pointer-keyed sTempStrings map.
        ImGui::PushID(label);

        // ---- Browse / modifier-branch button (Download/Upload/Info) ----
        if (!noBrowse)
        {
            if (IsControlDown())
            {
                if (ImGui::Button(ICON_IC_BASELINE_UPLOAD) && asset)
                {
                    GetEditorState()->BrowseToAsset(asset->GetName());
                }
            }
            else if (IsAltDown())
            {
                if (ImGui::Button(ICON_INFO) && asset)
                {
                    GetEditorState()->InspectObject(asset);
                }
            }
            else
            {
                if (ImGui::Button(ICON_IC_BASELINE_DOWNLOAD))
                {
                    Asset* selAsset = GetEditorState()->GetSelectedAsset();
                    if (AssetMatchesFilter(selAsset, typeFilter))
                    {
                        ApplyAssetAssignment(ref, selAsset, undo);
                    }
                }

                // Hover-delete shortcut: hovering the Browse button + pressing
                // Delete clears the slot. Preserves the inspector's prior
                // behaviour. Gated on !noBrowse since the button is the hover
                // target.
                if (asset != nullptr &&
                    ImGui::IsItemHovered() &&
                    EditorHotkeyMap::Get()->IsActionJustTriggered(EditorAction::Edit_DeleteSelected))
                {
                    ApplyAssetAssignment(ref, nullptr, undo);
                }
            }

            ImGui::SameLine();
        }

        // ---- Central element: autocomplete InputText OR labelled button ----
        ImGuiID inputId        = 0;
        ImVec2  inputRectMin   = ImVec2(0, 0);
        ImVec2  inputRectMax   = ImVec2(0, 0);
        bool    isInputFocused = false;
        bool    isInputActivated = false;
        bool    textActive     = false;
        std::string* sTempStringPtr = nullptr;

        if (noAutocomplete)
        {
            // Compact mode for tight cells (InputPromptMap rows, brush-mask
            // drop zone, etc.). The central element becomes a labelled button
            // sized to fill the remaining row width. Drag-drop, X clear,
            // Inspect, and Reveal still hang off it.
            const std::string assetLabel = asset ? asset->GetName() : std::string("[ none ]");
            ImGui::Button(assetLabel.c_str(), ImVec2(ImGui::CalcItemWidth(), 0.0f));
        }
        else
        {
            // ImGui-ID-keyed temp string buffer — replaces the old
            // owner-pointer-keyed sTempStrings map that collided for
            // owner==nullptr (synth-Property sites).
            static std::unordered_map<ImGuiID, std::string> sTempStrings;
            ImGuiID rowId = ImGui::GetID("##AssetNameStr");
            sTempStringPtr = &sTempStrings[rowId];
            std::string& sTempString = *sTempStringPtr;

            ImGuiInputTextFlags textFlags = ImGuiInputTextFlags_None;
            if (ImGui::IsItemFocused() || ImGui::IsItemActive())
                textFlags |= ImGuiInputTextFlags_CharsNoBlank;

            textActive       = ImGui::InputText("##AssetNameStr", &sTempString, textFlags);
            isInputFocused   = ImGui::IsItemFocused();
            isInputActivated = ImGui::IsItemActivated();
            inputId          = ImGui::GetItemID();
            inputRectMin     = ImGui::GetItemRectMin();
            inputRectMax     = ImGui::GetItemRectMax();

            // Reset temp string when not actively interacting (or when first
            // activating). The mouse-over-dropdown check preserves the filter
            // text while the user is clicking on an item.
            ImVec2 inSize = ImVec2(inputRectMax.x - inputRectMin.x,
                                   inputRectMax.y - inputRectMin.y);
            const float ddItemH = ImGui::GetTextLineHeightWithSpacing();
            const float ddMaxH  = ddItemH * 4 + ImGui::GetStyle().WindowPadding.y * 2;
            ImVec2 ddMin = ImVec2(inputRectMin.x, inputRectMin.y + inSize.y);
            ImVec2 ddMax = ImVec2(inputRectMax.x, inputRectMin.y + inSize.y + ddMaxH);
            ImVec2 mouse = ImGui::GetIO().MousePos;
            bool   mouseOverDD = mouse.x >= ddMin.x && mouse.x <= ddMax.x &&
                                 mouse.y >= ddMin.y && mouse.y <= ddMax.y;
            if (isInputActivated || (!isInputFocused && !mouseOverDD))
            {
                sTempString = asset ? asset->GetName() : "";
            }
        }

        // ---- Drag-drop accept on the central element ----
        if (!noDragDrop && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DRAGDROP_ASSET))
            {
                AssetStub* droppedStub = *(AssetStub**)payload->Data;
                if (droppedStub != nullptr)
                {
                    Asset* droppedAsset = LoadAsset(droppedStub->mName);
                    if (AssetMatchesFilter(droppedAsset, typeFilter))
                    {
                        ApplyAssetAssignment(ref, droppedAsset, undo);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // ---- X clear button (always present — this is the entire point) ----
        ImGui::SameLine(0.0f, 2.0f);
        if (ImGui::Button(ICON_DASHICONS_NO_ALT "##AssetClear"))
        {
            ApplyAssetAssignment(ref, nullptr, undo);
        }

        // ---- Inspect and Reveal — only when asset is assigned ----
        if (asset != nullptr && !noInspect)
        {
            ImGui::SameLine(0.0f, 2.0f);
            if (ImGui::Button(ICON_INFO "##Inspect"))
            {
                GetEditorState()->InspectObject(asset);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Inspect asset properties");
        }

        if (asset != nullptr && !noReveal)
        {
            ImGui::SameLine(0.0f, 2.0f);
            if (ImGui::Button(ICON_MDI_TARGET "##Reveal"))
            {
                GetEditorState()->BrowseToAsset(asset->GetName());
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reveal in Asset Panel");
        }

        // ---- Autocomplete dropdown + text-deactivated assignment ----
        if (!noAutocomplete)
        {
            // Refresh the suggestions list lazily — typeFilter rarely changes
            // and asset additions are infrequent at editor scale.
            static std::vector<std::string> assetSuggestions;
            static TypeId lastFilter      = TypeId(~0u);
            static double lastUpdateTime  = -1.0;
            const double currentTime      = ImGui::GetTime();
            if (typeFilter != lastFilter || currentTime - lastUpdateTime > 2.0)
            {
                assetSuggestions.clear();
                const auto& assetMap = AssetManager::Get()->GetAssetMap();
                for (const auto& pair : assetMap)
                {
                    AssetStub* stub = pair.second;
                    if (stub == nullptr)
                        continue;
                    bool typeMatches = (typeFilter == 0) || (stub->mType == typeFilter);
                    if (!typeMatches &&
                        typeFilter == Material::GetStaticType() &&
                        (stub->mType == MaterialBase::GetStaticType() ||
                         stub->mType == MaterialInstance::GetStaticType() ||
                         stub->mType == MaterialLite::GetStaticType()))
                    {
                        typeMatches = true;
                    }
                    if (typeMatches)
                        assetSuggestions.push_back(stub->mName);
                }
                lastFilter     = typeFilter;
                lastUpdateTime = currentTime;
            }

            std::string& sTempString = *sTempStringPtr;
            bool selectionMade = PolyphaseEditorInternal::DrawAutocompleteDropdown(
                "AssetAutocomplete",
                sTempString,
                assetSuggestions,
                &AssetAutocompleteFilter,
                isInputActivated || textActive || isInputFocused,
                inputId,
                inputRectMin,
                inputRectMax);

            if (selectionMade || ImGui::IsItemDeactivatedAfterEdit())
            {
                if (sTempString == "null" || sTempString == "NULL" || sTempString == "Null")
                {
                    ApplyAssetAssignment(ref, nullptr, undo);
                }
                else
                {
                    Asset* newAsset = LoadAsset(sTempString);
                    if (AssetMatchesFilter(newAsset, typeFilter))
                    {
                        ApplyAssetAssignment(ref, newAsset, undo);
                    }
                }
            }
        }

        ImGui::PopID();

        if (useAssetColor)
        {
            ImGui::PopStyleColor();
        }

        return ref.Get() != oldAsset;
    }
}

#endif
