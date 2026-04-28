#pragma once

#if EDITOR

#include <string>
#include <functional>
#include <cstdint>

class GitDeleteConfirmDialog
{
public:
    // Danger levels: 0 = normal, 1 = warning, 2 = critical (requires "I understand" checkbox)
    void Show(const std::string& title, const std::string& body, int32_t dangerLevel, std::function<void()> callback);
    void Draw();

private:
    bool mIsOpen = false;
    bool mJustOpened = false;

    std::string mTitle;
    std::string mBody;
    int32_t mDangerLevel = 0;
    std::function<void()> mCallback;
    bool mUnderstandChecked = false;
};

GitDeleteConfirmDialog* GetGitDeleteConfirmDialog();

#endif
