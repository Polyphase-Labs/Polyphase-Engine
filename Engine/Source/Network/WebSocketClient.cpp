#include "Network/WebSocketClient.h"

// Three arms, mirroring Network/Http/HttpClient.cpp - with one improvement.
// Http's middle arm keys on PLATFORM_WEB, which means a new engine paired with
// an older web package fails to link. This one keys on an opt-in define
// instead, so every Variant-2 build-target package that predates this feature
// keeps compiling untouched: it just falls through to the stub arm and reports
// WebSocket as unavailable. Only a package that actually ships a WebSocket
// implementation defines POLYPHASE_WS_PROVIDED_BY_ADDON.
#if !defined(POLYPHASE_PLATFORM_ADDON)

#include "Network/Network.h"
#include "Network/NetworkConstants.h"
#include "Network/WebSocketFraming.h"
#include "Network/WebSocketHandshake.h"
#include "Network/WebSocketTransport.h"
#include "Log.h"

#include <deque>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define WS_CONNECT_TIMEOUT_SECONDS 15
#define WS_CLOSE_GRACE_SECONDS     5

namespace
{
    // -- ws:// over a raw NET_ stream socket --------------------------------
    // A byte transport: the engine owns the handshake and the RFC 6455
    // framing on top of it.
    class WsByteTransport : public WsTransport
    {
    public:

        ~WsByteTransport() override
        {
            Shutdown();
        }

        bool Start(const WsUrl& url, std::string& outError)
        {
            const uint32_t ip = NET_ResolveHost(url.mHost.c_str());
            if (ip == 0)
            {
                outError = "Could not resolve host '" + url.mHost + "'";
                return false;
            }

            mSocket = NET_SocketCreateStream();
            if (mSocket == SocketHandle(NET_INVALID_SOCKET))
            {
                outError = "Could not create a stream socket";
                return false;
            }

            if (!NET_SocketConnectAsync(mSocket, ip, url.mPort))
            {
                NET_SocketClose(mSocket);
                mSocket = SocketHandle(NET_INVALID_SOCKET);

                // Report what we actually dialled. "Connect to 'localhost'
                // failed" reads like a name-resolution problem when the real
                // story is usually that loopback means this machine, which on a
                // console is the console and not the dev box running the server.
                char ipString[32] = {};
                NET_IpUint32ToString(ip, ipString);

                char detail[80];
                snprintf(detail, sizeof(detail), "' (%s:%u) failed", ipString, (unsigned)url.mPort);

                outError = "Connect to '" + url.mHost + detail;

                if ((ip >> 24) == 127)
                {
                    outError += ". A loopback address points at the machine running this build";
                }

                return false;
            }

            mConnectDeadline = time(nullptr) + WS_CONNECT_TIMEOUT_SECONDS;
            return true;
        }

        bool IsMessageTransport() const override { return false; }

        void Tick() override
        {
            if (mFailed || mSocket == SocketHandle(NET_INVALID_SOCKET))
            {
                return;
            }

            if (!mConnected)
            {
                const int32_t poll = NET_SocketConnectPoll(mSocket);
                if (poll < 0)
                {
                    Fail(WsError::Connect, "Connection refused or host unreachable");
                    return;
                }
                if (poll == 0)
                {
                    if (time(nullptr) >= mConnectDeadline)
                    {
                        Fail(WsError::Connect, "Connection timed out");
                    }
                    return;
                }

                mConnected = true;
            }

            FlushSend();
            Receive();
        }

        bool IsConnected() const override { return mConnected; }
        bool HasFailed() const override { return mFailed; }
        WsError GetError() const override { return mError; }
        const std::string& GetErrorMessage() const override { return mErrorMessage; }
        bool IsEndOfStream() const override { return mEndOfStream && mRecvBuffer.empty(); }

