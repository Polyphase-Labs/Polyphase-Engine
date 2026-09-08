#include "ContentPak.h"
#include "ContentObfuscation.h"
#include "Log.h"
#include "System/System.h"

#if !PLATFORM_MAC
#include <malloc.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>

// Header layout, all multi-byte fields written byte-wise little-endian so the
// same pak reads identically on big-endian Dolphin and little-endian desktop:
//
//    0  8  magic "PLYPAK1\0"
//    8  4  version
//   12  4  entryCount
//   16  4  indexOffset
//   20  4  indexSize      (bytes of the obfuscated index container)
//   24  4  headerCheck    FNV-1a-32 over bytes 0..23
//   28  4  reserved
//
// Index (after decoding the container), entryCount records of 24 bytes:
//
//    0  8  pathHash     FNV-1a-64 of the canonical key
//    8  4  dataOffset
//   12  4  dataSize
//   16  4  pathOffset   into the path blob that follows the records
//   20  4  pathLength
//
// Records are sorted by pathHash so lookups are a binary search.

static const char kPakMagic[8] = { 'P', 'L', 'Y', 'P', 'A', 'K', '1', '\0' };
static const uint32_t kPakVersion = 1;
static const uint32_t kRecordSize = 24;

#define FNV32_BASIS 0x811C9DC5u
#define FNV32_PRIME 0x01000193u
#define FNV64_BASIS 0xCBF29CE484222325ULL
#define FNV64_PRIME 0x00000100000001B3ULL

namespace
{
    struct PakRecord
    {
        uint64_t mPathHash = 0;
        uint32_t mDataOffset = 0;
        uint32_t mDataSize = 0;
        uint32_t mPathOffset = 0;
        uint32_t mPathLength = 0;
    };

    // One mounted archive, file- or memory-backed. Held behind unique_ptr in
    // sMounts so a later push_back never invalidates a pointer a caller is
    // still holding -- ContentPak::FindEntry hands a MountHandle to callers
    // that outlive the lookup (e.g. SysFile).
    struct PakMount
    {
        ContentPak::MountHandle mHandle = 0;
        std::string mDebugName;

        bool mIsMemory = false;

        // File-backed.
        FILE* mFile = nullptr;
        std::string mPakPath;   // resolved path, for independent streaming handles

        // Memory-backed.
        const uint8_t* mMemoryData = nullptr;
        uint32_t mMemorySize = 0;
        bool mOwnsMemory = false;

        MutexObject* mMutex = nullptr;
        std::vector<PakRecord> mRecords;
        std::vector<char> mPathBlob;

        ~PakMount()
        {
            if (mFile != nullptr) fclose(mFile);
            if (mOwnsMemory && mMemoryData != nullptr) free((void*)mMemoryData);
            if (mMutex != nullptr) SYS_DestroyMutex(mMutex);
        }
    };

    // Mounts stack in push order; sMounts.back() is searched first. A
    // monotonically increasing counter keeps handles unique across the
    // lifetime of the process even as mounts come and go.
    std::vector<std::unique_ptr<PakMount>> sMounts;
    ContentPak::MountHandle sNextHandle = 0;

    inline uint32_t RdLE32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    inline void WrLE32(uint8_t* p, uint32_t v)
    {
        p[0] = (uint8_t)(v);
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        p[3] = (uint8_t)(v >> 24);
    }

    inline uint64_t RdLE64(const uint8_t* p)
    {
        return (uint64_t)RdLE32(p) | ((uint64_t)RdLE32(p + 4) << 32);
    }

    inline void WrLE64(uint8_t* p, uint64_t v)
    {
        WrLE32(p, (uint32_t)(v & 0xFFFFFFFFu));
        WrLE32(p + 4, (uint32_t)(v >> 32));
    }

    uint32_t Fnv1a32(const void* data, uint32_t size)
    {
        const uint8_t* bytes = (const uint8_t*)data;
        uint32_t hash = FNV32_BASIS;
        for (uint32_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= FNV32_PRIME; }
        return hash;
    }

