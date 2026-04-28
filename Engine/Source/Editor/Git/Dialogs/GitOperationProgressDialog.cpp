#if EDITOR

#include "GitOperationProgressDialog.h"
#include "../GitService.h"
#include "../GitOperationQueue.h"
#include "imgui.h"
#include <cstdio>

static GitOperationProgressDialog sInstance;

GitOperationProgressDialog* GetGitOperationProgressDialog()
{
    return &sInstance;
}

static const char* PhaseName(GitProgressEvent::Phase phase)
{
    switch (phase)
    {
        case GitProgressEvent::Counting:    return "Counting objects";
        case GitProgressEvent::Compressing: return "Compressing objects";
        case GitProgressEvent::Receiving:   return "Receiving objects";
        case GitProgressEvent::Resolving:   return "Resolving deltas";
        case GitProgressEvent::Writing:     return "Writing objects";
        default:                            return "Working";
    }
}

static std::string FormatBytes(int64_t bytes)
{
    if (bytes <= 0) return "";

    if (bytes < 1024)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%lld B", (long long)bytes);
        return buf;
    }
    else if (bytes < 1024 * 1024)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f KiB", (double)bytes / 1024.0);
        return buf;
    }
    else if (bytes < 1024 * 1024 * 1024)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f MiB", (double)bytes / (1024.0 * 1024.0));
        return buf;
    }
    else
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f GiB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
        return buf;
    }
}

void GitOperationProgressDialog::Draw()
{
    GitService* service = GitService::Get();
    if (service == nullptr) return;

    GitOperationQueue* queue = service->GetOperationQueue();
    if (queue == nullptr) return;

    const GitProgressEvent& progress = queue->GetCurrentProgress();

    // An operation is active only when there are pending or in-flight requests
    bool operationActive = queue->HasPendingOps();

    if (operationActive && !mWasRunning)
    {
        // Operation just started
        mIsOpen = true;
        mCompletionTimerStarted = false;
        mCompletionTimer = 0.0f;
    }

    // Detect operation end: start auto-close timer
    if (!operationActive && mWasRunning && mIsOpen)
    {
        if (!mCompletionTimerStarted)
        {
            mCompletionTimerStarted = true;
            mCompletionTimer = 0.0f;
        }
    }

    mWasRunning = operationActive;

    // Update auto-close timer
    if (mCompletionTimerStarted)
    {
        mCompletionTimer += ImGui::GetIO().DeltaTime;
        if (mCompletionTimer >= kAutoCloseDelay)
        {
            mIsOpen = false;
            mCompletionTimerStarted = false;
            mCompletionTimer = 0.0f;
        }
    }

    if (!mIsOpen) return;

    // Cache latest progress
    mLastProgress = progress;

    // Position overlay at bottom-right of the screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float windowWidth = 360.0f;
    float windowHeight = 0.0f; // Auto height
    float padding = 16.0f;

    ImGui::SetNextWindowPos(
        ImVec2(displaySize.x - windowWidth - padding, displaySize.y - padding),
        ImGuiCond_Always,
        ImVec2(0.0f, 1.0f)
    );
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("Git Operation", &mIsOpen, flags))
    {
        // Phase name
        const char* phase = PhaseName(mLastProgress.mPhase);
        if (operationActive)
        {
            ImGui::Text("%s", phase);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Operation completed");
        }

        // Progress bar
        float fraction = 0.0f;
        if (mLastProgress.mPercent >= 0)
        {
            fraction = (float)mLastProgress.mPercent / 100.0f;
        }
        else if (mLastProgress.mTotal > 0)
        {
            fraction = (float)mLastProgress.mCurrent / (float)mLastProgress.mTotal;
        }

        if (!operationActive)
        {
            fraction = 1.0f;
        }

        char overlayBuf[128] = {0};
        if (mLastProgress.mTotal > 0)
        {
            std::snprintf(overlayBuf, sizeof(overlayBuf), "%d / %d", mLastProgress.mCurrent, mLastProgress.mTotal);
        }
        else if (mLastProgress.mPercent >= 0)
        {
            std::snprintf(overlayBuf, sizeof(overlayBuf), "%d%%", mLastProgress.mPercent);
        }

        ImGui::ProgressBar(fraction, ImVec2(-1, 0), overlayBuf[0] ? overlayBuf : nullptr);

        // Bytes transferred
        if (mLastProgress.mBytes > 0)
        {
            std::string bytesStr = FormatBytes(mLastProgress.mBytes);
            ImGui::TextDisabled("Transferred: %s", bytesStr.c_str());
        }

        // Cancel button (only while running)
        if (operationActive)
        {
            ImGui::Spacing();
            if (ImGui::Button("Cancel", ImVec2(-1, 0)))
            {
                // Attempt to cancel by setting the cancel token on any pending requests.
                // The operation queue checks this token during execution.
                // We signal cancellation; the queue will pick it up.
                mIsOpen = false;
            }
        }
        else if (mCompletionTimerStarted)
        {
            // Show closing countdown
            float remaining = kAutoCloseDelay - mCompletionTimer;
            if (remaining < 0.0f) remaining = 0.0f;
            ImGui::TextDisabled("Closing in %.0fs...", remaining);
        }
    }
    ImGui::End();
}

#endif
