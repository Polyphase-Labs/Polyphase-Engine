#pragma once

#if EDITOR

#include "../GitTypes.h"
#include <string>

class GitCliStatusDialog
{
public:
    void Open();
    void Close();
    bool IsOpen() const;
    void Draw();

private:
    bool mIsOpen = false;

    // CLI test state
    bool mTestRunning = false;
    bool mTestCompleted = false;
    bool mTestSuccess = false;
    std::string mTestOutput;
    GitCancelToken mTestCancelToken;
};

GitCliStatusDialog* GetGitCliStatusDialog();

#endif
