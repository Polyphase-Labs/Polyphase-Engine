#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>

class GitFetchDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    std::vector<std::string> mRemoteNames;
    int32_t mSelectedRemote = 0; // 0 = first remote, last index = "All Remotes"
    bool mFetchAll = false;

    bool mPrune = false;
    bool mFetchTags = false;
};

GitFetchDialog* GetGitFetchDialog();

#endif
