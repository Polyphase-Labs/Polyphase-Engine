#if EDITOR

#include "UpdatesModule.h"
#include "EditorWidgets.h"
#include "../JsonSettings.h"
#include "AutoUpdater/AutoUpdater.h"
#include "AutoUpdater/HttpClient.h"

#include "document.h"
#include "imgui.h"
#include "Log.h"

#include <algorithm>
#include <chrono>
#include <ctime>

DEFINE_PREFERENCES_MODULE(UpdatesModule, "Updates", "")

UpdatesModule* UpdatesModule::sInstance = nullptr;

UpdatesModule::UpdatesModule()
{
    sInstance = this;
}

UpdatesModule::~UpdatesModule()
{
    if (sInstance == this)
    {
        sInstance = nullptr;
    }
}

UpdatesModule* UpdatesModule::Get()
{
    return sInstance;
}

void UpdatesModule::Render()
{
    bool changed = false;

    ImGui::Text("Automatic Updates");
    ImGui::Spacing();

    if (Polyphase::Checkbox("Check for updates on startup", &mCheckOnStartup))
    {
        changed = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically check for new versions when the editor starts.");
    }

    ImGui::Spacing();

    int hours = mCheckIntervalHours;
    if (ImGui::InputInt("Check interval (hours)", &hours))
    {
        hours = std::clamp(hours, 1, 168); // 1 hour to 1 week
        if (hours != mCheckIntervalHours)
        {
            mCheckIntervalHours = hours;
            changed = true;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Minimum hours between automatic update checks.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Display last check time
    if (mLastCheckTime > 0)
    {
        std::time_t time = (std::time_t)(mLastCheckTime / 1000000000LL); // Convert from nanoseconds
        std::tm* tm = std::localtime(&time);
        if (tm)
        {
            char buffer[64];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
            ImGui::Text("Last check: %s", buffer);
        }
        else
        {
            ImGui::Text("Last check: Unknown");
        }
    }
    else
    {
        ImGui::Text("Last check: Never");
    }

    ImGui::Spacing();

    // Display skipped version if any
    if (!mSkippedVersion.empty())
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Skipped version: %s", mSkippedVersion.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            ClearSkippedVersion();
            changed = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Check availability
    if (!HttpClient::IsAvailable())
    {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.1f, 1.0f), "Update checking unavailable");
        std::string msg = HttpClient::GetMissingDependencyMessage();
        if (!msg.empty())
        {
            ImGui::TextWrapped("%s", msg.c_str());
        }
    }
    else
    {
        // Check Now button
        AutoUpdater* updater = AutoUpdater::Get();
        bool isChecking = updater && updater->GetState() == UpdateCheckState::Checking;

        ImGui::BeginDisabled(isChecking);
        if (ImGui::Button(isChecking ? "Checking..." : "Check Now"))
        {
            if (updater)
            {
                updater->CheckForUpdates(true);
            }
        }
        ImGui::EndDisabled();

        // Display current state
        if (updater)
        {
            UpdateCheckState state = updater->GetState();
            ImGui::SameLine();

            switch (state)
            {
            case UpdateCheckState::UpToDate:
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "You're up to date!");
                break;
            case UpdateCheckState::UpdateAvailable:
                ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Update available");
                break;
            case UpdateCheckState::Error:
                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Error checking");
                break;
            default:
                break;
            }
        }
    }

    if (changed)
    {
        SetDirty(true);
    }
}

void UpdatesModule::SetCheckOnStartup(bool enabled)
{
    mCheckOnStartup = enabled;
    SetDirty(true);
}

void UpdatesModule::SetCheckIntervalHours(int hours)
{
    mCheckIntervalHours = std::clamp(hours, 1, 168);
    SetDirty(true);
}

void UpdatesModule::SetLastCheckTime(int64_t time)
{
    mLastCheckTime = time;
    SetDirty(true);
}

void UpdatesModule::SetSkippedVersion(const std::string& version)
{
    mSkippedVersion = version;
    SetDirty(true);
}

void UpdatesModule::ClearSkippedVersion()
{
    mSkippedVersion.clear();
    SetDirty(true);
}

void UpdatesModule::LoadSettings(const rapidjson::Document& doc)
{
    mCheckOnStartup = JsonSettings::GetBool(doc, "checkOnStartup", true);
    mCheckIntervalHours = JsonSettings::GetInt(doc, "checkIntervalHours", 24);
    mLastCheckTime = JsonSettings::GetInt64(doc, "lastCheckTime", 0);
    mSkippedVersion = JsonSettings::GetString(doc, "skippedVersion", "");

    mCheckIntervalHours = std::clamp(mCheckIntervalHours, 1, 168);
}

void UpdatesModule::SaveSettings(rapidjson::Document& doc)
{
    JsonSettings::SetBool(doc, "checkOnStartup", mCheckOnStartup);
    JsonSettings::SetInt(doc, "checkIntervalHours", mCheckIntervalHours);
    JsonSettings::SetInt64(doc, "lastCheckTime", mLastCheckTime);
    JsonSettings::SetString(doc, "skippedVersion", mSkippedVersion);
}

#endif
