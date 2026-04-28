#if EDITOR

#include "GitProgressParser.h"
#include <string>
#include <cstdlib>

bool GitProgressParser::Parse(const std::string& stderrLine, GitProgressEvent& outEvent)
{
    try
    {
        std::string line = stderrLine;

        // Strip leading "remote: " prefix if present
        const std::string remotePrefix = "remote: ";
        if (line.size() >= remotePrefix.size() &&
            line.compare(0, remotePrefix.size(), remotePrefix) == 0)
        {
            line = line.substr(remotePrefix.size());
        }

        // Match against known phase keywords
        struct PhaseMapping
        {
            const char* mKeyword;
            GitProgressEvent::Phase mPhase;
        };

        static const PhaseMapping sMappings[] =
        {
            { "Counting objects",     GitProgressEvent::Counting },
            { "Compressing objects",  GitProgressEvent::Compressing },
            { "Receiving objects",    GitProgressEvent::Receiving },
            { "Resolving deltas",    GitProgressEvent::Resolving },
            { "Writing objects",     GitProgressEvent::Writing },
        };

        bool matched = false;
        for (const auto& mapping : sMappings)
        {
            if (line.find(mapping.mKeyword) == 0)
            {
                outEvent.mPhase = mapping.mPhase;
                matched = true;
                break;
            }
        }

        if (!matched)
        {
            return false;
        }

        // Reset fields
        outEvent.mPercent = -1;
        outEvent.mCurrent = 0;
        outEvent.mTotal = 0;
        outEvent.mBytes = 0;

        // Look for percentage pattern: find '%' and parse the number before it
        size_t percentPos = line.find('%');
        if (percentPos != std::string::npos && percentPos > 0)
        {
            // Walk backwards from just before '%' to find the start of the number
            size_t numEnd = percentPos;
            size_t numStart = percentPos - 1;
            while (numStart > 0 && line[numStart - 1] >= '0' && line[numStart - 1] <= '9')
            {
                --numStart;
            }

            if (numStart < numEnd && line[numStart] >= '0' && line[numStart] <= '9')
            {
                std::string numStr = line.substr(numStart, numEnd - numStart);
                outEvent.mPercent = std::stoi(numStr);
            }
        }

        // Look for fraction pattern: (N/M)
        size_t parenOpen = line.find('(');
        size_t parenClose = line.find(')', parenOpen != std::string::npos ? parenOpen : 0);
        if (parenOpen != std::string::npos && parenClose != std::string::npos && parenClose > parenOpen)
        {
            std::string inner = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
            size_t slashPos = inner.find('/');
            if (slashPos != std::string::npos)
            {
                std::string currentStr = inner.substr(0, slashPos);
                std::string totalStr = inner.substr(slashPos + 1);
                outEvent.mCurrent = std::stoi(currentStr);
                outEvent.mTotal = std::stoi(totalStr);
            }
        }

        // Look for byte size pattern: e.g. "2.50 MiB", "284 bytes", "1.23 KiB"
        // We do a simple scan for known size suffixes and parse the number before them
        struct SizeMapping
        {
            const char* mSuffix;
            int64_t mMultiplier;
        };

        static const SizeMapping sSizeMappings[] =
        {
            { " GiB", 1073741824LL },
            { " MiB", 1048576LL },
            { " KiB", 1024LL },
            { " bytes", 1LL },
        };

        for (const auto& sizeMapping : sSizeMappings)
        {
            size_t suffixPos = line.find(sizeMapping.mSuffix);
            if (suffixPos == std::string::npos)
            {
                continue;
            }

            // Walk backwards from the suffix to find the start of the number (may include '.')
            size_t numEnd2 = suffixPos;
            size_t numStart2 = suffixPos;
            while (numStart2 > 0 &&
                   (line[numStart2 - 1] == '.' ||
                    (line[numStart2 - 1] >= '0' && line[numStart2 - 1] <= '9')))
            {
                --numStart2;
            }

            if (numStart2 < numEnd2)
            {
                std::string sizeStr = line.substr(numStart2, numEnd2 - numStart2);
                double sizeValue = std::stod(sizeStr);
                outEvent.mBytes = static_cast<int64_t>(sizeValue * sizeMapping.mMultiplier);
            }
            break;
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

#endif
