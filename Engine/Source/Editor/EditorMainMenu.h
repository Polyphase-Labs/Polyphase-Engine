#pragma once

#if EDITOR

#include <cstdint>

namespace EditorMenuPos
{
    constexpr int32_t AfterFile  = 0;
    constexpr int32_t AfterEdit  = 1;
    constexpr int32_t AfterView  = 2;
    constexpr int32_t AfterWorld = 3;
    constexpr int32_t AfterTools = 4;

    // Legacy slots — preserved for addon-API stability after the top-level
    // Addons and Extra menus were folded into Tools and Help respectively.
    // Both now anchor between Tools and Help (where Addons/Extra used to live).
    constexpr int32_t Legacy_AfterAddons = 5;
    constexpr int32_t Legacy_AfterExtra  = 6;

    constexpr int32_t Append = -1;
}

void DrawMainMenuBarMenus(bool& outOpenSaveSceneAsModal);

#endif
