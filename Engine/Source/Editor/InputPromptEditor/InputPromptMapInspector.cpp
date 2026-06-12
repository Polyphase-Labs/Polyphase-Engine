#if EDITOR

#include "InputPromptMapInspector.h"
#include "Input/InputPromptResolver.h"
#include "Input/InputPath.h"

#include "Assets/InputPromptMap.h"
#include "Assets/Texture.h"
#include "Assets/Font.h"
#include "AssetManager.h"
#include "Asset.h"
#include "Utilities.h"

#include "Input/InputMap.h"
#include "Input/InputTypes.h"
#include "Input/InputConstants.h"
#include "EditorIcons.h"

#include "imgui.h"
#include "EditorConstants.h"   // DRAGDROP_ASSET
#include "EditorWidgets.h"     // Polyphase::AssetRefPicker

#include <cstdlib>             // strtoul

// ---------------------------------------------------------------------------
// Path-picker presets
// ---------------------------------------------------------------------------
//
// Every entry below names a (sourceType, code, axisDir) that can be turned
// into a canonical path via MakeInputPath(). The arrays are deliberately
// kept verbose rather than enumerated programmatically — the POLYPHASE_KEY_*
// values are an unordered enum, not a contiguous range, and listing them
// explicitly is what gives the picker a stable, designer-friendly ordering.

struct PathPreset
{
    InputSourceType type;
    int32_t         code;
    AxisDirection   axis;
};

static const PathPreset kKeyboardPresets[] = {
    { InputSourceType::Keyboard, POLYPHASE_KEY_A, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_B, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_C, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_D, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_E, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_G, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_H, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_I, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_J, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_K, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_L, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_M, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_N, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_O, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_P, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_Q, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_R, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_S, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_T, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_U, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_V, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_W, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_X, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_Y, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_Z, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_0, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_1, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_2, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_3, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_4, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_5, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_6, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_7, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_8, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_9, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_SPACE,     AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_ENTER,     AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_BACKSPACE, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_TAB,       AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_ESCAPE,    AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_SHIFT_L,   AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_CONTROL_L, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_ALT_L,     AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_UP,        AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_DOWN,      AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_LEFT,      AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_RIGHT,     AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F1,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F2,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F3,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F4,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F5,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F6,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F7,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F8,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F9,  AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F10, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F11, AxisDirection::Positive },
    { InputSourceType::Keyboard, POLYPHASE_KEY_F12, AxisDirection::Positive },
};

static const PathPreset kMousePresets[] = {
    { InputSourceType::MouseButton, MOUSE_LEFT,   AxisDirection::Positive },
    { InputSourceType::MouseButton, MOUSE_RIGHT,  AxisDirection::Positive },
    { InputSourceType::MouseButton, MOUSE_MIDDLE, AxisDirection::Positive },
    { InputSourceType::MouseButton, MOUSE_X1,     AxisDirection::Positive },
    { InputSourceType::MouseButton, MOUSE_X2,     AxisDirection::Positive },
};

// Synthesize a path for a preset by routing through MakeInputPath so the
// picker output is byte-identical to what Capture produces.
static std::string PathFromPreset(const PathPreset& p)
{
    InputActionBinding b;
    b.sourceType = p.type;
    b.code = p.code;
    b.axisDirection = p.axis;
    return MakeInputPath(b);
}

// Draws an ImGui::Selectable that previews the canonical path; clicking it
// returns true so the caller can commit the chosen path back to the entry.
static bool DrawPresetRow(const PathPreset& preset, std::string& outPath)
{
    const std::string path = PathFromPreset(preset);
    if (ImGui::Selectable(path.c_str()))
    {
        outPath = path;
        return true;
    }
    return false;
}

InputPromptMapInspector* InputPromptMapInspector::sInstance = nullptr;

InputPromptMapInspector* InputPromptMapInspector::Get()
{
    if (!sInstance)
        sInstance = new InputPromptMapInspector();
    return sInstance;
}

