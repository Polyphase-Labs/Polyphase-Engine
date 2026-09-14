#include "ContentPak.h"
#include "ContentObfuscation.h"
#include "Log.h"
#include "System/System.h"

#if !PLATFORM_MAC
#include <malloc.h>
#include <stdint.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

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

    FILE* sPakFile = nullptr;
    MutexObject* sPakMutex = nullptr;
    std::string sPakPath;
    std::vector<PakRecord> sRecords;
    std::vector<char> sPathBlob;

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

    // Binary search by hash, then confirm against the stored path. Equal hashes
    // are scanned linearly so a collision falls through to a miss instead of
    // returning the wrong entry.
    const PakRecord* Find(const char* path)
    {
        if (sRecords.empty() || path == nullptr) return nullptr;

        const std::string key = Canonicalise(path);
        const uint64_t hash = HashKey(key);

        size_t lo = 0;
        size_t hi = sRecords.size();
        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (sRecords[mid].mPathHash < hash) lo = mid + 1;
            else hi = mid;
        }

        for (size_t i = lo; i < sRecords.size() && sRecords[i].mPathHash == hash; ++i)
        {
            const PakRecord& rec = sRecords[i];
            if (rec.mPathLength == key.size() &&
                rec.mPathOffset + rec.mPathLength <= sPathBlob.size() &&
                memcmp(&sPathBlob[rec.mPathOffset], key.c_str(), key.size()) == 0)
            {
                return &rec;
            }
        }

        return nullptr;
    }
}

// Every byte that comes out of the pak goes through this. What it has to get
// right was established on real PS2 hardware, one photo of the boot tty at a
// time (2026-09-13), because PCSX2 reproduces none of it:
//   * every fopen/fseek/fread is a SIF RPC into one shared client buffer, and
//     the audio mixer thread is in that buffer every couple of milliseconds --
//     hence PAK_IO_LOCK around each request, exactly as SystemUtils' streaming
//     handle already did. Without it the index and assets came back scrambled;
//   * requests are kept sector-aligned and whole-sector-sized so the loader's
//     cdvdman (OPL) serves them in one piece rather than three;
//   * the destination is ordinary cached memory. Handing the IOP an "uncached"
//     0x2000_0000 alias looked clever and broke every read on hardware: the
//     DMA never lands there. ps2sdk's own write-back/invalidate is sufficient.
// The extra memcpy costs ~1 ms per MB; the scratch is a constant 32 KB; peak
// memory for a multi-MB asset does not double.
#if PLATFORM_PS2
extern void Ps2_SifLock();
extern void Ps2_SifUnlock();
extern void Ps2_SyncDCacheRange(void* p, size_t n);
extern bool Ps2_DiscReadRange(const char* path, uint32_t offset, uint32_t bytes, void* dst, uint32_t* got);
  // Disc boots read the pak with sceCdRead (see Ps2_DiscReadRange); returns
  // false when not applicable and the fread path below runs instead.
  #define PAK_DIRECT_READ(path, off, n, dst, got) Ps2_DiscReadRange((path), (off), (n), (dst), (got))
  #define PAK_IO_LOCK()        Ps2_SifLock()
  #define PAK_IO_UNLOCK()      Ps2_SifUnlock()
  // The read is a DMA into a buffer the CPU last touched moments ago; make the
  // data cache agree with RAM before copying out of it. See Ps2_SyncDCacheRange.
  #define PAK_IO_SYNC(p, n)    Ps2_SyncDCacheRange((p), (n))
#else
  #define PAK_DIRECT_READ(path, off, n, dst, got) (false)
  #define PAK_IO_LOCK()        ((void)0)
  #define PAK_IO_UNLOCK()      ((void)0)
  #define PAK_IO_SYNC(p, n)    ((void)0)
#endif

