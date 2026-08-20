#if PLATFORM_LINUX && !defined(POLYPHASE_PLATFORM_ADDON)

#include "Network/WebSocketTransport.h"
#include "Log.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include <atomic>
#include <deque>
#include <thread>

// ---------------------------------------------------------------------------
// wss:// on Linux, via libcurl's WebSocket support (7.86+). Same arrangement as
// HttpBackend_Linux: libcurl is dlopen'd rather than linked, because Polyphase
// does not ship it - a host without libcurl4 simply reports wss:// as
// unavailable while ws:// keeps working through the NET_ byte transport.
//
// CURLOPT_CONNECT_ONLY=2 hands us a connected, upgraded socket; curl_ws_recv /
// curl_ws_send are then non-blocking and polled straight from WebSocket::Tick.
// Only the handshake blocks, so that runs on a one-shot worker thread which is
// joined the moment it finishes.
// ---------------------------------------------------------------------------

namespace
{
    typedef void CURL;
    typedef int  CURLcode;
    typedef int  CURLoption;

    constexpr int CURLE_OK          = 0;
    constexpr int CURLE_AGAIN       = 81;

    constexpr int CURLOPT_URL               = 10002;
    constexpr int CURLOPT_HTTPHEADER        = 10023;
    constexpr int CURLOPT_CONNECTTIMEOUT_MS =   156;
    constexpr int CURLOPT_CONNECT_ONLY      =   141;
    constexpr int CURLOPT_USERAGENT         = 10018;

    constexpr unsigned CURLWS_TEXT   = 1u << 0;
    constexpr unsigned CURLWS_BINARY = 1u << 1;
    constexpr unsigned CURLWS_CONT   = 1u << 2;
    constexpr unsigned CURLWS_CLOSE  = 1u << 3;
    constexpr unsigned CURLWS_PING   = 1u << 4;
    constexpr unsigned CURLWS_PONG   = 1u << 6;

    struct curl_slist;

    struct curl_ws_frame
    {
        int      age;
        int      flags;
        int64_t  offset;
        int64_t  bytesleft;
        size_t   len;
    };

    using curl_easy_init_t      = CURL*       (*)();
    using curl_easy_cleanup_t   = void        (*)(CURL*);
    using curl_easy_setopt_t    = CURLcode    (*)(CURL*, CURLoption, ...);
    using curl_easy_perform_t   = CURLcode    (*)(CURL*);
    using curl_easy_strerror_t  = const char* (*)(CURLcode);
    using curl_slist_append_t   = curl_slist* (*)(curl_slist*, const char*);
    using curl_slist_free_all_t = void        (*)(curl_slist*);
    using curl_global_init_t    = CURLcode    (*)(long);
    using curl_ws_recv_t        = CURLcode    (*)(CURL*, void*, size_t, size_t*, const curl_ws_frame**);
    using curl_ws_send_t        = CURLcode    (*)(CURL*, const void*, size_t, size_t*, int64_t, unsigned int);

    struct CurlWsApi
    {
        void* lib = nullptr;

        curl_easy_init_t      easy_init      = nullptr;
        curl_easy_cleanup_t   easy_cleanup   = nullptr;
        curl_easy_setopt_t    easy_setopt    = nullptr;
        curl_easy_perform_t   easy_perform   = nullptr;
        curl_easy_strerror_t  easy_strerror  = nullptr;
        curl_slist_append_t   slist_append   = nullptr;
        curl_slist_free_all_t slist_free_all = nullptr;
        curl_global_init_t    global_init    = nullptr;
        curl_ws_recv_t        ws_recv        = nullptr;
        curl_ws_send_t        ws_send        = nullptr;

        bool        available = false;
        std::string missingMessage = "libcurl was not loaded";
    };

