#pragma once

#if EDITOR
#include "EngineTypes.h"

#include <cstdint>
#include <vector>
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

class Object;
class Property;
struct AssetStub;

typedef void(*FileBrowserCallbackFP)(const std::vector<std::string>& filePaths);


void EditorImguiInit();
void EditorImguiDraw();

/**
 * Returns the monospace terminal font (Roboto Mono with extended Unicode
 * coverage) loaded at editor init for the CLI Terminal panel. May return
 * nullptr if the font file was missing — callers should handle that case
 * gracefully and fall back to the current font.
 */
ImFont* GetEditorTerminalFont();

void EditorImguiShutdown();
void EditorImguiPreShutdown();

void EditorImguiGetViewport(uint32_t& x, uint32_t& y, uint32_t& width, uint32_t& height);
bool EditorIsInterfaceVisible();
bool EditorImguiIsViewportHovered();
void EditorOpenFileBrowser(FileBrowserCallbackFP callback, bool folderMode);
void EditorSetFileBrowserDir(const std::string& dir);
void EditorShowUnsavedAssetsModal(const std::vector<AssetStub*>& unsavedStubs);

void DrawAssetProperty(Property& prop, uint32_t index, Object* owner, PropertyOwnerType ownerType);

// Animated progress modal for long-running editor operations (scene save,
// asset save, enter/exit PIE, reload scripts). Long synchronous work in
// these paths would otherwise freeze the UI; calling Pump() at safe
// checkpoints renders one editor frame so the modal animates and the
// status label updates. Only valid to call from non-ImGui contexts -- i.e.
// from the deferred-end-of-frame dispatcher in EditorMain.cpp after
// EditorImguiDraw has returned. Begin/SetStatus/Step/End perform their
// own (throttled) pump.
namespace EditorProgress
{
    // Open the modal. status text is shown immediately. If cancellable is
    // true a Cancel button is rendered; loops should poll WasCancelled().
    void Begin(const char* title, const char* status, bool cancellable = false);

    // Update the status label and pump a frame (subject to ~60Hz throttle).
    void SetStatus(const char* msg);

    // Set the progress bar. Negative = indeterminate sine marquee; 0..1 =
    // determinate fill. No pump (use SetStatus or Step to redraw).
    void SetFraction(float f);

    // Convenience for "Step N of M" loops: updates label + determinate
    // fraction + pumps.
    void Step(const char* msg, int done, int total);

    // Force-render one editor frame. Throttled to ~60Hz; ignored when not
    // active. Asserts if called inside an ImGui frame scope.
    void Pump();

    // Close the modal and render one final frame.
    void End();

    bool IsActive();
    bool WasCancelled();
}

#endif
