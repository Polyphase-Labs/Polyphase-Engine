#pragma once

#if EDITOR

#include <string>
#include <memory>

class GitRepository;
class GitOperationQueue;
class GitCliProbe;

class GitService
{
public:
    static void Create();
    static void Destroy();
    static GitService* Get();

    void Update();

    bool OpenRepository(const std::string& path);
    bool InitRepository(const std::string& path, const std::string& initialBranch = "main");
    void CloneRepository(const std::string& url, const std::string& destPath, const std::string& branch = "");
    void CloseRepository();

    GitRepository* GetCurrentRepo();
    GitOperationQueue* GetOperationQueue();
    GitCliProbe* GetCliProbe();

    bool IsRepositoryOpen() const;

private:
    GitService();
    ~GitService();

    std::unique_ptr<GitRepository> mRepository;
    std::unique_ptr<GitOperationQueue> mOperationQueue;
    std::unique_ptr<GitCliProbe> mCliProbe;
};

#endif
