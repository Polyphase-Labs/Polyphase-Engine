#pragma once

#include "PolyphaseAPI.h"
#include "Network/WebSocketTypes.h"

// ---------------------------------------------------------------------------
// RFC 6455 wire format. Pure computation — no sockets, no engine singletons —
// so this is unit-testable and safe to share with any transport.
// ---------------------------------------------------------------------------

#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT         0x1
#define WS_OP_BINARY       0x2
#define WS_OP_CLOSE        0x8
#define WS_OP_PING         0x9
#define WS_OP_PONG         0xA

// Appends one complete, masked, unfragmented client frame to outBytes. RFC 6455
// 5.3 requires every client-to-server frame to be masked; the key is a fresh 4
// bytes per frame from a cheap PRNG (masking is not a security measure).
POLYPHASE_API void WsEncodeFrame(uint8_t opcode, const uint8_t* payload, uint32_t size, std::vector<uint8_t>& outBytes);

// Convenience for the Close handshake: builds the 2-byte code + UTF-8 reason
// payload and frames it.
POLYPHASE_API void WsEncodeClose(uint16_t code, const char* reason, std::vector<uint8_t>& outBytes);

// Incremental frame decoder. Feed() whatever the socket handed over, then pump
// Next() until it returns false.
class POLYPHASE_API WsDecoder
{
public:

    struct Event
    {
        enum Type
        {
            None = 0,
            Message,
            Ping,
            Pong,
            Close
        };

        Type                 mType   = None;
        std::vector<uint8_t> mData;
        bool                 mBinary = false;
        uint16_t             mCloseCode = 0;
        std::string          mCloseReason;
    };

    void Feed(const uint8_t* data, uint32_t size);
    bool Next(Event& outEvent);

    void SetMaxMessageBytes(uint32_t bytes) { mMaxMessageBytes = bytes; }

    bool               HasError() const { return mError != WsError::None; }
    WsError            GetError() const { return mError; }
    const std::string& GetErrorMessage() const { return mErrorMessage; }

private:

    void Fail(WsError err, const char* message);

    std::vector<uint8_t> mBuffer;
    size_t               mReadPos = 0;

    // Fragmentation reassembly.
    std::vector<uint8_t> mFragment;
    uint8_t              mFragmentOpcode = 0;
    bool                 mFragmenting    = false;

    uint32_t             mMaxMessageBytes = WS_MAX_MESSAGE_BYTES;
    WsError              mError           = WsError::None;
    std::string          mErrorMessage;
};

// SHA-1 and base64, used by the opening handshake. Implemented here so the
// subsystem carries no crypto dependency onto console toolchains.
POLYPHASE_API void        WsSha1(const uint8_t* data, uint32_t size, uint8_t outDigest[20]);
POLYPHASE_API std::string WsBase64Encode(const uint8_t* data, uint32_t size);

// xorshift32 PRNG. Used for mask keys and the handshake nonce only.
POLYPHASE_API uint32_t    WsRandom32();
