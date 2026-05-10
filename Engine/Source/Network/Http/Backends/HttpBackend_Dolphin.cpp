#if PLATFORM_DOLPHIN

#include "Network/Http/Backends/HttpBackend.h"
#include "Network/Network.h"
#include "Stream.h"
#include "Log.h"

#include <gccore.h>             // libogc2 umbrella (u32/u64 typedefs, etc.)
#include <ogc/lwp_watchdog.h>   // gettime() / gettick() — used by mbedtls_hardware_poll
#include <network.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <string>

// HTTPS support is provided by mbedTLS, vendored at External/mbedtls/.
// Plain HTTP works without it. Expand the guard to disable HTTPS if mbedTLS
// is not yet linked into the Wii/GCN build (e.g. early bring-up).
#ifndef POLYPHASE_DOLPHIN_TLS_ENABLED
#define POLYPHASE_DOLPHIN_TLS_ENABLED 1
#endif

#if POLYPHASE_DOLPHIN_TLS_ENABLED
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>

// MBEDTLS_ERR_NET_*_FAILED live in mbedtls/net_sockets.h, which is gated
// by MBEDTLS_NET_C — and we deliberately #undef that in our user config
// (HttpBackend_Dolphin uses libogc sockets via mbedtls_ssl_set_bio instead
// of mbedTLS's own POSIX-style net layer). The constants themselves are
// stable across mbedTLS versions; redefine here so the BIO callbacks below
// can return the conventional error codes.
#ifndef MBEDTLS_ERR_NET_SEND_FAILED
#define MBEDTLS_ERR_NET_SEND_FAILED   -0x004E
#endif
#ifndef MBEDTLS_ERR_NET_RECV_FAILED
#define MBEDTLS_ERR_NET_RECV_FAILED   -0x004C
#endif

// Custom entropy source. The user config defines MBEDTLS_NO_PLATFORM_ENTROPY
// (devkitPPC isn't Unix or Windows so mbedTLS's entropy_poll.c is compiled
// out) and MBEDTLS_ENTROPY_HARDWARE_ALT, which obliges us to provide this
// function. mbedTLS's entropy collector calls it during ctr_drbg seeding.
//
// libogc's gettime() reads the PowerPC time-base register, which advances at
// the bus frequency (~243 MHz on Wii). Combining successive samples with
// gettick gives us enough variability to seed a CSPRNG; mbedTLS's CTR-DRBG
// then handles all the cryptographic strength.
extern "C" int mbedtls_hardware_poll(void* /*data*/, unsigned char* output, size_t len, size_t* olen)
{
    if (output == nullptr || olen == nullptr) return -1;

    size_t produced = 0;
    while (produced < len)
    {
        const u64 t  = gettime();
        const u32 tk = gettick();
        const uint8_t bytes[12] = {
            (uint8_t)(t  >>  0), (uint8_t)(t  >>  8), (uint8_t)(t  >> 16), (uint8_t)(t  >> 24),
            (uint8_t)(t  >> 32), (uint8_t)(t  >> 40), (uint8_t)(t  >> 48), (uint8_t)(t  >> 56),
            (uint8_t)(tk >>  0), (uint8_t)(tk >>  8), (uint8_t)(tk >> 16), (uint8_t)(tk >> 24)
        };
        const size_t copy = (len - produced) < sizeof(bytes) ? (len - produced) : sizeof(bytes);
        memcpy(output + produced, bytes, copy);
        produced += copy;
        // Brief spin so successive gettime/gettick reads diverge.
        for (volatile int i = 0; i < 100; ++i) { (void)i; }
    }
    *olen = produced;
    return 0;
}
#endif

namespace
{
    // ------------------------------------------------------------------
    // URL parsing — small, no malloc, no exceptions, no regex.
    // ------------------------------------------------------------------
    struct ParsedUrl
    {
        std::string scheme;     // "http" or "https"
        std::string host;
        uint16_t    port = 0;
        std::string pathAndQuery;   // "/foo?bar"
        bool        valid = false;
    };

