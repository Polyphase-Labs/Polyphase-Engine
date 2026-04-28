#if EDITOR

#include "GitCreateBranchDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "imgui.h"
#include <cstring>

static GitCreateBranchDialog sInstance;

GitCreateBranchDialog* GetGitCreateBranchDialog()
{
    return &sInstance;
}

void GitCreateBranchDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    std::memset(mBranchName, 0, sizeof(mBranchName));
    mCheckoutAfterCreate = true;
    mSourceRefMode = 0;
    mSelectedSourceOid.clear();
    mErrorMessage.clear();
    mBranchExists = false;
}

void GitCreateBranchDialog::Open(const std::string& sourceOid)
{
    Open();
    if (!sourceOid.empty())
    {
        mSourceRefMode = 1;
        mSelectedSourceOid = sourceOid;
    }
}

void GitCreateBranchDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Create Branch");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Create Branch", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Branch Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##BranchName", mBranchName, sizeof(mBranchName));

        ImGui::Spacing();
        ImGui::Text("Source");

        ImGui::RadioButton("HEAD (current commit)", &mSourceRefMode, 0);
        ImGui::RadioButton("Specific commit", &mSourceRefMode, 1);

        if (mSourceRefMode == 1)
        {
            ImGui::Indent();
            GitRepository* repo = GitService::Get()->GetCurrentRepo();
            if (repo)
            {
                std::vector<GitCommitInfo> commits = repo->GetCommits(50);
                if (!commits.empty())
                {
                    // Find the currently selected index
                    int32_t currentIndex = 0;
                    for (int32_t i = 0; i < static_cast<int32_t>(commits.size()); ++i)
                    {
                        if (commits[i].mOid == mSelectedSourceOid)
                        {
                            currentIndex = i;
                            break;
                        }
                    }

                    // Build preview label
                    std::string previewLabel;
                    if (currentIndex < static_cast<int32_t>(commits.size()))
                    {
                        previewLabel = commits[currentIndex].mShortOid + " - " + commits[currentIndex].mSummary;
                    }

                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::BeginCombo("##SourceCommit", previewLabel.c_str()))
                    {
                        for (int32_t i = 0; i < static_cast<int32_t>(commits.size()); ++i)
                        {
                            std::string label = commits[i].mShortOid + " - " + commits[i].mSummary;
                            bool isSelected = (commits[i].mOid == mSelectedSourceOid);
                            if (ImGui::Selectable(label.c_str(), isSelected))
                            {
                                mSelectedSourceOid = commits[i].mOid;
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
        ImGui::Checkbox("Checkout after creation", &mCheckoutAfterCreate);

        // Error / existing branch message
        if (!mErrorMessage.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", mErrorMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool nameEmpty = (std::strlen(mBranchName) == 0);
        bool noRepo = (GitService::Get()->GetCurrentRepo() == nullptr);
        bool needsOid = (mSourceRefMode == 1 && mSelectedSourceOid.empty());

        if (nameEmpty || noRepo || needsOid)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            mErrorMessage.clear();
            mBranchExists = false;

            GitRepository* repo = GitService::Get()->GetCurrentRepo();
            if (repo)
            {
                std::string sourceOid;
                if (mSourceRefMode == 0)
                {
                    std::vector<GitCommitInfo> headCommits = repo->GetCommits(1);
                    if (!headCommits.empty())
                    {
                        sourceOid = headCommits[0].mOid;
                    }
                }
                else
                {
                    sourceOid = mSelectedSourceOid;
                }

                if (repo->CreateBranch(mBranchName, sourceOid, mCheckoutAfterCreate))
                {
                    repo->RefreshStatus();
                    mIsOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    mErrorMessage = "Branch '" + std::string(mBranchName) + "' already exists.";
                    mBranchExists = true;
                }
            }
        }

        if (nameEmpty || noRepo || needsOid)
        {
            ImGui::EndDisabled();
        }

        // Offer checkout if branch already exists
        if (mBranchExists)
        {
            ImGui::SameLine();
            if (ImGui::Button("Checkout Existing", ImVec2(140, 0)))
            {
                GitRepository* repo = GitService::Get()->GetCurrentRepo();
                if (repo)
                {
                    repo->CheckoutBranch(mBranchName);
                    repo->RefreshStatus();
                }
                mIsOpen = false;
                ImGui::CloseCurrentPopup();
            }
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
