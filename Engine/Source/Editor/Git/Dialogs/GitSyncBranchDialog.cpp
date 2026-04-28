#if EDITOR

#include "GitSyncBranchDialog.h"
#include "../GitService.h"
#include "../GitRepository.h"
#include "../GitProcessRunner.h"
#include "Log.h"
#include "imgui.h"
#include <thread>
#include <cstring>

static GitSyncBranchDialog sInstance;

GitSyncBranchDialog* GetGitSyncBranchDialog()
{
    return &sInstance;
}

void GitSyncBranchDialog::Open(const std::string& branchName)
{
    mIsOpen = true;
    mJustOpened = true;
    mTargetBranch = branchName;
    mSelectedCommitIndex = 0;
    mCheckoutAfter = false;
    mNoFastForward = false;
    mRunning = false;
    mDone = false;
    mResultMessage.clear();
    mResultLevel = 0;
    mCommits.clear();
    mCurrentBranch.clear();

    GitService* service = GitService::Get();
    if (service && service->IsRepositoryOpen())
    {
        GitRepository* repo = service->GetCurrentRepo();
        mCurrentBranch = repo->GetCurrentBranch();
        mCommits = repo->GetCommits(100);
    }
}

void GitSyncBranchDialog::Draw()
{
    if (!mIsOpen) return;

    if (mJustOpened)
    {
        ImGui::OpenPopup("Sync Branch With Commit###SyncBranch");
        mJustOpened = false;
    }

    ImVec2 center = ImGui::GetIO().DisplaySize;
    center.x *= 0.5f;
    center.y *= 0.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(550, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Sync Branch With Commit###SyncBranch", &mIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Target branch
        ImGui::Text("Branch to sync:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", mTargetBranch.c_str());

        ImGui::Spacing();

        // Commit selector
        ImGui::Text("Merge commit into branch:");
        ImGui::SetNextItemWidth(-1);
        if (!mCommits.empty())
        {
            std::string preview;
            if (mSelectedCommitIndex >= 0 && mSelectedCommitIndex < (int32_t)mCommits.size())
            {
                const auto& c = mCommits[mSelectedCommitIndex];
                preview = c.mShortOid + " - " + c.mSummary;
            }

            if (ImGui::BeginCombo("##SyncCommit", preview.c_str()))
            {
                for (int32_t i = 0; i < (int32_t)mCommits.size(); ++i)
                {
                    const auto& c = mCommits[i];
                    std::string label = c.mShortOid + " - " + c.mSummary;
                    bool selected = (mSelectedCommitIndex == i);
                    if (ImGui::Selectable(label.c_str(), selected))
                        mSelectedCommitIndex = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::TextDisabled("No commits found.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Checkout branch after sync", &mCheckoutAfter);
        ImGui::Checkbox("No fast-forward (--no-ff)", &mNoFastForward);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Always create a merge commit, even if the merge could be fast-forwarded.");
        }

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
            "This will: stash changes, checkout the branch, merge the\n"
            "selected commit, then restore your previous branch and\n"
            "unstash changes. If there are conflicts, resolve them\n"
            "in an external editor.");

        ImGui::Spacing();

        // Result display
        if (!mResultMessage.empty())
        {
            ImVec4 color;
            if (mResultLevel == 0)
                color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
            else if (mResultLevel == 1)
                color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
            else
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", mResultMessage.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (mRunning)
        {
            ImGui::TextDisabled("Sync in progress...");
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();

        bool canSync = !mRunning && !mDone && !mCommits.empty();
        if (!canSync) ImGui::BeginDisabled();

        if (ImGui::Button("Sync", ImVec2(120, 0)))
        {
            mRunning = true;
            mDone = false;
            mResultMessage.clear();
            mResultLevel = 0;

            std::string repoPath;
            GitService* service = GitService::Get();
            if (service && service->IsRepositoryOpen())
                repoPath = service->GetCurrentRepo()->GetPath();

            std::string targetBranch = mTargetBranch;
            std::string currentBranch = mCurrentBranch;
            std::string commitOid = mCommits[mSelectedCommitIndex].mOid;
            bool noFF = mNoFastForward;
            bool checkoutAfter = mCheckoutAfter;

            std::thread syncThread([this, repoPath, targetBranch, currentBranch, commitOid, noFF, checkoutAfter]()
            {
                auto runGit = [&](const std::vector<std::string>& args, std::string& output) -> int32_t
                {
                    GitCancelToken token = CreateCancelToken();
                    return GitProcessRunner::Run(
                        repoPath, args,
                        [&output](const std::string& line) { output += line + "\n"; },
                        [&output](const std::string& line) { output += line + "\n"; },
                        token
                    );
                };

                std::string output;
                int32_t exitCode = 0;
                bool hasConflicts = false;

                // 1. Stash current changes
                output.clear();
                exitCode = runGit({"git", "stash", "push", "-m", "polyphase-sync-temp"}, output);
                bool stashed = (output.find("No local changes") == std::string::npos && exitCode == 0);
                LogDebug("GitSyncBranch: stash %s", stashed ? "saved" : "skipped (no changes)");

                // 2. Checkout target branch
                output.clear();
                exitCode = runGit({"git", "checkout", targetBranch}, output);
                if (exitCode != 0)
                {
                    mResultMessage = "Failed to checkout branch '" + targetBranch + "'.\n" + output;
                    mResultLevel = 2;
                    // Try to restore
                    if (stashed) { std::string tmp; runGit({"git", "stash", "pop"}, tmp); }
                    mRunning = false;
                    mDone = true;
                    return;
                }

                // 3. Merge the commit
                output.clear();
                std::vector<std::string> mergeArgs = {"git", "merge"};
                if (noFF)
                    mergeArgs.push_back("--no-ff");
                mergeArgs.push_back(commitOid);
                exitCode = runGit(mergeArgs, output);

                if (exitCode != 0)
                {
                    if (output.find("CONFLICT") != std::string::npos)
                    {
                        hasConflicts = true;
                        mResultMessage = "Merge has conflicts on branch '" + targetBranch + "'.\n"
                            "Resolve them in an external editor, then commit.\n\n" + output;
                        mResultLevel = 1;
                    }
                    else
                    {
                        mResultMessage = "Merge failed on branch '" + targetBranch + "'.\n" + output;
                        mResultLevel = 2;
                    }
                }
                else
                {
                    mResultMessage = "Successfully synced '" + targetBranch + "' with commit " + commitOid.substr(0, 7) + ".";
                    mResultLevel = 0;
                }

                // 4. Checkout back to original branch (unless user wants to stay or conflicts)
                if (!hasConflicts && !checkoutAfter && currentBranch != targetBranch && currentBranch != "(detached)")
                {
                    std::string tmp;
                    runGit({"git", "checkout", currentBranch}, tmp);
                }

                // 5. Unstash
                if (stashed && !hasConflicts)
                {
                    std::string tmp;
                    int32_t popExit = runGit({"git", "stash", "pop"}, tmp);
                    if (popExit != 0)
                    {
                        mResultMessage += "\nWarning: failed to unstash changes.\n" + tmp;
                        if (mResultLevel == 0)
                            mResultLevel = 1;
                    }
                }

                mRunning = false;
                mDone = true;

                GitService* svc = GitService::Get();
                if (svc && svc->IsRepositoryOpen())
                    svc->GetCurrentRepo()->RefreshStatus();
            });

            syncThread.detach();
        }

        if (!canSync) ImGui::EndDisabled();

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
