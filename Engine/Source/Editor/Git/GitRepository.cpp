#if EDITOR

#include "GitRepository.h"
#include "Log.h"

#include <git2.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <ctime>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* SafeErrorMessage()
{
    const git_error* err = git_error_last();
    return (err && err->message) ? err->message : "unknown error";
}

static std::string OidToString(const git_oid* oid)
{
    char buf[GIT_OID_HEXSZ + 1];
    git_oid_tostr(buf, sizeof(buf), oid);
    return std::string(buf);
}

static std::string OidToShortString(const git_oid* oid)
{
    char buf[GIT_OID_HEXSZ + 1];
    git_oid_tostr(buf, sizeof(buf), oid);
    return std::string(buf, 7);
}

static std::string TrimString(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static GitChangeType MapStatusToChangeType(git_status_t flags, bool staged)
{
    unsigned int mask = staged
        ? (GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_INDEX_DELETED |
           GIT_STATUS_INDEX_RENAMED | GIT_STATUS_INDEX_TYPECHANGE)
        : (GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED | GIT_STATUS_WT_DELETED |
           GIT_STATUS_WT_RENAMED | GIT_STATUS_WT_TYPECHANGE);

    unsigned int relevant = flags & mask;

    if (staged)
    {
        if (relevant & GIT_STATUS_INDEX_NEW)        return GitChangeType::Added;
        if (relevant & GIT_STATUS_INDEX_DELETED)    return GitChangeType::Deleted;
        if (relevant & GIT_STATUS_INDEX_RENAMED)    return GitChangeType::Renamed;
        if (relevant & GIT_STATUS_INDEX_MODIFIED)   return GitChangeType::Modified;
        if (relevant & GIT_STATUS_INDEX_TYPECHANGE) return GitChangeType::Modified;
    }
    else
    {
        if (relevant & GIT_STATUS_WT_NEW)        return GitChangeType::Untracked;
        if (relevant & GIT_STATUS_WT_DELETED)    return GitChangeType::Deleted;
        if (relevant & GIT_STATUS_WT_RENAMED)    return GitChangeType::Renamed;
        if (relevant & GIT_STATUS_WT_MODIFIED)   return GitChangeType::Modified;
        if (relevant & GIT_STATUS_WT_TYPECHANGE) return GitChangeType::Modified;
    }

    if (flags & GIT_STATUS_CONFLICTED)
        return GitChangeType::Conflicted;

    if (flags & GIT_STATUS_IGNORED)
        return GitChangeType::Ignored;

    return GitChangeType::Modified;
}

static void ExtractDiffHunks(git_patch* patch, GitFileDiff& outDiff)
{
    size_t hunkCount = git_patch_num_hunks(patch);
    for (size_t h = 0; h < hunkCount; ++h)
    {
        const git_diff_hunk* rawHunk = nullptr;
        size_t lineCount = 0;
        if (git_patch_get_hunk(&rawHunk, &lineCount, patch, h) != 0)
            continue;

        GitDiffHunk hunk;
        hunk.mHeader = rawHunk->header;
        hunk.mOldStart = rawHunk->old_start;
        hunk.mOldLines = rawHunk->old_lines;
        hunk.mNewStart = rawHunk->new_start;
        hunk.mNewLines = rawHunk->new_lines;

        for (size_t l = 0; l < lineCount; ++l)
        {
            const git_diff_line* rawLine = nullptr;
            if (git_patch_get_line_in_hunk(&rawLine, patch, h, l) != 0)
                continue;

            GitDiffLine line;
            line.mContent = std::string(rawLine->content, rawLine->content_len);
            line.mOldLineNo = rawLine->old_lineno;
            line.mNewLineNo = rawLine->new_lineno;
            line.mOrigin = static_cast<char>(rawLine->origin);
            hunk.mLines.push_back(std::move(line));
        }

        outDiff.mHunks.push_back(std::move(hunk));
    }
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GitRepository::GitRepository()
{
}

GitRepository::~GitRepository()
{
    Close();
}

// ---------------------------------------------------------------------------
// Open / Init / Close
// ---------------------------------------------------------------------------

static std::string CleanPath(const std::string& path)
{
    std::string clean = path;
    while (!clean.empty() && (clean.back() == '/' || clean.back() == '\\'))
        clean.pop_back();
    return clean;
}

bool GitRepository::Open(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (mRepo)
    {
        git_repository_free(mRepo);
        mRepo = nullptr;
    }

    std::string cleanPath = CleanPath(path);
    int err = git_repository_open(&mRepo, cleanPath.c_str());
    if (err != 0)
    {
        LogError("GitRepository::Open: %s", SafeErrorMessage());
        mRepo = nullptr;
        return false;
    }

    mPath = cleanPath;
    LogDebug("GitRepository::Open: opened repository at %s", mPath.c_str());
    return true;
}

bool GitRepository::Init(const std::string& path, const std::string& initialBranch)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (mRepo)
    {
        git_repository_free(mRepo);
        mRepo = nullptr;
    }

    std::string cleanPath = CleanPath(path);
    int err = git_repository_init(&mRepo, cleanPath.c_str(), 0);
    if (err != 0)
    {
        LogError("GitRepository::Init: %s", SafeErrorMessage());
        mRepo = nullptr;
        return false;
    }

    mPath = cleanPath;

    // If the caller wants a branch name other than the default "master",
    // rename the HEAD symbolic reference target.
    if (!initialBranch.empty() && initialBranch != "master")
    {
        git_reference* headRef = nullptr;
        if (git_repository_head(&headRef, mRepo) == 0)
        {
            git_reference* newRef = nullptr;
            std::string newTarget = "refs/heads/" + initialBranch;
            int renameErr = git_reference_rename(&newRef, headRef, newTarget.c_str(), 1,
                                                  "init: rename default branch");
            if (renameErr != 0)
            {
                LogWarning("GitRepository::Init: could not rename default branch to %s: %s",
                           initialBranch.c_str(), SafeErrorMessage());
            }
            git_reference_free(newRef);
            git_reference_free(headRef);
        }
        else
        {
            // No HEAD yet (empty repo) -- just set the symbolic ref manually
            git_reference* symbolicRef = nullptr;
            std::string target = "refs/heads/" + initialBranch;
            int symErr = git_reference_symbolic_create(&symbolicRef, mRepo, "HEAD", target.c_str(),
                                                        1, "init: set initial branch");
            if (symErr != 0)
            {
                LogWarning("GitRepository::Init: could not set initial branch to %s: %s",
                           initialBranch.c_str(), SafeErrorMessage());
            }
            git_reference_free(symbolicRef);
        }
    }

    LogDebug("GitRepository::Init: initialized repository at %s (branch: %s)",
             mPath.c_str(), initialBranch.c_str());
    return true;
}

void GitRepository::Close()
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (mRepo)
    {
        git_repository_free(mRepo);
        mRepo = nullptr;
    }

    mPath.clear();
    mCachedStatus.clear();
}