void InputPromptMapInspector::Open(InputPromptMap* map)
{
    if (mTarget != map)
    {
        // Reset per-asset working state when retargeting; persistent UI prefs
        // (filter, test-device override) intentionally carry across.
        mSelectedEntry = -1;
        mCaptureRowIndex = -1;
    }
    mTarget = map;
    mIsOpen = (map != nullptr);
}

void InputPromptMapInspector::Close()
{
    mIsOpen = false;
    // Drop the test-device override so it doesn't leak into PIE or persist
    // after the editor window is closed.
    if (PlayerInputSystem* pis = PlayerInputSystem::Get())
        pis->ClearForcedDevice();
    mTestDeviceKind = -1;
}

void OpenInputPromptMapForEditing(InputPromptMap* map)
{
    InputPromptMapInspector::Get()->Open(map);
}

void InputPromptMapInspector::MarkDirty(InputPromptMap* map)
{
    if (map)
    {
        map->SetDirtyFlag();
        map->RebuildIndex();
    }
    if (InputPromptResolver* r = InputPromptResolver::Get())
        r->Invalidate();
}

void InputPromptMapInspector::AddEntry(InputPromptMap* map)
{
    if (!map) return;
    InputPromptEntry e;
    map->GetEntries().push_back(e);
    mSelectedEntry = int32_t(map->GetEntries().size()) - 1;
    MarkDirty(map);
}

void InputPromptMapInspector::DuplicateEntry(InputPromptMap* map, int32_t idx)
{
    if (!map) return;
    auto& entries = map->GetEntries();
    if (idx < 0 || idx >= (int32_t)entries.size()) return;
    entries.insert(entries.begin() + idx + 1, entries[idx]);
    mSelectedEntry = idx + 1;
    MarkDirty(map);
}

void InputPromptMapInspector::DeleteEntry(InputPromptMap* map, int32_t idx)
{
    if (!map) return;
    auto& entries = map->GetEntries();
    if (idx < 0 || idx >= (int32_t)entries.size()) return;
    entries.erase(entries.begin() + idx);
    if (mSelectedEntry >= (int32_t)entries.size())
        mSelectedEntry = (int32_t)entries.size() - 1;
    MarkDirty(map);
}

void InputPromptMapInspector::MoveEntry(InputPromptMap* map, int32_t idx, int32_t delta)
{
    if (!map) return;
    auto& entries = map->GetEntries();
    int32_t target = idx + delta;
    if (idx < 0 || idx >= (int32_t)entries.size()) return;
    if (target < 0 || target >= (int32_t)entries.size()) return;
    std::swap(entries[idx], entries[target]);
    mSelectedEntry = target;
    MarkDirty(map);
}

