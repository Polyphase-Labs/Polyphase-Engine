#if PLATFORM_WINDOWS && !defined(POLYPHASE_PLATFORM_ADDON)

#include "Network/WebSocketTransport.h"
#include "Log.h"

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// ---------------------------------------------------------------------------
// wss:// on Windows, via the WinHTTP WebSocket API (Windows 8+). WinHTTP owns
// the TLS handshake, the RFC 6455 upgrade and the framing, so this is a
// message transport - WebSocketFraming/WebSocketHandshake are not involved.
//
// WinHTTP's WebSocket calls block, so each connection gets a receive thread
// and a send thread. They only ever touch a lock-protected handoff queue;
// WebSocket::Tick drains it on the main thread, which is where Lua callbacks
// fire. Nothing below this comment ever touches Lua or an engine singleton
// other than the logger.
// ---------------------------------------------------------------------------

namespace
{
    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return std::wstring();
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
        return out;
    }

    std::string WideToUtf8(const wchar_t* w, int len)
    {
        if (w == nullptr || len <= 0) return std::string();
        const int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, len, &out[0], n, nullptr, nullptr);
        return out;
    }

    struct OutgoingMessage
    {
        std::vector<uint8_t> mData;
        bool                 mBinary = false;
    };

    class WsWinHttpTransport : public WsTransport
    {
    public:

        ~WsWinHttpTransport() override
        {
            Shutdown();
        }

        bool Start(const WsUrl& url, const WsConnectOptions& options, std::string& outError)
        {
            mSession = WinHttpOpen(L"Polyphase",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

            if (mSession == nullptr)
            {
                outError = "WinHttpOpen failed";
                return false;
            }

            mHost     = Utf8ToWide(url.mHost);
            mResource = Utf8ToWide(url.mResource);
            mPort     = url.mPort;

            for (size_t i = 0; i < options.mHeaders.size(); ++i)
            {
                if (options.mHeaders[i].first.find_first_of("\r\n") != std::string::npos ||
                    options.mHeaders[i].second.find_first_of("\r\n") != std::string::npos)
                {
                    continue;
                }
                mExtraHeaders += Utf8ToWide(options.mHeaders[i].first + ": " + options.mHeaders[i].second + "\r\n");
            }

            if (!options.mProtocols.empty())
            {
                std::string protocolList;
                for (size_t i = 0; i < options.mProtocols.size(); ++i)
                {
                    if (i != 0) protocolList += ", ";
                    protocolList += options.mProtocols[i];
                }
                mExtraHeaders += Utf8ToWide("Sec-WebSocket-Protocol: " + protocolList + "\r\n");
            }

            mRunning.store(true);
            mReceiveThread = std::thread(&WsWinHttpTransport::ReceiveLoop, this);
            return true;
        }

        // -- WsTransport ----------------------------------------------------

        bool IsMessageTransport() const override { return true; }

        void Tick() override {}

        bool IsConnected() const override { return mConnected.load(); }
        bool HasFailed() const override   { return mFailed.load(); }

        WsError GetError() const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mError;
        }

        const std::string& GetErrorMessage() const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mErrorMessage;
        }

        bool IsEndOfStream() const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEndOfStream && mRxQueue.empty() && !mHasClose;
        }

        const std::string& GetSelectedProtocol() const override { return mSelectedProtocol; }

        uint32_t GetPendingSendBytes() const override { return mPendingSendBytes.load(); }

        bool SendWholeMessage(const uint8_t* data, uint32_t size, bool binary) override
        {
            if (mFailed.load() || !mRunning.load())
            {
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                OutgoingMessage msg;
                msg.mBinary = binary;
                msg.mData.assign(data, data + size);
                mPendingSendBytes.fetch_add(size);
                mTxQueue.push_back(std::move(msg));
            }
            mTxCv.notify_one();
            return true;
        }

        bool TakeMessage(WsMessage& outMessage) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mRxQueue.empty())
            {
                return false;
            }

            outMessage = std::move(mRxQueue.front());
            mRxQueue.pop_front();
            return true;
        }

        bool TakeClose(uint16_t& outCode, std::string& outReason) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mHasClose)
            {
                return false;
            }

            mHasClose  = false;
            outCode    = mCloseCode;
            outReason  = mCloseReason;
            return true;
        }

        void StartClose(uint16_t code, const char* reason) override
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mCloseRequested) return;
                mCloseRequested = true;
                mRequestedCloseCode   = code;
                mRequestedCloseReason = reason != nullptr ? reason : "";
            }
            mTxCv.notify_one();
        }

        void Shutdown() override
        {
            if (!mRunning.exchange(false))
            {
                JoinThreads();
                CloseHandles();
                return;
            }

            mTxCv.notify_all();

            // Closing the socket handle makes an in-flight blocking
            // WinHttpWebSocketReceive return immediately.
            if (mWebSocket != nullptr)
            {
                WinHttpCloseHandle(mWebSocket);
                mWebSocket = nullptr;
            }

            JoinThreads();
            CloseHandles();
        }

    private:

        void JoinThreads()
        {
            if (mReceiveThread.joinable() && mReceiveThread.get_id() != std::this_thread::get_id())
            {
                mReceiveThread.join();
            }
            if (mSendThread.joinable() && mSendThread.get_id() != std::this_thread::get_id())
            {
                mSendThread.join();
            }
        }

        void CloseHandles()
        {
            if (mWebSocket != nullptr) { WinHttpCloseHandle(mWebSocket); mWebSocket = nullptr; }
            if (mRequest != nullptr)   { WinHttpCloseHandle(mRequest);   mRequest = nullptr; }
            if (mConnection != nullptr){ WinHttpCloseHandle(mConnection);mConnection = nullptr; }
            if (mSession != nullptr)   { WinHttpCloseHandle(mSession);   mSession = nullptr; }
        }

        void Fail(WsError err, const char* message)
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mError == WsError::None)
                {
                    mError = err;
                    mErrorMessage = message;
                }
                mEndOfStream = true;
            }
            mFailed.store(true);
            mTxCv.notify_all();
        }

        bool RunUpgrade()
        {
            mConnection = WinHttpConnect(mSession, mHost.c_str(), (INTERNET_PORT)mPort, 0);
            if (mConnection == nullptr)
            {
                Fail(WsError::Connect, "WinHttpConnect failed");
                return false;
            }

            mRequest = WinHttpOpenRequest(mConnection, L"GET", mResource.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (mRequest == nullptr)
            {
                Fail(WsError::Connect, "WinHttpOpenRequest failed");
                return false;
            }

            if (!WinHttpSetOption(mRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
            {
                Fail(WsError::Handshake, "This Windows build has no WinHTTP WebSocket support");
                return false;
            }

            if (!mExtraHeaders.empty())
            {
                WinHttpAddRequestHeaders(mRequest, mExtraHeaders.c_str(), (DWORD)-1L,
                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            }

            if (!WinHttpSendRequest(mRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            {
                Fail(WsError::Connect, "WinHttpSendRequest failed");
                return false;
            }

            if (!WinHttpReceiveResponse(mRequest, nullptr))
            {
                Fail(WsError::Handshake, "WinHttpReceiveResponse failed");
                return false;
            }

            // Read the negotiated subprotocol before the request handle goes.
            wchar_t protocolBuffer[256];
            DWORD   protocolBytes = sizeof(protocolBuffer);
            if (WinHttpQueryHeaders(mRequest, WINHTTP_QUERY_CUSTOM, L"Sec-WebSocket-Protocol",
                                    protocolBuffer, &protocolBytes, WINHTTP_NO_HEADER_INDEX))
            {
                mSelectedProtocol = WideToUtf8(protocolBuffer, (int)(protocolBytes / sizeof(wchar_t)));
            }

            mWebSocket = (HINTERNET)WinHttpWebSocketCompleteUpgrade(mRequest, 0);
            if (mWebSocket == nullptr)
            {
                Fail(WsError::Handshake, "Server refused the WebSocket upgrade");
                return false;
            }

            WinHttpCloseHandle(mRequest);
            mRequest = nullptr;

            mConnected.store(true);
            mSendThread = std::thread(&WsWinHttpTransport::SendLoop, this);
            return true;
        }

        void ReceiveLoop()
        {
            if (!RunUpgrade())
            {
                return;
            }

            std::vector<uint8_t> assembly;
            uint8_t chunk[WS_RECV_CHUNK_SIZE];

            while (mRunning.load())
            {
                DWORD                            read = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE   type = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;

                const DWORD rc = WinHttpWebSocketReceive(mWebSocket, chunk, sizeof(chunk), &read, &type);
                if (rc != NO_ERROR)
                {
                    if (mRunning.load())
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        mEndOfStream = true;
                    }
                    break;
                }

                if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
                {
                    USHORT  status = WS_CLOSE_NORMAL;
                    BYTE    reasonBuffer[123];
                    DWORD   reasonLength = 0;
                    std::string reason;

                    if (WinHttpWebSocketQueryCloseStatus(mWebSocket, &status,
                            reasonBuffer, sizeof(reasonBuffer), &reasonLength) == NO_ERROR)
                    {
                        reason.assign((const char*)reasonBuffer, reasonLength);
                    }

                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        mHasClose    = true;
                        mCloseCode   = (uint16_t)status;
                        mCloseReason = reason;
                        mEndOfStream = true;
                    }
                    break;
                }

                assembly.insert(assembly.end(), chunk, chunk + read);

                const bool fragment = (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)
                                   || (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE);
                if (fragment)
                {
                    if (assembly.size() > WS_MAX_MESSAGE_BYTES)
                    {
                        Fail(WsError::TooLarge, "Reassembled message exceeds the maximum message size");
                        break;
                    }
                    continue;
                }

                WsMessage message;
                message.mBinary = (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
                               || (type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);
                message.mData.swap(assembly);
                assembly.clear();

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mRxQueue.push_back(std::move(message));
                }
            }

            mTxCv.notify_all();
        }

        void SendLoop()
        {
            for (;;)
            {
                OutgoingMessage message;
                bool     haveMessage = false;
                bool     doClose     = false;
                uint16_t closeCode   = WS_CLOSE_NORMAL;
                std::string closeReason;

                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mTxCv.wait(lock, [this] {
                        return !mRunning.load() || mFailed.load() || !mTxQueue.empty() || mCloseRequested;
                    });

                    if (!mRunning.load() || mFailed.load())
                    {
                        return;
                    }

                    if (!mTxQueue.empty())
                    {
                        message = std::move(mTxQueue.front());
                        mTxQueue.pop_front();
                        haveMessage = true;
                    }
                    else if (mCloseRequested)
                    {
                        doClose         = true;
                        mCloseRequested = false;
                        closeCode       = mRequestedCloseCode;
                        closeReason     = mRequestedCloseReason;
                    }
                }

                if (haveMessage)
                {
                    const WINHTTP_WEB_SOCKET_BUFFER_TYPE type = message.mBinary
                        ? WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE
                        : WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;

                    const DWORD rc = WinHttpWebSocketSend(mWebSocket, type,
                        message.mData.empty() ? nullptr : (PVOID)message.mData.data(),
                        (DWORD)message.mData.size());

                    mPendingSendBytes.fetch_sub((uint32_t)message.mData.size());

                    if (rc != NO_ERROR)
                    {
                        Fail(WsError::Transport, "WinHttpWebSocketSend failed");
                        return;
                    }
                    continue;
                }

                if (doClose)
                {
                    WinHttpWebSocketClose(mWebSocket, closeCode,
                        closeReason.empty() ? nullptr : (PVOID)closeReason.data(),
                        (DWORD)closeReason.size());
                    return;
                }
            }
        }

        mutable std::mutex      mMutex;
        std::condition_variable mTxCv;

        HINTERNET mSession    = nullptr;
        HINTERNET mConnection = nullptr;
        HINTERNET mRequest    = nullptr;
        HINTERNET mWebSocket  = nullptr;

        std::wstring mHost;
        std::wstring mResource;
        std::wstring mExtraHeaders;
        uint16_t     mPort = 443;

        std::thread mReceiveThread;
        std::thread mSendThread;

        std::atomic<bool>     mRunning{ false };
        std::atomic<bool>     mConnected{ false };
        std::atomic<bool>     mFailed{ false };
        std::atomic<uint32_t> mPendingSendBytes{ 0 };

        WsError     mError = WsError::None;
        std::string mErrorMessage;
        std::string mSelectedProtocol;

        std::deque<WsMessage>      mRxQueue;
        std::deque<OutgoingMessage> mTxQueue;

        bool        mEndOfStream    = false;
        bool        mHasClose       = false;
        uint16_t    mCloseCode      = 0;
        std::string mCloseReason;

        bool        mCloseRequested       = false;
        uint16_t    mRequestedCloseCode   = WS_CLOSE_NORMAL;
        std::string mRequestedCloseReason;
    };
}

bool WsIsSecureSupported()
{
    return true;
}

const char* WsSecureUnavailableMessage()
{
    return "";
}

WsTransport* WsCreateSecureTransport(const WsUrl& url, const WsConnectOptions& options, std::string& outError)
{
    WsWinHttpTransport* transport = new WsWinHttpTransport();
    if (!transport->Start(url, options, outError))
    {
        delete transport;
        return nullptr;
    }
    return transport;
}

#endif  // PLATFORM_WINDOWS && !POLYPHASE_PLATFORM_ADDON
