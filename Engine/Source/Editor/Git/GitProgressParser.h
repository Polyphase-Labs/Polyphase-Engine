#pragma once

#if EDITOR

#include "GitTypes.h"
#include <string>

class GitProgressParser
{
public:
    static bool Parse(const std::string& stderrLine, GitProgressEvent& outEvent);
};

#endif
