#include "Network/WebSocketFraming.h"

// The web build-target addons do their framing in the browser, and the stub
// arm has no transport at all - neither needs this TU. See WebSocketClient.cpp
// for the full seam rationale.
#if !defined(POLYPHASE_PLATFORM_ADDON)

#include <string.h>
#include <time.h>

namespace
{
    uint32_t sRandomState = 0;

    void EnsureRandomSeeded()
    {
        if (sRandomState != 0)
        {
            return;
        }

        // Mask keys are not security-relevant (RFC 6455 5.3 masks to defeat
        // cache poisoning by intermediaries, not to hide anything), so a clock
        // plus one stack address is plenty of variety.
        uintptr_t stackBits = (uintptr_t)&sRandomState;
        sRandomState = (uint32_t)time(nullptr) ^ (uint32_t)stackBits ^ 0x9E3779B9u;

        if (sRandomState == 0)
        {
            sRandomState = 0x1234567u;
        }
    }
}

uint32_t WsRandom32()
{
    EnsureRandomSeeded();

    uint32_t x = sRandomState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sRandomState = x;
    return x;
}

void WsEncodeFrame(uint8_t opcode, const uint8_t* payload, uint32_t size, std::vector<uint8_t>& outBytes)
{
    outBytes.push_back(uint8_t(0x80 | (opcode & 0x0F)));    // FIN | opcode

    if (size < 126)
    {
        outBytes.push_back(uint8_t(0x80 | size));           // MASK | len
    }
    else if (size < 65536)
    {
        outBytes.push_back(uint8_t(0x80 | 126));
        outBytes.push_back(uint8_t((size >> 8) & 0xFF));
        outBytes.push_back(uint8_t(size & 0xFF));
    }
    else
    {
        outBytes.push_back(uint8_t(0x80 | 127));
        outBytes.push_back(0);
        outBytes.push_back(0);
        outBytes.push_back(0);
        outBytes.push_back(0);
        outBytes.push_back(uint8_t((size >> 24) & 0xFF));
        outBytes.push_back(uint8_t((size >> 16) & 0xFF));
        outBytes.push_back(uint8_t((size >> 8) & 0xFF));
        outBytes.push_back(uint8_t(size & 0xFF));
    }

    const uint32_t maskKey = WsRandom32();
    uint8_t mask[4];
    mask[0] = uint8_t((maskKey >> 24) & 0xFF);
    mask[1] = uint8_t((maskKey >> 16) & 0xFF);
    mask[2] = uint8_t((maskKey >> 8) & 0xFF);
    mask[3] = uint8_t(maskKey & 0xFF);

    outBytes.push_back(mask[0]);
    outBytes.push_back(mask[1]);
    outBytes.push_back(mask[2]);
    outBytes.push_back(mask[3]);

    const size_t payloadStart = outBytes.size();
    outBytes.resize(payloadStart + size);

    uint8_t* dst = outBytes.data() + payloadStart;
    for (uint32_t i = 0; i < size; ++i)
    {
        dst[i] = uint8_t(payload[i] ^ mask[i & 3]);
    }
}

void WsEncodeClose(uint16_t code, const char* reason, std::vector<uint8_t>& outBytes)
{
    std::vector<uint8_t> payload;

    if (code != 0)
    {
        payload.push_back(uint8_t((code >> 8) & 0xFF));
        payload.push_back(uint8_t(code & 0xFF));

        if (reason != nullptr)
        {
            // A Close payload is capped at 125 bytes total (it is a control
            // frame), so the reason gets 123.
            size_t reasonLen = strlen(reason);
            if (reasonLen > 123) reasonLen = 123;
            payload.insert(payload.end(), reason, reason + reasonLen);
        }
    }

    WsEncodeFrame(WS_OP_CLOSE, payload.empty() ? nullptr : payload.data(), (uint32_t)payload.size(), outBytes);
}

// -- Decoder ---------------------------------------------------------------

void WsDecoder::Feed(const uint8_t* data, uint32_t size)
{
    if (data == nullptr || size == 0)
    {
        return;
    }

    // Drop the already-consumed prefix before growing, so a long-lived
    // connection does not accumulate an ever-growing buffer.
    if (mReadPos > 0 && mReadPos == mBuffer.size())
    {
        mBuffer.clear();
        mReadPos = 0;
    }
    else if (mReadPos > 65536)
    {
        mBuffer.erase(mBuffer.begin(), mBuffer.begin() + mReadPos);
        mReadPos = 0;
    }

    mBuffer.insert(mBuffer.end(), data, data + size);
}

