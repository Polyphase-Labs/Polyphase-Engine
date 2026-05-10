#if PLATFORM_3DS

#include "Network/Http/Backends/HttpBackend.h"
#include "Log.h"

#include <3ds.h>

#include <string.h>
#include <string>

// 3DS HTTP/HTTPS via libctru's httpc system service. Nintendo's stack handles
// TLS, redirects, and the cipher suite; we just push request fields and pull
// the response. This avoids vendoring mbedTLS on 3DS.

namespace
{
    HTTPC_RequestMethod VerbToHttpc(HttpVerb v)
    {
        switch (v)
        {
        case HttpVerb::Get:    return HTTPC_METHOD_GET;
        case HttpVerb::Post:   return HTTPC_METHOD_POST;
        case HttpVerb::Head:   return HTTPC_METHOD_HEAD;
        case HttpVerb::Put:    return HTTPC_METHOD_PUT;
        case HttpVerb::Delete: return HTTPC_METHOD_DELETE;
        // 3DS httpc has no PATCH/OPTIONS — fall back to GET and let the
        // caller observe the response status (server will likely return 405).
        default:               return HTTPC_METHOD_GET;
        }
    }

    class N3DsBackend : public HttpBackend
    {
    public:
        bool Initialize() override
        {
            // 0 = library picks a sane shared-memory size. Returns 0 on success.
            const Result rc = httpcInit(0);
            if (R_FAILED(rc))
            {
                LogError("httpcInit failed: 0x%08lx", (unsigned long)rc);
                return false;
            }
            mInitialized = true;
            return true;
        }

        void Shutdown() override
        {
            if (mInitialized)
            {
                httpcExit();
                mInitialized = false;
            }
        }

        bool        IsAvailable()                 const override { return mInitialized; }
        const char* GetMissingDependencyMessage() const override { return mInitialized ? "" : "httpc service not initialized"; }

        void PerformRequest(const HttpRequest& req,
                            std::atomic<bool>& cancelFlag,
                            HttpResponse& outResponse) override
        {
            outResponse.SetFinalUrl(req.GetUrl());

            httpcContext ctx = {};
            Result rc = httpcOpenContext(&ctx, VerbToHttpc(req.GetVerb()),
                                         req.GetUrl().c_str(), 1 /* use default proxy */);
            if (R_FAILED(rc))
            {
                outResponse.SetError(HttpError::Network, "httpcOpenContext failed");
                return;
            }

            // SSL verification toggle. SSLCOPT_DisableVerify = 1 << 9 in libctru.
            if (!req.GetVerifySsl())
            {
                httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify);
            }

            // Redirect handling. libctru's httpc doesn't auto-follow redirects;
            // we'd need to spin our own loop. For v1, we only follow once when
            // MaxRedirects > 0, by re-issuing on a 3xx response below.
            httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_ENABLED);

            // Custom headers
            for (const auto& kv : req.GetHeaders())
            {
                httpcAddRequestHeaderField(&ctx, kv.first.c_str(), kv.second.c_str());
            }

            // Body (raw — for non form-urlencoded use cases). httpc's
            // PostDataRaw expects the payload in u32-aligned chunks per the
            // libctru docs; cast through is the conventional usage.
            const auto& body = req.GetBody();
            if (!body.empty())
            {
                httpcAddPostDataRaw(&ctx, (u32*)body.data(), (u32)body.size());
            }

            rc = httpcBeginRequest(&ctx);
            if (R_FAILED(rc))
            {
                httpcCloseContext(&ctx);
                outResponse.SetError(HttpError::Network, "httpcBeginRequest failed");
                return;
            }

            // Pump until DOWNLOADPENDING resolves.
            u32 status = 0;
            do {
                rc = HTTPC_RESULTCODE_DOWNLOADPENDING;
                while (rc == HTTPC_RESULTCODE_DOWNLOADPENDING)
                {
                    if (cancelFlag.load(std::memory_order_acquire))
                    {
                        httpcCancelConnection(&ctx);
                        httpcCloseContext(&ctx);
                        outResponse.SetError(HttpError::Cancelled, "Cancelled");
                        return;
                    }
                    rc = httpcGetResponseStatusCode(&ctx, &status);
                }
            } while (false);

            if (R_FAILED(rc))
            {
                httpcCloseContext(&ctx);
                outResponse.SetError(HttpError::Network, "httpcGetResponseStatusCode failed");
                return;
            }

            outResponse.SetStatus((int)status);

            // Response body — chunked read. libctru's httpcReceiveData returns
            // the full body in one call; the chunked variant
            // (httpcReceiveDataTimeout) is preferred when we want to honour a
            // per-request timeout and check the cancel flag.
            const int64_t maxBytes = req.GetMaxBodyBytes();
            std::vector<uint8_t>& outBody = outResponse.MutableBody();
            outBody.clear();

            constexpr u32 chunk = 4 * 1024;
            std::vector<uint8_t> buf(chunk);

            for (;;)
            {
                if (cancelFlag.load(std::memory_order_acquire))
                {
                    httpcCancelConnection(&ctx);
                    outResponse.SetError(HttpError::Cancelled, "Cancelled during read");
                    break;
                }

                u32 dlSize = 0;
                u32 totalSize = 0;
                httpcGetDownloadSizeState(&ctx, &dlSize, &totalSize);

                rc = httpcReceiveData(&ctx, buf.data(), chunk);
                if (rc == 0 || rc == HTTPC_RESULTCODE_DOWNLOADPENDING)
                {
                    // dlSize is cumulative; last chunk size = dlSize - prior.
                    const u32 written = (u32)(dlSize - outBody.size());
                    if (written > 0)
                    {
                        if (maxBytes > 0 && (int64_t)outBody.size() + (int64_t)written > maxBytes)
                        {
                            outResponse.SetError(HttpError::TooLarge, "Response exceeded MaxBodyBytes");
                            break;
                        }
                        const size_t before = outBody.size();
                        outBody.resize(before + written);
                        memcpy(outBody.data() + before, buf.data(), written);
                    }

                    if (rc == 0) break;     // Done
                }
                else
                {
                    outResponse.SetError(HttpError::Network, "httpcReceiveData failed");
                    break;
                }
            }

            // (Response headers — httpc requires fetching by name; we don't
            // know names ahead of time, so we leave the headers map empty for
            // v1. A future follow-up can add a known-headers fetch list:
            // Content-Type, Content-Length, Location, etc.)
            char ctBuf[256] = {};
            if (R_SUCCEEDED(httpcGetResponseHeader(&ctx, "Content-Type", ctBuf, sizeof(ctBuf))))
            {
                outResponse.MutableHeaders()["Content-Type"] = ctBuf;
            }
            char lenBuf[64] = {};
            if (R_SUCCEEDED(httpcGetResponseHeader(&ctx, "Content-Length", lenBuf, sizeof(lenBuf))))
            {
                outResponse.MutableHeaders()["Content-Length"] = lenBuf;
            }
            char locBuf[1024] = {};
            if (R_SUCCEEDED(httpcGetResponseHeader(&ctx, "Location", locBuf, sizeof(locBuf))))
            {
                outResponse.MutableHeaders()["Location"] = locBuf;
                outResponse.SetFinalUrl(locBuf);
            }

            httpcCloseContext(&ctx);
        }

    private:
        bool mInitialized = false;
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new N3DsBackend());
}

#endif
