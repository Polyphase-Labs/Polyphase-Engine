#pragma once

#if EDITOR

#include "../GitTypes.h"
#include <string>
#include <vector>

class GitCheckoutConfirmDialog
{
public:
    void Show(const std::string& targetRef);
    void Show(const std::string& targetRef, const std::vector<GitStatusEntry>& dirtyFiles);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    std::string mTargetRef;
    std::vector<GitStatusEntry> mDirtyFiles;
};

GitCheckoutConfirmDialog* GetGitCheckoutConfirmDialog();

#endif