void WsDecoder::Fail(WsError err, const char* message)
{
    if (mError == WsError::None)
    {
        mError = err;
        mErrorMessage = message;
    }
}

bool WsDecoder::Next(Event& outEvent)
{
    outEvent.mType = Event::None;
    outEvent.mData.clear();
    outEvent.mCloseCode = 0;
    outEvent.mCloseReason.clear();
    outEvent.mBinary = false;

    while (mError == WsError::None)
    {
        const size_t avail = mBuffer.size() - mReadPos;
        if (avail < 2)
        {
            return false;
        }

        const uint8_t* base = mBuffer.data() + mReadPos;
        const uint8_t  b0   = base[0];
        const uint8_t  b1   = base[1];

        const bool    fin    = (b0 & 0x80) != 0;
        const uint8_t rsv    = uint8_t(b0 & 0x70);
        const uint8_t opcode = uint8_t(b0 & 0x0F);
        const bool    masked = (b1 & 0x80) != 0;

        if (rsv != 0)
        {
            Fail(WsError::Protocol, "Reserved frame bits set (no extensions negotiated)");
            return false;
        }

        uint64_t len = uint64_t(b1 & 0x7F);
        size_t   pos = 2;

        if (len == 126)
        {
            if (avail < pos + 2) return false;
            len = (uint64_t(base[pos]) << 8) | uint64_t(base[pos + 1]);
            pos += 2;
        }
        else if (len == 127)
        {
            if (avail < pos + 8) return false;
            len = 0;
            for (int i = 0; i < 8; ++i)
            {
                len = (len << 8) | uint64_t(base[pos + i]);
            }
            pos += 8;

            if ((len >> 63) != 0)
            {
                Fail(WsError::Protocol, "Frame length has the high bit set");
                return false;
            }
        }

        const bool isControl = (opcode & 0x08) != 0;

        if (isControl)
        {
            if (!fin)
            {
                Fail(WsError::Protocol, "Fragmented control frame");
                return false;
            }
            if (len > 125)
            {
                Fail(WsError::Protocol, "Control frame payload exceeds 125 bytes");
                return false;
            }
        }
        else if (len > uint64_t(mMaxMessageBytes))
        {
            Fail(WsError::TooLarge, "Incoming frame exceeds the maximum message size");
            return false;
        }

        const uint8_t* maskKey = nullptr;
        if (masked)
        {
            // Servers must not mask (RFC 6455 5.1), but unmasking anyway costs
            // nothing and keeps us interoperable with sloppy servers.
            if (avail < pos + 4) return false;
            maskKey = base + pos;
            pos += 4;
        }

        if (avail < pos + size_t(len))
        {
            return false;
        }

        std::vector<uint8_t> payload;
        payload.resize(size_t(len));
        if (len > 0)
        {
            memcpy(payload.data(), base + pos, size_t(len));
            if (maskKey != nullptr)
            {
                for (size_t i = 0; i < payload.size(); ++i)
                {
                    payload[i] = uint8_t(payload[i] ^ maskKey[i & 3]);
                }
            }
        }

        mReadPos += pos + size_t(len);

        if (opcode == WS_OP_PING)
        {
            outEvent.mType = Event::Ping;
            outEvent.mData.swap(payload);
            return true;
        }

        if (opcode == WS_OP_PONG)
        {
            outEvent.mType = Event::Pong;
            outEvent.mData.swap(payload);
            return true;
        }

        if (opcode == WS_OP_CLOSE)
        {
            outEvent.mType = Event::Close;
            if (payload.size() >= 2)
            {
                outEvent.mCloseCode = uint16_t((uint16_t(payload[0]) << 8) | uint16_t(payload[1]));
                outEvent.mCloseReason.assign((const char*)payload.data() + 2, payload.size() - 2);
            }
            else
            {
                outEvent.mCloseCode = WS_CLOSE_NORMAL;
            }
            return true;
        }

        if (opcode == WS_OP_CONTINUATION)
        {
            if (!mFragmenting)
            {
                Fail(WsError::Protocol, "Continuation frame with no message in progress");
                return false;
            }
        }
        else if (opcode == WS_OP_TEXT || opcode == WS_OP_BINARY)
        {
            if (mFragmenting)
            {
                Fail(WsError::Protocol, "New data frame arrived mid-fragmentation");
                return false;
            }
        }
        else
        {
            Fail(WsError::Protocol, "Unknown opcode");
            return false;
        }

        if (fin && !mFragmenting)
        {
            outEvent.mType   = Event::Message;
            outEvent.mBinary = (opcode == WS_OP_BINARY);
            outEvent.mData.swap(payload);
            return true;
        }

        if (!mFragmenting)
        {
            mFragmenting    = true;
            mFragmentOpcode = opcode;
            mFragment.clear();
        }

        if (mFragment.size() + payload.size() > size_t(mMaxMessageBytes))
        {
            Fail(WsError::TooLarge, "Reassembled message exceeds the maximum message size");
            return false;
        }

        mFragment.insert(mFragment.end(), payload.begin(), payload.end());

        if (fin)
        {
            outEvent.mType   = Event::Message;
            outEvent.mBinary = (mFragmentOpcode == WS_OP_BINARY);
            outEvent.mData.swap(mFragment);
            mFragment.clear();
            mFragmenting = false;
            return true;
        }
    }

    return false;
}