    // Normalises a runtime path into the key form the cook wrote: forward
    // slashes, no leading "./". Kept deliberately small -- the pak index is the
    // naming authority, so this only has to absorb separator style, not resolve
    // platform device prefixes (those are added below Stream, inside
    // SYS_AcquireFileData, and never reach here).
    std::string Canonicalise(const char* path)
    {
        if (path == nullptr) return std::string();

        const char* in = path;
        if (in[0] == '.' && (in[1] == '/' || in[1] == '\\')) in += 2;

        std::string out;
        out.reserve(strlen(in));

        for (const char* p = in; *p != '\0'; ++p)
        {
            out.push_back(*p == '\\' ? '/' : *p);
        }

        return out;
    }

    uint64_t HashKey(const std::string& key)
    {
        uint64_t hash = FNV64_BASIS;
        for (size_t i = 0; i < key.size(); ++i)
        {
            hash ^= (uint8_t)key[i];
            hash *= FNV64_PRIME;
        }
        return hash;
    }

    PakMount* FindMount(ContentPak::MountHandle handle)
    {
        for (auto& mount : sMounts)
        {
            if (mount->mHandle == handle) return mount.get();
        }
        return nullptr;
    }

    // Binary search a single mount's records by hash, then confirm against the
    // stored path. Equal hashes are scanned linearly so a collision falls
    // through to a miss instead of returning the wrong entry.
    const PakRecord* FindInMount(const PakMount* mount, const std::string& key, uint64_t hash)
    {
        const std::vector<PakRecord>& records = mount->mRecords;

        size_t lo = 0;
        size_t hi = records.size();
        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (records[mid].mPathHash < hash) lo = mid + 1;
            else hi = mid;
        }

        for (size_t i = lo; i < records.size() && records[i].mPathHash == hash; ++i)
        {
            const PakRecord& rec = records[i];
            if (rec.mPathLength == key.size() &&
                rec.mPathOffset + rec.mPathLength <= mount->mPathBlob.size() &&
                memcmp(&mount->mPathBlob[rec.mPathOffset], key.c_str(), key.size()) == 0)
            {
                return &rec;
            }
        }

        return nullptr;
    }

    struct FindResult
    {
        PakMount* mMount = nullptr;
        const PakRecord* mRecord = nullptr;
    };

    // Newest-mounted-first, so a later mount shadows an earlier one instead of
    // colliding with it.
    FindResult Find(const char* path)
    {
        if (sMounts.empty() || path == nullptr) return FindResult();

        const std::string key = Canonicalise(path);
        const uint64_t hash = HashKey(key);

        for (auto it = sMounts.rbegin(); it != sMounts.rend(); ++it)
        {
            const PakRecord* rec = FindInMount(it->get(), key, hash);
            if (rec != nullptr)
            {
                FindResult result;
                result.mMount = it->get();
                result.mRecord = rec;
                return result;
            }
        }

        return FindResult();
    }

    // Shared header parse, fed either a file (Mount) or an in-memory buffer
    // (MountMemory).
    struct ParsedHeader
    {
        uint32_t mEntryCount = 0;
        uint32_t mIndexOffset = 0;
        uint32_t mIndexSize = 0;
    };

    bool ParseAndCheckHeader(const uint8_t* header, ParsedHeader& out)
    {
        if (memcmp(header, kPakMagic, sizeof(kPakMagic)) != 0 ||
            RdLE32(header + 8) != kPakVersion ||
            RdLE32(header + 24) != Fnv1a32(header, 24))
        {
            return false;
        }

        out.mEntryCount = RdLE32(header + 12);
        out.mIndexOffset = RdLE32(header + 16);
        out.mIndexSize = RdLE32(header + 20);
        return true;
    }

    // `index` holds indexSize raw bytes copied out of the source (file or
    // memory); ContentObfuscation::DecodeInPlace mutates it in place.
    bool FinishMount(PakMount& mount, std::vector<char>& index, const ParsedHeader& parsed)
    {
        uint32_t decodedSize = 0;
        if (!ContentObfuscation::DecodeInPlace(index.data(), parsed.mIndexSize, &decodedSize, nullptr))
        {
            LogError("ContentPak: '%s' index failed to decode", mount.mDebugName.c_str());
            return false;
        }

        const uint32_t recordBytes = parsed.mEntryCount * kRecordSize;
        if (decodedSize < recordBytes)
        {
            LogError("ContentPak: '%s' index is truncated", mount.mDebugName.c_str());
            return false;
        }

        mount.mRecords.resize(parsed.mEntryCount);
        for (uint32_t i = 0; i < parsed.mEntryCount; ++i)
        {
            const uint8_t* rec = (const uint8_t*)index.data() + (i * kRecordSize);
            mount.mRecords[i].mPathHash = RdLE64(rec);
            mount.mRecords[i].mDataOffset = RdLE32(rec + 8);
            mount.mRecords[i].mDataSize = RdLE32(rec + 12);
            mount.mRecords[i].mPathOffset = RdLE32(rec + 16);
            mount.mRecords[i].mPathLength = RdLE32(rec + 20);
        }

        mount.mPathBlob.assign(index.begin() + recordBytes, index.begin() + decodedSize);
        return true;
    }
}