        bool SendBytes(const uint8_t* data, uint32_t size) override
        {
            if (mFailed || mSocket == SocketHandle(NET_INVALID_SOCKET))
            {
                return false;
            }

            mSendBuffer.insert(mSendBuffer.end(), data, data + size);

            if (mConnected)
            {
                FlushSend();
            }
            return true;
        }

        bool TakeReceivedBytes(std::vector<uint8_t>& outBytes) override
        {
            if (mRecvBuffer.empty())
            {
                return false;
            }

            outBytes.swap(mRecvBuffer);
            mRecvBuffer.clear();
            return true;
        }

        uint32_t GetPendingSendBytes() const override { return (uint32_t)mSendBuffer.size(); }

        const std::string& GetSelectedProtocol() const override { return mUnusedProtocol; }

        void Shutdown() override
        {
            if (mSocket != SocketHandle(NET_INVALID_SOCKET))
            {
                NET_SocketClose(mSocket);
                mSocket = SocketHandle(NET_INVALID_SOCKET);
            }
            mConnected = false;
        }

    private:

        void Fail(WsError err, const char* message)
        {
            if (!mFailed)
            {
                mFailed = true;
                mError = err;
                mErrorMessage = message;
            }
        }

        void FlushSend()
        {
            while (!mSendBuffer.empty())
            {
                const int32_t sent = NET_SocketSend(mSocket, (const char*)mSendBuffer.data(), (uint32_t)mSendBuffer.size());
                if (sent > 0)
                {
                    mSendBuffer.erase(mSendBuffer.begin(), mSendBuffer.begin() + sent);
                    continue;
                }

                if (!NET_SocketWouldBlock(mSocket, sent))
                {
                    Fail(WsError::Transport, "Socket send failed");
                }
                return;
            }
        }

        void Receive()
        {
            // Bounded so one enormous burst can't monopolise a frame; whatever
            // is left is picked up on the next Tick.
            for (int32_t pass = 0; pass < 64; ++pass)
            {
                char chunk[WS_RECV_CHUNK_SIZE];
                const int32_t received = NET_SocketRecv(mSocket, chunk, sizeof(chunk));

                if (received > 0)
                {
                    mRecvBuffer.insert(mRecvBuffer.end(), (const uint8_t*)chunk, (const uint8_t*)chunk + received);
                    continue;
                }

                if (received == 0)
                {
                    mEndOfStream = true;
                    return;
                }

                if (!NET_SocketWouldBlock(mSocket, received))
                {
                    // A reset arriving after a clean Close frame is normal, so
                    // treat it as end-of-stream rather than an error and let
                    // the connection decide which it was.
                    mEndOfStream = true;
                }
                return;
            }
        }

        SocketHandle         mSocket = SocketHandle(NET_INVALID_SOCKET);
        bool                 mConnected   = false;
        bool                 mFailed      = false;
        bool                 mEndOfStream = false;
        WsError              mError       = WsError::None;
        std::string          mErrorMessage;
        std::string          mUnusedProtocol;
        std::vector<uint8_t> mSendBuffer;
        std::vector<uint8_t> mRecvBuffer;
        time_t               mConnectDeadline = 0;
    };

    // -- Connection ---------------------------------------------------------

    class WsConnectionImpl : public WsConnection
    {
    public:

        ~WsConnectionImpl() override
        {
            if (mTransport != nullptr)
            {
                mTransport->Shutdown();
                delete mTransport;
                mTransport = nullptr;
            }
        }

        void Init(const WsUrl& url, const WsConnectOptions& options, WsTransport* transport, bool downgraded)
        {
            mUrl            = url;
            mProtocols      = options.mProtocols;
            mHeaders        = options.mHeaders;
            mMaxQueuedBytes = options.mMaxQueuedBytes != 0 ? options.mMaxQueuedBytes : WS_DEFAULT_MAX_QUEUED_BYTES;
            mTransport      = transport;
            mDowngraded     = downgraded;

            mDecoder.SetMaxMessageBytes(WS_MAX_MESSAGE_BYTES);
        }

