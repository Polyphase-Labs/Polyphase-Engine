#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>

class GitPushDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    // Cached data
    std::vector<std::string> mRemoteNames;
    std::vector<std::string> mBranchNames;

    int32_t mSelectedRemote = 0;
    int32_t mSelectedSourceBranch = 0;
    int32_t mSelectedDestBranch = 0;

    bool mPushTags = false;
    bool mSetUpstream = false;
    bool mShowForcePush = false;
    bool mForcePush = false;
};

GitPushDialog* GetGitPushDialog();

#endif
