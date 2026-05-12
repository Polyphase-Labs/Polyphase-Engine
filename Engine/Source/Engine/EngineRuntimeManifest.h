#pragma once

// EngineRuntimeManifest
//
// In-memory representation of the `engine-runtime.json` file that the
// build system emits next to a shared Polyphase engine runtime
// (PolyphaseGame.dll / libPolyphaseGame.so). The validator (W3) reads
// this manifest to decide whether a vendored runtime is compatible with
// the current source tree.
//
// This header is intentionally NOT editor-only (no `#if EDITOR`): the
// parser/serializer must be reachable from the editor, from CLI / smoke
// utilities, and from any future headless tool.
//
// JSON schema (v1, frozen):
//   {
//     "schemaVersion": 1,
//     "engine": { "name", "version", "versionMajor", "abi",
//                 "buildHash", "buildTimestampUtc" },
//     "target": { "platform", "arch", "config" },
//     "addonApiVersion": <uint>,
//     "assetVersion":    <uint>,
//     "binary": { "module", "importLib", "debugSymbols",
//                 "moduleSha256", "moduleSize" },
//     "requiredExports": [ ... ],
//     "compiler": { "id", "version", "crt" }
//   }
//
// Optional binary fields (`importLib`, `debugSymbols`) are encoded as
// empty strings (`""`) when not applicable. The serializer ALWAYS emits
// the keys with empty string values rather than omitting them, so the
// schema shape stays uniform across platforms. The parser accepts both
// missing keys and empty strings.

#include <cstdint>
#include <string>
#include <vector>

struct EngineRuntimeManifest
{
    uint32_t schemaVersion = 0;

    std::string engineName;
    std::string engineVersion;     // e.g. "6.2.0-beta.4"
    uint32_t    engineVersionMajor = 0;
    uint32_t    engineAbi = 0;
    std::string buildHash;         // git short SHA or "local"
    std::string buildTimestampUtc; // ISO-8601

    std::string targetPlatform;    // "Windows" | "Linux"
    std::string targetArch;        // "x64"
    std::string targetConfig;      // "Debug" | "Release"

    uint32_t addonApiVersion = 0;
    uint32_t assetVersion    = 0;

    std::string binaryModule;        // file name only (e.g. "libPolyphaseGame.so")
    std::string binaryImportLib;     // empty if N/A (Linux .so has no import lib)
    std::string binaryDebugSymbols;  // empty if N/A
    std::string binaryModuleSha256;  // lowercase hex, 64 chars
    uint64_t    binaryModuleSize = 0;

    std::vector<std::string> requiredExports;

    std::string compilerId;       // "MSVC" | "GCC" | "Clang"
    std::string compilerVersion;
    std::string compilerCrt;      // "MultiThreadedDLL" | "libstdc++" | ...
};

// Parses a JSON document held in memory. Returns false and sets outError
// to a specific diagnostic on the first missing-required-key or
// type-mismatch problem encountered. Missing optional keys leave the
// corresponding `out` field at its default-initialized value.
bool ParseEngineRuntimeManifestJson(const std::string& json,
                                    EngineRuntimeManifest& out,
                                    std::string& outError);

// Reads jsonPath from disk and forwards to ParseEngineRuntimeManifestJson.
// Returns false (with outError set) on file-not-found or parse failure.
bool LoadEngineRuntimeManifest(const std::string& jsonPath,
                               EngineRuntimeManifest& out,
                               std::string& outError);

// Returns a pretty-printed (rapidjson PrettyWriter) JSON document.
// The output is valid JSON suitable for round-tripping through
// ParseEngineRuntimeManifestJson.
std::string SerializeEngineRuntimeManifest(const EngineRuntimeManifest& m);
