#pragma once

#if EDITOR

#include <string>

class GitCreateBranchDialog
{
public:
    void Open();
    void Open(const std::string& sourceOid);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    char mBranchName[128] = {0};
    bool mCheckoutAfterCreate = true;

    // Source ref selection: 0 = HEAD, 1 = specific commit
    int32_t mSourceRefMode = 0;
    std::string mSelectedSourceOid;

    // Error display
    std::string mErrorMessage;
    bool mBranchExists = false;
};

GitCreateBranchDialog* GetGitCreateBranchDialog();

#endif