ContentPak::MountHandle ContentPak::Mount(const char* pakPath)
{
    if (pakPath == nullptr) return 0;

    // Try the raw path first (works where the CWD is already the content root),
    // then the platform-resolved one. 3DS needs "romfs:/" prepended and PS2 a
    // "host:" device prefix, which SYS_GetAbsolutePath supplies -- without this
    // the pak simply fails to open on those targets.
    std::string resolvedPath = pakPath;
    FILE* file = fopen(pakPath, "rb");

    if (file == nullptr)
    {
        resolvedPath = SYS_GetAbsolutePath(pakPath);
        file = fopen(resolvedPath.c_str(), "rb");
    }

    if (file == nullptr) return 0;

    uint8_t header[kHeaderSize] = { };
    ParsedHeader parsed;
    if (fread(header, 1, kHeaderSize, file) != kHeaderSize ||
        !ParseAndCheckHeader(header, parsed))
    {
        fclose(file);
        LogError("ContentPak: '%s' is not a valid pak", pakPath);
        return 0;
    }

    std::vector<char> index(parsed.mIndexSize);
    if (parsed.mIndexSize == 0 ||
        fseek(file, (long)parsed.mIndexOffset, SEEK_SET) != 0 ||
        fread(index.data(), 1, parsed.mIndexSize, file) != parsed.mIndexSize)
    {
        fclose(file);
        LogError("ContentPak: '%s' index is unreadable", pakPath);
        return 0;
    }

    std::unique_ptr<PakMount> mount(new PakMount());
    mount->mDebugName = pakPath;
    mount->mFile = file;
    mount->mPakPath = resolvedPath;
    mount->mIsMemory = false;

    if (!FinishMount(*mount, index, parsed))
    {
        return 0;   // mount's destructor closes `file`
    }

    mount->mMutex = SYS_CreateMutex();
    mount->mHandle = ++sNextHandle;
    const MountHandle handle = mount->mHandle;
    const uint32_t entryCount = parsed.mEntryCount;

    sMounts.push_back(std::move(mount));

    LogDebug("ContentPak: mounted '%s' (%u entries, handle=%u)", pakPath, entryCount, handle);
    return handle;
}