        // -- WsConnection ---------------------------------------------------

        WsState GetState() const override { return mState; }

        bool SendText(const char* data, uint32_t size) override
        {
            return Send((const uint8_t*)data, size, false);
        }

        bool SendBinary(const uint8_t* data, uint32_t size) override
        {
            return Send(data, size, true);
        }

        void Close(uint16_t code, const char* reason) override
        {
            if (mState == WsState::Closed || mClosePending)
            {
                return;
            }

            const char* safeReason = reason != nullptr ? reason : "";

            if (mState == WsState::Connecting)
            {
                // Nothing has been negotiated yet - there is no close
                // handshake to run, so tear down and report it next Tick.
                mCloseCode   = code;
                mCloseReason = safeReason;
                mWasClean    = false;
                mClosePending = true;
                mState = WsState::Closing;
                return;
            }

            if (mTransport != nullptr && !mSentClose)
            {
                if (mTransport->IsMessageTransport())
                {
                    mTransport->StartClose(code, safeReason);
                }
                else
                {
                    std::vector<uint8_t> frame;
                    WsEncodeClose(code, safeReason, frame);
                    mTransport->SendBytes(frame.data(), (uint32_t)frame.size());
                }
                mSentClose = true;
            }

            if (mCloseCode == 0)
            {
                mCloseCode   = code;
                mCloseReason = safeReason;
            }

            mState = WsState::Closing;
            mCloseDeadline = time(nullptr) + WS_CLOSE_GRACE_SECONDS;
        }

        void SetOpenCallback(WsOpenCallback cb) override       { mOpenCallback    = std::move(cb); }
        void SetMessageCallback(WsMessageCallback cb) override  { mMessageCallback = std::move(cb); }
        void SetErrorCallback(WsErrorCallback cb) override      { mErrorCallback   = std::move(cb); }
        void SetClosedCallback(WsClosedCallback cb) override    { mClosedCallback  = std::move(cb); }

        uint32_t GetAvailablePacketCount() const override { return (uint32_t)mInQueue.size(); }

        bool TakePacket(WsMessage& outMessage) override
        {
            if (mInQueue.empty())
            {
                return false;
            }

            outMessage = std::move(mInQueue.front());
            mInQueue.pop_front();
            mInQueuedBytes -= (uint32_t)outMessage.mData.size();
            return true;
        }

        const std::string& GetSelectedProtocol() const override { return mSelectedProtocol; }
        uint16_t           GetCloseCode() const override        { return mCloseCode; }
        const std::string& GetCloseReason() const override      { return mCloseReason; }
        bool               WasDowngraded() const override       { return mDowngraded; }

        // -- Pumped by WebSocket::Tick --------------------------------------

        void Pump()
        {
            if (mState == WsState::Closed || mTransport == nullptr)
            {
                return;
            }

            if (mClosePending)
            {
                FinishClosed();
                return;
            }

            mTransport->Tick();

            if (mTransport->HasFailed())
            {
                FailWith(mTransport->GetErrorMessage().c_str(), WS_CLOSE_ABNORMAL);
                return;
            }

            if (mState == WsState::Connecting)
            {
                AdvanceHandshake();
                if (mState == WsState::Closed || mState == WsState::Connecting)
                {
                    return;
                }
            }

            if (mTransport->IsMessageTransport())
            {
                PumpMessageTransport();
            }
            else
            {
                PumpByteTransport();
            }

            if (mState == WsState::Closed)
            {
                return;
            }

            if (mTransport->IsEndOfStream())
            {
                if (mReceivedClose)
                {
                    FinishClosed();
                }
                else
                {
                    FailWith("Connection closed unexpectedly", WS_CLOSE_ABNORMAL);
                }
                return;
            }

            if (mState == WsState::Closing && mCloseDeadline != 0 && time(nullptr) >= mCloseDeadline)
            {
                // The peer never answered our Close frame. Drop it.
                mWasClean = false;
                FinishClosed();
            }
        }