void InputPromptMapInspector::DrawToolbar(InputPromptMap* map)
{
    if (ImGui::Button("+ Add Entry")) AddEntry(map);
    ImGui::SameLine();
    bool hasSel = mSelectedEntry >= 0 && map && mSelectedEntry < (int32_t)map->GetEntries().size();
    ImGui::BeginDisabled(!hasSel);
    if (ImGui::Button("Duplicate")) DuplicateEntry(map, mSelectedEntry);
    ImGui::SameLine();
    if (ImGui::Button("Delete"))    DeleteEntry(map, mSelectedEntry);
    ImGui::SameLine();
    if (ImGui::Button("Up"))        MoveEntry(map, mSelectedEntry, -1);
    ImGui::SameLine();
    if (ImGui::Button("Down"))      MoveEntry(map, mSelectedEntry, +1);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::TextUnformatted("Test Device:");
    ImGui::SameLine();
    const char* testKinds[] = { "Auto", "Keyboard", "Mouse", "Gamepad" };
    int kindIdx = mTestDeviceKind + 1;
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("##TestKind", &kindIdx, testKinds, IM_ARRAYSIZE(testKinds)))
        mTestDeviceKind = kindIdx - 1;
    if (mTestDeviceKind == 2)
    {
        ImGui::SameLine();
        const char* gpTypes[] = { "Standard", "GameCube", "Wiimote", "WiiClassic", "DualShock4", "DualSense" };
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("##TestGpType", &mTestGamepadType, gpTypes, IM_ARRAYSIZE(gpTypes));
    }

    // Apply / clear the override on PlayerInputSystem every frame. We re-apply
    // unconditionally rather than diffing because SetForcedDevice short-circuits
    // when the descriptor hasn't actually changed, so the cost is one memcmp
    // per frame and any other code that might have cleared the override
    // (e.g. PIE start) gets corrected on the next inspector frame.
    if (PlayerInputSystem* pis = PlayerInputSystem::Get())
    {
        if (mTestDeviceKind < 0)
        {
            pis->ClearForcedDevice();
        }
        else
        {
            InputDeviceDescriptor d;
            switch (mTestDeviceKind)
            {
            case 0: d.kind = InputDeviceKind::Keyboard;                   break;
            case 1: d.kind = InputDeviceKind::Mouse;                      break;
            case 2: d.kind = InputDeviceKind::Gamepad;
                    d.gamepadType = (GamepadType)mTestGamepadType;
                    d.gamepadIndex = 0;                                   break;
            default: pis->ClearForcedDevice(); return;
            }
            pis->SetForcedDevice(d);
        }

        // Status badge so it's obvious the override is live and which device
        // every widget in every open scene is currently rendering against.
        if (pis->HasForcedDevice())
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
            ImGui::TextUnformatted("(forced)");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Test Device is overriding PlayerInputSystem.\n"
                                  "Every InputActionPrompt widget shows this device's bindings.\n"
                                  "Set Test Device back to 'Auto' to resume real input.");
        }
    }
}

void InputPromptMapInspector::DrawFilterRow()
{
    const char* platforms[] = { "All", "Windows", "Linux", "Android", "GameCube", "Wii", "N3DS", "PSP" };
    int filterPlatformIdx = mFilterPlatform + 1;
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Platform##Filter", &filterPlatformIdx, platforms, IM_ARRAYSIZE(platforms)))
        mFilterPlatform = filterPlatformIdx - 1;

    ImGui::SameLine();
    const char* gpTypes[] = { "All", "Standard", "GameCube", "Wiimote", "WiiClassic", "DualShock4", "DualSense" };
    int filterGpIdx = mFilterGamepad + 1;
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Gamepad##Filter", &filterGpIdx, gpTypes, IM_ARRAYSIZE(gpTypes)))
        mFilterGamepad = filterGpIdx - 1;

    ImGui::SameLine();
    char buf[128];
    strncpy(buf, mFilterPath.c_str(), sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputText("Path##Filter", buf, sizeof(buf)))
        mFilterPath = buf;
}

static bool EntryPassesFilter(const InputPromptEntry& e,
                              int32_t filterPlatform, int32_t filterGamepad,
                              const std::string& filterPath)
{
    if (filterPlatform >= 0 && (int)e.mPlatform != filterPlatform)
        return false;
    if (filterGamepad >= 0 && (int)e.mGamepadType != filterGamepad)
        return false;
    if (!filterPath.empty() && e.mInputPath.find(filterPath) == std::string::npos)
        return false;
    return true;
}

