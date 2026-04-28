#pragma once

#if EDITOR

class GitInitDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    char mProjectPath[512] = {0};
    char mInitialBranchName[128] = {0};
    bool mCreateInitialCommit = true;
};

GitInitDialog* GetGitInitDialog();

#endif
