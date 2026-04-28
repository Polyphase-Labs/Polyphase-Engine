#pragma once

#if EDITOR

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Pre-release type classification for version strings.
 */
enum class PreReleaseType
{
    None = 0,   // Stable release (highest precedence)
    RC,         // Release Candidate
    Beta,       // Beta (includes "beata" typo variant)
    Alpha       // Alpha (lowest precedence)
};

/**
 * @brief Represents a downloadable asset from a GitHub release.
 */
struct ReleaseAsset
{
    std::string mName;              // e.g., "PolyphaseSetup-6.0.0.exe"
    std::string mDownloadUrl;       // Direct download URL
    std::string mContentType;       // e.g., "application/octet-stream"
    size_t mSize = 0;               // File size in bytes
};

/**
 * @brief Represents a GitHub release with version info and changelog.
 */
struct ReleaseInfo
{
    std::string mTagName;           // e.g., "v6.0.0"
    std::string mName;              // Release title, e.g., "Release 6.0.0"
    std::string mBody;              // Changelog/release notes (markdown)
    std::string mHtmlUrl;           // URL to GitHub release page
    std::string mPublishedAt;       // ISO timestamp
    bool mPrerelease = false;       // Whether this is a GitHub pre-release
    std::vector<ReleaseAsset> mAssets;

    /**
     * @brief Extract the version string from tag name (strips 'v' prefix).
     * @return Version string like "6.0.0"
     */
    std::string GetVersion() const
    {
        if (!mTagName.empty() && mTagName[0] == 'v')
        {
            return mTagName.substr(1);
        }
        return mTagName;
    }

    /**
     * @brief Compare this release version against the current version.
     * @param currentVersion The current version string (e.g., "5.0.0")
     * @return true if this release is newer than currentVersion
     */
    bool IsNewerThan(const std::string& currentVersion) const;

    /**
     * @brief Compare this release version against the current version with cutting edge support.
     * @param currentVersion The current version string
     * @param cuttingEdgeEnabled If true, pre-release versions are considered newer
     * @return true if this release is newer than currentVersion
     */
    bool IsNewerThan(const std::string& currentVersion, bool cuttingEdgeEnabled) const;

    /**
     * @brief Get the appropriate download asset for the current platform.
     * @return Pointer to the asset, or nullptr if not found
     */
    const ReleaseAsset* GetAssetForPlatform() const;

    /**
     * @brief Parse version string into major, minor, patch components.
     * @param version Version string like "6.0.0" or "6.1"
     * @param outMajor Output major version
     * @param outMinor Output minor version
     * @param outPatch Output patch version
     * @return true if parsing succeeded
     */
    static bool ParseVersion(const std::string& version, int& outMajor, int& outMinor, int& outPatch);

    /**
     * @brief Parse version string into full components including build and pre-release info.
     * @param version Version string like "6.1.1.1" or "6.1.1-beta.5"
     * @param outMajor Output major version
     * @param outMinor Output minor version
     * @param outPatch Output patch version
     * @param outBuild Output build number (4th component, 0 if absent)
     * @param outPreReleaseType Output pre-release type (None if stable)
     * @param outPreReleaseNum Output pre-release number (0 if absent)
     * @return true if parsing succeeded
     */
    static bool ParseVersion(const std::string& version, int& outMajor, int& outMinor, int& outPatch,
                             int& outBuild, PreReleaseType& outPreReleaseType, int& outPreReleaseNum);
};

#endif // EDITOR