bool GitRepository::IsOpen() const
{
    return mRepo != nullptr;
}

const std::string& GitRepository::GetPath() const
{
    return mPath;
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

void GitRepository::RefreshStatus()
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    mCachedStatus.clear();

    if (!mRepo)
        return;

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                 GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                 GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                 GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    git_status_list* statusList = nullptr;
    int err = git_status_list_new(&statusList, mRepo, &opts);
    if (err != 0)
    {
        LogError("GitRepository::RefreshStatus: %s", SafeErrorMessage());
        return;
    }

    size_t count = git_status_list_entrycount(statusList);
    for (size_t i = 0; i < count; ++i)
    {
        const git_status_entry* entry = git_status_byindex(statusList, i);
        if (!entry)
            continue;

        git_status_t flags = static_cast<git_status_t>(entry->status);

        // Determine staged state
        bool hasIndex = (flags & (GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED |
                                  GIT_STATUS_INDEX_DELETED | GIT_STATUS_INDEX_RENAMED |
                                  GIT_STATUS_INDEX_TYPECHANGE)) != 0;
        bool hasWorkdir = (flags & (GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED |
                                    GIT_STATUS_WT_DELETED | GIT_STATUS_WT_RENAMED |
                                    GIT_STATUS_WT_TYPECHANGE)) != 0;
        bool isConflicted = (flags & GIT_STATUS_CONFLICTED) != 0;

        if (hasIndex && hasWorkdir)
        {
            // File has both staged and unstaged changes -- emit two entries
            GitStatusEntry staged;
            staged.mStagedState = GitStagedState::Staged;
            staged.mChangeType = MapStatusToChangeType(flags, true);
            if (entry->head_to_index)
            {
                staged.mPath = entry->head_to_index->new_file.path
                    ? entry->head_to_index->new_file.path : "";
                staged.mOldPath = entry->head_to_index->old_file.path
                    ? entry->head_to_index->old_file.path : "";
            }
            mCachedStatus.push_back(std::move(staged));

            GitStatusEntry unstaged;
            unstaged.mStagedState = GitStagedState::Unstaged;
            unstaged.mChangeType = MapStatusToChangeType(flags, false);
            if (entry->index_to_workdir)
            {
                unstaged.mPath = entry->index_to_workdir->new_file.path
                    ? entry->index_to_workdir->new_file.path : "";
                unstaged.mOldPath = entry->index_to_workdir->old_file.path
                    ? entry->index_to_workdir->old_file.path : "";
            }
            mCachedStatus.push_back(std::move(unstaged));
        }
        else
        {
            GitStatusEntry statusEntry;
            if (hasIndex)
            {
                statusEntry.mStagedState = GitStagedState::Staged;
                statusEntry.mChangeType = MapStatusToChangeType(flags, true);
                if (entry->head_to_index)
                {
                    statusEntry.mPath = entry->head_to_index->new_file.path
                        ? entry->head_to_index->new_file.path : "";
                    statusEntry.mOldPath = entry->head_to_index->old_file.path
                        ? entry->head_to_index->old_file.path : "";
                }
            }
            else
            {
                statusEntry.mStagedState = GitStagedState::Unstaged;
                statusEntry.mChangeType = isConflicted
                    ? GitChangeType::Conflicted
                    : MapStatusToChangeType(flags, false);
                if (entry->index_to_workdir)
                {
                    statusEntry.mPath = entry->index_to_workdir->new_file.path
                        ? entry->index_to_workdir->new_file.path : "";
                    statusEntry.mOldPath = entry->index_to_workdir->old_file.path
                        ? entry->index_to_workdir->old_file.path : "";
                }
            }
            mCachedStatus.push_back(std::move(statusEntry));
        }
    }

    git_status_list_free(statusList);
}

const std::vector<GitStatusEntry>& GitRepository::GetStatus() const
{
    return mCachedStatus;
}

// ---------------------------------------------------------------------------
// Branch info
// ---------------------------------------------------------------------------

std::string GitRepository::GetCurrentBranch() const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return "";

    if (git_repository_head_detached(mRepo))
        return "(detached)";

    git_reference* headRef = nullptr;
    int err = git_repository_head(&headRef, mRepo);
    if (err != 0)
    {
        // Unborn branch -- read symbolic target of HEAD
        git_reference* symbolicRef = nullptr;
        if (git_reference_lookup(&symbolicRef, mRepo, "HEAD") == 0)
        {
            const char* target = git_reference_symbolic_target(symbolicRef);
            std::string name;
            if (target)
            {
                name = target;
                const std::string prefix = "refs/heads/";
                if (name.rfind(prefix, 0) == 0)
                    name = name.substr(prefix.size());
            }
            git_reference_free(symbolicRef);
            return name;
        }
        return "";
    }

    const char* shorthand = git_reference_shorthand(headRef);
    std::string result = shorthand ? shorthand : "";
    git_reference_free(headRef);
    return result;
}

