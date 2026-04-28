#if EDITOR

#include "GitErrorMap.h"
#include "Log.h"

GitErrorMap::ErrorInfo GitErrorMap::MapLibgit2Error(int errorCode, const char* errorMessage)
{
    ErrorInfo info;

    switch (errorCode)
    {
    case -1: // GIT_ERROR
        info.mFriendlyMessage = "A generic Git error occurred.";
        info.mSuggestedAction = "Check the repository state and try again.";
        break;

    case -3: // GIT_ENOTFOUND
        info.mFriendlyMessage = "The requested reference or object was not found.";
        info.mSuggestedAction = "Verify the branch, tag, or path exists.";
        break;

    case -4: // GIT_EEXISTS
        info.mFriendlyMessage = "The object already exists.";
        info.mSuggestedAction = "Use a different name or remove the existing object first.";
        break;

    case -7: // GIT_EMERGECONFLICT
        info.mFriendlyMessage = "A merge conflict was detected.";
        info.mSuggestedAction = "Resolve the conflicting files, then stage and commit.";
        break;

    case -10: // GIT_ECONFLICT
        info.mFriendlyMessage = "A checkout conflict prevents the operation.";
        info.mSuggestedAction = "Commit or stash your local changes before switching branches.";
        break;

    case -16: // GIT_EAUTH
        info.mFriendlyMessage = "Authentication failed.";
        info.mSuggestedAction = "Check your credentials and credential helper configuration.";
        break;

    default:
        if (errorMessage && errorMessage[0] != '\0')
        {
            info.mFriendlyMessage = errorMessage;
        }
        else
        {
            info.mFriendlyMessage = "Unknown Git error (code " + std::to_string(errorCode) + ").";
        }
        info.mSuggestedAction = "";
        break;
    }

    if (errorMessage && errorMessage[0] != '\0')
    {
        LogError("GitErrorMap: libgit2 error %d: %s", errorCode, errorMessage);
    }

    return info;
}

GitErrorMap::ErrorInfo GitErrorMap::MapCliError(int exitCode, const std::string& stderrOutput)
{
    ErrorInfo info;

    if (stderrOutput.find("fatal: Authentication failed") != std::string::npos)
    {
        info.mFriendlyMessage = "Authentication failed.";
        info.mSuggestedAction = "Check your credential helper configuration.";
    }
    else if (stderrOutput.find("could not read Username") != std::string::npos ||
             stderrOutput.find("could not read Password") != std::string::npos)
    {
        info.mFriendlyMessage = "Git could not read credentials.";
        info.mSuggestedAction = "Configure a credential helper: git config --global credential.helper manager";
    }
    else if (stderrOutput.find("error: failed to push some refs") != std::string::npos)
    {
        info.mFriendlyMessage = "Push rejected.";
        info.mSuggestedAction = "Pull the latest changes first, then try pushing again.";
    }
    else if (stderrOutput.find("fatal: refusing to merge unrelated histories") != std::string::npos)
    {
        info.mFriendlyMessage = "Cannot merge unrelated histories.";
        info.mSuggestedAction = "This usually means the remote was reinitialized.";
    }
    else if (stderrOutput.find("fatal: not a git repository") != std::string::npos)
    {
        info.mFriendlyMessage = "Not a Git repository.";
        info.mSuggestedAction = "Initialize a repository with 'git init' or open an existing one.";
    }
    else if (stderrOutput.find("fatal: repository") != std::string::npos &&
             stderrOutput.find("not found") != std::string::npos)
    {
        info.mFriendlyMessage = "Remote repository not found.";
        info.mSuggestedAction = "Check the URL.";
    }
    else
    {
        // Default: return the first line of stderr
        size_t lineEnd = stderrOutput.find_first_of("\r\n");
        if (lineEnd != std::string::npos)
        {
            info.mFriendlyMessage = stderrOutput.substr(0, lineEnd);
        }
        else
        {
            info.mFriendlyMessage = stderrOutput;
        }
        info.mSuggestedAction = "";
    }

    LogError("GitErrorMap: CLI error (exit %d): %s", exitCode, stderrOutput.c_str());

    return info;
}

#endif