    ParsedUrl ParseUrl(const std::string& url)
    {
        ParsedUrl out;

        size_t i = url.find("://");
        if (i == std::string::npos) return out;

        out.scheme = url.substr(0, i);
        for (char& c : out.scheme) c = (char)std::tolower((unsigned char)c);

        const size_t hostStart = i + 3;
        size_t pathStart = url.find('/', hostStart);
        if (pathStart == std::string::npos)
        {
            out.host = url.substr(hostStart);
            out.pathAndQuery = "/";
        }
        else
        {
            out.host = url.substr(hostStart, pathStart - hostStart);
            out.pathAndQuery = url.substr(pathStart);
        }

        // Optional :port suffix
        size_t portColon = out.host.rfind(':');
        // Avoid matching colons inside IPv6 brackets (we don't really support
        // IPv6 here, but be defensive).
        if (portColon != std::string::npos && out.host.find(']') == std::string::npos)
        {
            const std::string portStr = out.host.substr(portColon + 1);
            out.host = out.host.substr(0, portColon);
            out.port = (uint16_t)atoi(portStr.c_str());
        }
        if (out.port == 0)
        {
            out.port = (out.scheme == "https") ? 443 : 80;
        }

        out.valid = !out.host.empty();
        return out;
    }

    // ------------------------------------------------------------------
    // Transport abstraction. Plain TCP and (optional) TLS share an interface
    // so the HTTP framing code is identical for HTTP and HTTPS.
    // ------------------------------------------------------------------
    class ITransport
    {
    public:
        virtual ~ITransport() = default;
        virtual bool    Connect(const ParsedUrl& url, int timeoutMs) = 0;
        virtual int     Send(const char* data, size_t size) = 0;
        virtual int     Recv(char* buffer, size_t size) = 0;
        virtual void    Close() = 0;
    };

    class TcpTransport : public ITransport
    {
    public:
        bool Connect(const ParsedUrl& url, int timeoutMs) override
        {
            const uint32_t ip = NET_ResolveHost(url.host.c_str());
            if (ip == 0) return false;

            mSock = NET_SocketCreateStream();
            if (mSock < 0) return false;

            return NET_SocketConnect(mSock, ip, url.port, timeoutMs);
        }
        int Send(const char* data, size_t size) override
        {
            return NET_SocketSend(mSock, data, (uint32_t)size);
        }
        int Recv(char* buffer, size_t size) override
        {
            return NET_SocketRecv(mSock, buffer, (uint32_t)size);
        }
        void Close() override
        {
            if (mSock >= 0) { NET_SocketClose(mSock); mSock = -1; }
        }
    private:
        SocketHandle mSock = -1;
    };

#if POLYPHASE_DOLPHIN_TLS_ENABLED
    // mbedTLS over libogc sockets. The CA bundle is loaded once at startup.
    // We use the public API only — no platform glue beyond the BIO callbacks.
    static int MbedNetSend(void* ctx, const unsigned char* buf, size_t len)
    {
        SocketHandle h = (SocketHandle)(intptr_t)ctx;
        const int n = NET_SocketSend(h, (const char*)buf, (uint32_t)len);
        return n < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : n;
    }
    static int MbedNetRecv(void* ctx, unsigned char* buf, size_t len)
    {
        SocketHandle h = (SocketHandle)(intptr_t)ctx;
        const int n = NET_SocketRecv(h, (char*)buf, (uint32_t)len);
        return n < 0 ? MBEDTLS_ERR_NET_RECV_FAILED : n;
    }

    class TlsTransport : public ITransport
    {
    public:
        TlsTransport(mbedtls_x509_crt* caChain,
                     mbedtls_ctr_drbg_context* drbg,
                     bool verify)
            : mCa(caChain), mDrbg(drbg), mVerify(verify)
        {
            mbedtls_ssl_init(&mSsl);
            mbedtls_ssl_config_init(&mCfg);
        }
        ~TlsTransport() override
        {
            Close();
            mbedtls_ssl_free(&mSsl);
            mbedtls_ssl_config_free(&mCfg);
        }

