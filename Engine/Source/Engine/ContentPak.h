#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "PolyphaseAPI.h"

// Content.pak -- a single-archive shipping format for Static builds.
//
// Phase 1 obfuscated loose files: contents were protected but the filenames and
// directory tree stayed readable, and every asset still cost a directory entry
// on the target's filesystem. A pak folds all of it into one file:
//
//   [ header  32 bytes, plain -- just enough to find the index ]
//   [ data    per-entry ContentObfuscation containers, concatenated ]
//   [ index   entry table + path blob, itself obfuscated ]
//
// Only the index is resident at runtime; entry bytes are read and decoded on
// demand, so console memory behaviour matches the loose-file path it replaces.
// Because the index is obfuscated, the paths are hidden too -- a shipped pak
// leaks neither content nor names.
//
// Entries are stored as the same containers Phase 1 writes, so Stream::ReadFile's
// existing decode handles them unchanged; the pak layer only has to hand back the
// right span of bytes. That also keeps truncated reads working: the keystream is
// offset-addressable, so a capped read decodes a correct prefix.
//
// Lookups are path-keyed, which Phase 1 deliberately avoided. It is safe here
// because the pak index is the single authority on naming -- keys are written by
// the cook and read back verbatim, with no per-platform path canonicalisation to
// disagree about. Hash hits are confirmed against the stored path, so a 64-bit
// collision degrades to a miss rather than to silently serving the wrong asset.

namespace ContentPak
{
    static const uint32_t kHeaderSize = 32;

    // ---- Runtime -----------------------------------------------------------

    // Reads the header and index only. Safe to call when the file is absent --
    // returns false and leaves the pak unmounted so callers fall back to loose
    // files.
    bool Mount(const char* pakPath);
    void Unmount();
    bool IsMounted();

    bool Exists(const char* path);

    // Reads an entry's stored (still-obfuscated) bytes. `outData` is malloc'd so
    // Stream can take ownership and free() it, matching SYS_AcquireFileData.
    // `maxSize > 0` reads a prefix; the container header is accounted for.
    bool Read(const char* path, int32_t maxSize, char*& outData, uint32_t& outSize);

    // Locates an entry's raw span for the seekable streaming reader, which needs
    // its own handle rather than a whole-file read.
    bool FindEntry(const char* path, uint32_t& outDataOffset, uint32_t& outDataSize);

    // Path the pak was mounted from, for opening independent streaming handles.
    const char* GetPakPath();

    // Every key under `prefix`. Needed because some content is discovered by
    // walking a directory rather than by name -- the Vulkan global shaders are
    // enumerated from Engine/Shaders/GLSL/bin/ -- and a packed build has no
    // directory to walk.
    void List(const char* prefix, std::vector<std::string>& outKeys);

    // ---- Cook --------------------------------------------------------------

    struct SourceFile
    {
        std::string mKey;          // canonical, package-relative (e.g. "Game/Assets/T.oct")
        std::string mSourcePath;   // absolute path to read from
    };

    // Writes `pakPath` from `files`. Each file is wrapped in a ContentObfuscation
    // container (or passed through if already wrapped). Returns false on any I/O
    // failure -- callers must not prune loose content unless this succeeded.
    bool Build(const char* pakPath, const std::vector<SourceFile>& files, uint32_t& outEntryCount);
}
