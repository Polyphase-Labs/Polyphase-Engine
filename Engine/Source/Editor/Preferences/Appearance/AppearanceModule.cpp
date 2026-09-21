#if EDITOR

#include "AppearanceModule.h"
#include "../JsonSettings.h"
#include "../PreferencesManager.h"
#include "../../EditorImgui.h"

#include "Engine.h"
#include "Maths.h"

#include "document.h"
#include "imgui.h"
#include "imgui_dock.h"

static const float kMinTextScale = 0.75f;
static const float kMaxTextScale = 2.0f;
static const float kMinInterfaceScale = 0.5f;
static const float kMaxInterfaceScale = 3.0f;

DEFINE_PREFERENCES_MODULE(AppearanceModule, "Appearance", "")

float AppearanceModule::LoadSavedTextScale()
{
    rapidjson::Document doc;
    std::string settingsPath = JsonSettings::GetPreferencesDirectory() + "/Appearance.json";

    float scale = 1.0f;
    if (JsonSettings::LoadFromFile(settingsPath, doc))
    {
        scale = JsonSettings::GetFloat(doc, "textScale", 1.0f);
    }

    return glm::clamp(scale, kMinTextScale, kMaxTextScale);
}

AppearanceModule::AppearanceModule()
{
    // ApplyTabRounding will be called after LoadSettings
}

AppearanceModule::~AppearanceModule()
{
}

void AppearanceModule::Render()
{
    bool changed = false;

    ImGui::Text("Text Scale");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::SliderFloat("##TextScale", &mPendingTextScale, kMinTextScale, kMaxTextScale, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size of the editor text and icons. Fonts are re-baked at the new size, so text stays sharp.\nCtrl+Click to type a value. Takes effect when you press Apply.");
    ImGui::SameLine();
    if (ImGui::Button("Apply##TextScale"))
    {
        ApplyTextScale(mPendingTextScale);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##TextScale"))
    {
        ApplyTextScale(1.0f);
    }

    ImGui::Spacing();

    // Interface scale lives in EngineConfig (Config.ini) and can also change
    // from the View menu, so re-seed the slider whenever the live value moves.
    const float liveInterfaceScale = GetEngineConfig()->mEditorInterfaceScale;
    if (liveInterfaceScale != mSeenInterfaceScale)
    {
        mSeenInterfaceScale = liveInterfaceScale;
        mPendingInterfaceScale = liveInterfaceScale;
    }

    ImGui::Text("Interface Scale");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::SliderFloat("##InterfaceScale", &mPendingInterfaceScale, kMinInterfaceScale, kMaxInterfaceScale, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Magnifies the whole editor interface.\nCtrl+Click to type a value. Takes effect when you press Apply.");
    ImGui::SameLine();
    if (ImGui::Button("Apply##InterfaceScale"))
    {
        ApplyEditorInterfaceScale(mPendingInterfaceScale);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##InterfaceScale"))
    {
        ApplyEditorInterfaceScale(GetDefaultEditorInterfaceScale());
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to %.2f", GetDefaultEditorInterfaceScale());

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Panel Tab Rounding");

    ImGui::Text("Left");
    ImGui::SameLine();
    if (ImGui::SliderFloat("##TabRoundingLeft", &mTabRoundingLeft, 0.0f, 15.0f, "%.1f"))
    {
        changed = true;
        ApplyTabRounding();
    }

    ImGui::Text("Right");
    ImGui::SameLine();
    if (ImGui::SliderFloat("##TabRoundingRight", &mTabRoundingRight, 0.0f, 15.0f, "%.1f"))
    {
        changed = true;
        ApplyTabRounding();
    }

    if (changed)
    {
        SetDirty(true);
    }
}

void AppearanceModule::LoadSettings(const rapidjson::Document& doc)
{
    mTabRoundingLeft = JsonSettings::GetFloat(doc, "tabRoundingLeft", 0.0f);
    mTabRoundingRight = JsonSettings::GetFloat(doc, "tabRoundingRight", 0.0f);
    mTextScale = glm::clamp(JsonSettings::GetFloat(doc, "textScale", 1.0f), kMinTextScale, kMaxTextScale);
    mPendingTextScale = mTextScale;
    ApplyTabRounding();

    // Also runs on Preferences > Cancel. The rebuild is skipped when the scale
    // already matches the live fonts.
    RequestEditorFontRebuild(mTextScale);
}

void AppearanceModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetFloat(doc, "tabRoundingLeft", mTabRoundingLeft);
    JsonSettings::SetFloat(doc, "tabRoundingRight", mTabRoundingRight);
    JsonSettings::SetFloat(doc, "textScale", mTextScale);
}

void AppearanceModule::ApplyTabRounding()
{
    ImGui::SetDockTabRounding(mTabRoundingLeft, mTabRoundingRight);
}

void AppearanceModule::ApplyTextScale(float scale)
{
    mTextScale = glm::clamp(scale, kMinTextScale, kMaxTextScale);
    mPendingTextScale = mTextScale;
    RequestEditorFontRebuild(mTextScale);

    // Persist immediately so Cancel doesn't leave the saved value and the
    // live fonts out of step.
    SetDirty(true);
    PreferencesManager* prefs = PreferencesManager::Get();
    if (prefs)
    {
        prefs->SaveModule(this);
    }
}

#endif