        bool Connect(const ParsedUrl& url, int timeoutMs) override
        {
            const uint32_t ip = NET_ResolveHost(url.host.c_str());
            if (ip == 0) return false;
            mSock = NET_SocketCreateStream();
            if (mSock < 0) return false;
            if (!NET_SocketConnect(mSock, ip, url.port, timeoutMs))
            {
                NET_SocketClose(mSock); mSock = -1; return false;
            }

            if (mbedtls_ssl_config_defaults(&mCfg,
                MBEDTLS_SSL_IS_CLIENT,
                MBEDTLS_SSL_TRANSPORT_STREAM,
                MBEDTLS_SSL_PRESET_DEFAULT) != 0) return false;

            mbedtls_ssl_conf_authmode(&mCfg, mVerify ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
            mbedtls_ssl_conf_ca_chain(&mCfg, mCa, nullptr);
            mbedtls_ssl_conf_rng(&mCfg, mbedtls_ctr_drbg_random, mDrbg);

            if (mbedtls_ssl_setup(&mSsl, &mCfg) != 0) return false;
            if (mbedtls_ssl_set_hostname(&mSsl, url.host.c_str()) != 0) return false;

            mbedtls_ssl_set_bio(&mSsl, (void*)(intptr_t)mSock, MbedNetSend, MbedNetRecv, nullptr);

            int rc = 0;
            while ((rc = mbedtls_ssl_handshake(&mSsl)) != 0)
            {
                if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE)
                {
                    char buf[128] = {};
                    mbedtls_strerror(rc, buf, sizeof(buf));
                    LogError("mbedtls_ssl_handshake failed: %s", buf);
                    return false;
                }
            }
            return true;
        }