static bool ReadThroughAlignedBounce(FILE* file, const char* pakPath, uint32_t offset, uint32_t size, char* dst)
{
    constexpr uint32_t kSector = 2048;
    constexpr uint32_t kAlign  = 64;
    // 32 KB slices: long enough to amortise the per-request cost of a disc
    // read, short enough that the audio mixer (which needs the same lock every
    // ~2 ms) is not starved for more than one slice.
    constexpr uint32_t kBounce = 32 * 1024;

    static char* sBounceRaw = nullptr;
    static char* sBounce    = nullptr;
    if (sBounce == nullptr)
    {
        sBounceRaw = (char*)malloc(kBounce + kAlign);
        if (sBounceRaw == nullptr) return false;
        sBounce = (char*)(((uintptr_t)sBounceRaw + (kAlign - 1)) & ~(uintptr_t)(kAlign - 1));
    }

    if (size == 0) return true;

    uint32_t pos = offset - (offset % kSector);
    const uint32_t end = offset + size;
    bool seeked = false;   // the fread path seeks lazily, only if the direct path declines

    while (pos < end)
    {
        uint32_t want = end - pos;
        if (want % kSector != 0) want += kSector - (want % kSector);   // whole sectors
        if (want > kBounce) want = kBounce;

        uint32_t got = 0;
        if (!PAK_DIRECT_READ(pakPath, pos, want, sBounce, &got))
        {
            PAK_IO_LOCK();
            if (!seeked)
            {
                seeked = (fseek(file, (long)pos, SEEK_SET) == 0);
            }
            if (seeked)
            {
                while (got < want)
                {
                    const size_t n = fread(sBounce + got, 1, (size_t)(want - got), file);
                    if (n == 0) break;
                    got += (uint32_t)n;
                }
            }
            PAK_IO_UNLOCK();
            if (!seeked) return false;
            PAK_IO_SYNC(sBounce, got);
        }
        if (got == 0) return false;

        const uint32_t chunkEnd  = pos + got;
        const uint32_t copyStart = (offset > pos) ? offset : pos;
        const uint32_t copyEnd   = (end < chunkEnd) ? end : chunkEnd;
        if (copyEnd > copyStart)
        {
            memcpy(dst + (copyStart - offset), sBounce + (copyStart - pos), copyEnd - copyStart);
        }

        pos = chunkEnd;
        if (got < want && pos < end) return false;   // hit EOF before the range was covered
    }
    return true;
}

bool ContentPak::Mount(const char* pakPath)
{
    Unmount();

    if (pakPath == nullptr) return false;

    // Try the raw path first (works where the CWD is already the content root),
    // then the platform-resolved one. 3DS needs "romfs:/" prepended and PS2 a
    // "host:" device prefix, which SYS_GetAbsolutePath supplies -- without this
    // the pak simply fails to open on those targets.
    std::string resolvedPath = pakPath;
    PAK_IO_LOCK();
    FILE* file = fopen(pakPath, "rb");
    PAK_IO_UNLOCK();

    if (file == nullptr)
    {
        resolvedPath = SYS_GetAbsolutePath(pakPath);
        PAK_IO_LOCK();
        file = fopen(resolvedPath.c_str(), "rb");
        PAK_IO_UNLOCK();
    }

    if (file == nullptr) return false;

    // The header goes through the same aligned, uncached path as everything
    // else: a plain fread would land it in newlib's recycled FILE buffer, which
    // on PS2 hardware came back as garbage ("not a valid pak") over SMB.
    uint8_t header[kHeaderSize] = { };
    if (!ReadThroughAlignedBounce(file, resolvedPath.c_str(), 0, kHeaderSize, (char*)header) ||
        memcmp(header, kPakMagic, sizeof(kPakMagic)) != 0 ||
        RdLE32(header + 8) != kPakVersion ||
        RdLE32(header + 24) != Fnv1a32(header, 24))
    {
        fclose(file);
        LogError("ContentPak: '%s' is not a valid pak", pakPath);
        return false;
    }

    const uint32_t entryCount = RdLE32(header + 12);
    const uint32_t indexOffset = RdLE32(header + 16);
    const uint32_t indexSize = RdLE32(header + 20);

    if (indexSize == 0)
    {
        fclose(file);
        LogError("ContentPak: '%s' index is unreadable", pakPath);
        return false;
    }

    // Read + decode, retried. DecodeInPlace verifies a checksum over the whole
    // index, so it doubles as an end-to-end check of the read itself; on the
    // one platform where reads have come back wrong (PS2 hardware, see
    // ReadThroughAlignedBounce) a second attempt is cheap and tells the log
    // whether the fault was transient or deterministic.
    std::vector<char> index(indexSize);
    uint32_t decodedSize = 0;
    bool indexOk = false;
    for (int attempt = 1; attempt <= 3 && !indexOk; ++attempt)
    {
        if (!ReadThroughAlignedBounce(file, resolvedPath.c_str(), indexOffset, indexSize, index.data()))
        {
            LogWarning("ContentPak: '%s' index read failed (attempt %d of 3)", pakPath, attempt);
            continue;
        }
        decodedSize = 0;
        indexOk = ContentObfuscation::DecodeInPlace(index.data(), indexSize, &decodedSize, nullptr);
        if (!indexOk)
        {
            LogWarning("ContentPak: '%s' index decode failed (attempt %d of 3) -- re-reading", pakPath, attempt);
        }
    }
    if (!indexOk)
    {
        fclose(file);
        LogError("ContentPak: '%s' index failed to decode", pakPath);
        return false;
    }

    const uint32_t recordBytes = entryCount * kRecordSize;
    if (decodedSize < recordBytes)
    {
        fclose(file);
        LogError("ContentPak: '%s' index is truncated", pakPath);
        return false;
    }

    sRecords.resize(entryCount);
    for (uint32_t i = 0; i < entryCount; ++i)
    {
        const uint8_t* rec = (const uint8_t*)index.data() + (i * kRecordSize);
        sRecords[i].mPathHash = RdLE64(rec);
        sRecords[i].mDataOffset = RdLE32(rec + 8);
        sRecords[i].mDataSize = RdLE32(rec + 12);
        sRecords[i].mPathOffset = RdLE32(rec + 16);
        sRecords[i].mPathLength = RdLE32(rec + 20);
    }

    sPathBlob.assign(index.begin() + recordBytes, index.begin() + decodedSize);

    sPakFile = file;
    // Store what actually opened, so streaming handles reopen the same file.
    sPakPath = resolvedPath;
    if (sPakMutex == nullptr)
    {
        sPakMutex = SYS_CreateMutex();
    }

    LogDebug("ContentPak: mounted '%s' (%u entries)", pakPath, entryCount);
    return true;
}

