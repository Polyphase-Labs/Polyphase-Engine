#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>

enum class GitChangeType
{
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Conflicted,
    Ignored,

    Count
};

enum class GitStagedState
{
    Unstaged,
    Staged,
    Both,

    Count
};

enum class GitOperationKind
{
    Clone,
    Fetch,
    Pull,
    Push,
    PushTags,

    Count
};

enum class GitOperationStatus
{
    Pending,
    InProgress,
    Success,
    Failed,
    Cancelled,

    Count
};

struct GitStatusEntry
{
    std::string mPath;
    std::string mOldPath;
    GitChangeType mChangeType = GitChangeType::Modified;
    GitStagedState mStagedState = GitStagedState::Unstaged;
};

struct GitCommitInfo
{
    std::string mOid;
    std::string mShortOid;
    std::string mSummary;
    std::string mBody;
    std::string mAuthorName;
    std::string mAuthorEmail;
    std::string mCommitterName;
    std::string mCommitterEmail;
    int64_t mTimestamp = 0;
    std::vector<std::string> mParentOids;
    std::vector<std::string> mRefNames;
    bool mIsHead = false;
};

struct GitBranchInfo
{
    std::string mName;
    std::string mUpstreamName;
    std::string mLastCommitOid;
    std::string mLastCommitSummary;
    bool mIsLocal = true;
    bool mIsCurrent = false;
    int32_t mAheadCount = 0;
    int32_t mBehindCount = 0;
};

struct GitTagInfo
{
    std::string mName;
    std::string mTargetOid;
    std::string mMessage;
    std::string mTaggerName;
    int64_t mTimestamp = 0;
    bool mIsAnnotated = false;
};

struct GitRemoteInfo
{
    std::string mName;
    std::string mFetchUrl;
    std::string mPushUrl;
    bool mIsDefault = false;
};

struct GitDiffLine
{
    std::string mContent;
    int32_t mOldLineNo = -1;
    int32_t mNewLineNo = -1;
    char mOrigin = ' ';
};

struct GitDiffHunk
{
    std::string mHeader;
    int32_t mOldStart = 0;
    int32_t mOldLines = 0;
    int32_t mNewStart = 0;
    int32_t mNewLines = 0;
    std::vector<GitDiffLine> mLines;
};

struct GitFileDiff
{
    std::string mOldPath;
    std::string mNewPath;
    bool mIsBinary = false;
    std::vector<GitDiffHunk> mHunks;
};

struct GitProgressEvent
{
    enum Phase
    {
        Counting,
        Compressing,
        Receiving,
        Resolving,
        Writing,
        Unknown
    };

    Phase mPhase = Unknown;
    int32_t mPercent = -1;
    int32_t mCurrent = 0;
    int32_t mTotal = 0;
    int64_t mBytes = 0;
};

struct GitOperationResult
{
    GitOperationKind mKind = GitOperationKind::Fetch;
    GitOperationStatus mStatus = GitOperationStatus::Pending;
    std::string mMessage;
    std::string mDetailedError;
    std::string mSuggestedAction;
    int32_t mExitCode = 0;
};

using GitCancelToken = std::shared_ptr<std::atomic_bool>;

inline GitCancelToken CreateCancelToken()
{
    return std::make_shared<std::atomic_bool>(false);
}

#endif
