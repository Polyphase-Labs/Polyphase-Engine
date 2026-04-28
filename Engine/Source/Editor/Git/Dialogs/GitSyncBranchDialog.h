#pragma once

#if EDITOR

#include "../GitTypes.h"
#include <string>
#include <vector>
#include <cstdint>

class GitSyncBranchDialog
{
public:
    void Open(const std::string& branchName);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    std::string mTargetBranch;
    std::string mCurrentBranch;

    // Commit selector
    std::vector<GitCommitInfo> mCommits;
    int32_t mSelectedCommitIndex = 0;

    bool mCheckoutAfter = false;
    bool mNoFastForward = false;

    // Operation state
    bool mRunning = false;
    bool mDone = false;
    std::string mResultMessage;
    int32_t mResultLevel = 0; // 0=success, 1=warning, 2=error
};

GitSyncBranchDialog* GetGitSyncBranchDialog();

#endif