    private:

        bool Send(const uint8_t* data, uint32_t size, bool binary)
        {
            if (mState != WsState::Open || mTransport == nullptr)
            {
                return false;
            }

            const uint32_t pending = mTransport->GetPendingSendBytes();
            if (uint64_t(pending) + uint64_t(size) > uint64_t(mMaxQueuedBytes))
            {
                if (!mLoggedSendOverflow)
                {
                    mLoggedSendOverflow = true;
                    LogWarning("WebSocket: outgoing queue is full (%u bytes queued, cap %u) - dropping sends on %s",
                        (unsigned)pending, (unsigned)mMaxQueuedBytes, mUrl.mHost.c_str());
                }
                return false;
            }

            if (mTransport->IsMessageTransport())
            {
                return mTransport->SendWholeMessage(data, size, binary);
            }

            std::vector<uint8_t> frame;
            WsEncodeFrame(binary ? WS_OP_BINARY : WS_OP_TEXT, data, size, frame);
            return mTransport->SendBytes(frame.data(), (uint32_t)frame.size());
        }

        void AdvanceHandshake()
        {
            if (!mTransport->IsConnected())
            {
                return;
            }

            if (mTransport->IsMessageTransport())
            {
                // WinHTTP / libcurl / the browser ran the handshake for us.
                EnterOpen(mTransport->GetSelectedProtocol());
                return;
            }

            if (!mSentHandshake)
            {
                const std::string request = WsBuildHandshakeRequest(mUrl, mProtocols, mHeaders, mExpectedAccept);
                mTransport->SendBytes((const uint8_t*)request.c_str(), (uint32_t)request.size());
                mSentHandshake = true;
            }

            std::vector<uint8_t> incoming;
            if (mTransport->TakeReceivedBytes(incoming))
            {
                mHandshakeBuffer.insert(mHandshakeBuffer.end(), incoming.begin(), incoming.end());
            }

            if (mHandshakeBuffer.empty())
            {
                return;
            }

            uint32_t    consumed = 0;
            std::string selectedProtocol;
            std::string error;
            const WsHandshakeResult result = WsParseHandshakeResponse(
                mHandshakeBuffer.data(), (uint32_t)mHandshakeBuffer.size(),
                mExpectedAccept, consumed, selectedProtocol, error);

            if (result == WsHandshakeResult::NeedMoreData)
            {
                return;
            }

            if (result == WsHandshakeResult::Failed)
            {
                FailWith(error.c_str(), WS_CLOSE_ABNORMAL);
                return;
            }

            // Anything past the header block is already frame data.
            if (consumed < mHandshakeBuffer.size())
            {
                mDecoder.Feed(mHandshakeBuffer.data() + consumed, (uint32_t)(mHandshakeBuffer.size() - consumed));
            }
            mHandshakeBuffer.clear();

            EnterOpen(selectedProtocol);
        }

        void EnterOpen(const std::string& selectedProtocol)
        {
            mSelectedProtocol = selectedProtocol;
            mState = WsState::Open;

            if (mOpenCallback)
            {
                mOpenCallback();
            }
        }

