#pragma once

#if PLATFORM_MAC

// Polyphase's Cocoa platform backend for Dear ImGui. Forked from the vendored
// backends/imgui_impl_osx.mm and restructured to match imgui_impl_xcb: the
// engine's NSEvent pump (System_MacCocoa.mm) hands every event to
// ImGui_ImplMac_EventHandler, mouse coordinates are in backing pixels (what
// the engine uses for mWindowWidth/Height), and NewFrame takes DisplaySize
// and DeltaTime from the engine rather than from the view.
//
// The header is ObjC-free so it can be included from C++ TUs; the void*
// parameters are NSView* / NSEvent*.

#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

#include <stdint.h>

IMGUI_IMPL_API bool     ImGui_ImplMac_Init(void* nsView);
IMGUI_IMPL_API void     ImGui_ImplMac_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplMac_NewFrame();

IMGUI_IMPL_API int32_t  ImGui_ImplMac_EventHandler(void* nsEvent);

#endif // #ifndef IMGUI_DISABLE

#endif
