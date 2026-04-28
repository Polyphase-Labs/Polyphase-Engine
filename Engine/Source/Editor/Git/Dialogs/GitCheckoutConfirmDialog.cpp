#if EDITOR

#include "GitCheckoutConfirmDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "imgui.h"

static GitCheckoutConfirmDialog sInstance;

GitCheckoutConfirmDialog* GetGitCheckoutConfirmDialog()
{
    return &sInstance;
}

void GitCheckoutConfirmDialog::Show(const std::string& targetRef)
{
    mTargetRef = targetRef;
    mIsOpen = true;
    mJustOpened = true;

    // Populate dirty files from the current repo status
    mDirtyFiles.clear();
    GitRepository* repo = GitService::Get()->GetCurrentRepo();
    if (repo)
    {
        repo->RefreshStatus();
        const std::vector<GitStatusEntry>& status = repo->GetStatus();
        for (const auto& entry : status)
        {
            if (entry.mChangeType != GitChangeType::Ignored)
            {
                mDirtyFiles.push_back(entry);
            }
        }
    }
}

void GitCheckoutConfirmDialog::Show(const std::string& targetRef, const std::vector<GitStatusEntry>& dirtyFiles)
{
    mTargetRef = targetRef;
    mDirtyFiles = dirtyFiles;
    mIsOpen = true;
    mJustOpened = true;
}

static const char* GetChangeTypeName(GitChangeType type)
{
    switch (type)
    {
        case GitChangeType::Modified:   return "Modified";
        case GitChangeType::Added:      return "Added";
        case GitChangeType::Deleted:    return "Deleted";
        case GitChangeType::Renamed:    return "Renamed";
        case GitChangeType::Copied:     return "Copied";
        case GitChangeType::Untracked:  return "Untracked";
        case GitChangeType::Conflicted: return "Conflicted";
        case GitChangeType::Ignored:    return "Ignored";
        default:                        return "Unknown";
    }
}

void GitCheckoutConfirmDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Checkout - Unsaved Changes");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Checkout - Unsaved Changes", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Warning:");
        ImGui::SameLine();
        ImGui::Text("Checking out '%s' would affect dirty files.", mTargetRef.c_str());

        ImGui::Spacing();
        ImGui::Text("The following files have uncommitted changes:");
        ImGui::Spacing();

        // Scrollable file list
        float listHeight = ImGui::GetTextLineHeightWithSpacing() * (float)(mDirtyFiles.size() < 10 ? mDirtyFiles.size() : 10);
        if (listHeight < ImGui::GetTextLineHeightWithSpacing() * 3)
        {
            listHeight = ImGui::GetTextLineHeightWithSpacing() * 3;
        }

        if (ImGui::BeginChild("##DirtyFilesList", ImVec2(-1, listHeight), true))
        {
            for (const auto& entry : mDirtyFiles)
            {
                ImVec4 color(1.0f, 1.0f, 1.0f, 1.0f);
                switch (entry.mChangeType)
                {
                    case GitChangeType::Modified:   color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f); break;
                    case GitChangeType::Added:      color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f); break;
                    case GitChangeType::Deleted:    color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); break;
                    case GitChangeType::Untracked:  color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
                    case GitChangeType::Conflicted: color = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); break;
                    default: break;
                }

                ImGui::TextColored(color, "[%s]", GetChangeTypeName(entry.mChangeType));
                ImGui::SameLine();
                ImGui::Text("%s", entry.mPath.c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextWrapped("Choose how to proceed:");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Cancel", ImVec2(140, 0)))
        {
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Stash and Continue", ImVec2(160, 0)))
        {
            // Stash is not yet implemented; show a warning via tooltip
            ImGui::OpenPopup("StashNotImplemented");
        }

        if (ImGui::BeginPopup("StashNotImplemented"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Stash is not yet implemented.");
            ImGui::Text("Please commit or discard your changes manually.");
            if (ImGui::Button("OK##StashWarn", ImVec2(80, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

        if (ImGui::Button("Force Checkout", ImVec2(140, 0)))
        {
            GitRepository* repo = GitService::Get()->GetCurrentRepo();
            if (repo)
            {
                // Try as branch first, fall back to commit checkout
                if (!repo->CheckoutBranch(mTargetRef))
                {
                    repo->CheckoutCommit(mTargetRef);
                }
            }
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }
}

#endif
