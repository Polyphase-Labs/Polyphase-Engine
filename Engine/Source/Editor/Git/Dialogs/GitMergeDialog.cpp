#if EDITOR

#include "GitMergeDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "../GitProcessRunner.h"
#include "Log.h"
#include "imgui.h"
#include <cstring>
#include <thread>
#include <atomic>

static GitMergeDialog sInstance;

GitMergeDialog* GetGitMergeDialog()
{
    return &sInstance;
}

void GitMergeDialog::Open()
{
    mIsOpen = true;
    mJustOpened = true;
    mMode = 0;
    mSelectedSourceBranch = 0;
    mNoFastForward = false;
    mMergeInProgress = false;
    mMergeDone = false;
    mMergeResult.clear();
    mMergeResultLevel = 0;
    mTargetCommitOid.clear();
    mTargetCommitSummary.clear();

    mBranchNames.clear();
    mCurrentBranch.clear();

    GitService* service = GitService::Get();
    if (service && service->IsRepositoryOpen())
    {
        GitRepository* repo = service->GetCurrentRepo();
        mCurrentBranch = repo->GetCurrentBranch();

        std::vector<GitBranchInfo> branches = repo->GetBranches();
        for (const auto& branch : branches)
        {
            if (branch.mName != mCurrentBranch)
                mBranchNames.push_back(branch.mName);
        }
    }
}

void GitMergeDialog::Open(const std::string& commitOid, const std::string& commitSummary)
{
    Open();
    mMode = 1;
    mTargetCommitOid = commitOid;
    mTargetCommitSummary = commitSummary;
}

void GitMergeDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Merge###MergeDialog");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Merge###MergeDialog", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Current branch display
        ImGui::Text("Merge into:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", mCurrentBranch.c_str());

        ImGui::Spacing();

        if (mMode == 0)
        {
            // Branch mode
            ImGui::Text("Merge from branch:");
            ImGui::SetNextItemWidth(-1);
            if (!mBranchNames.empty())
            {
                if (ImGui::BeginCombo("##MergeSourceBranch", mBranchNames[mSelectedSourceBranch].c_str()))
                {
                    for (int32_t i = 0; i < (int32_t)mBranchNames.size(); ++i)
                    {
                        bool selected = (i == mSelectedSourceBranch);
                        if (ImGui::Selectable(mBranchNames[i].c_str(), selected))
                            mSelectedSourceBranch = i;
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                ImGui::TextDisabled("No other branches available to merge.");
            }
        }
        else
        {
            // Commit mode
            ImGui::Text("Merge commit:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", mTargetCommitOid.substr(0, 7).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", mTargetCommitSummary.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("No fast-forward (--no-ff)", &mNoFastForward);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Always create a merge commit, even if the merge could be fast-forwarded.");
        }

        ImGui::Spacing();

        // Conflict warning
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
            "If there are merge conflicts, resolve them in an external\n"
            "editor and then commit the result manually.");

        ImGui::Spacing();

        // Merge result display
        if (!mMergeResult.empty())
        {
            ImVec4 resultColor;
            if (mMergeResultLevel == 0)
                resultColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
            else if (mMergeResultLevel == 1)
                resultColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
            else
                resultColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, resultColor);
            ImGui::TextWrapped("%s", mMergeResult.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (mMergeInProgress)
        {
            ImGui::TextDisabled("Merge in progress...");
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();

        bool canMerge = !mMergeInProgress && !mMergeDone;
        if (mMode == 0)
            canMerge = canMerge && !mBranchNames.empty();

        if (!canMerge) ImGui::BeginDisabled();

        if (ImGui::Button("Merge", ImVec2(120, 0)))
        {
            mMergeInProgress = true;
            mMergeDone = false;
            mMergeResult.clear();
            mMergeResultLevel = 0;

            std::string repoPath;
            GitService* service = GitService::Get();
            if (service && service->IsRepositoryOpen())
                repoPath = service->GetCurrentRepo()->GetPath();

            std::string mergeTarget;
            if (mMode == 0)
                mergeTarget = mBranchNames[mSelectedSourceBranch];
            else
                mergeTarget = mTargetCommitOid;

            bool noFF = mNoFastForward;

            std::thread mergeThread([this, repoPath, mergeTarget, noFF]()
            {
                std::vector<std::string> args;
                args.push_back("git");
                args.push_back("merge");

                if (noFF)
                    args.push_back("--no-ff");

                args.push_back(mergeTarget);

                std::string stdoutAccum;
                std::string stderrAccum;

                GitCancelToken token = CreateCancelToken();
                int32_t exitCode = GitProcessRunner::Run(
                    repoPath,
                    args,
                    [&stdoutAccum](const std::string& line) { stdoutAccum += line + "\n"; },
                    [&stderrAccum](const std::string& line) { stderrAccum += line + "\n"; },
                    token
                );

                if (exitCode == 0)
                {
                    mMergeResult = "Merge completed successfully.";
                    if (!stdoutAccum.empty())
                        mMergeResult += "\n" + stdoutAccum;
                    mMergeResultLevel = 0;
                }
                else
                {
                    std::string combined = stderrAccum + stdoutAccum;
                    if (combined.find("CONFLICT") != std::string::npos ||
                        combined.find("Merge conflict") != std::string::npos)
                    {
                        mMergeResult = "Merge has conflicts. Resolve them in an external editor,\nthen stage the files and commit.\n\n" + combined;
                        mMergeResultLevel = 1;
                    }
                    else
                    {
                        mMergeResult = "Merge failed (exit " + std::to_string(exitCode) + ").\n" + combined;
                        mMergeResultLevel = 2;
                    }
                }

                mMergeInProgress = false;
                mMergeDone = true;

                GitService* svc = GitService::Get();
                if (svc && svc->IsRepositoryOpen())
                    svc->GetCurrentRepo()->RefreshStatus();
            });

            mergeThread.detach();
        }

        if (!canMerge) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0)))
        {
            mIsOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

#endif
