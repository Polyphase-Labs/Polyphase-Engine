#include "Network/WebSocketHandshake.h"

#if !defined(POLYPHASE_PLATFORM_ADDON)

#include "Network/WebSocketFraming.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    // RFC 6455 4.2.2 magic. The server appends this to our Sec-WebSocket-Key,
    // SHA-1s it and base64s the digest.
    const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    char LowerAscii(char c)
    {
        return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }

    bool EqualsIgnoreCase(const std::string& a, const char* b)
    {
        size_t i = 0;
        for (; i < a.size(); ++i)
        {
            if (b[i] == '\0') return false;
            if (LowerAscii(a[i]) != LowerAscii(b[i])) return false;
        }
        return b[i] == '\0';
    }

    std::string Trim(const std::string& s)
    {
        size_t b = 0;
        size_t e = s.size();
        while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
        while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
        return s.substr(b, e - b);
    }
}

bool WsParseUrl(const char* url, WsUrl& outUrl, std::string& outError)
{
    outUrl = WsUrl();

    if (url == nullptr || *url == '\0')
    {
        outError = "Empty URL";
        return false;
    }

    std::string s(url);

    const size_t schemeEnd = s.find("://");
    if (schemeEnd == std::string::npos)
    {
        outError = "URL is missing a ws:// or wss:// scheme";
        return false;
    }

    std::string scheme = s.substr(0, schemeEnd);
    for (size_t i = 0; i < scheme.size(); ++i)
    {
        scheme[i] = LowerAscii(scheme[i]);
    }

    if (scheme != "ws" && scheme != "wss")
    {
        outError = "Unsupported scheme '" + scheme + "' (expected ws or wss)";
        return false;
    }

    outUrl.mScheme = scheme;
    outUrl.mSecure = (scheme == "wss");

    std::string rest = s.substr(schemeEnd + 3);

    // Strip any fragment - it is never sent to the server.
    const size_t hashPos = rest.find('#');
    if (hashPos != std::string::npos)
    {
        rest = rest.substr(0, hashPos);
    }

    size_t authorityEnd = rest.find('/');
    if (authorityEnd == std::string::npos)
    {
        const size_t qPos = rest.find('?');
        authorityEnd = (qPos == std::string::npos) ? rest.size() : qPos;
        outUrl.mResource = (qPos == std::string::npos) ? "/" : ("/" + rest.substr(qPos));
    }
    else
    {
        outUrl.mResource = rest.substr(authorityEnd);
    }

    std::string authority = rest.substr(0, authorityEnd);

    // Reject userinfo outright rather than silently sending it as a hostname.
    if (authority.find('@') != std::string::npos)
    {
        outError = "user:password in a WebSocket URL is not supported";
        return false;
    }

    if (!authority.empty() && authority[0] == '[')
    {
        outError = "IPv6 literals are not supported";
        return false;
    }

    const size_t colonPos = authority.find(':');
    if (colonPos != std::string::npos)
    {
        outUrl.mHost = authority.substr(0, colonPos);
        const std::string portStr = authority.substr(colonPos + 1);

        long port = 0;
        for (size_t i = 0; i < portStr.size(); ++i)
        {
            if (portStr[i] < '0' || portStr[i] > '9')
            {
                outError = "Port is not numeric";
                return false;
            }
            port = port * 10 + (portStr[i] - '0');
            if (port > 65535)
            {
                outError = "Port is out of range";
                return false;
            }
        }

        if (port == 0)
        {
            outError = "Port is out of range";
            return false;
        }

        outUrl.mPort = uint16_t(port);
    }
    else
    {
        outUrl.mHost = authority;
        outUrl.mPort = outUrl.mSecure ? 443 : 80;
    }

    if (outUrl.mHost.empty())
    {
        outError = "URL has no host";
        return false;
    }

    if (outUrl.mResource.empty())
    {
        outUrl.mResource = "/";
    }

    return true;
}

