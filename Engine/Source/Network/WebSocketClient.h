#pragma once

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include "PolyphaseAPI.h"
#include "Network/WebSocketTypes.h"

// ---------------------------------------------------------------------------
// WebSocket client. Parallel to Http:: - realtime bidirectional messaging with
// a dedicated server, which HTTP cannot do.
//
// Every callback fires on the main thread from WebSocket::Tick(), never from a
// socket poll or an OS callback thread. Same contract as Http::.
// ---------------------------------------------------------------------------

using WsOpenCallback    = std::function<void()>;
using WsMessageCallback = std::function<void(const uint8_t* data, uint32_t size, bool binary)>;
using WsErrorCallback   = std::function<void(const char* message)>;
using WsClosedCallback  = std::function<void(uint16_t code, const char* reason, bool wasClean)>;

// One connection. Abstract so the byte transport, the platform message
// transports and the web build-target addon can each supply their own concrete
// type behind the same handle.
class POLYPHASE_API WsConnection
{
public:

    virtual ~WsConnection() {}

    virtual WsState GetState() const = 0;

    // Return false when the connection is not Open or the outgoing queue is
    // full. Never raises, never invokes callbacks re-entrantly.
    virtual bool SendText(const char* data, uint32_t size) = 0;
    virtual bool SendBinary(const uint8_t* data, uint32_t size) = 0;

    // Begins the close handshake. Safe to call in any state.
    virtual void Close(uint16_t code, const char* reason) = 0;

    // Callbacks. Passing an empty std::function clears one. While a message
    // callback is set, incoming messages are delivered to it instead of being
    // queued for TakePacket().
    virtual void SetOpenCallback(WsOpenCallback cb) = 0;
    virtual void SetMessageCallback(WsMessageCallback cb) = 0;
    virtual void SetErrorCallback(WsErrorCallback cb) = 0;
    virtual void SetClosedCallback(WsClosedCallback cb) = 0;

    // Polling alternative to the message callback.
    virtual uint32_t GetAvailablePacketCount() const = 0;
    virtual bool     TakePacket(WsMessage& outMessage) = 0;

    virtual const std::string& GetSelectedProtocol() const = 0;
    virtual uint16_t           GetCloseCode() const = 0;
    virtual const std::string& GetCloseReason() const = 0;
    virtual bool               WasDowngraded() const = 0;
};

namespace WebSocket
{
    // Lifecycle. Initialize is called from Engine::Initialize right after
    // Http::Initialize.
    POLYPHASE_API void Initialize();
    POLYPHASE_API void Shutdown();

    // Pumps every live connection and fires the queued callbacks. Called once
    // per frame from Engine::Update - this is the only place Lua is touched.
    POLYPHASE_API void Tick();

    // False on platforms with no transport at all (Variant-2 addon targets
    // that do not supply their own implementation).
    POLYPHASE_API bool IsAvailable();

    // Why not, when IsAvailable() is false. Empty string when available.
    POLYPHASE_API const char* GetMissingDependencyMessage();

    // Returns null and fills outError when the URL is malformed or the scheme
    // has no transport on this build.
    POLYPHASE_API std::shared_ptr<WsConnection> Connect(const WsConnectOptions& options, std::string& outError);
}