ContentPak::MountHandle ContentPak::MountMemory(const void* data, uint32_t size,
                                                const char* debugName, bool takeOwnership)
{
    if (data == nullptr || size < kHeaderSize) return 0;

    const uint8_t* bytes = (const uint8_t*)data;

    ParsedHeader parsed;
    if (!ParseAndCheckHeader(bytes, parsed))
    {
        LogError("ContentPak: memory mount '%s' is not a valid pak",
            debugName != nullptr ? debugName : "<unnamed>");
        return 0;
    }

    if (parsed.mIndexSize == 0 ||
        (uint64_t)parsed.mIndexOffset + parsed.mIndexSize > size)
    {
        LogError("ContentPak: memory mount '%s' index is out of range",
            debugName != nullptr ? debugName : "<unnamed>");
        return 0;
    }

    // DecodeInPlace mutates its buffer, so copy the index out of the caller's
    // (possibly read-only, possibly shared) memory first.
    std::vector<char> index(bytes + parsed.mIndexOffset, bytes + parsed.mIndexOffset + parsed.mIndexSize);

    std::unique_ptr<PakMount> mount(new PakMount());
    mount->mDebugName = debugName != nullptr ? debugName : "<unnamed>";
    mount->mIsMemory = true;
    mount->mMemoryData = bytes;
    mount->mMemorySize = size;
    mount->mOwnsMemory = takeOwnership;

    if (!FinishMount(*mount, index, parsed))
    {
        mount->mOwnsMemory = false;   // caller retains ownership on failure
        return 0;
    }

    mount->mMutex = SYS_CreateMutex();
    mount->mHandle = ++sNextHandle;
    const MountHandle handle = mount->mHandle;
    const uint32_t entryCount = parsed.mEntryCount;

    // Log before the move -- sMounts.push_back(std::move(mount)) nulls out
    // this local unique_ptr, so `mount->mDebugName` right after it was a
    // null-pointer deref (mDebugName lives at some small offset into the
    // now-gone object, so it reliably crashed inside std::string's own SSO
    // check rather than at the dereference itself, which made it look like
    // string corruption rather than what it actually was).
    LogDebug("ContentPak: mounted '%s' from memory (%u bytes, %u entries, handle=%u)",
        mount->mDebugName.c_str(), size, entryCount, handle);

    sMounts.push_back(std::move(mount));
    return handle;
}

void ContentPak::Unmount(MountHandle handle)
{
    for (size_t i = 0; i < sMounts.size(); ++i)
    {
        if (sMounts[i]->mHandle == handle)
        {
            sMounts.erase(sMounts.begin() + i);
            return;
        }
    }
}

void ContentPak::UnmountAll()
{
    sMounts.clear();
}

bool ContentPak::IsMounted()
{
    return !sMounts.empty();
}

bool ContentPak::Exists(const char* path)
{
    return Find(path).mRecord != nullptr;
}

const char* ContentPak::GetPakPath(MountHandle handle)
{
    PakMount* mount = FindMount(handle);
    static const char* kEmpty = "";
    return (mount != nullptr && !mount->mIsMemory) ? mount->mPakPath.c_str() : kEmpty;
}

bool ContentPak::GetMountMemory(MountHandle handle, const uint8_t*& outData, uint32_t& outSize)
{
    PakMount* mount = FindMount(handle);
    if (mount == nullptr || !mount->mIsMemory) { outData = nullptr; outSize = 0; return false; }

    outData = mount->mMemoryData;
    outSize = mount->mMemorySize;
    return true;
}

void ContentPak::List(const char* prefix, std::vector<std::string>& outKeys)
{
    outKeys.clear();

    if (sMounts.empty() || prefix == nullptr) return;

    const std::string want = Canonicalise(prefix);

    // Records aren't ordered by path within a mount, and mounts aren't ordered
    // by content either, so this is a full linear scan. It runs once per prefix
    // at startup over a few hundred entries -- not worth a second index. Newer
    // mounts are scanned first and a key already emitted is skipped, so a
    // shadowing entry in a later mount is the one reported.
    for (auto it = sMounts.rbegin(); it != sMounts.rend(); ++it)
    {
        const PakMount* mount = it->get();
        for (size_t i = 0; i < mount->mRecords.size(); ++i)
        {
            const PakRecord& rec = mount->mRecords[i];

            if (rec.mPathLength < want.size() ||
                rec.mPathOffset + rec.mPathLength > mount->mPathBlob.size())
            {
                continue;
            }

            if (memcmp(&mount->mPathBlob[rec.mPathOffset], want.c_str(), want.size()) == 0)
            {
                std::string key(&mount->mPathBlob[rec.mPathOffset], rec.mPathLength);
                if (std::find(outKeys.begin(), outKeys.end(), key) == outKeys.end())
                {
                    outKeys.push_back(std::move(key));
                }
            }
        }
    }
}

