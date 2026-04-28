#if EDITOR

#include "GitOpenDialog.h"
#include "../GitService.h"
#include "imgui.h"
#include <cstring>

static GitOpenDialog sInstance;

GitOpenDialog* GetGitOpenDialog()
{
    return &sInstance;
}

void GitOpenDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    std::memset(mLocalPath, 0, sizeof(mLocalPath));
}

void GitOpenDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Open Repository");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Open Repository", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Local Repository Path");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##OpenRepoPath", mLocalPath, sizeof(mLocalPath));

        ImGui::Spacing();
        ImGui::TextWrapped("Enter the path to a local git repository (.git folder or working directory).");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool pathEmpty = (std::strlen(mLocalPath) == 0);

        if (pathEmpty)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Open", ImVec2(120, 0)))
        {
            GitService::Get()->OpenRepository(mLocalPath);
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (pathEmpty)
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
