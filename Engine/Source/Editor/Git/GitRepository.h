#pragma once

#if EDITOR

#include "GitTypes.h"
#include <string>
#include <vector>
#include <mutex>

// Forward declare the libgit2 struct to avoid including git2.h in the header
struct git_repository;

class GitRepository
{
public:
    GitRepository();
    ~GitRepository();

    bool Open(const std::string& path);
    bool Init(const std::string& path, const std::string& initialBranch = "main");
    void Close();
    bool IsOpen() const;
    const std::string& GetPath() const;

    // Status
    void RefreshStatus();
    const std::vector<GitStatusEntry>& GetStatus() const;

    // Branch info
    std::string GetCurrentBranch() const;
    std::vector<GitBranchInfo> GetBranches() const;
    bool CreateBranch(const std::string& name, const std::string& sourceOid, bool checkout);
    bool DeleteBranch(const std::string& name, bool force);
    bool CheckoutBranch(const std::string& name);
    bool CheckoutCommit(const std::string& oid);
    bool SetUpstream(const std::string& branch, const std::string& remote, const std::string& remoteBranch);

    // Tags
    std::vector<GitTagInfo> GetTags() const;
    bool CreateTag(const std::string& name, const std::string& oid, bool annotated, const std::string& message);
    bool DeleteTag(const std::string& name);

    // Remotes
    std::vector<GitRemoteInfo> GetRemotes() const;
    bool AddRemote(const std::string& name, const std::string& url);
    bool EditRemote(const std::string& name, const std::string& url);
    bool RemoveRemote(const std::string& name);

    // History
    std::vector<GitCommitInfo> GetCommits(int32_t maxCount, const std::string& branchFilter = "") const;
    GitCommitInfo GetCommitDetails(const std::string& oid) const;

    // Diff
    GitFileDiff GetWorkingTreeDiff(const std::string& path) const;
    GitFileDiff GetStagedDiff(const std::string& path) const;
    GitFileDiff GetCommitDiff(const std::string& oid) const;

    // Staging
    bool StageFiles(const std::vector<std::string>& paths);
    bool UnstageFiles(const std::vector<std::string>& paths);
    bool DiscardFiles(const std::vector<std::string>& paths);

    // Commit
    bool Commit(const std::string& summary, const std::string& body, bool amend);

    std::mutex& GetMutex() { return mRepoMutex; }

private:
    git_repository* mRepo = nullptr;
    std::string mPath;
    std::vector<GitStatusEntry> mCachedStatus;
    mutable std::mutex mRepoMutex;
};

#endif