std::string WsBuildHandshakeRequest(
    const WsUrl& url,
    const std::vector<std::string>& protocols,
    const std::vector<std::pair<std::string, std::string> >& headers,
    std::string& outExpectedAccept)
{
    uint8_t nonce[16];
    for (int i = 0; i < 16; i += 4)
    {
        const uint32_t r = WsRandom32();
        nonce[i + 0] = uint8_t((r >> 24) & 0xFF);
        nonce[i + 1] = uint8_t((r >> 16) & 0xFF);
        nonce[i + 2] = uint8_t((r >> 8) & 0xFF);
        nonce[i + 3] = uint8_t(r & 0xFF);
    }

    const std::string key = WsBase64Encode(nonce, 16);

    const std::string acceptSource = key + kWsGuid;
    uint8_t digest[20];
    WsSha1((const uint8_t*)acceptSource.c_str(), (uint32_t)acceptSource.size(), digest);
    outExpectedAccept = WsBase64Encode(digest, 20);

    std::string req;
    req.reserve(256);

    req += "GET ";
    req += url.mResource;
    req += " HTTP/1.1\r\n";

    req += "Host: ";
    req += url.mHost;
    if ((url.mSecure && url.mPort != 443) || (!url.mSecure && url.mPort != 80))
    {
        char portBuf[16];
        snprintf(portBuf, sizeof(portBuf), ":%u", (unsigned)url.mPort);
        req += portBuf;
    }
    req += "\r\n";

    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: ";
    req += key;
    req += "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";

    if (!protocols.empty())
    {
        req += "Sec-WebSocket-Protocol: ";
        for (size_t i = 0; i < protocols.size(); ++i)
        {
            if (i != 0) req += ", ";
            req += protocols[i];
        }
        req += "\r\n";
    }

    for (size_t i = 0; i < headers.size(); ++i)
    {
        // Header injection guard: a caller-supplied CR/LF would let a game
        // script forge extra headers or a whole second request.
        if (headers[i].first.find_first_of("\r\n") != std::string::npos ||
            headers[i].second.find_first_of("\r\n") != std::string::npos)
        {
            continue;
        }

        req += headers[i].first;
        req += ": ";
        req += headers[i].second;
        req += "\r\n";
    }

    req += "\r\n";
    return req;
}

WsHandshakeResult WsParseHandshakeResponse(
    const uint8_t* data,
    uint32_t size,
    const std::string& expectedAccept,
    uint32_t& outConsumed,
    std::string& outSelectedProtocol,
    std::string& outError)
{
    outConsumed = 0;
    outSelectedProtocol.clear();

    if (data == nullptr || size == 0)
    {
        return WsHandshakeResult::NeedMoreData;
    }

    // Find the end of the header block.
    size_t headerEnd = std::string::npos;
    for (uint32_t i = 0; i + 3 < size; ++i)
    {
        if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n')
        {
            headerEnd = i + 4;
            break;
        }
    }

    if (headerEnd == std::string::npos)
    {
        // A server that never terminates its headers must not be allowed to
        // grow our buffer forever.
        if (size > 32768)
        {
            outError = "Handshake response headers exceeded 32 KiB";
            return WsHandshakeResult::Failed;
        }
        return WsHandshakeResult::NeedMoreData;
    }

    const std::string header((const char*)data, headerEnd);

    size_t lineStart = 0;
    size_t lineEnd = header.find("\r\n");
    if (lineEnd == std::string::npos)
    {
        outError = "Malformed handshake response";
        return WsHandshakeResult::Failed;
    }

    const std::string statusLine = header.substr(0, lineEnd);
    if (statusLine.size() < 12 || statusLine.compare(0, 5, "HTTP/") != 0)
    {
        outError = "Handshake response is not HTTP";
        return WsHandshakeResult::Failed;
    }

    const int statusCode = atoi(statusLine.c_str() + 9);
    if (statusCode != 101)
    {
        outError = "Server refused the upgrade: " + statusLine;
        return WsHandshakeResult::Failed;
    }

    bool sawUpgrade    = false;
    bool sawConnection = false;
    bool sawAccept     = false;

    lineStart = lineEnd + 2;
    while (lineStart < header.size())
    {
        lineEnd = header.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd == lineStart)
        {
            break;
        }

        const std::string line = header.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 2;

        const size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        const std::string name  = Trim(line.substr(0, colon));
        const std::string value = Trim(line.substr(colon + 1));

        if (EqualsIgnoreCase(name, "upgrade"))
        {
            sawUpgrade = EqualsIgnoreCase(value, "websocket");
        }
        else if (EqualsIgnoreCase(name, "connection"))
        {
            // Value can be a comma-separated list; look for the token.
            std::string lowered = value;
            for (size_t i = 0; i < lowered.size(); ++i) lowered[i] = LowerAscii(lowered[i]);
            sawConnection = lowered.find("upgrade") != std::string::npos;
        }
        else if (EqualsIgnoreCase(name, "sec-websocket-accept"))
        {
            sawAccept = (value == expectedAccept);
            if (!sawAccept)
            {
                outError = "Sec-WebSocket-Accept mismatch";
                return WsHandshakeResult::Failed;
            }
        }
        else if (EqualsIgnoreCase(name, "sec-websocket-protocol"))
        {
            outSelectedProtocol = value;
        }
        else if (EqualsIgnoreCase(name, "sec-websocket-extensions") && !value.empty())
        {
            // We never offer extensions, so the server must not select one.
            outError = "Server selected an unrequested extension: " + value;
            return WsHandshakeResult::Failed;
        }
    }

    if (!sawUpgrade || !sawConnection)
    {
        outError = "Handshake response is missing the Upgrade/Connection headers";
        return WsHandshakeResult::Failed;
    }

    if (!sawAccept)
    {
        outError = "Handshake response is missing Sec-WebSocket-Accept";
        return WsHandshakeResult::Failed;
    }

    outConsumed = (uint32_t)headerEnd;
    return WsHandshakeResult::Ok;
}

#endif  // !POLYPHASE_PLATFORM_ADDON
