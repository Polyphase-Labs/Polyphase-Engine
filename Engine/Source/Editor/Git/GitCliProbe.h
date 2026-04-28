#pragma once

#if EDITOR

#include <string>

class GitCliProbe
{
public:
    void Probe();

    bool IsGitAvailable() const;
    const std::string& GetGitVersion() const;
    const std::string& GetGitUserName() const;
    const std::string& GetUserEmail() const;
    const std::string& GetCredentialHelper() const;
    const std::string& GetGitPath() const;

private:
    bool mProbed = false;
    bool mGitAvailable = false;
    std::string mGitVersion;
    std::string mUserName;
    std::string mUserEmail;
    std::string mCredentialHelper;
    std::string mGitPath;
};

#endif
