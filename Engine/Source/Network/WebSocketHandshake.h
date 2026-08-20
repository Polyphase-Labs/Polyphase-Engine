#pragma once

#include "PolyphaseAPI.h"
#include "Network/WebSocketTypes.h"

// ---------------------------------------------------------------------------
// RFC 6455 opening handshake: the HTTP/1.1 GET upgrade request and the 101
// response parse. Used by byte transports only - message transports (WinHTTP,
// libcurl, the browser) do the handshake themselves.
// ---------------------------------------------------------------------------

// Splits "ws://host:port/path?query". Returns false and fills outError when the
// scheme is not ws/wss or the host is empty. Port defaults to 80 / 443.
POLYPHASE_API bool WsParseUrl(const char* url, WsUrl& outUrl, std::string& outError);

// Builds the GET upgrade request. outExpectedAccept receives the
// Sec-WebSocket-Accept value the server must echo back.
POLYPHASE_API std::string WsBuildHandshakeRequest(
    const WsUrl& url,
    const std::vector<std::string>& protocols,
    const std::vector<std::pair<std::string, std::string> >& headers,
    std::string& outExpectedAccept);

enum class WsHandshakeResult : uint8_t
{
    NeedMoreData = 0,
    Ok,
    Failed
};

// Parses the server response out of an accumulating byte buffer. On Ok,
// outConsumed is the number of bytes the header occupied - everything after it
// is the first of the WebSocket frame stream.
POLYPHASE_API WsHandshakeResult WsParseHandshakeResponse(
    const uint8_t* data,
    uint32_t size,
    const std::string& expectedAccept,
    uint32_t& outConsumed,
    std::string& outSelectedProtocol,
    std::string& outError);
