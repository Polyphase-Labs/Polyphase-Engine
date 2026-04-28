#pragma once

#if EDITOR

#include "GitTypes.h"
#include "GitOperationRequest.h"
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

class GitOperationQueue
{
public:
    void Start();
    void Stop();

    void Enqueue(const GitOperationRequest& request);
    void PollResults(std::vector<GitOperationResult>& outResults);

    bool IsRunning() const { return mRunning.load(); }
    bool HasPendingOps() const;

    const GitProgressEvent& GetCurrentProgress() const { return mCurrentProgress; }

private:
    void WorkerLoop();
    void ExecuteOperation(const GitOperationRequest& request);
    std::vector<std::string> BuildCommandArgs(const GitOperationRequest& request) const;

    std::thread mWorkerThread;
    std::atomic_bool mRunning{false};
    std::atomic_bool mExecuting{false};

    std::mutex mQueueMutex;
    std::condition_variable mQueueCV;
    std::deque<GitOperationRequest> mPendingRequests;

    std::mutex mResultMutex;
    std::deque<GitOperationResult> mResults;

    GitProgressEvent mCurrentProgress;
};

#endif
