#pragma once

#include <stdint.h>

#include "PolyphaseAPI.h"

// Content obfuscation for "Static Content" packages.
//
// Cooked .oct assets, .lua scripts and AssetRegistry.txt are wrapped in a small
// container and XOR'd with a keystream at package time, then decoded per-file at
// read time. This defeats casual extraction and editing of a shipped package. It
// is NOT DRM -- the key is a compile-time constant in ContentObfuscation.cpp and
// therefore ships inside the executable.
//
// Three properties drive the design:
//
//  * Endian independence. Every header field is read and written a byte at a
//    time and the keystream byte is extracted by shifting, never by aliasing a
//    uint32_t. The same package decodes identically on little-endian desktop and
//    big-endian GameCube/Wii, with no swap and no #if.
//
//  * Offset addressability. A byte's keystream depends only on its own payload
//    offset, never on preceding bytes. That is what lets a truncated read
//    (Asset::LoadFile's console loadCap, AssetManager's header-only reads)
//    decode a prefix correctly on its own, and lets AudioManager seek into the
//    middle of an obfuscated .oct to stream PCM.
//
//  * No per-file state and no allocation. Decode is in-place and single-pass, so
//    it never doubles peak memory while loading a multi-MB texture on GameCube,
//    and it is reentrant on the async asset-load worker.
//
// Detection is by magic, not by a config flag, so plain and obfuscated files
// coexist in one package and the editor keeps reading its own project files
// untouched.

namespace ContentObfuscation
{
    // Container header, all multi-byte fields little-endian byte-wise:
    //
    //   0..5   magic 'P','L','Y','O','B','F'
    //   6      version (kVersion)
    //   7      flags   (kFlagChecksum)
    //   8..11  decodedSize
    //  12..15  salt
    //  16..19  checksum    FNV-1a-32 over the decoded payload
    //  20..23  headerCheck FNV-1a-32 over header bytes 0..19
    //  24..    payload
    static const uint32_t kHeaderSize = 24;
    static const uint8_t  kVersion = 1;
    static const uint8_t  kFlagChecksum = 0x01;

    // Extra bytes a capped Stream::ReadFile must request so that stripping the
    // header still leaves the caller its requested byte count. Deliberately
    // larger than kHeaderSize so a future header can grow without re-auditing
    // every capped call site.
    static const int32_t kReadHeadroom = 32;

    // True if `data` carries a well-formed container header. Cheap enough to run
    // on every file read: a magic compare plus a 20-byte hash.
    bool IsContainer(const void* data, uint32_t size);

    // Decoded payload length declared by a valid container header.
    uint32_t GetDecodedSize(const void* data);

    // Strips the header and decodes the payload in place, moving it down to
    // offset 0. Returns false if `data` is not a container or the checksum
    // failed; on success `outSize` receives the plaintext byte count, which is
    // legitimately 0 for an empty file.
    //
    // `available` is how many bytes were actually read, which may be less than
    // kHeaderSize + decodedSize for a capped read. In that case the available
    // prefix is decoded, `outTruncated` is set, and the checksum is skipped
    // because it covers the whole payload.
    bool DecodeInPlace(char* data, uint32_t available, uint32_t* outSize, bool* outTruncated);

    // Random-access decode of an already-read range, for the streaming reader
    // that never holds the whole file. `payloadOffset` is an offset into the
    // decoded payload, not into the file. Symmetric -- the same call encodes.
    void DecodeRange(void* buffer, uint32_t count, uint32_t payloadOffset, uint32_t salt);

    // Reads the salt out of a valid container header, for a streaming reader
    // that keeps the handle open and decodes range by range.
    uint32_t GetSalt(const void* data);

    // Cook side. Wraps `src` into a newly malloc'd container; caller frees with
    // free(). Not EDITOR-gated: the packaging sweep, a future .pak writer and
    // build-target addons all need it.
    bool Encode(const void* src, uint32_t srcSize, char** outData, uint32_t* outSize);

    // Reads `path`, encodes it, writes it back. Returns true and does nothing if
    // the file already carries a container, so re-running the packaging sweep is
    // safe and idempotent.
    bool EncodeFileInPlace(const char* path);
}
