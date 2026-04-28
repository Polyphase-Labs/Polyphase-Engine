#pragma once

#if EDITOR

#include "GitTypes.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

class GitProcessRunner
{
public:
    static int32_t Run(
        const std::string& workDir,
        const std::vector<std::string>& args,
        std::function<void(const std::string&)> onStdoutLine,
        std::function<void(const std::string&)> onStderrLine,
        GitCancelToken cancelToken
    );
};

#endif
