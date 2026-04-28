#pragma once

#if EDITOR

#include "../GitTypes.h"
#include <string>
#include <vector>
#include <cstdint>

class GitMergeDialog
{
public:
    void Open();
    void Open(const std::string& commitOid, const std::string& commitSummary);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    // Mode: 0 = merge branch, 1 = merge specific commit
    int32_t mMode = 0;

    // Branch mode
    std::vector<std::string> mBranchNames;
    int32_t mSelectedSourceBranch = 0;

    // Commit mode
    std::string mTargetCommitOid;
    std::string mTargetCommitSummary;

    std::string mCurrentBranch;
    bool mNoFastForward = false;

    bool mMergeInProgress = false;
    bool mMergeDone = false;
    std::string mMergeResult;
    int32_t mMergeResultLevel = 0; // 0=success, 1=warning, 2=error
};

GitMergeDialog* GetGitMergeDialog();

#endif