// -- SHA-1 / base64 --------------------------------------------------------

void WsSha1(const uint8_t* data, uint32_t size, uint8_t outDigest[20])
{
    uint32_t h[5];
    h[0] = 0x67452301u;
    h[1] = 0xEFCDAB89u;
    h[2] = 0x98BADCFEu;
    h[3] = 0x10325476u;
    h[4] = 0xC3D2E1F0u;

    // Message + 0x80 + zero padding to 56 mod 64 + 8-byte big-endian bit count.
    const uint64_t bitLen   = uint64_t(size) * 8u;
    const uint32_t totalLen = ((size + 8u) / 64u + 1u) * 64u;

    std::vector<uint8_t> msg;
    msg.resize(totalLen, 0);
    if (size > 0)
    {
        memcpy(msg.data(), data, size);
    }
    msg[size] = 0x80;
    for (int i = 0; i < 8; ++i)
    {
        msg[totalLen - 1 - i] = uint8_t((bitLen >> (8 * i)) & 0xFF);
    }

    uint32_t w[80];
    for (uint32_t chunk = 0; chunk < totalLen; chunk += 64)
    {
        const uint8_t* p = msg.data() + chunk;

        for (int i = 0; i < 16; ++i)
        {
            w[i] = (uint32_t(p[i * 4 + 0]) << 24)
                 | (uint32_t(p[i * 4 + 1]) << 16)
                 | (uint32_t(p[i * 4 + 2]) << 8)
                 |  uint32_t(p[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i)
        {
            const uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (v << 1) | (v >> 31);
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

        for (int i = 0; i < 80; ++i)
        {
            uint32_t f = 0;
            uint32_t k = 0;

            if (i < 20)      { f = (b & c) | ((~b) & d);        k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }

            const uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    for (int i = 0; i < 5; ++i)
    {
        outDigest[i * 4 + 0] = uint8_t((h[i] >> 24) & 0xFF);
        outDigest[i * 4 + 1] = uint8_t((h[i] >> 16) & 0xFF);
        outDigest[i * 4 + 2] = uint8_t((h[i] >> 8) & 0xFF);
        outDigest[i * 4 + 3] = uint8_t(h[i] & 0xFF);
    }
}

std::string WsBase64Encode(const uint8_t* data, uint32_t size)
{
    static const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((size + 2) / 3) * 4);

    uint32_t i = 0;
    while (i + 3 <= size)
    {
        const uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | uint32_t(data[i + 2]);
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        out.push_back(kAlphabet[v & 0x3F]);
        i += 3;
    }

    const uint32_t rem = size - i;
    if (rem == 1)
    {
        const uint32_t v = uint32_t(data[i]) << 16;
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    }
    else if (rem == 2)
    {
        const uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

#endif  // !POLYPHASE_PLATFORM_ADDON