void InputPromptMapInspector::DrawEntryTable(InputPromptMap* map)
{
    if (!map) return;
    auto& entries = map->GetEntries();

    const char* platforms[] = { "Any", "Windows", "Linux", "Android", "GameCube", "Wii", "N3DS", "PSP" };
    const char* gpTypes[]   = { "Any/N-A", "Standard", "GameCube", "Wiimote", "WiiClassic", "DualShock4", "DualSense" };
    const char* kinds[]     = { "Sprite", "Glyph", "Text" };

    // SizingStretchSame distributes any free pixels across Stretch columns
    // proportional to their weight, while Fixed columns keep their pixel size.
    // The Resizable flag exposes draggable dividers so the artist can also
    // hand-tune widths without us needing to guess every screen size.
    //
    // Theme-aware row coloring: the editor's table row background is the
    // same value as FrameBg in the current Polyphase theme, which makes
    // InputTexts / combos / the checkbox blend invisibly into their row.
    // Re-derive both row colors from WindowBg (which any sensible theme
    // keeps distinct from FrameBg) so widgets contrast naturally without
    // hardcoded values. The alt-row tint is just a 4% lift of each channel
    // for subtle stripe-readability.
    const ImVec4 wbg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const float kAltDelta = 0.015f;
    const ImVec4 wbgAlt(
        wbg.x + (wbg.x < 0.5f ? kAltDelta : -kAltDelta),
        wbg.y + (wbg.y < 0.5f ? kAltDelta : -kAltDelta),
        wbg.z + (wbg.z < 0.5f ? kAltDelta : -kAltDelta),
        wbg.w);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,    wbg);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, wbgAlt);

    if (ImGui::BeginTable("##InputPromptEntries", 8,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                          ImGuiTableFlags_SizingStretchSame |
                          ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_ScrollY))
    {
        // Stretch weights — Path gets the most because input paths vary in
        // length and house the most chrome (text field + capture + picker).
        ImGui::TableSetupColumn("#",           ImGuiTableColumnFlags_WidthFixed,    52.0f);
        ImGui::TableSetupColumn("Platform",    ImGuiTableColumnFlags_WidthStretch,  1.0f);
        ImGui::TableSetupColumn("Device",      ImGuiTableColumnFlags_WidthStretch,  1.4f);
        ImGui::TableSetupColumn("Path",        ImGuiTableColumnFlags_WidthStretch,  2.5f);
        ImGui::TableSetupColumn("Kind",        ImGuiTableColumnFlags_WidthFixed,    72.0f);
        ImGui::TableSetupColumn("Asset/Glyph", ImGuiTableColumnFlags_WidthStretch,  2.0f);
        ImGui::TableSetupColumn("Fallback",    ImGuiTableColumnFlags_WidthStretch,  1.0f);
        ImGui::TableSetupColumn("",            ImGuiTableColumnFlags_WidthFixed,    28.0f);
        ImGui::TableHeadersRow();

        for (int32_t i = 0; i < (int32_t)entries.size(); ++i)
        {
            InputPromptEntry& e = entries[i];
            if (!EntryPassesFilter(e, mFilterPlatform, mFilterGamepad, mFilterPath))
                continue;

            ImGui::PushID(i);
            ImGui::TableNextRow();

            // Selection cell. A plain checkbox next to the row number — only
            // one row can be selected at a time (single-select semantics), so
            // ticking another row's box clears this one. The checkbox is its
            // own unambiguous click target and its state is visible at a
            // glance, which the row-spanning Selectable approach couldn't
            // deliver on multi-line rows.
            ImGui::TableNextColumn();
            bool isSelected = (mSelectedEntry == i);
            if (ImGui::Checkbox("##sel", &isSelected))
            {
                mSelectedEntry = isSelected ? i : -1;
            }
            ImGui::SameLine();
            ImGui::Text("%d", i);

            // Platform combo
            ImGui::TableNextColumn();
            int pIdx = (e.mPlatform == Platform::Count) ? 0 : (int)e.mPlatform + 1;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##plat", &pIdx, platforms, IM_ARRAYSIZE(platforms)))
            {
                e.mPlatform = (pIdx == 0) ? Platform::Count : (Platform)(pIdx - 1);
                MarkDirty(map);
            }

            // Device combo
            ImGui::TableNextColumn();
            int gIdx = (e.mGamepadType == GamepadType::Count) ? 0 : (int)e.mGamepadType + 1;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##gp", &gIdx, gpTypes, IM_ARRAYSIZE(gpTypes)))
            {
                e.mGamepadType = (gIdx == 0) ? GamepadType::Count : (GamepadType)(gIdx - 1);
                MarkDirty(map);
            }

            // Path field + capture + manual picker. The two trailing buttons
            // reserve a fixed pixel slot at the right so the InputText scales
            // with the column instead of overflowing into the Kind cell.
            ImGui::TableNextColumn();
            {
                constexpr float kCaptureBtnWidth = 22.0f;
                constexpr float kPickerBtnWidth  = 22.0f;
                constexpr float kRightPadding    = 8.0f;
                // InputText reserves: capture + picker + interbutton spacing +
                // trailing right-side padding (visual gap before cell border).
                constexpr float kButtonsTotal    = kCaptureBtnWidth + kPickerBtnWidth +
                                                   8.0f /* spacing */ + kRightPadding;

                char pbuf[128];
                strncpy(pbuf, e.mInputPath.c_str(), sizeof(pbuf));
                pbuf[sizeof(pbuf) - 1] = 0;
                ImGui::SetNextItemWidth(-kButtonsTotal);
                if (ImGui::InputText("##path", pbuf, sizeof(pbuf)))
                {
                    e.mInputPath = pbuf;
                    MarkDirty(map);
                }

                ImGui::SameLine();
                // Single-glyph buttons keep the Path column readable at narrow
                // widths. "\xe2\x97\x8f" = U+25CF BLACK CIRCLE  ("record" icon)
                if (ImGui::Button(ICON_BOXICONS_EYEDROPPER_FILLED, ImVec2(kCaptureBtnWidth, 0.0f)))
                {
                    mCaptureRowIndex = i;
                    InputPromptMap* mapRef = map;
                    mCapture.Start([this, mapRef, i](const InputCaptureModal::Result& r) {
                        auto& entries = mapRef->GetEntries();
                        if (i < (int32_t)entries.size())
                        {
                            entries[i].mInputPath = r.path;
                            if (r.gamepadDetected)
                                entries[i].mGamepadType = r.gamepadType;
                            else if (r.binding.sourceType == InputSourceType::Keyboard ||
                                     r.binding.sourceType == InputSourceType::MouseButton)
                                entries[i].mGamepadType = GamepadType::Count;
                            MarkDirty(mapRef);
                        }
                    });
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Capture: press any input to fill this row");

                ImGui::SameLine();
                // "\xe2\x96\xbe" = U+25BE BLACK DOWN-POINTING SMALL TRIANGLE
                if (ImGui::Button(ICON_DASHICONS_ARROW_DOWN, ImVec2(kPickerBtnWidth, 0.0f)))
                    ImGui::OpenPopup("##pathPicker");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Pick: choose from a list of known paths");
                // Visual padding so the picker button doesn't sit flush against
                // the cell border (looks cramped, especially next to the Kind
                // column's combo arrow).
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(8.0f, 0.0f));

                if (ImGui::BeginPopup("##pathPicker"))
                {
                    std::string picked;
                    bool didPick = false;

                    if (ImGui::BeginMenu("Keyboard"))
                    {
                        for (const PathPreset& p : kKeyboardPresets)
                            if (DrawPresetRow(p, picked)) didPick = true;
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Mouse"))
                    {
                        for (const PathPreset& p : kMousePresets)
                            if (DrawPresetRow(p, picked)) didPick = true;
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Gamepad Button"))
                    {
                        for (int32_t b = 0; b < GAMEPAD_BUTTON_COUNT; ++b)
                        {
                            PathPreset p{ InputSourceType::GamepadButton, b, AxisDirection::Positive };
                            if (DrawPresetRow(p, picked)) didPick = true;
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Gamepad Axis"))
                    {
                        for (int32_t a = 0; a < GAMEPAD_AXIS_COUNT; ++a)
                        {
                            PathPreset pp{ InputSourceType::GamepadAxis, a, AxisDirection::Positive };
                            if (DrawPresetRow(pp, picked)) didPick = true;
                            PathPreset pn{ InputSourceType::GamepadAxis, a, AxisDirection::Negative };
                            if (DrawPresetRow(pn, picked)) didPick = true;
                        }
                        ImGui::EndMenu();
                    }

                    if (didPick)
                    {
                        e.mInputPath = picked;
                        // Auto-default the device column to make the row valid
                        // out of the box. Keyboard/Mouse paths force the
                        // "Any/N-A" sentinel; Gamepad paths leave the existing
                        // device choice alone (artist may want a specific
                        // type like DualSense).
                        if (picked.rfind("Keyboard/", 0) == 0 ||
                            picked.rfind("Mouse/",    0) == 0)
                        {
                            e.mGamepadType = GamepadType::Count;
                        }
                        MarkDirty(map);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }

            // Kind combo
            ImGui::TableNextColumn();
            int kIdx = (int)e.mKind;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##kind", &kIdx, kinds, IM_ARRAYSIZE(kinds)))
            {
                e.mKind = (InputPromptKind)kIdx;
                MarkDirty(map);
            }

            // Asset / Glyph cell. Both Sprite and Glyph kinds use the unified
            // Polyphase::AssetRefPicker — it brings consistent drag-drop, an X
            // clear button, type filtering, and the same Material-style
            // polymorphism as the inspector. NoAutocomplete keeps the cell
            // compact for the table layout; NoInspect/NoReveal/NoBrowse drops
            // the icons the row layout doesn't have room for.
            ImGui::TableNextColumn();
            const Polyphase::AssetPickerFlags kCompactFlags =
                Polyphase::AssetPickerFlags_NoAutocomplete |
                Polyphase::AssetPickerFlags_NoBrowse       |
                Polyphase::AssetPickerFlags_NoInspect      |
                Polyphase::AssetPickerFlags_NoReveal;

            if (e.mKind == InputPromptKind::Sprite)
            {
                ImGui::SetNextItemWidth(-32.0f);
                if (Polyphase::AssetRefPicker("##spr", e.mSprite, Texture::GetStaticType(), kCompactFlags))
                {
                    MarkDirty(map);
                }
            }
            else if (e.mKind == InputPromptKind::Glyph)
            {
                Font* font = e.mGlyphFont.Get<Font>();
                ImGui::SetNextItemWidth(-32.0f);
                if (Polyphase::AssetRefPicker("##fnt", e.mGlyphFont, Font::GetStaticType(), kCompactFlags))
                {
                    MarkDirty(map);
                }
                // Hex codepoint input. Note: ImGui::InputInt always parses
                // DECIMAL even with CharsHexadecimal — that flag only restricts
                // typed chars. Use InputText + strtoul so "E012" becomes 0xE012.
                char cpbuf[16];
                snprintf(cpbuf, sizeof(cpbuf), "%X", e.mGlyphCodepoint);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::InputText("U+##cp", cpbuf, sizeof(cpbuf),
                                     ImGuiInputTextFlags_CharsHexadecimal |
                                     ImGuiInputTextFlags_CharsUppercase))
                {
                    e.mGlyphCodepoint = (uint32_t)strtoul(cpbuf, nullptr, 16);
                    MarkDirty(map);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Hex codepoint (Unicode). Example: E012 for U+E012.\n"
                                      "Kenney input-prompt fonts ship at U+E000+.");
                }

                // Quick "is this codepoint actually in the font?" feedback so
                // artists don't have to launch the game to discover a typo.
                ImGui::SameLine();
                bool present = false;
                if (font && e.mGlyphCodepoint != 0)
                {
                    for (const Character& c : font->GetCharacters())
                    {
                        if ((uint32_t)c.mCodePoint == e.mGlyphCodepoint) { present = true; break; }
                    }
                }
                if (e.mGlyphCodepoint == 0)
                {
                    ImGui::TextDisabled("(no codepoint)");
                }
                else if (font && !present)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("missing!");
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("U+%X is not present in %s.\n"
                                          "Open the font's bitmap to confirm which codepoints it ships.",
                                          e.mGlyphCodepoint, font->GetName().c_str());
                }
                else if (font)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                    ImGui::TextUnformatted("OK");
                    ImGui::PopStyleColor();
                }
            }
            else
            {
                ImGui::TextDisabled("(text fallback)");
            }

            // Fallback text
            ImGui::TableNextColumn();
            {
                char fbuf[64];
                strncpy(fbuf, e.mFallbackText.c_str(), sizeof(fbuf));
                fbuf[sizeof(fbuf) - 1] = 0;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputText("##fb", fbuf, sizeof(fbuf)))
                {
                    e.mFallbackText = fbuf;
                    MarkDirty(map);
                }
            }

            // Per-row mini delete
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X##del"))
            {
                DeleteEntry(map, i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Pair the PushStyleColor calls from before BeginTable.
    ImGui::PopStyleColor(2);
}

void InputPromptMapInspector::DrawValidationStrip(InputPromptMap* map)
{
    if (!map) return;
    auto& entries = map->GetEntries();
    int warnCount = 0;

    ImGui::Separator();
    ImGui::TextUnformatted("Validation:");

    for (int32_t i = 0; i < (int32_t)entries.size(); ++i)
    {
        const InputPromptEntry& e = entries[i];
        if (e.mKind == InputPromptKind::Sprite && e.mSprite.Get() == nullptr)
        {
            ImGui::BulletText("Entry %d: Sprite kind but no texture assigned.", i);
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton("Jump"))
                mSelectedEntry = i;
            ImGui::PopID();
            warnCount++;
        }
        else if (e.mKind == InputPromptKind::Glyph &&
                 (e.mGlyphFont.Get() == nullptr || e.mGlyphCodepoint == 0))
        {
            ImGui::BulletText("Entry %d: Glyph kind missing font or codepoint.", i);
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton("Jump"))
                mSelectedEntry = i;
            ImGui::PopID();
            warnCount++;
        }
        if (e.mInputPath.empty())
        {
            ImGui::BulletText("Entry %d: empty input path.", i);
            ImGui::SameLine();
            ImGui::PushID(10000 + i);
            if (ImGui::SmallButton("Jump"))
                mSelectedEntry = i;
            ImGui::PopID();
            warnCount++;
        }
    }
    if (warnCount == 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
        ImGui::BulletText("OK");
        ImGui::PopStyleColor();
    }
}

void InputPromptMapInspector::DrawWindow()
{
    if (!mIsOpen || !mTarget)
        return;

    // Title — use ### to make the window id stable across asset retargets so
    // dock layout / position persists when the user opens a different map.
    char title[160];
    snprintf(title, sizeof(title), "Input Prompt Map: %s###InputPromptMapEditor",
             mTarget->GetName().c_str());

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(960.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title, &open))
    {
        DrawToolbar(mTarget);
        DrawFilterRow();
        ImGui::Spacing();
        DrawEntryTable(mTarget);

        mCapture.Draw();

        DrawValidationStrip(mTarget);
    }
    ImGui::End();

    // Route the X-button close through Close() so the test-device override
    // gets cleared even when the user dismisses the window directly.
    if (!open)
        Close();
}

void InputPromptMapInspector::DrawInspectorButton(InputPromptMap* map)
{
    if (!map) return;
    ImGui::Separator();
    if (ImGui::Button("Open Editor..."))
        Open(map);
    ImGui::SameLine();
    ImGui::TextDisabled("(or double-click the asset)");
}

#endif // EDITOR
