#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>

class GitPullDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    std::vector<std::string> mRemoteNames;
    std::vector<std::string> mBranchNames;

    int32_t mSelectedRemote = 0;
    int32_t mSelectedBranch = 0;
    int32_t mSelectedStrategy = 0; // 0 = Merge, 1 = Fast-Forward Only
};

GitPullDialog* GetGitPullDialog();

#endif
