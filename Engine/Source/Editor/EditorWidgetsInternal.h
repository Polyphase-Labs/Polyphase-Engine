#pragma once

#if EDITOR

// Internal bridge between EditorImgui.cpp (the canonical source of the
// inspector's asset-row primitives) and EditorWidgets.cpp (where the public
// Polyphase::AssetRefPicker widget lives). The two helpers below were once
// `static` inside EditorImgui.cpp; they're now non-static so the unified
// widget can reuse them without duplicating ~300 lines of dropdown logic.
//
// This header is private to the Editor target — do NOT include it from
// engine, runtime, or addon code.

#include "EngineTypes.h"   // PropertyOwnerType enum
#include "imgui.h"

#include <string>
#include <vector>

class Property;
class Object;
class Asset;

namespace PolyphaseEditorInternal
{
    // Stateless filter signature used by every existing autocomplete caller
    // (asset name picker, script picker, asset-replace picker). The widget
    // used to be a template over the filter type; flattening to a function
    // pointer lets us share one definition across translation units without
    // pulling the implementation into a header.
    using AutocompleteFilter = bool (*)(const std::string& suggestion,
                                        const std::string& input);

    // Defined in EditorImgui.cpp.
    bool DrawAutocompleteDropdown(const char*                     dropdownId,
                                  std::string&                    inputText,
                                  const std::vector<std::string>& suggestions,
                                  AutocompleteFilter              filterFunc,
                                  bool                            forceActive    = false,
                                  ImGuiID                         overrideInputId = 0,
                                  ImVec2                          overrideRectMin = ImVec2(0, 0),
                                  ImVec2                          overrideRectMax = ImVec2(0, 0));

    // Defined in EditorImgui.cpp. Encapsulates the type-filter check (with
    // Material polymorphism HACK) and the undo-wrapping vs direct-write
    // branch for Property-backed asset assignment.
    void AssignAssetToProperty(Object*           owner,
                               PropertyOwnerType ownerType,
                               Property&         prop,
                               uint32_t          index,
                               Asset*            newAsset);
}

#endif // EDITOR