void ContentPak::Unmount()
{
    if (sPakFile != nullptr)
    {
        fclose(sPakFile);
        sPakFile = nullptr;
    }

    sPakPath.clear();
    sRecords.clear();
    sPathBlob.clear();
}

bool ContentPak::IsMounted()
{
    return sPakFile != nullptr;
}

bool ContentPak::Exists(const char* path)
{
    return IsMounted() && Find(path) != nullptr;
}

const char* ContentPak::GetPakPath()
{
    return sPakPath.c_str();
}

void ContentPak::List(const char* prefix, std::vector<std::string>& outKeys)
{
    outKeys.clear();

    if (!IsMounted() || prefix == nullptr) return;

    const std::string want = Canonicalise(prefix);

    // Records are ordered by hash, not by path, so this is a linear scan. It runs
    // once per prefix at startup over a few hundred entries -- not worth a second
    // index.
    for (size_t i = 0; i < sRecords.size(); ++i)
    {
        const PakRecord& rec = sRecords[i];

        if (rec.mPathLength < want.size() ||
            rec.mPathOffset + rec.mPathLength > sPathBlob.size())
        {
            continue;
        }

        if (memcmp(&sPathBlob[rec.mPathOffset], want.c_str(), want.size()) == 0)
        {
            outKeys.push_back(std::string(&sPathBlob[rec.mPathOffset], rec.mPathLength));
        }
    }
}

bool ContentPak::FindEntry(const char* path, uint32_t& outDataOffset, uint32_t& outDataSize)
{
    outDataOffset = 0;
    outDataSize = 0;

    if (!IsMounted()) return false;

    const PakRecord* rec = Find(path);
    if (rec == nullptr) return false;

    outDataOffset = rec->mDataOffset;
    outDataSize = rec->mDataSize;
    return true;
}

bool ContentPak::Read(const char* path, int32_t maxSize, char*& outData, uint32_t& outSize)
{
    outData = nullptr;
    outSize = 0;

    if (!IsMounted()) return false;

    const PakRecord* rec = Find(path);
    if (rec == nullptr) return false;

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

    // The async asset loader shares this handle with the main thread, so the
    // seek and read have to be atomic with respect to each other.
    {
        ScopedLock lock(sPakMutex);

        // Sector-aligned, cache-line-aligned, short-read-safe. See
        // ReadThroughAlignedBounce for why a plain fseek+fread is not enough.
        if (!ReadThroughAlignedBounce(sPakFile, sPakPath.c_str(), rec->mDataOffset, readSize, buffer))
        {
            free(buffer);
            LogError("ContentPak: read of %u bytes at %u failed for '%s'",
                     (unsigned)readSize, (unsigned)rec->mDataOffset, path);
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
