#if EDITOR

#include "ViewportModule.h"
#include "EditorWidgets.h"
#include "../../JsonSettings.h"
#include "../../PreferencesManager.h"

#include "document.h"
#include "imgui.h"
#include "Renderer.h"
#include "Engine.h"
#include "Log.h"
#include "../../../Grid.h"
#if API_VULKAN
#include "Graphics/Vulkan/VulkanContext.h"
#endif

DEFINE_PREFERENCES_MODULE(ViewportModule, "Viewport", "Appearance")

ViewportModule* ViewportModule::sInstance = nullptr;
bool ViewportModule::sSyncingGridState = false;

ViewportModule::ViewportModule()
{
    sInstance = this;
}

ViewportModule::~ViewportModule()
{
    if (sInstance == this)
    {
        sInstance = nullptr;
    }
}

void ViewportModule::Render()
{
    bool changed = false;

    ImGui::Text("Menu Bar Padding");
    if (ImGui::SliderFloat("##MenuBarPadding", &mMenuBarPadding, 1.0f, 12.0f, "%.0f px"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the vertical padding of the main menu bar.");

    ImGui::Spacing();

    ImGui::Text("Viewport Resolution Scale");
    static const char* kResolutionScaleLabels[] = { "Auto", "100%", "75%", "50%" };
    if (ImGui::Combo("##ViewportResolutionScale", &mResolutionScaleMode, kResolutionScaleLabels, 4))
    {
        changed = true;
        ApplyResolutionScale();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Render the 3D scene at a fraction of the window's pixel size; the interface stays sharp.\nAuto picks 50% on integrated Intel/AMD GPUs driving a HiDPI window and 100% everywhere else.");

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Background Color");
    if (ImGui::ColorEdit4("##BackgroundColor", &mBackgroundColor.x, ImGuiColorEditFlags_NoInputs))
    {
        changed = true;
        ApplyBackgroundColorToRenderer();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the viewport background color.");

    ImGui::Spacing();

    if (Polyphase::Checkbox("Show Grid", &mShowGrid))
    {
        changed = true;
        ApplyGridVisibility();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle grid visibility in the viewport.");

    ImGui::Spacing();

    ImGui::BeginDisabled(!mShowGrid);
    {
        ImGui::Text("Grid Color");
        if (ImGui::ColorEdit4("##GridColor", &mGridColor.x, ImGuiColorEditFlags_NoInputs))
        {
            changed = true;
            SetGridColor(mGridColor);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the grid line color.");

        ImGui::Spacing();

        // ImGui::Text("Grid Size");
        // if (ImGui::SliderFloat("##GridSize", &mGridSize, 0.1f, 10.0f, "%.1f"))
        // {
        //     changed = true;
        // }
        // if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the spacing between grid lines.");
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Selected Color");
    if (ImGui::ColorEdit4("##SelectedColor", &mSelectedColor.x, ImGuiColorEditFlags_AlphaBar))
    {
        changed = true;
        ApplySelectedOverlay();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the selection overlay color and opacity.");

    ImGui::Spacing();

    ImGui::Text("Checker Size");
    if (ImGui::SliderFloat("##SelectedCheckerSize", &mSelectedCheckerSize, 1.0f, 32.0f, "%.0f px"))
    {
        changed = true;
        ApplySelectedOverlay();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the size of the selection checker pattern in pixels.");

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Transform Snapping");
    ImGui::Spacing();

    if (Polyphase::Checkbox("Snapping Enabled", &mSnapEnabled))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap gizmo and G/R/S transforms. Hold Shift while transforming to invert this setting.");

    ImGui::Text("Snap To");
    static const char* kSnapModeLabels[] = { "Increment", "Vertex", "Edge", "Face" };
    if (ImGui::Combo("##SnapMode", &mSnapMode, kSnapModeLabels, 4))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Vertex, Edge and Face move the object's pivot onto scene geometry while translating.\nRotate and scale always use increments.");

    ImGui::Text("Translate Increment");
    if (ImGui::DragFloat("##SnapTranslate", &mSnapTranslate, 0.01f, 0.0f, 1000.0f, "%.3f"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("World units per translate step. 0 disables translate snapping.");

    ImGui::Text("Rotate Increment");
    if (ImGui::DragFloat("##SnapRotate", &mSnapRotate, 0.1f, 0.0f, 180.0f, "%.1f deg"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Degrees per rotate step. 0 disables rotate snapping.");

    ImGui::Text("Scale Increment");
    if (ImGui::DragFloat("##SnapScale", &mSnapScale, 0.005f, 0.0f, 100.0f, "%.3f"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale amount per step. 0 disables scale snapping.");

    ImGui::Text("Widget Increment");
    if (ImGui::DragFloat("##SnapWidgetPixels", &mSnapWidgetPixels, 0.1f, 0.0f, 1024.0f, "%.0f px"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pixels per step when moving or resizing widgets in the 2D viewport.");

    ImGui::Text("Vertex / Edge Pick Radius");
    if (ImGui::SliderFloat("##SnapPixelThreshold", &mSnapPixelThreshold, 4.0f, 64.0f, "%.0f px"))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("How close (on screen) the pivot must be to a vertex or edge before it snaps.");

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Transform Precision");
    ImGui::Spacing();

    ImGui::Text("Normal");
    if (ImGui::SliderFloat("##PrecisionNormal", &mPrecisionNormal, 0.01f, 4.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mouse sensitivity of G/R/S transforms.");

    ImGui::Text("While Control Is Held");
    if (ImGui::SliderFloat("##PrecisionCtrl", &mPrecisionCtrl, 0.01f, 4.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mouse sensitivity while Control is held, for G/R/S transforms and gizmo handle drags.");

    if (changed)
    {
        SetDirty(true);
    }
}

void ViewportModule::LoadSettings(const rapidjson::Document& doc)
{
    mBackgroundColor = JsonSettings::GetVec4(doc, "backgroundColor", glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
    mShowGrid = JsonSettings::GetBool(doc, "showGrid", true);
    mGridColor = JsonSettings::GetVec4(doc, "gridColor", glm::vec4(0.3f, 0.3f, 0.3f, 1.0f));
    mGridSize = JsonSettings::GetFloat(doc, "gridSize", 1.0f);
    mSelectedColor = JsonSettings::GetVec4(doc, "selectedColor", glm::vec4(0.2f, 0.1f, 1.0f, 0.6f));
    mSelectedCheckerSize = JsonSettings::GetFloat(doc, "selectedCheckerSize", 8.0f);
    mMenuBarPadding = JsonSettings::GetFloat(doc, "menuBarPadding", 8.0f);
    mShowGizmosInPreview = JsonSettings::GetBool(doc, "showGizmosInPreview", false);
    mResolutionScaleMode = JsonSettings::GetInt(doc, "resolutionScaleMode", 0);

    mSnapEnabled = JsonSettings::GetBool(doc, "snapEnabled", false);
    mSnapMode = glm::clamp(JsonSettings::GetInt(doc, "snapMode", 0), 0, 3);
    mSnapTranslate = glm::max(JsonSettings::GetFloat(doc, "snapTranslate", 1.0f), 0.0f);
    mSnapRotate = glm::max(JsonSettings::GetFloat(doc, "snapRotate", 15.0f), 0.0f);
    mSnapScale = glm::max(JsonSettings::GetFloat(doc, "snapScale", 0.1f), 0.0f);
    mSnapWidgetPixels = glm::max(JsonSettings::GetFloat(doc, "snapWidgetPixels", 8.0f), 0.0f);
    mSnapPixelThreshold = glm::clamp(JsonSettings::GetFloat(doc, "snapPixelThreshold", 16.0f), 4.0f, 64.0f);
    mPrecisionNormal = glm::clamp(JsonSettings::GetFloat(doc, "precisionNormal", 1.0f), 0.01f, 4.0f);
    mPrecisionCtrl = glm::clamp(JsonSettings::GetFloat(doc, "precisionCtrl", 0.1f), 0.01f, 4.0f);

    ApplyResolutionScale();
    ApplyBackgroundColorToRenderer();
    SetGridColor(mGridColor);
    ApplyGridVisibility();
    ApplySelectedOverlay();
}

void ViewportModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetVec4(doc, "backgroundColor", mBackgroundColor);
    JsonSettings::SetBool(doc, "showGrid", mShowGrid);
    JsonSettings::SetVec4(doc, "gridColor", mGridColor);
    JsonSettings::SetFloat(doc, "gridSize", mGridSize);
    JsonSettings::SetVec4(doc, "selectedColor", mSelectedColor);
    JsonSettings::SetFloat(doc, "selectedCheckerSize", mSelectedCheckerSize);
    JsonSettings::SetFloat(doc, "menuBarPadding", mMenuBarPadding);
    JsonSettings::SetBool(doc, "showGizmosInPreview", mShowGizmosInPreview);
    JsonSettings::SetInt(doc, "resolutionScaleMode", mResolutionScaleMode);

    JsonSettings::SetBool(doc, "snapEnabled", mSnapEnabled);
    JsonSettings::SetInt(doc, "snapMode", mSnapMode);
    JsonSettings::SetFloat(doc, "snapTranslate", mSnapTranslate);
    JsonSettings::SetFloat(doc, "snapRotate", mSnapRotate);
    JsonSettings::SetFloat(doc, "snapScale", mSnapScale);
    JsonSettings::SetFloat(doc, "snapWidgetPixels", mSnapWidgetPixels);
    JsonSettings::SetFloat(doc, "snapPixelThreshold", mSnapPixelThreshold);
    JsonSettings::SetFloat(doc, "precisionNormal", mPrecisionNormal);
    JsonSettings::SetFloat(doc, "precisionCtrl", mPrecisionCtrl);
}

void ViewportModule::ApplyResolutionScale() const
{
    float scale = 1.0f;
    switch (mResolutionScaleMode)
    {
    case 1: scale = 1.0f; break;
    case 2: scale = 0.75f; break;
    case 3: scale = 0.5f; break;
    default:
    {
        // Auto: an integrated GPU filling a HiDPI (Retina / 4K) window is
        // fill-bound in the scene pass, so halve it there. Apple GPUs report
        // as integrated too (vendor 0x106B) but have the bandwidth, so they
        // stay at 100%. Re-evaluated whenever the window's pixel size changes
        // (EditorState::Update), so going full screen on a Retina panel from a
        // small window flips to 50% and back.
        bool integrated = false;
#if API_VULKAN
        if (GetVulkanContext() != nullptr)
        {
            const VkPhysicalDeviceProperties& props = GetVulkanContext()->GetDeviceProperties();
            integrated = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && props.vendorID != 0x106B;
        }
#endif
        const uint64_t pixels = (uint64_t)GetEngineState()->mWindowWidth * (uint64_t)GetEngineState()->mWindowHeight;
        scale = (integrated && pixels >= 2500000ull) ? 0.5f : 1.0f;
        break;
    }
    }

    if (Renderer::Get() != nullptr && Renderer::Get()->GetResolutionScale() != scale)
    {
        Renderer::Get()->SetResolutionScale(scale);
        LogDebug("Viewport resolution scale %.2f (%s)", scale, mResolutionScaleMode == 0 ? "auto" : "preference");
    }
}

void ViewportModule::ApplyBackgroundColorToRenderer() const
{
    if (!IsPlayingInEditor())
    {
        Renderer::Get()->SetClearColor(mBackgroundColor);
    }
}

ViewportModule* ViewportModule::Get()
{
    return sInstance;
}

void ViewportModule::HandleExternalGridToggle(bool enabled)
{
    if (sSyncingGridState)
    {
        return;
    }

    ViewportModule* instance = Get();
    if (instance == nullptr)
    {
        return;
    }

    if (instance->mShowGrid != enabled)
    {
        instance->mShowGrid = enabled;
        instance->SetDirty(true);

        PreferencesManager* prefs = PreferencesManager::Get();
        if (prefs)
        {
            prefs->SaveModule(instance);
        }
    }
}

void ViewportModule::ApplySelectedOverlay() const
{
    Renderer::Get()->SetSelectedColor(mSelectedColor);
    Renderer::Get()->SetSelectedCheckerSize(mSelectedCheckerSize);
}

void ViewportModule::ApplyGridVisibility()
{
    sSyncingGridState = true;
    EnableGrid(mShowGrid);
    sSyncingGridState = false;
}

void ViewportModule::SetShowGizmosInPreview(bool show)
{
    if (mShowGizmosInPreview != show)
    {
        mShowGizmosInPreview = show;
        SetDirty(true);

        PreferencesManager* prefs = PreferencesManager::Get();
        if (prefs)
        {
            prefs->SaveModule(this);
        }
    }
}

void ViewportModule::SetSnapEnabled(bool enabled)
{
    if (mSnapEnabled != enabled)
    {
        mSnapEnabled = enabled;
        SetDirty(true);

        PreferencesManager* prefs = PreferencesManager::Get();
        if (prefs)
        {
            prefs->SaveModule(this);
        }
    }
}

void ViewportModule::SetSnapMode(int mode)
{
    mode = glm::clamp(mode, 0, 3);
    if (mSnapMode != mode)
    {
        mSnapMode = mode;
        SetDirty(true);

        PreferencesManager* prefs = PreferencesManager::Get();
        if (prefs)
        {
            prefs->SaveModule(this);
        }
    }
}

#endif
