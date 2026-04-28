#if EDITOR

#include "GitDeleteConfirmDialog.h"
#include "imgui.h"

static GitDeleteConfirmDialog sInstance;

GitDeleteConfirmDialog* GetGitDeleteConfirmDialog()
{
    return &sInstance;
}

void GitDeleteConfirmDialog::Show(const std::string& title, const std::string& body, int32_t dangerLevel, std::function<void()> callback)
{
    mTitle = title;
    mBody = body;
    mDangerLevel = dangerLevel;
    mCallback = callback;
    mUnderstandChecked = false;
    mIsOpen = true;
    mJustOpened = true;
}

void GitDeleteConfirmDialog::Draw()
{
    if (!mIsOpen) return;

    // Use a stable popup ID derived from the title
    std::string popupId = mTitle.empty() ? "Confirm Action" : mTitle;

    if (mJustOpened)
    {
        ImGui::OpenPopup(popupId.c_str());
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(popupId.c_str(), &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Warning icon and color based on danger level
        if (mDangerLevel >= 2)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Text("DANGER - This action is destructive and cannot be undone!");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        else if (mDangerLevel == 1)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.0f, 1.0f));
            ImGui::Text("Warning - Please confirm this action.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Body text
        if (!mBody.empty())
        {
            ImGui::TextWrapped("%s", mBody.c_str());
        }

        // "I understand" checkbox for critical actions
        if (mDangerLevel >= 2)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Checkbox("I understand the consequences of this action", &mUnderstandChecked);
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Confirm button - disabled if high danger and checkbox not checked
        bool confirmDisabled = (mDangerLevel >= 2 && !mUnderstandChecked);
        if (confirmDisabled)
        {
            ImGui::BeginDisabled();
        }

        // Style the confirm button red for danger
        if (mDangerLevel >= 1)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        }

        if (ImGui::Button("Confirm", ImVec2(120, 0)))
        {
            if (mCallback)
            {
                mCallback();
            }
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (mDangerLevel >= 1)
        {
            ImGui::PopStyleColor(3);
        }

        if (confirmDisabled)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

#endif
