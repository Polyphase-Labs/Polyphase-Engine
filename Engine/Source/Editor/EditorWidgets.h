#pragma once

#if EDITOR

#include "imgui.h"

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
}

#endif
