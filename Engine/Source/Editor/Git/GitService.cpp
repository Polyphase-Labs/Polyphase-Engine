#if EDITOR

#include "GitService.h"
#include "GitRepository.h"
#include "GitOperationQueue.h"
#include "GitCliProbe.h"
#include "Log.h"

#include <git2.h>

static GitService* sInstance = nullptr;

void GitService::Create()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
    sInstance = new GitService();
}

void GitService::Destroy()
{
    if (sInstance != nullptr)
    {
        delete sInstance;
        sInstance = nullptr;
    }
}

GitService* GitService::Get()
{
    return sInstance;
}

GitService::GitService()
{
    git_libgit2_init();

    mOperationQueue = std::make_unique<GitOperationQueue>();
    mCliProbe = std::make_unique<GitCliProbe>();

    mOperationQueue->Start();
    mCliProbe->Probe();

    if (mCliProbe->IsGitAvailable())
        LogDebug("GitService: git CLI available (version %s)", mCliProbe->GetGitVersion().c_str());
    else
        LogWarning("GitService: git CLI not found; network operations will be unavailable");
}

GitService::~GitService()
{
    CloseRepository();
    mOperationQueue->Stop();

    git_libgit2_shutdown();
}

void GitService::Update()
{
    std::vector<GitOperationResult> results;
    mOperationQueue->PollResults(results);

    for (const GitOperationResult& result : results)
    {
        switch (result.mStatus)
        {
        case GitOperationStatus::InProgress:
            LogDebug("GitService: operation in progress (kind %d)", (int)result.mKind);
            break;

        case GitOperationStatus::Success:
            LogDebug("GitService: operation succeeded (kind %d)", (int)result.mKind);

            // Refresh local state after a successful network operation
            if (mRepository && mRepository->IsOpen())
                mRepository->RefreshStatus();
            break;

        case GitOperationStatus::Failed:
            LogError("GitService: operation failed (kind %d): %s", (int)result.mKind, result.mMessage.c_str());
            break;

        case GitOperationStatus::Cancelled:
            LogDebug("GitService: operation cancelled (kind %d)", (int)result.mKind);
            break;

        default:
            break;
        }
    }
}

bool GitService::OpenRepository(const std::string& path)
{
    CloseRepository();

    mRepository = std::make_unique<GitRepository>();

    if (!mRepository->Open(path))
    {
        LogError("GitService: failed to open repository at %s", path.c_str());
        mRepository.reset();
        return false;
    }

    mRepository->RefreshStatus();
    LogDebug("GitService: opened repository at %s", path.c_str());
    return true;
}

bool GitService::InitRepository(const std::string& path, const std::string& initialBranch)
{
    CloseRepository();

    mRepository = std::make_unique<GitRepository>();

    if (!mRepository->Init(path, initialBranch))
    {
        LogError("GitService: failed to init repository at %s", path.c_str());
        mRepository.reset();
        return false;
    }

    mRepository->RefreshStatus();
    LogDebug("GitService: initialized new repository at %s (branch: %s)", path.c_str(), initialBranch.c_str());
    return true;
}

void GitService::CloneRepository(const std::string& url, const std::string& destPath, const std::string& branch)
{
    GitOperationRequest request;
    request.mKind = GitOperationKind::Clone;
    request.mSourceUrl = url;
    request.mDestPath = destPath;
    request.mBranchName = branch;
    request.mCancelToken = CreateCancelToken();

    mOperationQueue->Enqueue(request);
    LogDebug("GitService: clone enqueued (url: %s, dest: %s)", url.c_str(), destPath.c_str());
}

void GitService::CloseRepository()
{
    if (mRepository)
    {
        LogDebug("GitService: closing repository");
        mRepository.reset();
    }
}

GitRepository* GitService::GetCurrentRepo()
{
    return mRepository.get();
}

GitOperationQueue* GitService::GetOperationQueue()
{
    return mOperationQueue.get();
}

GitCliProbe* GitService::GetCliProbe()
{
    return mCliProbe.get();
}

bool GitService::IsRepositoryOpen() const
{
    return mRepository != nullptr && mRepository->IsOpen();
}

#endif
