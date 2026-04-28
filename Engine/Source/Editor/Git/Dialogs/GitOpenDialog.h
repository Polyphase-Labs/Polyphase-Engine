#pragma once

#if EDITOR

class GitOpenDialog
{
public:
    void Open();
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    char mLocalPath[512] = {0};
};

GitOpenDialog* GetGitOpenDialog();

#endif