        void PumpByteTransport()
        {
            std::vector<uint8_t> incoming;
            if (mTransport->TakeReceivedBytes(incoming) && !incoming.empty())
            {
                mDecoder.Feed(incoming.data(), (uint32_t)incoming.size());
            }

            WsDecoder::Event event;
            while (mState != WsState::Closed && mDecoder.Next(event))
            {
                switch (event.mType)
                {
                case WsDecoder::Event::Message:
                    DeliverMessage(event.mData, event.mBinary);
                    break;

                case WsDecoder::Event::Ping:
                {
                    std::vector<uint8_t> pong;
                    WsEncodeFrame(WS_OP_PONG, event.mData.empty() ? nullptr : event.mData.data(),
                                  (uint32_t)event.mData.size(), pong);
                    mTransport->SendBytes(pong.data(), (uint32_t)pong.size());
                    break;
                }

                case WsDecoder::Event::Pong:
                    break;

                case WsDecoder::Event::Close:
                    HandlePeerClose(event.mCloseCode, event.mCloseReason);
                    break;

                default:
                    break;
                }
            }

            if (mDecoder.HasError())
            {
                if (!mSentClose && mTransport != nullptr)
                {
                    std::vector<uint8_t> frame;
                    const uint16_t code = (mDecoder.GetError() == WsError::TooLarge)
                        ? uint16_t(WS_CLOSE_TOO_LARGE) : uint16_t(WS_CLOSE_PROTOCOL_ERROR);
                    WsEncodeClose(code, "", frame);
                    mTransport->SendBytes(frame.data(), (uint32_t)frame.size());
                    mSentClose = true;
                }

                FailWith(mDecoder.GetErrorMessage().c_str(), WS_CLOSE_PROTOCOL_ERROR);
            }
        }

        void PumpMessageTransport()
        {
            WsMessage message;
            while (mState != WsState::Closed && mTransport->TakeMessage(message))
            {
                DeliverMessage(message.mData, message.mBinary);
                message.mData.clear();
            }

            uint16_t    code = 0;
            std::string reason;
            if (mState != WsState::Closed && mTransport->TakeClose(code, reason))
            {
                HandlePeerClose(code, reason);
            }
        }

        void DeliverMessage(std::vector<uint8_t>& data, bool binary)
        {
            if (mMessageCallback)
            {
                mMessageCallback(data.empty() ? nullptr : data.data(), (uint32_t)data.size(), binary);
                return;
            }

            if (mInQueuedBytes + data.size() > mMaxQueuedBytes)
            {
                if (!mLoggedRecvOverflow)
                {
                    mLoggedRecvOverflow = true;
                    LogWarning("WebSocket: incoming queue is full (cap %u bytes) - dropping messages on %s. "
                               "Set a message callback or drain with GetPacket().",
                        (unsigned)mMaxQueuedBytes, mUrl.mHost.c_str());
                }
                return;
            }

            WsMessage queued;
            queued.mBinary = binary;
            queued.mData.swap(data);
            mInQueuedBytes += (uint32_t)queued.mData.size();
            mInQueue.push_back(std::move(queued));
        }

        void HandlePeerClose(uint16_t code, const std::string& reason)
        {
            mReceivedClose = true;
            mWasClean      = true;

            if (mCloseCode == 0 || !mSentClose)
            {
                mCloseCode   = code != 0 ? code : uint16_t(WS_CLOSE_NORMAL);
                mCloseReason = reason;
            }

            if (!mSentClose && mTransport != nullptr && !mTransport->IsMessageTransport())
            {
                // RFC 6455 5.5.1: echo the Close back, then we are done.
                std::vector<uint8_t> frame;
                WsEncodeClose(mCloseCode, mCloseReason.c_str(), frame);
                mTransport->SendBytes(frame.data(), (uint32_t)frame.size());
                mSentClose = true;
            }

            mState = WsState::Closing;
            FinishClosed();
        }

        void FailWith(const char* message, uint16_t closeCode)
        {
            if (mState == WsState::Closed)
            {
                return;
            }

            mWasClean = false;
            if (mCloseCode == 0)
            {
                mCloseCode   = closeCode;
                mCloseReason.clear();
            }

            if (mErrorCallback)
            {
                mErrorCallback(message != nullptr && *message != '\0' ? message : "WebSocket error");
            }

            FinishClosed();
        }

