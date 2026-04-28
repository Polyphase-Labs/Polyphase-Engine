#if EDITOR

#include "GitCreateTagDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "imgui.h"
#include <cstring>

static GitCreateTagDialog sInstance;

GitCreateTagDialog* GetGitCreateTagDialog()
{
    return &sInstance;
}

void GitCreateTagDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    std::memset(mTagName, 0, sizeof(mTagName));
    std::memset(mMessage, 0, sizeof(mMessage));
    mAnnotated = false;
    mTargetMode = 0;
    mSelectedTargetOid.clear();
}

void GitCreateTagDialog::Open(const std::string& targetOid)
{
    Open();
    if (!targetOid.empty())
    {
        mTargetMode = 1;
        mSelectedTargetOid = targetOid;
    }
}

void GitCreateTagDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Create Tag");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Create Tag", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Tag Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##TagName", mTagName, sizeof(mTagName));

        ImGui::Spacing();
        ImGui::Text("Target Commit");

        ImGui::RadioButton("HEAD (current commit)##TagTarget", &mTargetMode, 0);
        ImGui::RadioButton("Specific commit##TagTarget", &mTargetMode, 1);

        if (mTargetMode == 1)
        {
            ImGui::Indent();
            GitRepository* repo = GitService::Get()->GetCurrentRepo();
            if (repo)
            {
                std::vector<GitCommitInfo> commits = repo->GetCommits(50);
                if (!commits.empty())
                {
                    int32_t currentIndex = 0;
                    for (int32_t i = 0; i < static_cast<int32_t>(commits.size()); ++i)
                    {
                        if (commits[i].mOid == mSelectedTargetOid)
                        {
                            currentIndex = i;
                            break;
                        }
                    }

                    std::string previewLabel;
                    if (currentIndex < static_cast<int32_t>(commits.size()))
                    {
                        previewLabel = commits[currentIndex].mShortOid + " - " + commits[currentIndex].mSummary;
                    }

                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##TagTargetCommit", previewLabel.c_str()))
                    {
                        for (int32_t i = 0; i < static_cast<int32_t>(commits.size()); ++i)
                        {
                            std::string label = commits[i].mShortOid + " - " + commits[i].mSummary;
                            bool isSelected = (commits[i].mOid == mSelectedTargetOid);
                            if (ImGui::Selectable(label.c_str(), isSelected))
                            {
                                mSelectedTargetOid = commits[i].mOid;
                            }
                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "No commits found in repository.");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No repository open.");
            }
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Checkbox("Annotated tag", &mAnnotated);

        if (mAnnotated)
        {
            ImGui::Spacing();
            ImGui::Text("Message");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextMultiline("##TagMessage", mMessage, sizeof(mMessage), ImVec2(-1, ImGui::GetTextLineHeight() * 5));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool nameEmpty = (std::strlen(mTagName) == 0);
        bool noRepo = (GitService::Get()->GetCurrentRepo() == nullptr);
        bool needsOid = (mTargetMode == 1 && mSelectedTargetOid.empty());
        bool needsMessage = (mAnnotated && std::strlen(mMessage) == 0);

        if (nameEmpty || noRepo || needsOid || needsMessage)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            GitRepository* repo = GitService::Get()->GetCurrentRepo();
            if (repo)
            {
                std::string targetOid;
                if (mTargetMode == 0)
                {
                    std::vector<GitCommitInfo> headCommits = repo->GetCommits(1);
                    if (!headCommits.empty())
                    {
                        targetOid = headCommits[0].mOid;
                    }
                }
                else
                {
                    targetOid = mSelectedTargetOid;
                }

                repo->CreateTag(mTagName, targetOid, mAnnotated, mMessage);
            }

            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameEmpty || noRepo || needsOid || needsMessage)
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
