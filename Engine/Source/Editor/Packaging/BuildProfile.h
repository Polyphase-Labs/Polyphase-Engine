#pragma once

#if EDITOR

#include <string>
#include <cstdint>
#include <unordered_map>
#include "EngineTypes.h"
#include "document.h"

/**
 * @brief Represents a single build profile configuration.
 *
 * Build profiles store platform-specific packaging settings including
 * target platform, output directory, and Docker usage preferences.
 */
struct BuildProfile
{
    /** @brief Unique identifier for this profile */
    uint32_t mId = 0;

    /** @brief The display name of this build profile */
    std::string mName = "Default";

    /** @brief Target platform for packaging (legacy / backward-compat fallback). */
    Platform mTargetPlatform = Platform::Windows;

    /**
     * @brief Registered build-target id (e.g. "polyphase.windows", "homebrew.dreamcast").
     *
     * If non-empty, this is the authoritative target lookup key against
     * `BuildTargetRegistry`. If empty, the engine falls back to
     * `mTargetPlatform` and resolves it to the matching built-in via
     * `BuildTargetRegistry::FindBuiltInByPlatform`. Empty for profiles
     * saved before this field existed.
     */
    std::string mTargetId = "";

    /**
     * @brief Opaque per-target options, owned by this profile.
     *
     * Flat key/value strings. Targets read/write through the
     * `GetProfileSetting` / `SetProfileSetting` trampolines on
     * `PolyphaseBuildContext`. The map is JSON-serialized as a single
     * nested object under "targetOptions". Targets that need rich
     * structure should pack their JSON into a single value, or split
     * across multiple keys with a target-specific namespace prefix.
     */
    std::unordered_map<std::string, std::string> mTargetOptions;

    /** @brief Whether to embed assets into the executable */
    bool mEmbedded = true;

    /**
     * @brief Custom output directory override.
     * If empty, defaults to {ProjectDir}/Packaged/{Platform}/
     */
    std::string mOutputDirectory = "";

    /** @brief Force Docker build even when local build is available */
    bool mUseDocker = false;

    /** @brief Open output directory when build finishes */
    bool mOpenDirectoryOnFinish = true;

    /**
     * @brief Loads profile data from a JSON value.
     * @param value The JSON value containing profile data
     */
    void LoadFromJson(const rapidjson::Value& value);

    /**
     * @brief Saves profile data to a JSON document.
     * @param doc The target JSON document
     * @param value The JSON value to populate
     */
    void SaveToJson(rapidjson::Document& doc, rapidjson::Value& value) const;
};

/**
 * @brief Gets the file extension for a platform's output executable.
 * @param platform The target platform
 * @return File extension including the dot (e.g., ".dol", ".3dsx")
 */
const char* GetPlatformOutputExtension(Platform platform);

/**
 * @brief Checks if a platform requires Docker on Windows for building.
 * @param platform The target platform
 * @return True if Docker is required on Windows (currently always false — Windows builds all platforms natively)
 */
bool PlatformRequiresDockerOnWindows(Platform platform);

/**
 * @brief Checks if a platform supports running via emulator.
 * @param platform The target platform
 * @return True if the platform has emulator support (GameCube, Wii, 3DS)
 */
bool PlatformSupportsRun(Platform platform);

#endif