        void FinishClosed()
        {
            if (mState == WsState::Closed)
            {
                return;
            }

            mState = WsState::Closed;
            mClosePending = false;

            if (mTransport != nullptr)
            {
                mTransport->Shutdown();
            }

            if (mCloseCode == 0)
            {
                mCloseCode = WS_CLOSE_ABNORMAL;
            }

            // Closed is terminal and fires exactly once, so every callback can
            // be released here - and must be. A Lua callback almost always
            // captures the connection handle it was set on, which makes
            // registry -> closure -> userdata -> connection -> closure a cycle
            // Lua's collector can never break on its own. Dropping the
            // std::functions the moment the connection is done releases the
            // registry refs and lets the handle become collectable.
            WsClosedCallback closedCallback;
            closedCallback.swap(mClosedCallback);
            mOpenCallback    = WsOpenCallback();
            mMessageCallback = WsMessageCallback();
            mErrorCallback   = WsErrorCallback();

            if (closedCallback)
            {
                closedCallback(mCloseCode, mCloseReason.c_str(), mWasClean);
            }
        }

        WsUrl                                             mUrl;
        std::vector<std::string>                          mProtocols;
        std::vector<std::pair<std::string, std::string> > mHeaders;

        WsTransport*         mTransport = nullptr;
        WsDecoder            mDecoder;
        std::vector<uint8_t> mHandshakeBuffer;
        std::string          mExpectedAccept;
        bool                 mSentHandshake = false;

        WsState  mState = WsState::Connecting;
        bool     mSentClose     = false;
        bool     mReceivedClose = false;
        bool     mClosePending  = false;
        bool     mWasClean      = false;
        bool     mDowngraded    = false;
        uint16_t mCloseCode     = 0;
        std::string mCloseReason;
        std::string mSelectedProtocol;
        time_t      mCloseDeadline = 0;

        std::deque<WsMessage> mInQueue;
        uint32_t              mInQueuedBytes  = 0;
        uint32_t              mMaxQueuedBytes = WS_DEFAULT_MAX_QUEUED_BYTES;
        bool                  mLoggedSendOverflow = false;
        bool                  mLoggedRecvOverflow = false;

        WsOpenCallback    mOpenCallback;
        WsMessageCallback mMessageCallback;
        WsErrorCallback   mErrorCallback;
        WsClosedCallback  mClosedCallback;
    };

    struct ClientState
    {
        std::vector<std::weak_ptr<WsConnectionImpl> > mConnections;
    };

    ClientState* sState = nullptr;
}

#if !PLATFORM_WINDOWS && !PLATFORM_LINUX && !PLATFORM_MAC

// No TLS stack on this target. Phase 3 supplies the Windows (WinHTTP) and
// Linux (libcurl) message transports; everything else fails wss:// loudly
// unless the build opted into POLYPHASE_WS_DOWNGRADE_WSS.
bool WsIsSecureSupported()
{
    return false;
}

const char* WsSecureUnavailableMessage()
{
    return "wss:// is not supported on this platform";
}

WsTransport* WsCreateSecureTransport(const WsUrl&, const WsConnectOptions&, std::string& outError)
{
    outError = WsSecureUnavailableMessage();
    return nullptr;
}

#endif

namespace WebSocket
{
    void Initialize()
    {
        if (sState != nullptr)
        {
            return;
        }

        sState = new ClientState();
    }

    void Shutdown()
    {
        if (sState == nullptr)
        {
            return;
        }

        for (size_t i = 0; i < sState->mConnections.size(); ++i)
        {
            std::shared_ptr<WsConnectionImpl> conn = sState->mConnections[i].lock();
            if (conn != nullptr)
            {
                conn->Close(WS_CLOSE_GOING_AWAY, "Engine shutting down");
            }
        }

        delete sState;
        sState = nullptr;
    }