std::vector<GitBranchInfo> GitRepository::GetBranches() const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    std::vector<GitBranchInfo> branches;

    if (!mRepo)
        return branches;

    git_branch_iterator* iter = nullptr;
    int err = git_branch_iterator_new(&iter, mRepo, GIT_BRANCH_ALL);
    if (err != 0)
    {
        LogError("GitRepository::GetBranches: %s", SafeErrorMessage());
        return branches;
    }

    git_reference* ref = nullptr;
    git_branch_t branchType;

    while (git_branch_next(&ref, &branchType, iter) == 0)
    {
        GitBranchInfo info;
        const char* branchName = nullptr;
        git_branch_name(&branchName, ref);
        info.mName = branchName ? branchName : "";
        info.mIsLocal = (branchType == GIT_BRANCH_LOCAL);
        info.mIsCurrent = (git_branch_is_head(ref) != 0);

        // Last commit on this branch
        const git_oid* tipOid = git_reference_target(ref);
        if (tipOid)
        {
            info.mLastCommitOid = OidToString(tipOid);

            git_commit* tipCommit = nullptr;
            if (git_commit_lookup(&tipCommit, mRepo, tipOid) == 0)
            {
                const char* summary = git_commit_summary(tipCommit);
                info.mLastCommitSummary = summary ? summary : "";
                git_commit_free(tipCommit);
            }
        }

        // Upstream tracking info (only for local branches)
        if (info.mIsLocal)
        {
            git_reference* upstream = nullptr;
            if (git_branch_upstream(&upstream, ref) == 0)
            {
                const char* upstreamName = nullptr;
                git_branch_name(&upstreamName, upstream);
                info.mUpstreamName = upstreamName ? upstreamName : "";

                // Ahead / behind counts
                const git_oid* localOid = git_reference_target(ref);
                const git_oid* upstreamOid = git_reference_target(upstream);
                if (localOid && upstreamOid)
                {
                    size_t ahead = 0, behind = 0;
                    if (git_graph_ahead_behind(&ahead, &behind, mRepo, localOid, upstreamOid) == 0)
                    {
                        info.mAheadCount = static_cast<int32_t>(ahead);
                        info.mBehindCount = static_cast<int32_t>(behind);
                    }
                }

                git_reference_free(upstream);
            }
        }

        branches.push_back(std::move(info));
        git_reference_free(ref);
        ref = nullptr;
    }

    git_branch_iterator_free(iter);
    return branches;
}

bool GitRepository::CreateBranch(const std::string& name, const std::string& sourceOid, bool checkout)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    // Resolve the source commit
    git_oid oid;
    if (git_oid_fromstr(&oid, sourceOid.c_str()) != 0)
    {
        LogError("GitRepository::CreateBranch: invalid OID %s", sourceOid.c_str());
        return false;
    }

    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, mRepo, &oid) != 0)
    {
        LogError("GitRepository::CreateBranch: %s", SafeErrorMessage());
        return false;
    }

    git_reference* branchRef = nullptr;
    int err = git_branch_create(&branchRef, mRepo, name.c_str(), commit, 0);
    git_commit_free(commit);

    if (err != 0)
    {
        // GIT_EEXISTS (-4) is expected when the caller will fall back to checkout
        if (err == -4)
            LogDebug("GitRepository::CreateBranch: branch '%s' already exists", name.c_str());
        else
            LogError("GitRepository::CreateBranch: %s", SafeErrorMessage());
        return false;
    }

    if (checkout)
    {
        // Checkout the newly created branch
        git_object* treeish = nullptr;
        if (git_reference_peel(&treeish, branchRef, GIT_OBJECT_TREE) == 0)
        {
            git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
            opts.checkout_strategy = GIT_CHECKOUT_SAFE;
            git_checkout_tree(mRepo, treeish, &opts);
            git_object_free(treeish);
        }

        std::string refName = "refs/heads/" + name;
        git_repository_set_head(mRepo, refName.c_str());
    }

    git_reference_free(branchRef);
    LogDebug("GitRepository::CreateBranch: created branch %s", name.c_str());
    return true;
}

bool GitRepository::DeleteBranch(const std::string& name, bool force)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_reference* ref = nullptr;
    std::string refName = "refs/heads/" + name;
    int err = git_reference_lookup(&ref, mRepo, refName.c_str());
    if (err != 0)
    {
        LogError("GitRepository::DeleteBranch: %s", SafeErrorMessage());
        return false;
    }

    if (!force && git_branch_is_head(ref))
    {
        LogError("GitRepository::DeleteBranch: cannot delete the current branch without force");
        git_reference_free(ref);
        return false;
    }

    err = git_branch_delete(ref);
    git_reference_free(ref);

    if (err != 0)
    {
        LogError("GitRepository::DeleteBranch: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::DeleteBranch: deleted branch %s", name.c_str());
    return true;
}