bool ContentPak::FindEntry(const char* path, MountHandle& outMount, uint32_t& outDataOffset, uint32_t& outDataSize)
{
    outMount = 0;
    outDataOffset = 0;
    outDataSize = 0;

    const FindResult found = Find(path);
    if (found.mRecord == nullptr) return false;

    outMount = found.mMount->mHandle;
    outDataOffset = found.mRecord->mDataOffset;
    outDataSize = found.mRecord->mDataSize;
    return true;
}

bool ContentPak::Read(const char* path, int32_t maxSize, char*& outData, uint32_t& outSize)
{
    outData = nullptr;
    outSize = 0;

    const FindResult found = Find(path);
    if (found.mRecord == nullptr) return false;

    PakMount* mount = found.mMount;
    const PakRecord* rec = found.mRecord;

    uint32_t readSize = rec->mDataSize;
    if (maxSize > 0)
    {
        // The caller's cap is on decoded bytes; the container header rides on top
        // of it. Stream::ReadFile already adds the same headroom for loose files.
        const uint32_t capped = (uint32_t)maxSize + ContentObfuscation::kHeaderSize;
        if (capped < readSize) readSize = capped;
    }

    char* buffer = (char*)malloc(readSize > 0 ? readSize : 1);
    if (buffer == nullptr)
    {
        LogError("ContentPak: out of memory reading '%s' (%u bytes)", path, readSize);
        return false;
    }

    if (mount->mIsMemory)
    {
        if ((uint64_t)rec->mDataOffset + readSize > mount->mMemorySize)
        {
            free(buffer);
            LogError("ContentPak: '%s' entry runs past the end of its memory mount", path);
            return false;
        }
        memcpy(buffer, mount->mMemoryData + rec->mDataOffset, readSize);
    }
    else
    {
        // The async asset loader shares this handle with the main thread, so the
        // seek and read have to be atomic with respect to each other.
        ScopedLock lock(mount->mMutex);

        if (fseek(mount->mFile, (long)rec->mDataOffset, SEEK_SET) != 0 ||
            fread(buffer, 1, readSize, mount->mFile) != readSize)
        {
            free(buffer);
            LogError("ContentPak: failed reading '%s' from pak", path);
            return false;
        }
    }

    outData = buffer;
    outSize = readSize;
    return true;
}