    void Tick()
    {
        if (sState == nullptr)
        {
            return;
        }

        // Lock every live connection first. A Lua callback fired from Pump()
        // can call WebSocket.Connect, which appends to mConnections - so the
        // compaction has to be finished before any pumping starts.
        std::vector<std::shared_ptr<WsConnectionImpl> > live;
        live.reserve(sState->mConnections.size());

        size_t write = 0;
        for (size_t i = 0; i < sState->mConnections.size(); ++i)
        {
            std::shared_ptr<WsConnectionImpl> conn = sState->mConnections[i].lock();
            if (conn == nullptr)
            {
                continue;
            }

            sState->mConnections[write++] = sState->mConnections[i];
            live.push_back(conn);
        }
        sState->mConnections.resize(write);

        for (size_t i = 0; i < live.size(); ++i)
        {
            live[i]->Pump();
        }
    }

    bool IsAvailable()
    {
        return sState != nullptr && NET_IsActive();
    }

    const char* GetMissingDependencyMessage()
    {
        if (sState == nullptr)
        {
            return "WebSocket::Initialize() was not called";
        }
        if (!NET_IsActive())
        {
            return "No network connection is available.";
        }
        return "";
    }

    std::shared_ptr<WsConnection> Connect(const WsConnectOptions& options, std::string& outError)
    {
        outError.clear();

        if (sState == nullptr)
        {
            outError = "WebSocket::Initialize() was not called";
            return nullptr;
        }

        WsUrl url;
        if (!WsParseUrl(options.mUrl.c_str(), url, outError))
        {
            return nullptr;
        }

        bool downgraded = false;

#if defined(POLYPHASE_WS_DOWNGRADE_WSS) && POLYPHASE_WS_DOWNGRADE_WSS
        if (url.mSecure)
        {
            const bool defaultPort = (url.mPort == 443);
            url.mScheme = "ws";
            url.mSecure = false;
            if (defaultPort)
            {
                url.mPort = 80;
            }
            downgraded = true;

            LogWarning("WebSocket: wss:// downgraded to ws:// on this build target - traffic is UNENCRYPTED: %s",
                options.mUrl.c_str());
        }
#endif

        WsTransport* transport = nullptr;

        if (url.mSecure)
        {
            if (!WsIsSecureSupported())
            {
                outError = WsSecureUnavailableMessage();
                return nullptr;
            }

            transport = WsCreateSecureTransport(url, options, outError);
            if (transport == nullptr)
            {
                if (outError.empty())
                {
                    outError = "Could not create a secure WebSocket transport";
                }
                return nullptr;
            }
        }
        else
        {
            WsByteTransport* byteTransport = new WsByteTransport();
            if (!byteTransport->Start(url, outError))
            {
                delete byteTransport;
                return nullptr;
            }
            transport = byteTransport;
        }

        std::shared_ptr<WsConnectionImpl> conn = std::make_shared<WsConnectionImpl>();
        conn->Init(url, options, transport, downgraded);

        sState->mConnections.push_back(conn);
        return conn;
    }
}

#elif defined(POLYPHASE_WS_PROVIDED_BY_ADDON)

// ---------------------------------------------------------------------------
// Nothing here. The build-target package supplies every WebSocket:: symbol and
// its own WsConnection subclass (see Runtime/Web/Ws_Web.cpp in the web
// packages, built on the browser's WebSocket global).
// ---------------------------------------------------------------------------

#else  // POLYPHASE_PLATFORM_ADDON without an addon-supplied implementation

// ---------------------------------------------------------------------------
// Self-contained stubs. Every Variant-2 build-target package that predates
// this feature lands here and keeps linking with zero edits - WebSocket simply
// reports itself unavailable.
// ---------------------------------------------------------------------------

namespace WebSocket
{
    void Initialize() {}
    void Shutdown()   {}
    void Tick()       {}

    bool IsAvailable() { return false; }

    const char* GetMissingDependencyMessage()
    {
        return "WebSocket is not supported on this platform.";
    }

    std::shared_ptr<WsConnection> Connect(const WsConnectOptions&, std::string& outError)
    {
        outError = "WebSocket is not supported on this platform.";
        return nullptr;
    }
}

#endif  // POLYPHASE_PLATFORM_ADDON
