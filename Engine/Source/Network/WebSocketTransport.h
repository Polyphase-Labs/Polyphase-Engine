#pragma once

#include "Network/WebSocketTypes.h"

#if !defined(POLYPHASE_PLATFORM_ADDON)

// ---------------------------------------------------------------------------
// Internal transport interface. Two flavours:
//
//   byte transport     raw TCP via NET_. The engine does the RFC 6455
//                      handshake and framing on top (WebSocketHandshake /
//                      WebSocketFraming).
//   message transport  WinHTTP / libcurl. The platform owns the handshake and
//                      the framing; we only push and pull whole messages.
//
// Not exported and not included by any public header - WsConnection is the
// only thing gameplay code ever sees.
// ---------------------------------------------------------------------------
class WsTransport
{
public:

    virtual ~WsTransport() {}

    virtual bool IsMessageTransport() const = 0;

    // Advance IO. Must never block.
    virtual void Tick() = 0;

    // Connecting -> usable. For a message transport this means the handshake
    // has completed.
    virtual bool IsConnected() const = 0;

    virtual bool               HasFailed() const = 0;
    virtual WsError            GetError() const = 0;
    virtual const std::string& GetErrorMessage() const = 0;

    // True once the peer closed the stream and everything buffered has been
    // handed over.
    virtual bool IsEndOfStream() const = 0;

    virtual void Shutdown() = 0;

    // -- Byte transports ---------------------------------------------------
    // Queue bytes for sending. Returns false only on a dead transport.
    virtual bool SendBytes(const uint8_t* /*data*/, uint32_t /*size*/) { return false; }
    // Move everything received since the last call into outBytes.
    virtual bool TakeReceivedBytes(std::vector<uint8_t>& /*outBytes*/) { return false; }

    // Bytes accepted but not yet handed to the OS. Drives the outgoing-queue
    // cap; message transports report the size of their pending message queue.
    virtual uint32_t GetPendingSendBytes() const { return 0; }

    // -- Message transports ------------------------------------------------
    // NOT "SendMessage" - <windows.h> macro-expands that to SendMessageA/W.
    virtual bool SendWholeMessage(const uint8_t* /*data*/, uint32_t /*size*/, bool /*binary*/) { return false; }
    virtual bool TakeMessage(WsMessage& /*outMessage*/) { return false; }
    // Fills the close code/reason once the peer has closed. Returns false
    // while none has arrived.
    virtual bool TakeClose(uint16_t& /*outCode*/, std::string& /*outReason*/) { return false; }
    virtual void StartClose(uint16_t /*code*/, const char* /*reason*/) {}

    virtual const std::string& GetSelectedProtocol() const = 0;
};

// Secure (wss://) message transport. Implemented per platform in
// WebSocketWss_Windows.cpp / WebSocketWss_Linux.cpp; WebSocketClient.cpp
// carries the "no TLS here" fallback for every other target.
bool         WsIsSecureSupported();
const char*  WsSecureUnavailableMessage();
WsTransport* WsCreateSecureTransport(const WsUrl& url, const WsConnectOptions& options, std::string& outError);

#endif  // !POLYPHASE_PLATFORM_ADDON