        int Send(const char* data, size_t size) override
        {
            int rc;
            do { rc = mbedtls_ssl_write(&mSsl, (const unsigned char*)data, size); }
            while (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
            return rc < 0 ? -1 : rc;
        }
        int Recv(char* buffer, size_t size) override
        {
            int rc;
            do { rc = mbedtls_ssl_read(&mSsl, (unsigned char*)buffer, size); }
            while (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
            if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return 0;
            return rc < 0 ? -1 : rc;
        }
        void Close() override
        {
            mbedtls_ssl_close_notify(&mSsl);
            if (mSock >= 0) { NET_SocketClose(mSock); mSock = -1; }
        }

    private:
        mbedtls_x509_crt*          mCa;
        mbedtls_ctr_drbg_context*  mDrbg;
        bool                       mVerify = true;
        mbedtls_ssl_context        mSsl;
        mbedtls_ssl_config         mCfg;
        SocketHandle               mSock = -1;
    };
#endif // POLYPHASE_DOLPHIN_TLS_ENABLED

    // ------------------------------------------------------------------
    // HTTP/1.1 framing — works identically over plain TCP and TLS.
    // ------------------------------------------------------------------
    bool ReadAll(ITransport& t, std::string& out, std::atomic<bool>& cancel, int64_t maxBytes, bool& tooLarge)
    {
        char buf[4096];
        while (true)
        {
            if (cancel.load(std::memory_order_acquire)) return false;
            const int n = t.Recv(buf, sizeof(buf));
            if (n < 0) return false;
            if (n == 0) return true;
            if (maxBytes > 0 && (int64_t)out.size() + (int64_t)n > maxBytes)
            {
                tooLarge = true;
                return false;
            }
            out.append(buf, (size_t)n);
        }
    }

    void TrimRight(std::string& s)
    {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
            s.pop_back();
    }

    bool ParseResponse(const std::string& raw, HttpResponse& outResponse)
    {
        const size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return false;

        const std::string headers = raw.substr(0, headerEnd);
        const std::string body    = raw.substr(headerEnd + 4);

        size_t lineStart = 0;
        bool firstLine = true;
        while (lineStart < headers.size())
        {
            size_t eol = headers.find("\r\n", lineStart);
            if (eol == std::string::npos) eol = headers.size();
            const std::string line = headers.substr(lineStart, eol - lineStart);
            lineStart = eol + 2;

            if (firstLine)
            {
                firstLine = false;
                // "HTTP/1.1 200 OK"
                size_t sp1 = line.find(' ');
                if (sp1 == std::string::npos) return false;
                size_t sp2 = line.find(' ', sp1 + 1);
                if (sp2 == std::string::npos) sp2 = line.size();
                outResponse.SetStatus(atoi(line.c_str() + sp1 + 1));
                continue;
            }

            const size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim whitespace
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
            TrimRight(value);
            outResponse.MutableHeaders().emplace(std::move(name), std::move(value));
        }

        // Body. Handle Content-Length or Transfer-Encoding: chunked.
        const auto& hdrs = outResponse.GetHeaders();
        auto teIt = hdrs.find("Transfer-Encoding");
        if (teIt != hdrs.end() && teIt->second.find("chunked") != std::string::npos)
        {
            // Parse chunked body.
            std::string& outBody = (std::string&)body; // lvalue alias
            std::vector<uint8_t>& bodyBytes = outResponse.MutableBody();
            size_t pos = 0;
            while (pos < outBody.size())
            {
                size_t crlf = outBody.find("\r\n", pos);
                if (crlf == std::string::npos) break;
                std::string sizeLine = outBody.substr(pos, crlf - pos);
                // Strip extensions after ';'
                size_t semi = sizeLine.find(';');
                if (semi != std::string::npos) sizeLine.resize(semi);
                const size_t chunkSize = (size_t)strtoul(sizeLine.c_str(), nullptr, 16);
                pos = crlf + 2;
                if (chunkSize == 0) break;
                if (pos + chunkSize > outBody.size()) break;
                const size_t before = bodyBytes.size();
                bodyBytes.resize(before + chunkSize);
                memcpy(bodyBytes.data() + before, outBody.data() + pos, chunkSize);
                pos += chunkSize + 2; // chunk + CRLF
            }
        }
        else
        {
            outResponse.MutableBody().assign(body.begin(), body.end());
        }
        return true;
    }

    // ------------------------------------------------------------------
    // Backend
    // ------------------------------------------------------------------
    class DolphinBackend : public HttpBackend
    {
    public:
        bool Initialize() override
        {
#if POLYPHASE_DOLPHIN_TLS_ENABLED
            mbedtls_x509_crt_init(&mCa);
            mbedtls_entropy_init(&mEntropy);
            mbedtls_ctr_drbg_init(&mDrbg);

            const char* pers = "polyphase-http";
            if (mbedtls_ctr_drbg_seed(&mDrbg, mbedtls_entropy_func, &mEntropy,
                                      (const unsigned char*)pers, strlen(pers)) != 0)
            {
                LogError("mbedtls_ctr_drbg_seed failed");
                mTlsReady = false;
            }
            else
            {
                // Load CA bundle from runtime asset path. On failure, HTTPS
                // requests will fall back to verify=none (still encrypted, no
                // identity check). Caller can SetVerifySsl(false) explicitly.
                Stream s;
                if (s.ReadFile("Engine/CACerts/cacert.pem", true))
                {
                    // The PEM parser wants a NUL-terminated buffer.
                    std::vector<unsigned char> pem(s.GetSize() + 1);
                    memcpy(pem.data(), s.GetData(), s.GetSize());
                    pem[s.GetSize()] = 0;
                    if (mbedtls_x509_crt_parse(&mCa, pem.data(), pem.size()) < 0)
                    {
                        LogWarning("CA bundle parse failed; HTTPS verify will be unavailable");
                    }
                }
                else
                {
                    LogWarning("CA bundle Engine/CACerts/cacert.pem not found; HTTPS verify will be unavailable");
                }
                mTlsReady = true;
            }
#endif
            mInitialized = true;
            return true;
        }

        void Shutdown() override
        {
#if POLYPHASE_DOLPHIN_TLS_ENABLED
            mbedtls_ctr_drbg_free(&mDrbg);
            mbedtls_entropy_free(&mEntropy);
            mbedtls_x509_crt_free(&mCa);
            mTlsReady = false;
#endif
            mInitialized = false;
        }

        bool        IsAvailable()                 const override { return mInitialized; }
        const char* GetMissingDependencyMessage() const override { return ""; }

        void PerformRequest(const HttpRequest& req,
                            std::atomic<bool>& cancelFlag,
                            HttpResponse& outResponse) override
        {
            ParsedUrl url = ParseUrl(req.GetUrl());
            if (!url.valid)
            {
                outResponse.SetError(HttpError::InvalidUrl, "URL parse failed");
                return;
            }
            outResponse.SetFinalUrl(req.GetUrl());

            std::unique_ptr<ITransport> t;
            if (url.scheme == "https")
            {
#if POLYPHASE_DOLPHIN_TLS_ENABLED
                if (!mTlsReady)
                {
                    outResponse.SetError(HttpError::Tls, "mbedTLS not initialized");
                    return;
                }
                t.reset(new TlsTransport(&mCa, &mDrbg, req.GetVerifySsl()));
#else
                outResponse.SetError(HttpError::Tls, "HTTPS not enabled in this build");
                return;
#endif
            }
            else if (url.scheme == "http")
            {
                t.reset(new TcpTransport());
            }
            else
            {
                outResponse.SetError(HttpError::InvalidUrl, "Only http:// and https:// schemes supported");
                return;
            }

            if (!t->Connect(url, req.GetTimeoutMs()))
            {
                outResponse.SetError(HttpError::Network, "Connect failed");
                return;
            }

            // Build the request line + headers.
            std::string reqBuf;
            reqBuf.reserve(512);
            reqBuf.append(HttpVerbToString(req.GetVerb()));
            reqBuf.push_back(' ');
            reqBuf.append(url.pathAndQuery);
            reqBuf.append(" HTTP/1.1\r\n");
            reqBuf.append("Host: "); reqBuf.append(url.host); reqBuf.append("\r\n");
            reqBuf.append("User-Agent: Polyphase/1.0\r\n");
            reqBuf.append("Connection: close\r\n");

            bool hasContentLength = false;
            for (const auto& kv : req.GetHeaders())
            {
                reqBuf.append(kv.first); reqBuf.append(": ");
                reqBuf.append(kv.second); reqBuf.append("\r\n");
                if (kv.first.size() == 14
                    && strncasecmp(kv.first.c_str(), "Content-Length", 14) == 0)
                {
                    hasContentLength = true;
                }
            }
            if (!req.GetBody().empty() && !hasContentLength)
            {
                char clbuf[64];
                snprintf(clbuf, sizeof(clbuf), "Content-Length: %u\r\n", (unsigned)req.GetBody().size());
                reqBuf.append(clbuf);
            }
            reqBuf.append("\r\n");
            if (!req.GetBody().empty())
            {
                reqBuf.append(reinterpret_cast<const char*>(req.GetBody().data()), req.GetBody().size());
            }

            if (t->Send(reqBuf.data(), reqBuf.size()) < (int)reqBuf.size())
            {
                outResponse.SetError(HttpError::Network, "send() truncated");
                return;
            }

            std::string raw;
            bool tooLarge = false;
            if (!ReadAll(*t, raw, cancelFlag, req.GetMaxBodyBytes(), tooLarge))
            {
                if (cancelFlag.load(std::memory_order_acquire))
                    outResponse.SetError(HttpError::Cancelled, "Cancelled");
                else if (tooLarge)
                    outResponse.SetError(HttpError::TooLarge, "Response exceeded MaxBodyBytes");
                else
                    outResponse.SetError(HttpError::Network, "recv() failed");
                return;
            }

            if (!ParseResponse(raw, outResponse))
            {
                outResponse.SetError(HttpError::BadResponse, "Malformed HTTP response");
                return;
            }
        }

    private:
        bool mInitialized = false;
#if POLYPHASE_DOLPHIN_TLS_ENABLED
        bool                          mTlsReady = false;
        mbedtls_x509_crt              mCa;
        mbedtls_entropy_context       mEntropy;
        mbedtls_ctr_drbg_context      mDrbg;
#endif
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new DolphinBackend());
}

#endif
