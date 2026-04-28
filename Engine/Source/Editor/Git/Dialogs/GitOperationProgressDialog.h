#pragma once

#if EDITOR

#include "../GitTypes.h"
#include <cstdint>

class GitOperationProgressDialog
{
public:
    void Draw();

private:
    bool mIsOpen = false;

    // Auto-close timer: when operation completes, wait 2 seconds then close
    bool mCompletionTimerStarted = false;
    float mCompletionTimer = 0.0f;
    static constexpr float kAutoCloseDelay = 2.0f;

    // Cached last known operation state
    bool mWasRunning = false;
    GitProgressEvent mLastProgress;

    // Cancel token for the current operation
    GitCancelToken mActiveCancelToken;
};

GitOperationProgressDialog* GetGitOperationProgressDialog();

#endif