    CurlWsApi& GetCurl()
    {
        static CurlWsApi sApi;
        static bool sTried = false;

        if (sTried)
        {
            return sApi;
        }
        sTried = true;

        const char* names[] = {
            "libcurl.so.4",
            "libcurl.so",
            "libcurl-gnutls.so.4",
            "libcurl.so.3"
        };

        for (const char* n : names)
        {
            sApi.lib = dlopen(n, RTLD_LAZY);
            if (sApi.lib != nullptr) break;
        }

        if (sApi.lib == nullptr)
        {
            sApi.missingMessage = "wss:// needs libcurl4 (>= 7.86) - install it to enable secure WebSockets.";
            return sApi;
        }

        sApi.easy_init      = (curl_easy_init_t)      dlsym(sApi.lib, "curl_easy_init");
        sApi.easy_cleanup   = (curl_easy_cleanup_t)   dlsym(sApi.lib, "curl_easy_cleanup");
        sApi.easy_setopt    = (curl_easy_setopt_t)    dlsym(sApi.lib, "curl_easy_setopt");
        sApi.easy_perform   = (curl_easy_perform_t)   dlsym(sApi.lib, "curl_easy_perform");
        sApi.easy_strerror  = (curl_easy_strerror_t)  dlsym(sApi.lib, "curl_easy_strerror");
        sApi.slist_append   = (curl_slist_append_t)   dlsym(sApi.lib, "curl_slist_append");
        sApi.slist_free_all = (curl_slist_free_all_t) dlsym(sApi.lib, "curl_slist_free_all");
        sApi.global_init    = (curl_global_init_t)    dlsym(sApi.lib, "curl_global_init");
        sApi.ws_recv        = (curl_ws_recv_t)        dlsym(sApi.lib, "curl_ws_recv");
        sApi.ws_send        = (curl_ws_send_t)        dlsym(sApi.lib, "curl_ws_send");

        if (sApi.easy_init == nullptr || sApi.easy_cleanup == nullptr
            || sApi.easy_setopt == nullptr || sApi.easy_perform == nullptr)
        {
            dlclose(sApi.lib);
            sApi.lib = nullptr;
            sApi.missingMessage = "The installed libcurl is missing the symbols we need.";
            return sApi;
        }

        if (sApi.ws_recv == nullptr || sApi.ws_send == nullptr)
        {
            // Pre-7.86 libcurl, or a build with --disable-websockets.
            dlclose(sApi.lib);
            sApi.lib = nullptr;
            sApi.missingMessage = "The installed libcurl has no WebSocket support (needs >= 7.86).";
            return sApi;
        }

        if (sApi.global_init != nullptr)
        {
            sApi.global_init(3 /* CURL_GLOBAL_ALL */);
        }

        sApi.available = true;
        sApi.missingMessage.clear();
        return sApi;
    }

    class WsCurlTransport : public WsTransport
    {
    public:

        ~WsCurlTransport() override
        {
            Shutdown();
        }

