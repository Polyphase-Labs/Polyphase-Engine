// Sha256.cpp
//
// SHA-256 implementation, written from scratch following the NIST
// FIPS-180-4 specification:
//   https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
//
// No external code is imported, so there is no third-party license to
// satisfy. This file is original work belonging to the Polyphase Engine
// project. Test vectors verified against the FIPS examples (empty
// string, "abc") in the W2 smoke harness.

#include "Sha256.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace
{
    // First 32 bits of the fractional parts of the cube roots of the
    // first 64 primes (FIPS-180-4 section 4.2.2).
    const uint32_t kK[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    inline uint32_t Rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
    inline uint32_t Ch (uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    inline uint32_t BigSig0(uint32_t x) { return Rotr(x, 2)  ^ Rotr(x, 13) ^ Rotr(x, 22); }
    inline uint32_t BigSig1(uint32_t x) { return Rotr(x, 6)  ^ Rotr(x, 11) ^ Rotr(x, 25); }
    inline uint32_t SmallSig0(uint32_t x) { return Rotr(x, 7) ^ Rotr(x, 18) ^ (x >> 3);  }
    inline uint32_t SmallSig1(uint32_t x) { return Rotr(x, 17) ^ Rotr(x, 19) ^ (x >> 10); }

    struct Sha256State
    {
        uint32_t h[8];
        uint64_t bitLen = 0;     // total message length in bits
        uint8_t  buf[64];
        size_t   bufLen = 0;     // bytes currently held in buf

        Sha256State()
        {
            // Initial hash values: first 32 bits of the fractional parts
            // of the square roots of the first 8 primes (FIPS-180-4 5.3.3).
            h[0] = 0x6a09e667u; h[1] = 0xbb67ae85u;
            h[2] = 0x3c6ef372u; h[3] = 0xa54ff53au;
            h[4] = 0x510e527fu; h[5] = 0x9b05688cu;
            h[6] = 0x1f83d9abu; h[7] = 0x5be0cd19u;
        }

        void ProcessBlock(const uint8_t block[64])
        {
            uint32_t w[64];
            for (int t = 0; t < 16; ++t)
            {
                w[t] = (uint32_t(block[t * 4 + 0]) << 24)
                     | (uint32_t(block[t * 4 + 1]) << 16)
                     | (uint32_t(block[t * 4 + 2]) <<  8)
                     | (uint32_t(block[t * 4 + 3])      );
            }
            for (int t = 16; t < 64; ++t)
            {
                w[t] = SmallSig1(w[t-2]) + w[t-7] + SmallSig0(w[t-15]) + w[t-16];
            }

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

            for (int t = 0; t < 64; ++t)
            {
                uint32_t t1 = hh + BigSig1(e) + Ch(e, f, g) + kK[t] + w[t];
                uint32_t t2 = BigSig0(a) + Maj(a, b, c);
                hh = g; g = f; f = e;
                e  = d + t1;
                d  = c; c = b; b = a;
                a  = t1 + t2;
            }

            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        void Update(const uint8_t* data, size_t size)
        {
            bitLen += static_cast<uint64_t>(size) * 8u;
            // Fill any partial buffer first.
            if (bufLen > 0)
            {
                size_t take = 64 - bufLen;
                if (take > size) take = size;
                std::memcpy(buf + bufLen, data, take);
                bufLen += take;
                data   += take;
                size   -= take;
                if (bufLen == 64)
                {
                    ProcessBlock(buf);
                    bufLen = 0;
                }
            }
            // Direct block processing.
            while (size >= 64)
            {
                ProcessBlock(data);
                data += 64;
                size -= 64;
            }
            // Stash the tail.
            if (size > 0)
            {
                std::memcpy(buf, data, size);
                bufLen = size;
            }
        }

        void Final(uint8_t out[32])
        {
            // Append 0x80, then pad with zeros so that length mod 64 == 56,
            // then append 64-bit big-endian message length in bits.
            buf[bufLen++] = 0x80u;
            if (bufLen > 56)
            {
                while (bufLen < 64) buf[bufLen++] = 0u;
                ProcessBlock(buf);
                bufLen = 0;
            }
            while (bufLen < 56) buf[bufLen++] = 0u;

            for (int i = 7; i >= 0; --i)
            {
                buf[bufLen++] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFFu);
            }
            ProcessBlock(buf);

            for (int i = 0; i < 8; ++i)
            {
                out[i * 4 + 0] = static_cast<uint8_t>((h[i] >> 24) & 0xFFu);
                out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFFu);
                out[i * 4 + 2] = static_cast<uint8_t>((h[i] >>  8) & 0xFFu);
                out[i * 4 + 3] = static_cast<uint8_t>((h[i]      ) & 0xFFu);
            }
        }
    };

    std::string ToHex(const uint8_t digest[32])
    {
        static const char kHex[] = "0123456789abcdef";
        std::string s;
        s.resize(64);
        for (int i = 0; i < 32; ++i)
        {
            s[i * 2 + 0] = kHex[(digest[i] >> 4) & 0x0Fu];
            s[i * 2 + 1] = kHex[ digest[i]       & 0x0Fu];
        }
        return s;
    }
}

std::string Sha256::HashHex(const void* data, size_t size)
{
    Sha256State state;
    if (size > 0 && data != nullptr)
    {
        state.Update(static_cast<const uint8_t*>(data), size);
    }
    uint8_t digest[32];
    state.Final(digest);
    return ToHex(digest);
}

bool Sha256::HashFileHex(const std::string& path,
                         std::string& outHex,
                         std::string& outError)
{
    outHex.clear();
    outError.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        outError = "Sha256::HashFileHex: failed to open '" + path + "'";
        return false;
    }

    Sha256State state;
    constexpr size_t kChunk = 64 * 1024;
    // std::string scratch avoids needing <vector> in this TU.
    std::string scratch;
    scratch.resize(kChunk);

    while (in)
    {
        in.read(&scratch[0], static_cast<std::streamsize>(kChunk));
        std::streamsize got = in.gcount();
        if (got > 0)
        {
            state.Update(reinterpret_cast<const uint8_t*>(scratch.data()),
                         static_cast<size_t>(got));
        }
    }

    if (in.bad())
    {
        outError = "Sha256::HashFileHex: I/O error while reading '" + path + "'";
        return false;
    }

    uint8_t digest[32];
    state.Final(digest);
    outHex = ToHex(digest);
    return true;
}