bool ContentPak::Build(const char* pakPath, const std::vector<SourceFile>& files, uint32_t& outEntryCount)
{
    outEntryCount = 0;

    if (pakPath == nullptr) return false;

    FILE* out = fopen(pakPath, "wb");
    if (out == nullptr)
    {
        LogError("ContentPak: cannot create '%s'", pakPath);
        return false;
    }

    // Reserve the header; it's rewritten once the index offset is known.
    uint8_t header[kHeaderSize] = { };
    if (fwrite(header, 1, kHeaderSize, out) != kHeaderSize)
    {
        fclose(out);
        LogError("ContentPak: cannot write header to '%s'", pakPath);
        return false;
    }

    std::vector<PakRecord> records;
    std::vector<char> pathBlob;
    records.reserve(files.size());

    uint32_t cursor = kHeaderSize;

    for (size_t i = 0; i < files.size(); ++i)
    {
        const SourceFile& src = files[i];

        FILE* in = fopen(src.mSourcePath.c_str(), "rb");
        if (in == nullptr)
        {
            fclose(out);
            LogError("ContentPak: cannot read '%s'", src.mSourcePath.c_str());
            return false;
        }

        fseek(in, 0, SEEK_END);
        const long fileSize = ftell(in);
        fseek(in, 0, SEEK_SET);

        if (fileSize < 0)
        {
            fclose(in);
            fclose(out);
            LogError("ContentPak: cannot size '%s'", src.mSourcePath.c_str());
            return false;
        }

        std::vector<char> raw((size_t)fileSize > 0 ? (size_t)fileSize : 1);
        if (fileSize > 0 && fread(raw.data(), (size_t)fileSize, 1, in) != 1)
        {
            fclose(in);
            fclose(out);
            LogError("ContentPak: short read on '%s'", src.mSourcePath.c_str());
            return false;
        }
        fclose(in);

        // Files may already carry a container if the loose-file sweep ran first;
        // wrapping twice would just grow them, so pass those through.
        const char* payload = raw.data();
        uint32_t payloadSize = (uint32_t)fileSize;
        char* encoded = nullptr;
        uint32_t encodedSize = 0;

        if (!ContentObfuscation::IsContainer(raw.data(), (uint32_t)fileSize))
        {
            if (!ContentObfuscation::Encode(raw.data(), (uint32_t)fileSize, &encoded, &encodedSize))
            {
                fclose(out);
                LogError("ContentPak: failed to encode '%s'", src.mSourcePath.c_str());
                return false;
            }
            payload = encoded;
            payloadSize = encodedSize;
        }

        const bool wrote = (payloadSize == 0) || (fwrite(payload, payloadSize, 1, out) == 1);
        free(encoded);

        if (!wrote)
        {
            fclose(out);
            LogError("ContentPak: failed writing entry '%s'", src.mKey.c_str());
            return false;
        }

        PakRecord rec;
        rec.mPathHash = HashKey(src.mKey);
        rec.mDataOffset = cursor;
        rec.mDataSize = payloadSize;
        rec.mPathOffset = (uint32_t)pathBlob.size();
        rec.mPathLength = (uint32_t)src.mKey.size();
        records.push_back(rec);

        pathBlob.insert(pathBlob.end(), src.mKey.begin(), src.mKey.end());
        cursor += payloadSize;
    }

    // Sorted by hash so the runtime can binary search.
    std::sort(records.begin(), records.end(),
              [](const PakRecord& a, const PakRecord& b) { return a.mPathHash < b.mPathHash; });

    std::vector<char> index(records.size() * kRecordSize + pathBlob.size());
    for (size_t i = 0; i < records.size(); ++i)
    {
        uint8_t* rec = (uint8_t*)index.data() + (i * kRecordSize);
        WrLE64(rec, records[i].mPathHash);
        WrLE32(rec + 8, records[i].mDataOffset);
        WrLE32(rec + 12, records[i].mDataSize);
        WrLE32(rec + 16, records[i].mPathOffset);
        WrLE32(rec + 20, records[i].mPathLength);
    }
    if (!pathBlob.empty())
    {
        memcpy(index.data() + (records.size() * kRecordSize), pathBlob.data(), pathBlob.size());
    }

    // Obfuscating the index is what keeps the shipped pak from listing every
    // asset path in the clear.
    char* encodedIndex = nullptr;
    uint32_t encodedIndexSize = 0;
    if (!ContentObfuscation::Encode(index.data(), (uint32_t)index.size(), &encodedIndex, &encodedIndexSize))
    {
        fclose(out);
        LogError("ContentPak: failed to encode index");
        return false;
    }

    const uint32_t indexOffset = cursor;
    const bool indexWritten = (fwrite(encodedIndex, encodedIndexSize, 1, out) == 1);
    free(encodedIndex);

    if (!indexWritten)
    {
        fclose(out);
        LogError("ContentPak: failed writing index");
        return false;
    }

    memcpy(header, kPakMagic, sizeof(kPakMagic));
    WrLE32(header + 8, kPakVersion);
    WrLE32(header + 12, (uint32_t)records.size());
    WrLE32(header + 16, indexOffset);
    WrLE32(header + 20, encodedIndexSize);
    WrLE32(header + 24, Fnv1a32(header, 24));

    if (fseek(out, 0, SEEK_SET) != 0 ||
        fwrite(header, 1, kHeaderSize, out) != kHeaderSize)
    {
        fclose(out);
        LogError("ContentPak: failed rewriting header");
        return false;
    }

    fclose(out);

    outEntryCount = (uint32_t)records.size();
    return true;
}