        bool Start(const WsUrl& url, const WsConnectOptions& options, std::string& outError)
        {
            CurlWsApi& curl = GetCurl();
            if (!curl.available)
            {
                outError = curl.missingMessage;
                return false;
            }

            mCurl = curl.easy_init();
            if (mCurl == nullptr)
            {
                outError = "curl_easy_init failed";
                return false;
            }

            char portSuffix[16];
            snprintf(portSuffix, sizeof(portSuffix), ":%u", (unsigned)url.mPort);
            mFullUrl = "wss://" + url.mHost + portSuffix + url.mResource;

            curl.easy_setopt(mCurl, CURLOPT_URL, mFullUrl.c_str());
            curl.easy_setopt(mCurl, CURLOPT_CONNECT_ONLY, 2L);
            curl.easy_setopt(mCurl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
            curl.easy_setopt(mCurl, CURLOPT_USERAGENT, "Polyphase");

            for (size_t i = 0; i < options.mHeaders.size(); ++i)
            {
                if (options.mHeaders[i].first.find_first_of("\r\n") != std::string::npos ||
                    options.mHeaders[i].second.find_first_of("\r\n") != std::string::npos)
                {
                    continue;
                }
                const std::string line = options.mHeaders[i].first + ": " + options.mHeaders[i].second;
                mHeaderList = curl.slist_append(mHeaderList, line.c_str());
            }

            if (!options.mProtocols.empty())
            {
                std::string protocolList = "Sec-WebSocket-Protocol: ";
                for (size_t i = 0; i < options.mProtocols.size(); ++i)
                {
                    if (i != 0) protocolList += ", ";
                    protocolList += options.mProtocols[i];
                }
                mHeaderList = curl.slist_append(mHeaderList, protocolList.c_str());
                mRequestedProtocol = options.mProtocols[0];
            }

            if (mHeaderList != nullptr)
            {
                curl.easy_setopt(mCurl, CURLOPT_HTTPHEADER, mHeaderList);
            }

            mHandshakeThread = std::thread([this]()
            {
                CurlWsApi& api = GetCurl();
                const CURLcode rc = api.easy_perform(mCurl);
                if (rc == CURLE_OK)
                {
                    mHandshakeOk.store(true);
                }
                else
                {
                    mHandshakeError = (api.easy_strerror != nullptr)
                        ? api.easy_strerror(rc) : "wss:// handshake failed";
                }
                mHandshakeDone.store(true);
            });

            return true;
        }

        // -- WsTransport ----------------------------------------------------

        bool IsMessageTransport() const override { return true; }

        void Tick() override
        {
            if (mFailed || mCurl == nullptr)
            {
                return;
            }

            if (!mConnected)
            {
                if (!mHandshakeDone.load())
                {
                    return;
                }

                if (mHandshakeThread.joinable())
                {
                    mHandshakeThread.join();
                }

                if (!mHandshakeOk.load())
                {
                    Fail(WsError::Handshake, mHandshakeError.empty()
                        ? "wss:// handshake failed" : mHandshakeError.c_str());
                    return;
                }

                // libcurl does not report the negotiated subprotocol through
                // any public API in CONNECT_ONLY mode, so echo back what we
                // offered first - the server either accepted it or refused the
                // upgrade entirely.
                mSelectedProtocol = mRequestedProtocol;
                mConnected = true;
            }

            FlushSend();
            Receive();
        }

        bool IsConnected() const override { return mConnected; }
        bool HasFailed() const override   { return mFailed; }
        WsError GetError() const override { return mError; }
        const std::string& GetErrorMessage() const override { return mErrorMessage; }

        bool IsEndOfStream() const override { return mEndOfStream && mRxQueue.empty() && !mHasClose; }

        const std::string& GetSelectedProtocol() const override { return mSelectedProtocol; }

        uint32_t GetPendingSendBytes() const override { return mPendingSendBytes; }

        bool SendWholeMessage(const uint8_t* data, uint32_t size, bool binary) override
        {
            if (mFailed || mCurl == nullptr)
            {
                return false;
            }

            Outgoing out;
            out.mFlags = binary ? CURLWS_BINARY : CURLWS_TEXT;
            out.mData.assign(data, data + size);
            mPendingSendBytes += size;
            mTxQueue.push_back(std::move(out));

            if (mConnected)
            {
                FlushSend();
            }
            return true;
        }

        bool TakeMessage(WsMessage& outMessage) override
        {
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
            if (!mHasClose)
            {
                return false;
            }

            mHasClose = false;
            outCode   = mCloseCode;
            outReason = mCloseReason;
            return true;
        }

        void StartClose(uint16_t code, const char* reason) override
        {
            if (mSentClose || mCurl == nullptr)
            {
                return;
            }
            mSentClose = true;

            Outgoing out;
            out.mFlags = CURLWS_CLOSE;
            out.mData.push_back(uint8_t((code >> 8) & 0xFF));
            out.mData.push_back(uint8_t(code & 0xFF));
            if (reason != nullptr)
            {
                size_t len = strlen(reason);
                if (len > 123) len = 123;
                out.mData.insert(out.mData.end(), reason, reason + len);
            }

            mPendingSendBytes += (uint32_t)out.mData.size();
            mTxQueue.push_back(std::move(out));
            FlushSend();
        }

        void Shutdown() override
        {
            if (mHandshakeThread.joinable())
            {
                // The handshake owns mCurl until it finishes; there is no way
                // to abort curl_easy_perform from outside, so wait it out. The
                // CURLOPT_CONNECTTIMEOUT_MS above bounds this.
                mHandshakeThread.join();
            }

            if (mCurl != nullptr)
            {
                CurlWsApi& curl = GetCurl();
                curl.easy_cleanup(mCurl);
                mCurl = nullptr;

                if (mHeaderList != nullptr && curl.slist_free_all != nullptr)
                {
                    curl.slist_free_all(mHeaderList);
                    mHeaderList = nullptr;
                }
            }

            mConnected = false;
        }

    private:

        struct Outgoing
        {
            std::vector<uint8_t> mData;
            unsigned             mFlags = 0;
        };

        void Fail(WsError err, const char* message)
        {
            if (!mFailed)
            {
                mFailed = true;
                mError = err;
                mErrorMessage = message;
                mEndOfStream = true;
            }
        }

        void FlushSend()
        {
            CurlWsApi& curl = GetCurl();

            while (!mTxQueue.empty())
            {
                Outgoing& out = mTxQueue.front();

                size_t sent = 0;
                const CURLcode rc = curl.ws_send(mCurl,
                    out.mData.empty() ? "" : (const void*)out.mData.data(),
                    out.mData.size(), &sent, 0, out.mFlags);

                if (rc == CURLE_AGAIN)
                {
                    return;
                }

                if (rc != CURLE_OK)
                {
                    Fail(WsError::Transport, "curl_ws_send failed");
                    return;
                }

                mPendingSendBytes -= (uint32_t)out.mData.size();
                mTxQueue.pop_front();
            }
        }

        void Receive()
        {
            CurlWsApi& curl = GetCurl();

            for (int32_t pass = 0; pass < 64; ++pass)
            {
                char                  chunk[WS_RECV_CHUNK_SIZE];
                size_t                received = 0;
                const curl_ws_frame*  meta = nullptr;

                const CURLcode rc = curl.ws_recv(mCurl, chunk, sizeof(chunk), &received, &meta);

                if (rc == CURLE_AGAIN)
                {
                    return;
                }

                if (rc != CURLE_OK || meta == nullptr)
                {
                    mEndOfStream = true;
                    return;
                }

                const unsigned flags = (unsigned)meta->flags;

                if ((flags & CURLWS_CLOSE) != 0)
                {
                    mHasClose = true;
                    mCloseCode = WS_CLOSE_NORMAL;
                    mCloseReason.clear();

                    if (received >= 2)
                    {
                        mCloseCode = uint16_t((uint16_t((uint8_t)chunk[0]) << 8) | uint16_t((uint8_t)chunk[1]));
                        mCloseReason.assign(chunk + 2, received - 2);
                    }

                    mEndOfStream = true;
                    return;
                }

                if ((flags & CURLWS_PING) != 0)
                {
                    size_t sent = 0;
                    curl.ws_send(mCurl, chunk, received, &sent, 0, CURLWS_PONG);
                    continue;
                }

                if ((flags & CURLWS_PONG) != 0)
                {
                    continue;
                }

                if (mAssembly.size() + received > WS_MAX_MESSAGE_BYTES)
                {
                    Fail(WsError::TooLarge, "Reassembled message exceeds the maximum message size");
                    return;
                }

                if (mAssembly.empty())
                {
                    mAssemblyBinary = (flags & CURLWS_TEXT) == 0;
                }
                mAssembly.insert(mAssembly.end(), chunk, chunk + received);

                // CURLWS_CONT means more fragments follow; bytesleft > 0 means
                // this frame is still arriving in pieces.
                const bool more = ((flags & CURLWS_CONT) != 0) || (meta->bytesleft > 0);
                if (more)
                {
                    continue;
                }

                WsMessage message;
                message.mBinary = mAssemblyBinary;
                message.mData.swap(mAssembly);
                mAssembly.clear();
                mRxQueue.push_back(std::move(message));
            }
        }

        CURL*        mCurl       = nullptr;
        curl_slist*  mHeaderList = nullptr;
        std::string  mFullUrl;
        std::string  mRequestedProtocol;
        std::string  mSelectedProtocol;

        std::thread       mHandshakeThread;
        std::atomic<bool> mHandshakeDone{ false };
        std::atomic<bool> mHandshakeOk{ false };
        std::string       mHandshakeError;

        bool        mConnected   = false;
        bool        mFailed      = false;
        bool        mEndOfStream = false;
        bool        mSentClose   = false;
        WsError     mError       = WsError::None;
        std::string mErrorMessage;

        std::deque<WsMessage> mRxQueue;
        std::deque<Outgoing>  mTxQueue;
        uint32_t              mPendingSendBytes = 0;

        std::vector<uint8_t> mAssembly;
        bool                 mAssemblyBinary = false;

        bool        mHasClose  = false;
        uint16_t    mCloseCode = 0;
        std::string mCloseReason;
    };
}

bool WsIsSecureSupported()
{
    return GetCurl().available;
}

const char* WsSecureUnavailableMessage()
{
    CurlWsApi& curl = GetCurl();
    return curl.available ? "" : curl.missingMessage.c_str();
}

WsTransport* WsCreateSecureTransport(const WsUrl& url, const WsConnectOptions& options, std::string& outError)
{
    WsCurlTransport* transport = new WsCurlTransport();
    if (!transport->Start(url, options, outError))
    {
        delete transport;
        return nullptr;
    }
    return transport;
}

#endif  // PLATFORM_LINUX && !POLYPHASE_PLATFORM_ADDON
