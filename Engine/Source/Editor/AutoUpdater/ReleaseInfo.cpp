#if EDITOR

#include "ReleaseInfo.h"
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>

static PreReleaseType ParsePreReleaseTypeString(const char* str, const char* end)
{
    // Extract and lowercase the type string
    std::string type;
    for (const char* p = str; p < end; ++p)
    {
        type += (char)std::tolower((unsigned char)*p);
    }

    if (type == "beta" || type == "beata")
    {
        return PreReleaseType::Beta;
    }
    if (type == "alpha")
    {
        return PreReleaseType::Alpha;
    }
    if (type == "rc")
    {
        return PreReleaseType::RC;
    }

    // Unknown pre-release type, treat as beta
    return PreReleaseType::Beta;
}

bool ReleaseInfo::ParseVersion(const std::string& version, int& outMajor, int& outMinor, int& outPatch)
{
    int build = 0;
    PreReleaseType preType = PreReleaseType::None;
    int preNum = 0;
    return ParseVersion(version, outMajor, outMinor, outPatch, build, preType, preNum);
}

bool ReleaseInfo::ParseVersion(const std::string& version, int& outMajor, int& outMinor, int& outPatch,
                               int& outBuild, PreReleaseType& outPreReleaseType, int& outPreReleaseNum)
{
    outMajor = 0;
    outMinor = 0;
    outPatch = 0;
    outBuild = 0;
    outPreReleaseType = PreReleaseType::None;
    outPreReleaseNum = 0;

    if (version.empty())
    {
        return false;
    }

    // Skip 'v' prefix if present
    const char* str = version.c_str();
    if (*str == 'v' || *str == 'V')
    {
        str++;
    }

    // Parse major
    char* end = nullptr;
    outMajor = (int)strtol(str, &end, 10);
    if (end == str)
    {
        return false; // No digits parsed
    }

    // Check for minor
    if (*end == '.')
    {
        str = end + 1;
        outMinor = (int)strtol(str, &end, 10);

        // Check for patch
        if (*end == '.')
        {
            str = end + 1;
            outPatch = (int)strtol(str, &end, 10);
        }
    }

    // Check for 4th component (build) or pre-release suffix
    if (*end == '.')
    {
        // Could be build number (e.g., 6.1.1.1) or end of patch
        const char* afterDot = end + 1;
        char* buildEnd = nullptr;
        int buildNum = (int)strtol(afterDot, &buildEnd, 10);
        if (buildEnd != afterDot)
        {
            outBuild = buildNum;
            end = buildEnd;
        }
    }

    if (*end == '-')
    {
        // Pre-release suffix (e.g., -beta.5, -beta-5, -beta5, -alpha.1, -rc.2)
        const char* typeStart = end + 1;
        const char* typeEnd = typeStart;

        // Find end of type string: stop at separator ('.', '-'), digit, or end
        while (*typeEnd != '\0' && *typeEnd != '.' && *typeEnd != '-' &&
               !std::isdigit((unsigned char)*typeEnd))
        {
            typeEnd++;
        }

        if (typeEnd > typeStart)
        {
            outPreReleaseType = ParsePreReleaseTypeString(typeStart, typeEnd);

            // Parse pre-release number if present, allowing '.', '-', or no separator
            const char* numStart = typeEnd;
            if (*numStart == '.' || *numStart == '-')
            {
                numStart++;
            }

            char* numEnd = nullptr;
            int preNum = (int)strtol(numStart, &numEnd, 10);
            if (numEnd != numStart)
            {
                outPreReleaseNum = preNum;
            }
        }
    }

    return true;
}

bool ReleaseInfo::IsNewerThan(const std::string& currentVersion) const
{
    return IsNewerThan(currentVersion, false);
}

bool ReleaseInfo::IsNewerThan(const std::string& currentVersion, bool cuttingEdgeEnabled) const
{
    int relMajor, relMinor, relPatch, relBuild;
    PreReleaseType relPreType;
    int relPreNum;

    int curMajor, curMinor, curPatch, curBuild;
    PreReleaseType curPreType;
    int curPreNum;

    std::string releaseVer = GetVersion();

    if (!ParseVersion(releaseVer, relMajor, relMinor, relPatch, relBuild, relPreType, relPreNum))
    {
        return false;
    }

    if (!ParseVersion(currentVersion, curMajor, curMinor, curPatch, curBuild, curPreType, curPreNum))
    {
        return false;
    }

    // Compare major.minor.patch
    if (relMajor != curMajor)
    {
        return relMajor > curMajor;
    }
    if (relMinor != curMinor)
    {
        return relMinor > curMinor;
    }
    if (relPatch != curPatch)
    {
        return relPatch > curPatch;
    }

    // Base versions are equal -- compare build/pre-release
    if (cuttingEdgeEnabled)
    {
        bool relIsPreRelease = (relPreType != PreReleaseType::None);
        bool curIsPreRelease = (curPreType != PreReleaseType::None);

        if (relIsPreRelease && !curIsPreRelease)
        {
            // Release is a pre-release for a future build, current is stable
            return true;
        }
        if (!relIsPreRelease && curIsPreRelease)
        {
            // Release is stable, current is pre-release -- stable supersedes
            return true;
        }
        if (relIsPreRelease && curIsPreRelease)
        {
            // Both pre-release: compare type rank (RC > Beta > Alpha)
            // Lower enum value = higher precedence (None=0, RC=1, Beta=2, Alpha=3)
            if (relPreType != curPreType)
            {
                return relPreType < curPreType;
            }
            return relPreNum > curPreNum;
        }

        // Both stable: compare build numbers
        return relBuild > curBuild;
    }

    // Cutting edge disabled: only compare build numbers for stable versions
    return relBuild > curBuild;
}

const ReleaseAsset* ReleaseInfo::GetAssetForPlatform() const
{
    for (const ReleaseAsset& asset : mAssets)
    {
#if PLATFORM_WINDOWS
        // Look for Windows installer (.exe)
        if (asset.mName.find(".exe") != std::string::npos ||
            asset.mName.find("Setup") != std::string::npos ||
            asset.mName.find("Windows") != std::string::npos ||
            asset.mName.find("windows") != std::string::npos)
        {
            return &asset;
        }
#elif PLATFORM_LINUX
        // Prefer .deb package, fallback to tarball
        if (asset.mName.find(".deb") != std::string::npos)
        {
            return &asset;
        }
#endif
    }

#if PLATFORM_LINUX
    // Fallback: look for tarball
    for (const ReleaseAsset& asset : mAssets)
    {
        if (asset.mName.find(".tar.gz") != std::string::npos ||
            asset.mName.find("linux") != std::string::npos ||
            asset.mName.find("Linux") != std::string::npos)
        {
            return &asset;
        }
    }
#endif

    return nullptr;
}

#endif // EDITOR
