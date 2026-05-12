#pragma once

// Sha256
//
// Minimal SHA-256 helper used by the engine-runtime manifest and the
// runtime validator. Implementation is a self-contained translation of
// the FIPS-180-4 specification (no third-party code imported), so it is
// license-clean for inclusion in shipping engine builds.
//
// Output convention: lowercase hex, 64 characters, no separators.

#include <cstddef>
#include <cstdint>
#include <string>

struct Sha256
{
    // Hash an arbitrary contiguous buffer.
    static std::string HashHex(const void* data, size_t size);

    // Stream-hash a file from disk in fixed-size chunks (does not load
    // the whole file into memory). On failure, returns false and fills
    // outError with a diagnostic. On success, outHex receives the
    // 64-character lowercase hex digest.
    static bool HashFileHex(const std::string& path,
                            std::string& outHex,
                            std::string& outError);
};
