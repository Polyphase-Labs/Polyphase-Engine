#pragma once

#if EDITOR

#include <string>

class GitCloneDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    char mUrl[512] = {0};
    char mDestPath[512] = {0};
    char mFolderName[256] = {0};
    char mBranch[128] = {0};
};

GitCloneDialog* GetGitCloneDialog();

#endif
