#pragma once

#if EDITOR

#include "Editor/Validation/ValidationLog.h"
#include "Engine/EngineRuntimeManifest.h"
#include "Engine/PolyphaseAbi.h"

#include <cstdint>
#include <string>

// EngineRuntimeValidator
//
// Reads a vendored engine runtime from
//   <Project>/Vendor/PolyphaseRuntime/<Platform>-<Arch>-<Config>/
// and produces a Validation::Report describing whether the runtime is
// usable. Stable error codes (each becomes a single Validation::Message):
//
//   RT_FOLDER_MISSING       — vendored runtime folder absent
//   RT_MANIFEST_MISSING     — engine-runtime.json absent
//   RT_MANIFEST_PARSE       — manifest JSON parse error
//   RT_SCHEMA_VERSION       — manifest schemaVersion newer than supported
//   RT_TARGET_MISMATCH      — platform/arch/config != requested
//   RT_ABI_MISMATCH         — manifest engine.abi != requiredAbi
//   RT_VERSION_OLDER        — warning when versions differ but ABI matches
//   RT_BINARY_MISSING       — binary.module absent on disk
//   RT_BINARY_HASH          — on-disk SHA256 != binary.moduleSha256
//   RT_IMPORTLIB_MISSING    — Windows: binary.importLib absent (Warning by
//                             default, Error when attemptSymbolProbe is set)
//   RT_RPATH_NONLOCAL       — Linux: readelf shows no $ORIGIN rpath
//                             (Warning; degrades to silent skip if readelf
//                             unavailable)
//   RT_SYMBOL_PROBE_FAILED  — when attemptSymbolProbe is set, dlopen +
//                             dlsym for each requiredExports entry failed
//
// Validators expose the parsed manifest on success so callers can render
// it back to the user. Update() copies a source runtime into the project's
// Vendor/ tree and re-runs validation against the copied result.

constexpr uint32_t kRuntimeManifestSchemaVersionSupported = 1;

struct RuntimeValidationOptions
{
    std::string projectDir;
    std::string platform; // "Windows" | "Linux"
    std::string arch;     // "x64"
    std::string config;   // "Debug" | "Release"
    uint32_t    requiredAbi = POLYPHASE_ENGINE_ABI;
    bool        attemptSymbolProbe = false;
};

struct RuntimeValidationResult
{
    Validation::Report report;
    EngineRuntimeManifest manifest;
    std::string runtimeFolder; // absolute path that was inspected
};

struct RuntimeUpdateOptions
{
    std::string projectDir;
    std::string sourceRuntimePath; // directory containing module + manifest
    std::string platform;
    std::string arch;
    std::string config;
    bool overwrite = true;
};

// Composes the vendored runtime path for a given option set:
//   <projectDir>/Vendor/PolyphaseRuntime/<Platform>-<Arch>-<Config>
// Returns an empty string if any of projectDir / platform / arch / config
// is empty.
std::string GetProjectRuntimeFolder(const std::string& projectDir,
                                    const std::string& platform,
                                    const std::string& arch,
                                    const std::string& config);

RuntimeValidationResult ValidateProjectRuntime(const RuntimeValidationOptions& opts);
RuntimeValidationResult UpdateProjectRuntime(const RuntimeUpdateOptions& opts);

#endif // EDITOR
