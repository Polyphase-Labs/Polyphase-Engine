#pragma once

#if EDITOR

#include "GitTypes.h"
#include <string>

struct GitOperationRequest
{
    GitOperationKind mKind = GitOperationKind::Fetch;
    std::string mRepoPath;
    std::string mRemoteName;
    std::string mBranchName;
    std::string mSourceUrl;
    std::string mDestPath;
    bool mForce = false;
    bool mPushTags = false;
    bool mSetUpstream = false;
    bool mPrune = false;
    bool mFetchAll = false;
    bool mFetchTags = false;
    bool mFastForwardOnly = false;
    GitCancelToken mCancelToken;
};

#endif
