#pragma once

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Shared value types for the WebSocket client. Header-only on purpose: the web
// build-target addons supply their own WebSocket:: implementation and include
// this header, so nothing here may depend on an engine TU.
// ---------------------------------------------------------------------------

// Ready state. Values match RFC 6455 / the browser WebSocket.readyState enum
// and the WebSocketState table exposed to Lua.
enum class WsState : uint8_t
{
    Connecting = 0,
    Open       = 1,
    Closing    = 2,
    Closed     = 3
};

enum class WsError : uint8_t
{
    None = 0,
    BadUrl,
    NoTransport,        // wss:// on a TLS-less build, or no NET_ stack at all
    Dns,
    Connect,
    Handshake,
    Protocol,
    TooLarge,
    Transport           // socket died mid-stream
};

// Close codes we originate. Peer-supplied codes pass through untouched.
#define WS_CLOSE_NORMAL         1000
#define WS_CLOSE_GOING_AWAY     1001
#define WS_CLOSE_PROTOCOL_ERROR 1002
#define WS_CLOSE_TOO_LARGE      1009
#define WS_CLOSE_ABNORMAL       1006    // never sent on the wire; synthesized locally

// Per-direction queue cap and the largest single message we will reassemble.
#define WS_DEFAULT_MAX_QUEUED_BYTES (1u * 1024u * 1024u)
#define WS_MAX_MESSAGE_BYTES        (16u * 1024u * 1024u)

// Read chunk pulled off the socket per Tick pass.
#define WS_RECV_CHUNK_SIZE 8192

struct WsUrl
{
    std::string mScheme;        // "ws" or "wss"
    std::string mHost;
    std::string mResource;      // path + query, always starts with '/'
    uint16_t    mPort   = 0;
    bool        mSecure = false;
};

struct WsConnectOptions
{
    std::string                                       mUrl;
    std::vector<std::string>                          mProtocols;
    std::vector<std::pair<std::string, std::string> > mHeaders;
    uint32_t                                          mMaxQueuedBytes = WS_DEFAULT_MAX_QUEUED_BYTES;
};

struct WsMessage
{
    std::vector<uint8_t> mData;
    bool                 mBinary = false;
};

inline const char* WsErrorToString(WsError err)
{
    switch (err)
    {
    case WsError::None:        return "None";
    case WsError::BadUrl:      return "BadUrl";
    case WsError::NoTransport: return "NoTransport";
    case WsError::Dns:         return "Dns";
    case WsError::Connect:     return "Connect";
    case WsError::Handshake:   return "Handshake";
    case WsError::Protocol:    return "Protocol";
    case WsError::TooLarge:    return "TooLarge";
    case WsError::Transport:   return "Transport";
    }
    return "Unknown";
}
