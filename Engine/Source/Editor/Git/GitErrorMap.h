#pragma once

#if EDITOR

#include <string>

class GitErrorMap
{
public:
    struct ErrorInfo
    {
        std::string mFriendlyMessage;
        std::string mSuggestedAction;
    };

    static ErrorInfo MapLibgit2Error(int errorCode, const char* errorMessage);
    static ErrorInfo MapCliError(int exitCode, const std::string& stderrOutput);
};

#endif
