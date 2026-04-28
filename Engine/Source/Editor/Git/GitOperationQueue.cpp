#if EDITOR

#include "GitOperationQueue.h"
#include "GitProcessRunner.h"
#include "GitProgressParser.h"
#include "GitErrorMap.h"
#include "Log.h"

void GitOperationQueue::Start()
{
    mRunning = true;
    mWorkerThread = std::thread(&GitOperationQueue::WorkerLoop, this);
}

void GitOperationQueue::Stop()
{
    mRunning = false;
    mQueueCV.notify_one();

    if (mWorkerThread.joinable())
        mWorkerThread.join();
}

void GitOperationQueue::Enqueue(const GitOperationRequest& request)
{
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mPendingRequests.push_back(request);
    }
    mQueueCV.notify_one();
}

void GitOperationQueue::PollResults(std::vector<GitOperationResult>& outResults)
{
    std::lock_guard<std::mutex> lock(mResultMutex);
    outResults.insert(outResults.end(), mResults.begin(), mResults.end());
    mResults.clear();
}

bool GitOperationQueue::HasPendingOps() const
{
    if (mExecuting.load())
        return true;
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mQueueMutex));
    return !mPendingRequests.empty();
}

void GitOperationQueue::WorkerLoop()
{
    while (mRunning)
    {
        GitOperationRequest request;

        {
            std::unique_lock<std::mutex> lock(mQueueMutex);
            mQueueCV.wait(lock, [this]()
            {
                return !mPendingRequests.empty() || !mRunning;
            });

            if (!mRunning && mPendingRequests.empty())
                break;

            if (mPendingRequests.empty())
                continue;

            request = mPendingRequests.front();
            mPendingRequests.pop_front();
        }

        mExecuting.store(true);
        ExecuteOperation(request);
        mExecuting.store(false);

        // Reset progress after operation completes
        mCurrentProgress = GitProgressEvent();
    }
}

void GitOperationQueue::ExecuteOperation(const GitOperationRequest& request)
{
    // Post an in-progress result
    {
        GitOperationResult inProgress;
        inProgress.mKind = request.mKind;
        inProgress.mStatus = GitOperationStatus::InProgress;
        inProgress.mMessage = "Operation in progress...";

        std::lock_guard<std::mutex> lock(mResultMutex);
        mResults.push_back(inProgress);
    }

    std::vector<std::string> args = BuildCommandArgs(request);

    std::string stderrAccumulator;

    auto stdoutCallback = [](const std::string& /*line*/)
    {
        // stdout is not used for progress; contents are ignored for network ops
    };

    auto stderrCallback = [this, &stderrAccumulator](const std::string& line)
    {
        stderrAccumulator += line + "\n";

        GitProgressEvent progress;
        if (GitProgressParser::Parse(line, progress))
            mCurrentProgress = progress;
    };

    LogDebug("GitOperationQueue: executing operation (kind %d)", (int)request.mKind);

    int32_t exitCode = GitProcessRunner::Run(
        request.mRepoPath,
        args,
        stdoutCallback,
        stderrCallback,
        request.mCancelToken
    );

    // Build the final result
    GitOperationResult result;
    result.mKind = request.mKind;
    result.mExitCode = exitCode;

    bool cancelled = request.mCancelToken && request.mCancelToken->load();

    if (cancelled)
    {
        result.mStatus = GitOperationStatus::Cancelled;
        result.mMessage = "Operation was cancelled.";
        LogDebug("GitOperationQueue: operation cancelled (kind %d)", (int)request.mKind);
    }
    else if (exitCode != 0)
    {
        result.mStatus = GitOperationStatus::Failed;

        GitErrorMap::ErrorInfo errorInfo = GitErrorMap::MapCliError(exitCode, stderrAccumulator);
        result.mMessage = errorInfo.mFriendlyMessage;
        result.mDetailedError = stderrAccumulator;
        result.mSuggestedAction = errorInfo.mSuggestedAction;

        LogError("GitOperationQueue: operation failed (kind %d, exit %d): %s",
                 (int)request.mKind, exitCode, result.mMessage.c_str());
    }
    else
    {
        result.mStatus = GitOperationStatus::Success;
        result.mMessage = "Operation completed successfully.";
        LogDebug("GitOperationQueue: operation succeeded (kind %d)", (int)request.mKind);
    }

    // Reset progress
    mCurrentProgress = GitProgressEvent();

    {
        std::lock_guard<std::mutex> lock(mResultMutex);
        mResults.push_back(result);
    }
}

std::vector<std::string> GitOperationQueue::BuildCommandArgs(const GitOperationRequest& request) const
{
    std::vector<std::string> args;
    args.push_back("git");

    switch (request.mKind)
    {
    case GitOperationKind::Clone:
    {
        args.push_back("clone");
        args.push_back("--progress");

        if (!request.mBranchName.empty())
        {
            args.push_back("--branch");
            args.push_back(request.mBranchName);
        }

        args.push_back(request.mSourceUrl);
        args.push_back(request.mDestPath);
        break;
    }

    case GitOperationKind::Fetch:
    {
        args.push_back("fetch");
        args.push_back("--progress");

        if (request.mFetchAll)
            args.push_back("--all");

        if (request.mPrune)
            args.push_back("--prune");

        if (request.mFetchTags)
            args.push_back("--tags");

        if (!request.mRemoteName.empty())
            args.push_back(request.mRemoteName);

        break;
    }

    case GitOperationKind::Pull:
    {
        args.push_back("pull");
        args.push_back("--progress");

        if (request.mFastForwardOnly)
            args.push_back("--ff-only");

        if (!request.mRemoteName.empty())
            args.push_back(request.mRemoteName);

        if (!request.mBranchName.empty())
            args.push_back(request.mBranchName);

        break;
    }

    case GitOperationKind::Push:
    {
        args.push_back("push");
        args.push_back("--progress");
        args.push_back("-u");

        if (request.mForce)
            args.push_back("--force-with-lease");

        if (!request.mRemoteName.empty())
            args.push_back(request.mRemoteName);

        if (!request.mBranchName.empty())
            args.push_back(request.mBranchName);

        break;
    }

    case GitOperationKind::PushTags:
    {
        args.push_back("push");
        args.push_back("--tags");
        args.push_back("--progress");

        if (!request.mRemoteName.empty())
            args.push_back(request.mRemoteName);

        break;
    }

    default:
        LogWarning("GitOperationQueue: unknown operation kind %d", (int)request.mKind);
        break;
    }

    return args;
}

#endif
