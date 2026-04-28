#if EDITOR

#include "GitCliProbe.h"
#include "GitProcessRunner.h"
#include "GitTypes.h"
#include "Log.h"

#include <algorithm>

static std::string TrimWhitespace(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::string ParseVersionString(const std::string& versionOutput)
{
    // Input: "git version 2.43.0.windows.1"
    // Output: "2.43.0"
    const std::string prefix = "git version ";
    size_t pos = versionOutput.find(prefix);
    if (pos == std::string::npos)
        return versionOutput;

    std::string version = versionOutput.substr(pos + prefix.size());
    version = TrimWhitespace(version);

    // Take only the first three numeric segments (major.minor.patch)
    int dotCount = 0;
    size_t end = 0;
    for (size_t i = 0; i < version.size(); ++i)
    {
        if (version[i] == '.')
        {
            ++dotCount;
            if (dotCount >= 3)
            {
                end = i;
                break;
            }
        }
        end = i + 1;
    }

    return version.substr(0, end);
}

void GitCliProbe::Probe()
{
    if (mProbed)
        return;

    mProbed = true;

    // 1. git --version
    {
        std::string output;
        GitCancelToken token = CreateCancelToken();
        int32_t exitCode = GitProcessRunner::Run(
            ".",
            { "git", "--version" },
            [&output](const std::string& line) { output += line; },
            nullptr,
            token
        );

        if (exitCode != 0)
        {
            mGitAvailable = false;
            LogWarning("GitCliProbe: git CLI not found (exit code %d)", exitCode);
            return;
        }

        mGitAvailable = true;
        mGitVersion = ParseVersionString(TrimWhitespace(output));
        LogDebug("GitCliProbe: detected git version %s", mGitVersion.c_str());
    }

    // 2. git config --global user.name
    {
        std::string output;
        GitCancelToken token = CreateCancelToken();
        GitProcessRunner::Run(
            ".",
            { "git", "config", "--global", "user.name" },
            [&output](const std::string& line) { output += line; },
            nullptr,
            token
        );
        mUserName = TrimWhitespace(output);
    }

    // 3. git config --global user.email
    {
        std::string output;
        GitCancelToken token = CreateCancelToken();
        GitProcessRunner::Run(
            ".",
            { "git", "config", "--global", "user.email" },
            [&output](const std::string& line) { output += line; },
            nullptr,
            token
        );
        mUserEmail = TrimWhitespace(output);
    }

    // 4. git config --global credential.helper
    {
        std::string output;
        GitCancelToken token = CreateCancelToken();
        GitProcessRunner::Run(
            ".",
            { "git", "config", "--global", "credential.helper" },
            [&output](const std::string& line) { output += line; },
            nullptr,
            token
        );
        mCredentialHelper = TrimWhitespace(output);
    }

    // 5. Locate git binary path
    {
        std::string output;
        GitCancelToken token = CreateCancelToken();
#ifdef _WIN32
        GitProcessRunner::Run(
            ".",
            { "where", "git" },
            [&output](const std::string& line) { if (output.empty()) output = line; },
            nullptr,
            token
        );
#else
        GitProcessRunner::Run(
            ".",
            { "which", "git" },
            [&output](const std::string& line) { if (output.empty()) output = line; },
            nullptr,
            token
        );
#endif
        mGitPath = TrimWhitespace(output);
    }

    LogDebug("GitCliProbe: user.name=\"%s\", user.email=\"%s\", credential.helper=\"%s\", path=\"%s\"",
        mUserName.c_str(), mUserEmail.c_str(), mCredentialHelper.c_str(), mGitPath.c_str());
}

bool GitCliProbe::IsGitAvailable() const
{
    return mGitAvailable;
}

const std::string& GitCliProbe::GetGitVersion() const
{
    return mGitVersion;
}

const std::string& GitCliProbe::GetGitUserName() const
{
    return mUserName;
}

const std::string& GitCliProbe::GetUserEmail() const
{
    return mUserEmail;
}

const std::string& GitCliProbe::GetCredentialHelper() const
{
    return mCredentialHelper;
}

const std::string& GitCliProbe::GetGitPath() const
{
    return mGitPath;
}

#endif
