#pragma once

#if EDITOR

#include <string>

class GitCreateTagDialog
{
public:
    void Open();
    void Open(const std::string& targetOid);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    char mTagName[128] = {0};
    char mMessage[1024] = {0};
    bool mAnnotated = false;

    // Target commit: 0 = HEAD, 1 = specific commit
    int32_t mTargetMode = 0;
    std::string mSelectedTargetOid;
};

GitCreateTagDialog* GetGitCreateTagDialog();

#endif
