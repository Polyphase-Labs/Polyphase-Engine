#include "ContentObfuscation.h"
#include "Log.h"

#if !PLATFORM_MAC
#include <malloc.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Obfuscation pepper. Changing either constant invalidates every previously
// packaged Static build, so bump kVersion alongside if these ever change.
#define POLYPHASE_OBF_PEPPER_A 0x5F3A9C21u
#define POLYPHASE_OBF_PEPPER_B 0xB7E15163u

#define FNV_OFFSET_BASIS 0x811C9DC5u
#define FNV_PRIME        0x01000193u

static const char kMagic[6] = { 'P', 'L', 'Y', 'O', 'B', 'F' };

// Shift-based accessors. Never alias a uint32_t over the buffer -- the header
// must read identically on big-endian Dolphin and little-endian desktop.
static inline uint32_t RdLE32(const uint8_t* p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline void WrLE32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)(value);
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t Fnv1a32(const void* data, uint32_t size)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = FNV_OFFSET_BASIS;

    for (uint32_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

// One keystream word per 4-byte block, indexed by block rather than chained, so
// any byte range can be decoded without touching the bytes before it. Integer
// ops only -- no FPU, no tables, no state.
static inline uint32_t Keystream(uint32_t k0, uint32_t k1, uint32_t block)
{
    uint32_t h = block ^ k0;
    h ^= h >> 16; h *= 0x85EBCA6Bu;
    h ^= h >> 13; h *= 0xC2B2AE35u;
    h ^= h >> 16;
    h ^= k1;
    h ^= h >> 15; h *= 0x2545F491u;
    h ^= h >> 13;
    return h;
}

static inline void DeriveKeys(uint32_t salt, uint32_t& k0, uint32_t& k1)
{
    k0 = Keystream(POLYPHASE_OBF_PEPPER_A, salt, 0x9E3779B9u);
    k1 = Keystream(POLYPHASE_OBF_PEPPER_B, salt, 0x85EBCA77u);
}

static void XorRange(uint8_t* buffer, uint32_t count, uint32_t payloadOffset, uint32_t k0, uint32_t k1)
{
    uint32_t block = payloadOffset >> 2;
    uint32_t phase = payloadOffset & 3;
    uint32_t word = Keystream(k0, k1, block);

    for (uint32_t i = 0; i < count; ++i)
    {
        buffer[i] ^= (uint8_t)(word >> (8 * phase));

        if (++phase == 4)
        {
            phase = 0;
            word = Keystream(k0, k1, ++block);
        }
    }
}

bool ContentObfuscation::IsContainer(const void* data, uint32_t size)
{
    if (data == nullptr || size < kHeaderSize)
        return false;

    const uint8_t* header = (const uint8_t*)data;

    if (memcmp(header, kMagic, sizeof(kMagic)) != 0)
        return false;

    if (header[6] != kVersion)
        return false;

    // The header self-check is what makes a false positive effectively
    // impossible, so a .lua that happens to start with the magic bytes is still
    // read as plain text rather than being mangled.
    return RdLE32(header + 20) == Fnv1a32(header, 20);
}

uint32_t ContentObfuscation::GetDecodedSize(const void* data)
{
    return RdLE32((const uint8_t*)data + 8);
}

uint32_t ContentObfuscation::GetSalt(const void* data)
{
    return RdLE32((const uint8_t*)data + 12);
}

bool ContentObfuscation::DecodeInPlace(char* data, uint32_t available, uint32_t* outSize, bool* outTruncated)
{
    if (outSize != nullptr) *outSize = 0;
    if (outTruncated != nullptr) *outTruncated = false;

    if (!IsContainer(data, available))
        return false;

    const uint8_t* header = (const uint8_t*)data;
    const uint8_t flags = header[7];
    const uint32_t decodedSize = RdLE32(header + 8);
    const uint32_t salt = RdLE32(header + 12);
    const uint32_t checksum = RdLE32(header + 16);

    const uint32_t payloadAvailable = available - kHeaderSize;
    const bool truncated = (payloadAvailable < decodedSize);
    const uint32_t count = truncated ? payloadAvailable : decodedSize;

    uint32_t k0 = 0;
    uint32_t k1 = 0;
    DeriveKeys(salt, k0, k1);

    // Decode, checksum and shift-to-front in one forward pass. dst trails src by
    // kHeaderSize so the overlapping copy is safe, and we never allocate a second
    // buffer -- peak memory while loading a multi-MB texture must not double.
    uint8_t* dst = (uint8_t*)data;
    const uint8_t* src = (const uint8_t*)data + kHeaderSize;

    uint32_t block = 0;
    uint32_t phase = 0;
    uint32_t word = Keystream(k0, k1, 0);
    uint32_t hash = FNV_OFFSET_BASIS;

    for (uint32_t i = 0; i < count; ++i)
    {
        const uint8_t plain = (uint8_t)(src[i] ^ (uint8_t)(word >> (8 * phase)));
        dst[i] = plain;

        hash ^= plain;
        hash *= FNV_PRIME;

        if (++phase == 4)
        {
            phase = 0;
            word = Keystream(k0, k1, ++block);
        }
    }

    // The checksum covers the whole payload, so it can only be verified when the
    // whole payload was read. A capped read is intentional, not corruption.
    if (!truncated &&
        (flags & kFlagChecksum) != 0 &&
        hash != checksum)
    {
        LogError("ContentObfuscation: checksum mismatch -- content is corrupt or was built with a different engine key");
        return false;
    }

    if (outSize != nullptr) *outSize = count;
    if (outTruncated != nullptr) *outTruncated = truncated;
    return true;
}

void ContentObfuscation::DecodeRange(void* buffer, uint32_t count, uint32_t payloadOffset, uint32_t salt)
{
    if (buffer == nullptr || count == 0)
        return;

    uint32_t k0 = 0;
    uint32_t k1 = 0;
    DeriveKeys(salt, k0, k1);

    XorRange((uint8_t*)buffer, count, payloadOffset, k0, k1);
}

// Salt only has to be unique-ish per file, not cryptographically random: it
// diversifies the keystream so two identical files don't encode identically.
static uint32_t MakeSalt(uint32_t contentHash)
{
    static uint32_t sCounter = 0;
    ++sCounter;

    return Keystream((uint32_t)time(nullptr) ^ contentHash, sCounter, 0x1B873593u);
}

bool ContentObfuscation::Encode(const void* src, uint32_t srcSize, char** outData, uint32_t* outSize)
{
    if (outData == nullptr || outSize == nullptr)
        return false;

    *outData = nullptr;
    *outSize = 0;

    if (src == nullptr && srcSize > 0)
        return false;

    if (IsContainer(src, srcSize))
        return false;

    const uint32_t total = kHeaderSize + srcSize;
    char* buffer = (char*)malloc(total);

    if (buffer == nullptr)
    {
        LogError("ContentObfuscation: out of memory encoding %u bytes", srcSize);
        return false;
    }

    const uint32_t contentHash = Fnv1a32(src, srcSize);
    const uint32_t salt = MakeSalt(contentHash);

    uint8_t* header = (uint8_t*)buffer;
    memcpy(header, kMagic, sizeof(kMagic));
    header[6] = kVersion;
    header[7] = kFlagChecksum;
    WrLE32(header + 8, srcSize);
    WrLE32(header + 12, salt);
    WrLE32(header + 16, contentHash);
    WrLE32(header + 20, Fnv1a32(header, 20));

    if (srcSize > 0)
    {
        memcpy(buffer + kHeaderSize, src, srcSize);

        uint32_t k0 = 0;
        uint32_t k1 = 0;
        DeriveKeys(salt, k0, k1);
        XorRange((uint8_t*)buffer + kHeaderSize, srcSize, 0, k0, k1);
    }

    *outData = buffer;
    *outSize = total;
    return true;
}

bool ContentObfuscation::EncodeFileInPlace(const char* path)
{
    if (path == nullptr)
        return false;

    FILE* file = fopen(path, "rb");

    if (file == nullptr)
    {
        LogWarning("ContentObfuscation: could not open '%s' for obfuscation", path);
        return false;
    }

    fseek(file, 0, SEEK_END);
    const long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize < 0)
    {
        fclose(file);
        LogWarning("ContentObfuscation: could not size '%s'", path);
        return false;
    }

    char* source = (char*)malloc((size_t)fileSize > 0 ? (size_t)fileSize : 1);

    if (source == nullptr)
    {
        fclose(file);
        LogError("ContentObfuscation: out of memory reading '%s'", path);
        return false;
    }

    if (fileSize > 0 &&
        fread(source, (size_t)fileSize, 1, file) != 1)
    {
        fclose(file);
        free(source);
        LogWarning("ContentObfuscation: short read on '%s'", path);
        return false;
    }

    fclose(file);
    file = nullptr;

    // Already wrapped by an earlier sweep over the same staging dir. Re-wrapping
    // would still decode (the runtime would just peel one layer) but would grow
    // the file on every rebuild, so treat it as done.
    if (IsContainer(source, (uint32_t)fileSize))
    {
        free(source);
        return true;
    }

    char* encoded = nullptr;
    uint32_t encodedSize = 0;

    if (!Encode(source, (uint32_t)fileSize, &encoded, &encodedSize))
    {
        free(source);
        return false;
    }

    free(source);
    source = nullptr;

    file = fopen(path, "wb");

    if (file == nullptr)
    {
        free(encoded);
        LogWarning("ContentObfuscation: could not rewrite '%s'", path);
        return false;
    }

    const bool wrote = (fwrite(encoded, encodedSize, 1, file) == 1);
    fclose(file);
    free(encoded);

    if (!wrote)
    {
        LogWarning("ContentObfuscation: failed writing '%s'", path);
        return false;
    }

    return true;
}
