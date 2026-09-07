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
//
// Multiple mounts stack rather than replace: Mount()/MountMemory() push a new
// layer and Find() searches newest-first, so a downloaded content bundle can
// shadow (or add to) the base pak without evicting it. MountMemory() exists
// because some platforms have no writable storage at all for a downloaded
// bundle (e.g. GameCube memory cards) -- the pak is read straight out of the
// buffer it arrived in.

namespace ContentPak
{
    static const uint32_t kHeaderSize = 32;

    // Identifies one mounted pak. 0 is never a valid handle.
    typedef uint32_t MountHandle;

    // ---- Runtime -----------------------------------------------------------

    // Reads the header and index of a pak on disk and pushes it as a new,
    // top-priority mount. Safe to call when the file is absent -- returns 0 and
    // leaves prior mounts (if any) untouched, so callers fall back to loose
    // files or to whatever was already mounted.
    POLYPHASE_API MountHandle Mount(const char* pakPath);

    // Same, but for a pak that already lives in memory -- the only option on a
    // platform with no writable storage for a downloaded bundle. If
    // takeOwnership is true, ContentPak frees `data` with free() on Unmount, so
    // the buffer must come from malloc (Stream and Http response bodies do); if
    // false, the caller owns `data` and must keep it alive until Unmount.
    // debugName is used only for logging.
    POLYPHASE_API MountHandle MountMemory(const void* data, uint32_t size,
                                          const char* debugName, bool takeOwnership);

    // Removes one mount. Entries from other mounts are unaffected.
    POLYPHASE_API void Unmount(MountHandle handle);

    // Removes every mount.
    POLYPHASE_API void UnmountAll();

    // True if at least one pak is mounted.
    POLYPHASE_API bool IsMounted();

    POLYPHASE_API bool Exists(const char* path);

    // Reads an entry's stored (still-obfuscated) bytes from whichever mount has
    // it, newest first. `outData` is malloc'd so Stream can take ownership and
    // free() it, matching SYS_AcquireFileData. `maxSize > 0` reads a prefix; the
    // container header is accounted for.
    POLYPHASE_API bool Read(const char* path, int32_t maxSize, char*& outData, uint32_t& outSize);

    // Locates an entry's raw span for the seekable streaming reader, which needs
    // its own handle rather than a whole-file read. outMount identifies which
    // mount owns the entry, for GetPakPath / GetMountMemory.
    POLYPHASE_API bool FindEntry(const char* path, MountHandle& outMount,
                                 uint32_t& outDataOffset, uint32_t& outDataSize);

    // Path a *file-backed* mount was opened from, for opening an independent
    // streaming handle on the same archive. Returns "" for a memory mount or an
    // unrecognised handle -- try GetMountMemory() in that case.
    POLYPHASE_API const char* GetPakPath(MountHandle handle);

    // Raw buffer backing a *memory* mount, so the seekable reader can read
    // straight out of it instead of opening a file. Returns false for a
    // file-backed mount or an unrecognised handle.
    POLYPHASE_API bool GetMountMemory(MountHandle handle, const uint8_t*& outData, uint32_t& outSize);

    // Every key under `prefix`, merged across all mounts (each key reported
    // once, from whichever mount would win a Find()). Needed because some
    // content is discovered by walking a directory rather than by name -- the
    // Vulkan global shaders are enumerated from Engine/Shaders/GLSL/bin/ -- and
    // a packed build has no directory to walk.
    POLYPHASE_API void List(const char* prefix, std::vector<std::string>& outKeys);

    // ---- Cook --------------------------------------------------------------

    struct SourceFile
    {
        std::string mKey;          // canonical, package-relative (e.g. "Game/Assets/T.oct")
        std::string mSourcePath;   // absolute path to read from
    };

    // Writes `pakPath` from `files`. Each file is wrapped in a ContentObfuscation
    // container (or passed through if already wrapped). Returns false on any I/O
    // failure -- callers must not prune loose content unless this succeeded.
    POLYPHASE_API bool Build(const char* pakPath, const std::vector<SourceFile>& files, uint32_t& outEntryCount);
}