bool GitRepository::CheckoutBranch(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    std::string refName = "refs/heads/" + name;

    // Resolve the branch ref to a commit tree for checkout
    git_reference* ref = nullptr;
    int err = git_reference_lookup(&ref, mRepo, refName.c_str());
    if (err != 0)
    {
        LogError("GitRepository::CheckoutBranch: %s", SafeErrorMessage());
        return false;
    }

    git_object* treeish = nullptr;
    err = git_reference_peel(&treeish, ref, GIT_OBJECT_TREE);
    if (err != 0)
    {
        LogError("GitRepository::CheckoutBranch: %s", SafeErrorMessage());
        git_reference_free(ref);
        return false;
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    err = git_checkout_tree(mRepo, treeish, &opts);
    git_object_free(treeish);
    git_reference_free(ref);

    if (err != 0)
    {
        LogError("GitRepository::CheckoutBranch: %s", SafeErrorMessage());
        return false;
    }

    err = git_repository_set_head(mRepo, refName.c_str());
    if (err != 0)
    {
        LogError("GitRepository::CheckoutBranch: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::CheckoutBranch: checked out %s", name.c_str());
    return true;
}

bool GitRepository::CheckoutCommit(const std::string& oid)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_oid commitOid;
    if (git_oid_fromstr(&commitOid, oid.c_str()) != 0)
    {
        LogError("GitRepository::CheckoutCommit: invalid OID %s", oid.c_str());
        return false;
    }

    git_commit* commit = nullptr;
    int err = git_commit_lookup(&commit, mRepo, &commitOid);
    if (err != 0)
    {
        LogError("GitRepository::CheckoutCommit: %s", SafeErrorMessage());
        return false;
    }

    git_object* treeish = reinterpret_cast<git_object*>(commit);

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    err = git_checkout_tree(mRepo, treeish, &opts);
    if (err != 0)
    {
        LogError("GitRepository::CheckoutCommit: %s", SafeErrorMessage());
        git_commit_free(commit);
        return false;
    }

    err = git_repository_set_head_detached(mRepo, &commitOid);
    git_commit_free(commit);

    if (err != 0)
    {
        LogError("GitRepository::CheckoutCommit: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::CheckoutCommit: detached HEAD at %s", oid.c_str());
    return true;
}

bool GitRepository::SetUpstream(const std::string& branch, const std::string& remote,
                                const std::string& remoteBranch)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_reference* branchRef = nullptr;
    std::string refName = "refs/heads/" + branch;
    int err = git_reference_lookup(&branchRef, mRepo, refName.c_str());
    if (err != 0)
    {
        LogError("GitRepository::SetUpstream: %s", SafeErrorMessage());
        return false;
    }

    std::string upstreamSpec = remote + "/" + remoteBranch;
    err = git_branch_set_upstream(branchRef, upstreamSpec.c_str());
    git_reference_free(branchRef);

    if (err != 0)
    {
        LogError("GitRepository::SetUpstream: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::SetUpstream: %s -> %s", branch.c_str(), upstreamSpec.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

std::vector<GitTagInfo> GitRepository::GetTags() const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    std::vector<GitTagInfo> tags;

    if (!mRepo)
        return tags;

    git_strarray tagNames = { nullptr, 0 };
    int err = git_tag_list(&tagNames, mRepo);
    if (err != 0)
    {
        LogError("GitRepository::GetTags: %s", SafeErrorMessage());
        return tags;
    }

    for (size_t i = 0; i < tagNames.count; ++i)
    {
        GitTagInfo info;
        info.mName = tagNames.strings[i];

        std::string refName = "refs/tags/" + info.mName;
        git_reference* ref = nullptr;
        if (git_reference_lookup(&ref, mRepo, refName.c_str()) != 0)
            continue;

        // Resolve to final target
        git_object* target = nullptr;
        if (git_reference_peel(&target, ref, GIT_OBJECT_COMMIT) == 0)
        {
            info.mTargetOid = OidToString(git_object_id(target));
            git_object_free(target);
        }

        // Check if it is an annotated tag
        const git_oid* tagOid = git_reference_target(ref);
        git_tag* tagObj = nullptr;
        if (tagOid && git_tag_lookup(&tagObj, mRepo, tagOid) == 0)
        {
            info.mIsAnnotated = true;
            const char* msg = git_tag_message(tagObj);
            info.mMessage = msg ? msg : "";

            const git_signature* tagger = git_tag_tagger(tagObj);
            if (tagger)
            {
                info.mTaggerName = tagger->name ? tagger->name : "";
                info.mTimestamp = static_cast<int64_t>(tagger->when.time);
            }

            git_tag_free(tagObj);
        }
        else
        {
            info.mIsAnnotated = false;
        }

        git_reference_free(ref);
        tags.push_back(std::move(info));
    }

    git_strarray_free(&tagNames);
    return tags;
}

bool GitRepository::CreateTag(const std::string& name, const std::string& oid, bool annotated,
                              const std::string& message)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_oid targetOid;
    if (git_oid_fromstr(&targetOid, oid.c_str()) != 0)
    {
        LogError("GitRepository::CreateTag: invalid OID %s", oid.c_str());
        return false;
    }

    git_object* target = nullptr;
    int err = git_object_lookup(&target, mRepo, &targetOid, GIT_OBJECT_COMMIT);
    if (err != 0)
    {
        LogError("GitRepository::CreateTag: %s", SafeErrorMessage());
        return false;
    }

    git_oid newTagOid;

    if (annotated)
    {
        git_signature* sig = nullptr;
        err = git_signature_default(&sig, mRepo);
        if (err != 0)
        {
            // Fallback signature
            err = git_signature_now(&sig, "Unknown", "unknown@unknown");
            if (err != 0)
            {
                LogError("GitRepository::CreateTag: could not create signature: %s",
                         SafeErrorMessage());
                git_object_free(target);
                return false;
            }
        }

        err = git_tag_create(&newTagOid, mRepo, name.c_str(), target, sig, message.c_str(), 0);
        git_signature_free(sig);
    }
    else
    {
        err = git_tag_create_lightweight(&newTagOid, mRepo, name.c_str(), target, 0);
    }

    git_object_free(target);

    if (err != 0)
    {
        LogError("GitRepository::CreateTag: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::CreateTag: created tag %s", name.c_str());
    return true;
}

bool GitRepository::DeleteTag(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    int err = git_tag_delete(mRepo, name.c_str());
    if (err != 0)
    {
        LogError("GitRepository::DeleteTag: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::DeleteTag: deleted tag %s", name.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Remotes
// ---------------------------------------------------------------------------

std::vector<GitRemoteInfo> GitRepository::GetRemotes() const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    std::vector<GitRemoteInfo> remotes;

    if (!mRepo)
        return remotes;

    git_strarray remoteNames = { nullptr, 0 };
    int err = git_remote_list(&remoteNames, mRepo);
    if (err != 0)
    {
        LogError("GitRepository::GetRemotes: %s", SafeErrorMessage());
        return remotes;
    }

    for (size_t i = 0; i < remoteNames.count; ++i)
    {
        git_remote* remote = nullptr;
        if (git_remote_lookup(&remote, mRepo, remoteNames.strings[i]) != 0)
            continue;

        GitRemoteInfo info;
        info.mName = remoteNames.strings[i];

        const char* fetchUrl = git_remote_url(remote);
        info.mFetchUrl = fetchUrl ? fetchUrl : "";

        const char* pushUrl = git_remote_pushurl(remote);
        info.mPushUrl = pushUrl ? pushUrl : info.mFetchUrl;

        // Consider "origin" the default remote
        info.mIsDefault = (info.mName == "origin");

        git_remote_free(remote);
        remotes.push_back(std::move(info));
    }

    git_strarray_free(&remoteNames);
    return remotes;
}

bool GitRepository::AddRemote(const std::string& name, const std::string& url)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_remote* remote = nullptr;
    int err = git_remote_create(&remote, mRepo, name.c_str(), url.c_str());
    if (err != 0)
    {
        LogError("GitRepository::AddRemote: %s", SafeErrorMessage());
        return false;
    }

    git_remote_free(remote);
    LogDebug("GitRepository::AddRemote: added remote %s -> %s", name.c_str(), url.c_str());
    return true;
}

bool GitRepository::EditRemote(const std::string& name, const std::string& url)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    int err = git_remote_set_url(mRepo, name.c_str(), url.c_str());
    if (err != 0)
    {
        LogError("GitRepository::EditRemote: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::EditRemote: updated remote %s -> %s", name.c_str(), url.c_str());
    return true;
}

bool GitRepository::RemoveRemote(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    int err = git_remote_delete(mRepo, name.c_str());
    if (err != 0)
    {
        LogError("GitRepository::RemoveRemote: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::RemoveRemote: removed remote %s", name.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

std::vector<GitCommitInfo> GitRepository::GetCommits(int32_t maxCount,
                                                      const std::string& branchFilter) const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    std::vector<GitCommitInfo> commits;

    if (!mRepo)
        return commits;

    git_revwalk* walk = nullptr;
    int err = git_revwalk_new(&walk, mRepo);
    if (err != 0)
    {
        LogError("GitRepository::GetCommits: %s", SafeErrorMessage());
        return commits;
    }

    git_revwalk_sorting(walk, GIT_SORT_TIME);

    if (branchFilter.empty())
    {
        // "All branches" — push all local and remote branch tips + HEAD
        git_revwalk_push_head(walk);
        git_revwalk_push_glob(walk, "refs/heads/*");
        git_revwalk_push_glob(walk, "refs/remotes/*");
        err = 0;
    }
    else if (branchFilter == "(detached)")
    {
        err = git_revwalk_push_head(walk);
    }
    else
    {
        std::string refName = "refs/heads/" + branchFilter;
        err = git_revwalk_push_ref(walk, refName.c_str());
    }

    if (err != 0)
    {
        LogError("GitRepository::GetCommits: %s", SafeErrorMessage());
        git_revwalk_free(walk);
        return commits;
    }

    // Determine current HEAD OID for marking
    git_oid headOid;
    bool hasHead = (git_reference_name_to_id(&headOid, mRepo, "HEAD") == 0);

    // Get current branch name
    std::string currentBranch;
    {
        git_reference* headRef = nullptr;
        if (git_repository_head(&headRef, mRepo) == 0)
        {
            const char* shorthand = git_reference_shorthand(headRef);
            if (shorthand)
                currentBranch = shorthand;
            git_reference_free(headRef);
        }
    }

    // Build a map of OID string -> list of ref names (branches + tags)
    // so we can decorate each commit row with badges.
    std::unordered_map<std::string, std::vector<std::string>> refMap;

    // Branches
    {
        git_branch_iterator* brIter = nullptr;
        if (git_branch_iterator_new(&brIter, mRepo, GIT_BRANCH_ALL) == 0)
        {
            git_reference* brRef = nullptr;
            git_branch_t brType;
            while (git_branch_next(&brRef, &brType, brIter) == 0)
            {
                const git_oid* target = git_reference_target(brRef);
                if (!target)
                {
                    // Symbolic ref — resolve it
                    git_reference* resolved = nullptr;
                    if (git_reference_resolve(&resolved, brRef) == 0)
                    {
                        target = git_reference_target(resolved);
                        if (target)
                        {
                            const char* brName = nullptr;
                            git_branch_name(&brName, brRef);
                            if (brName)
                                refMap[OidToString(target)].push_back(std::string(brName));
                        }
                        git_reference_free(resolved);
                    }
                }
                else
                {
                    const char* brName = nullptr;
                    git_branch_name(&brName, brRef);
                    if (brName)
                        refMap[OidToString(target)].push_back(std::string(brName));
                }
                git_reference_free(brRef);
            }
            git_branch_iterator_free(brIter);
        }
    }

    // Tags
    {
        git_strarray tagNames = {0};
        if (git_tag_list(&tagNames, mRepo) == 0)
        {
            for (size_t t = 0; t < tagNames.count; ++t)
            {
                std::string refName = "refs/tags/" + std::string(tagNames.strings[t]);
                git_oid tagOid;
                if (git_reference_name_to_id(&tagOid, mRepo, refName.c_str()) == 0)
                {
                    // Peel to commit in case of annotated tag
                    git_object* peeled = nullptr;
                    if (git_revparse_single(&peeled, mRepo, refName.c_str()) == 0)
                    {
                        git_object* commitObj = nullptr;
                        if (git_object_peel(&commitObj, peeled, GIT_OBJECT_COMMIT) == 0)
                        {
                            std::string key = OidToString(git_object_id(commitObj));
                            refMap[key].push_back("tag: " + std::string(tagNames.strings[t]));
                            git_object_free(commitObj);
                        }
                        git_object_free(peeled);
                    }
                }
            }
            git_strarray_free(&tagNames);
        }
    }

    git_oid oid;
    int32_t count = 0;
    while (count < maxCount && git_revwalk_next(&oid, walk) == 0)
    {
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, mRepo, &oid) != 0)
            continue;

        GitCommitInfo info;
        info.mOid = OidToString(&oid);
        info.mShortOid = OidToShortString(&oid);

        const char* summary = git_commit_summary(commit);
        info.mSummary = summary ? summary : "";

        const char* body = git_commit_body(commit);
        info.mBody = body ? body : "";

        const git_signature* author = git_commit_author(commit);
        if (author)
        {
            info.mAuthorName = author->name ? author->name : "";
            info.mAuthorEmail = author->email ? author->email : "";
            info.mTimestamp = static_cast<int64_t>(author->when.time);
        }

        const git_signature* committer = git_commit_committer(commit);
        if (committer)
        {
            info.mCommitterName = committer->name ? committer->name : "";
            info.mCommitterEmail = committer->email ? committer->email : "";
        }

        unsigned int parentCount = git_commit_parentcount(commit);
        for (unsigned int p = 0; p < parentCount; ++p)
        {
            const git_oid* parentOid = git_commit_parent_id(commit, p);
            if (parentOid)
                info.mParentOids.push_back(OidToString(parentOid));
        }

        // HEAD marker
        if (hasHead && git_oid_equal(&oid, &headOid))
            info.mIsHead = true;

        // Attach ref names from the map
        auto it = refMap.find(info.mOid);
        if (it != refMap.end())
            info.mRefNames = it->second;

        commits.push_back(std::move(info));
        git_commit_free(commit);
        ++count;
    }

    git_revwalk_free(walk);
    return commits;
}

GitCommitInfo GitRepository::GetCommitDetails(const std::string& oid) const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    GitCommitInfo info;

    if (!mRepo)
        return info;

    git_oid commitOid;
    if (git_oid_fromstr(&commitOid, oid.c_str()) != 0)
    {
        LogError("GitRepository::GetCommitDetails: invalid OID %s", oid.c_str());
        return info;
    }

    git_commit* commit = nullptr;
    int err = git_commit_lookup(&commit, mRepo, &commitOid);
    if (err != 0)
    {
        LogError("GitRepository::GetCommitDetails: %s", SafeErrorMessage());
        return info;
    }

    info.mOid = OidToString(&commitOid);
    info.mShortOid = OidToShortString(&commitOid);

    const char* summary = git_commit_summary(commit);
    info.mSummary = summary ? summary : "";

    const char* body = git_commit_body(commit);
    info.mBody = body ? body : "";

    const git_signature* author = git_commit_author(commit);
    if (author)
    {
        info.mAuthorName = author->name ? author->name : "";
        info.mAuthorEmail = author->email ? author->email : "";
        info.mTimestamp = static_cast<int64_t>(author->when.time);
    }

    const git_signature* committer = git_commit_committer(commit);
    if (committer)
    {
        info.mCommitterName = committer->name ? committer->name : "";
        info.mCommitterEmail = committer->email ? committer->email : "";
    }

    unsigned int parentCount = git_commit_parentcount(commit);
    for (unsigned int p = 0; p < parentCount; ++p)
    {
        const git_oid* parentOid = git_commit_parent_id(commit, p);
        if (parentOid)
            info.mParentOids.push_back(OidToString(parentOid));
    }

    // Check if this is HEAD
    git_oid headOid;
    if (git_reference_name_to_id(&headOid, mRepo, "HEAD") == 0)
    {
        info.mIsHead = git_oid_equal(&commitOid, &headOid) != 0;
    }

    git_commit_free(commit);
    return info;
}

// ---------------------------------------------------------------------------
// Diff
// ---------------------------------------------------------------------------

GitFileDiff GitRepository::GetWorkingTreeDiff(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    GitFileDiff result;

    if (!mRepo)
        return result;

    git_diff_options diffOpts = GIT_DIFF_OPTIONS_INIT;
    const char* pathspec = path.c_str();
    diffOpts.pathspec.strings = const_cast<char**>(&pathspec);
    diffOpts.pathspec.count = 1;

    git_diff* diff = nullptr;
    int err = git_diff_index_to_workdir(&diff, mRepo, nullptr, &diffOpts);
    if (err != 0)
    {
        LogError("GitRepository::GetWorkingTreeDiff: %s", SafeErrorMessage());
        return result;
    }

    size_t numDeltas = git_diff_num_deltas(diff);
    if (numDeltas > 0)
    {
        const git_diff_delta* delta = git_diff_get_delta(diff, 0);
        if (delta)
        {
            result.mOldPath = delta->old_file.path ? delta->old_file.path : "";
            result.mNewPath = delta->new_file.path ? delta->new_file.path : "";
            result.mIsBinary = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;
        }

        if (!result.mIsBinary)
        {
            git_patch* patch = nullptr;
            if (git_patch_from_diff(&patch, diff, 0) == 0 && patch)
            {
                ExtractDiffHunks(patch, result);
                git_patch_free(patch);
            }
        }
    }

    git_diff_free(diff);
    return result;
}

GitFileDiff GitRepository::GetStagedDiff(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    GitFileDiff result;

    if (!mRepo)
        return result;

    // Get HEAD tree
    git_object* headObj = nullptr;
    git_tree* headTree = nullptr;
    if (git_revparse_single(&headObj, mRepo, "HEAD^{tree}") == 0)
    {
        headTree = reinterpret_cast<git_tree*>(headObj);
    }

    git_diff_options diffOpts = GIT_DIFF_OPTIONS_INIT;
    const char* pathspec = path.c_str();
    diffOpts.pathspec.strings = const_cast<char**>(&pathspec);
    diffOpts.pathspec.count = 1;

    git_diff* diff = nullptr;
    int err = git_diff_tree_to_index(&diff, mRepo, headTree, nullptr, &diffOpts);

    if (headObj)
        git_object_free(headObj);

    if (err != 0)
    {
        LogError("GitRepository::GetStagedDiff: %s", SafeErrorMessage());
        return result;
    }

    size_t numDeltas = git_diff_num_deltas(diff);
    if (numDeltas > 0)
    {
        const git_diff_delta* delta = git_diff_get_delta(diff, 0);
        if (delta)
        {
            result.mOldPath = delta->old_file.path ? delta->old_file.path : "";
            result.mNewPath = delta->new_file.path ? delta->new_file.path : "";
            result.mIsBinary = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;
        }

        if (!result.mIsBinary)
        {
            git_patch* patch = nullptr;
            if (git_patch_from_diff(&patch, diff, 0) == 0 && patch)
            {
                ExtractDiffHunks(patch, result);
                git_patch_free(patch);
            }
        }
    }

    git_diff_free(diff);
    return result;
}

GitFileDiff GitRepository::GetCommitDiff(const std::string& oid) const
{
    std::lock_guard<std::mutex> lock(mRepoMutex);
    GitFileDiff result;

    if (!mRepo)
        return result;

    git_oid commitOid;
    if (git_oid_fromstr(&commitOid, oid.c_str()) != 0)
    {
        LogError("GitRepository::GetCommitDiff: invalid OID %s", oid.c_str());
        return result;
    }

    git_commit* commit = nullptr;
    if (git_commit_lookup(&commit, mRepo, &commitOid) != 0)
    {
        LogError("GitRepository::GetCommitDiff: %s", SafeErrorMessage());
        return result;
    }

    git_tree* commitTree = nullptr;
    if (git_commit_tree(&commitTree, commit) != 0)
    {
        LogError("GitRepository::GetCommitDiff: %s", SafeErrorMessage());
        git_commit_free(commit);
        return result;
    }

    // Get parent tree (nullptr if this is the root commit)
    git_tree* parentTree = nullptr;
    if (git_commit_parentcount(commit) > 0)
    {
        git_commit* parentCommit = nullptr;
        if (git_commit_parent(&parentCommit, commit, 0) == 0)
        {
            git_commit_tree(&parentTree, parentCommit);
            git_commit_free(parentCommit);
        }
    }

    git_diff* diff = nullptr;
    int err = git_diff_tree_to_tree(&diff, mRepo, parentTree, commitTree, nullptr);

    if (parentTree)
        git_tree_free(parentTree);
    git_tree_free(commitTree);
    git_commit_free(commit);

    if (err != 0)
    {
        LogError("GitRepository::GetCommitDiff: %s", SafeErrorMessage());
        return result;
    }

    // For a commit diff, aggregate all files' hunks
    size_t numDeltas = git_diff_num_deltas(diff);
    if (numDeltas > 0)
    {
        const git_diff_delta* delta = git_diff_get_delta(diff, 0);
        if (delta)
        {
            result.mOldPath = delta->old_file.path ? delta->old_file.path : "";
            result.mNewPath = delta->new_file.path ? delta->new_file.path : "";
            result.mIsBinary = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;
        }

        if (!result.mIsBinary)
        {
            for (size_t d = 0; d < numDeltas; ++d)
            {
                git_patch* patch = nullptr;
                if (git_patch_from_diff(&patch, diff, d) == 0 && patch)
                {
                    ExtractDiffHunks(patch, result);
                    git_patch_free(patch);
                }
            }
        }
    }

    git_diff_free(diff);
    return result;
}

// ---------------------------------------------------------------------------
// Staging
// ---------------------------------------------------------------------------

bool GitRepository::StageFiles(const std::vector<std::string>& paths)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    git_index* index = nullptr;
    int err = git_repository_index(&index, mRepo);
    if (err != 0)
    {
        LogError("GitRepository::StageFiles: %s", SafeErrorMessage());
        return false;
    }

    bool success = true;
    for (const std::string& path : paths)
    {
        // Check if the file was deleted -- if so, remove from index
        // Try add first; if that fails because the file doesn't exist on disk,
        // remove it from the index instead.
        err = git_index_add_bypath(index, path.c_str());
        if (err != 0)
        {
            // Attempt to remove (handles deleted files)
            int removeErr = git_index_remove_bypath(index, path.c_str());
            if (removeErr != 0)
            {
                LogError("GitRepository::StageFiles: failed to stage %s: %s",
                         path.c_str(), SafeErrorMessage());
                success = false;
            }
        }
    }

    if (git_index_write(index) != 0)
    {
        LogError("GitRepository::StageFiles: %s", SafeErrorMessage());
        git_index_free(index);
        return false;
    }

    git_index_free(index);
    return success;
}

bool GitRepository::UnstageFiles(const std::vector<std::string>& paths)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    // Build a pathspec array
    std::vector<char*> cPaths;
    cPaths.reserve(paths.size());
    for (const std::string& p : paths)
        cPaths.push_back(const_cast<char*>(p.c_str()));

    git_strarray pathspecs;
    pathspecs.strings = cPaths.data();
    pathspecs.count = cPaths.size();

    // Get HEAD as a git_object for reset
    git_object* headObj = nullptr;
    git_reference* headRef = nullptr;
    int err = git_repository_head(&headRef, mRepo);
    if (err == 0)
    {
        git_reference_peel(&headObj, headRef, GIT_OBJECT_COMMIT);
        git_reference_free(headRef);
    }

    // git_reset_default resets index entries to HEAD for the given paths
    // headObj can be nullptr for an unborn branch -- reset_default handles that
    err = git_reset_default(mRepo, headObj, &pathspecs);

    if (headObj)
        git_object_free(headObj);

    if (err != 0)
    {
        LogError("GitRepository::UnstageFiles: %s", SafeErrorMessage());
        return false;
    }

    return true;
}

bool GitRepository::DiscardFiles(const std::vector<std::string>& paths)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    std::vector<char*> cPaths;
    cPaths.reserve(paths.size());
    for (const std::string& p : paths)
        cPaths.push_back(const_cast<char*>(p.c_str()));

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    opts.paths.strings = cPaths.data();
    opts.paths.count = cPaths.size();

    int err = git_checkout_head(mRepo, &opts);
    if (err != 0)
    {
        LogError("GitRepository::DiscardFiles: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::DiscardFiles: discarded %zu file(s)", paths.size());
    return true;
}

// ---------------------------------------------------------------------------
// Commit
// ---------------------------------------------------------------------------

bool GitRepository::Commit(const std::string& summary, const std::string& body, bool amend)
{
    std::lock_guard<std::mutex> lock(mRepoMutex);

    if (!mRepo)
        return false;

    // Build the full commit message
    std::string message = summary;
    std::string trimmedBody = TrimString(body);
    if (!trimmedBody.empty())
        message += "\n\n" + trimmedBody;

    // Get the index and write it as a tree
    git_index* index = nullptr;
    int err = git_repository_index(&index, mRepo);
    if (err != 0)
    {
        LogError("GitRepository::Commit: %s", SafeErrorMessage());
        return false;
    }

    git_oid treeOid;
    err = git_index_write_tree(&treeOid, index);
    git_index_free(index);

    if (err != 0)
    {
        LogError("GitRepository::Commit: %s", SafeErrorMessage());
        return false;
    }

    git_tree* tree = nullptr;
    err = git_tree_lookup(&tree, mRepo, &treeOid);
    if (err != 0)
    {
        LogError("GitRepository::Commit: %s", SafeErrorMessage());
        return false;
    }

    // Create signature
    git_signature* sig = nullptr;
    err = git_signature_default(&sig, mRepo);
    if (err != 0)
    {
        // Fallback: use a generic signature
        err = git_signature_now(&sig, "Unknown", "unknown@unknown");
        if (err != 0)
        {
            LogError("GitRepository::Commit: could not create signature: %s", SafeErrorMessage());
            git_tree_free(tree);
            return false;
        }
        LogWarning("GitRepository::Commit: using fallback signature (configure user.name and user.email)");
    }

    if (amend)
    {
        // Amend the current HEAD commit
        git_reference* headRef = nullptr;
        git_commit* headCommit = nullptr;
        err = git_repository_head(&headRef, mRepo);
        if (err != 0)
        {
            LogError("GitRepository::Commit: cannot amend -- no HEAD: %s", SafeErrorMessage());
            git_signature_free(sig);
            git_tree_free(tree);
            return false;
        }

        const git_oid* headOid = git_reference_target(headRef);
        err = git_commit_lookup(&headCommit, mRepo, headOid);
        git_reference_free(headRef);

        if (err != 0)
        {
            LogError("GitRepository::Commit: %s", SafeErrorMessage());
            git_signature_free(sig);
            git_tree_free(tree);
            return false;
        }

        git_oid newOid;
        err = git_commit_amend(&newOid, headCommit, "HEAD", sig, sig, nullptr,
                               message.c_str(), tree);

        git_commit_free(headCommit);
        git_signature_free(sig);
        git_tree_free(tree);

        if (err != 0)
        {
            LogError("GitRepository::Commit: amend failed: %s", SafeErrorMessage());
            return false;
        }

        LogDebug("GitRepository::Commit: amended commit %s", OidToShortString(&newOid).c_str());
        return true;
    }

    // Normal commit -- get parent (HEAD commit, if it exists)
    git_commit* parentCommit = nullptr;
    const git_commit* parents[1] = { nullptr };
    int parentCount = 0;

    git_reference* headRef = nullptr;
    if (git_repository_head(&headRef, mRepo) == 0)
    {
        const git_oid* headOid = git_reference_target(headRef);
        if (headOid && git_commit_lookup(&parentCommit, mRepo, headOid) == 0)
        {
            parents[0] = parentCommit;
            parentCount = 1;
        }
        git_reference_free(headRef);
    }

    git_oid newOid;
    err = git_commit_create(&newOid, mRepo, "HEAD", sig, sig, nullptr,
                            message.c_str(), tree, parentCount, parents);

    if (parentCommit)
        git_commit_free(parentCommit);
    git_signature_free(sig);
    git_tree_free(tree);

    if (err != 0)
    {
        LogError("GitRepository::Commit: %s", SafeErrorMessage());
        return false;
    }

    LogDebug("GitRepository::Commit: created commit %s", OidToShortString(&newOid).c_str());
    return true;
}

#endif
