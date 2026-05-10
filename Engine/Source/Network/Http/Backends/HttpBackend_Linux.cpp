#if PLATFORM_LINUX

#include "Network/Http/Backends/HttpBackend.h"
#include "Log.h"

#include <dlfcn.h>
#include <string.h>
#include <string>

// libcurl is loaded via dlopen so we don't take a hard link dependency.
// Polyphase doesn't ship libcurl; gameplay code that wants HTTP gets it
// through the host's installed libcurl4 package.

namespace
{
    // ---- libcurl C API mirror (only what we use) ------------------------
    typedef void  CURL;
    typedef int   CURLcode;
    typedef int   CURLoption;
    typedef int   CURLINFO;

    // Subset of CURLcode
    constexpr int CURLE_OK = 0;

    // CURLOPT_* values (taken from curl.h; ABI-stable)
    constexpr int CURLOPT_URL                  = 10002;
    constexpr int CURLOPT_WRITEFUNCTION        = 20011;
    constexpr int CURLOPT_WRITEDATA            = 10001;
    constexpr int CURLOPT_HEADERFUNCTION       = 20079;
    constexpr int CURLOPT_HEADERDATA           = 10029;
    constexpr int CURLOPT_TIMEOUT_MS           =   155;
    constexpr int CURLOPT_CONNECTTIMEOUT_MS    =   156;
    constexpr int CURLOPT_USERAGENT            = 10018;
    constexpr int CURLOPT_HTTPHEADER           = 10023;
    constexpr int CURLOPT_FOLLOWLOCATION       =    52;
    constexpr int CURLOPT_MAXREDIRS            =    68;
    constexpr int CURLOPT_NOPROGRESS           =    43;
    constexpr int CURLOPT_XFERINFOFUNCTION     = 20219;
    constexpr int CURLOPT_XFERINFODATA         = 10057;
    constexpr int CURLOPT_CUSTOMREQUEST        = 10036;
    constexpr int CURLOPT_POSTFIELDS           = 10015;
    constexpr int CURLOPT_POSTFIELDSIZE_LARGE  = 30120;
    constexpr int CURLOPT_NOBODY               =    44;
    constexpr int CURLOPT_SSL_VERIFYPEER       =    64;
    constexpr int CURLOPT_SSL_VERIFYHOST       =    81;
    constexpr int CURLOPT_POST                 =    47;

    constexpr int CURLINFO_RESPONSE_CODE       = 0x200002;
    constexpr int CURLINFO_EFFECTIVE_URL       = 0x100001;

    struct curl_slist;

    using curl_easy_init_t        = CURL*       (*)();
    using curl_easy_cleanup_t     = void        (*)(CURL*);
    using curl_easy_setopt_t      = CURLcode    (*)(CURL*, CURLoption, ...);
    using curl_easy_perform_t     = CURLcode    (*)(CURL*);
    using curl_easy_getinfo_t     = CURLcode    (*)(CURL*, CURLINFO, ...);
    using curl_easy_strerror_t    = const char* (*)(CURLcode);
    using curl_slist_append_t     = curl_slist* (*)(curl_slist*, const char*);
    using curl_slist_free_all_t   = void        (*)(curl_slist*);
    using curl_global_init_t      = CURLcode    (*)(long);
    using curl_global_cleanup_t   = void        (*)();

    struct CurlAPI
    {
        void* lib = nullptr;
        curl_easy_init_t      easy_init      = nullptr;
        curl_easy_cleanup_t   easy_cleanup   = nullptr;
        curl_easy_setopt_t    easy_setopt    = nullptr;
        curl_easy_perform_t   easy_perform   = nullptr;
        curl_easy_getinfo_t   easy_getinfo   = nullptr;
        curl_easy_strerror_t  easy_strerror  = nullptr;
        curl_slist_append_t   slist_append   = nullptr;
        curl_slist_free_all_t slist_free_all = nullptr;
        curl_global_init_t    global_init    = nullptr;
        curl_global_cleanup_t global_cleanup = nullptr;
        bool                  available      = false;
    };

    bool LoadCurl(CurlAPI& api)
    {
        const char* names[] = {
            "libcurl.so.4",
            "libcurl.so",
            "libcurl-gnutls.so.4",
            "libcurl.so.3"
        };
        for (const char* n : names)
        {
            api.lib = dlopen(n, RTLD_LAZY);
            if (api.lib != nullptr) break;
        }
        if (api.lib == nullptr) return false;

        api.easy_init      = (curl_easy_init_t)      dlsym(api.lib, "curl_easy_init");
        api.easy_cleanup   = (curl_easy_cleanup_t)   dlsym(api.lib, "curl_easy_cleanup");
        api.easy_setopt    = (curl_easy_setopt_t)    dlsym(api.lib, "curl_easy_setopt");
        api.easy_perform   = (curl_easy_perform_t)   dlsym(api.lib, "curl_easy_perform");
        api.easy_getinfo   = (curl_easy_getinfo_t)   dlsym(api.lib, "curl_easy_getinfo");
        api.easy_strerror  = (curl_easy_strerror_t)  dlsym(api.lib, "curl_easy_strerror");
        api.slist_append   = (curl_slist_append_t)   dlsym(api.lib, "curl_slist_append");
        api.slist_free_all = (curl_slist_free_all_t) dlsym(api.lib, "curl_slist_free_all");
        api.global_init    = (curl_global_init_t)    dlsym(api.lib, "curl_global_init");
        api.global_cleanup = (curl_global_cleanup_t) dlsym(api.lib, "curl_global_cleanup");

        if (api.easy_init == nullptr || api.easy_cleanup == nullptr
            || api.easy_setopt == nullptr || api.easy_perform == nullptr
            || api.easy_getinfo == nullptr || api.slist_append == nullptr)
        {
            dlclose(api.lib);
            api.lib = nullptr;
            return false;
        }

        if (api.global_init != nullptr) api.global_init(3 /* CURL_GLOBAL_ALL */);
        api.available = true;
        return true;
    }

    struct WriteBuffer
    {
        std::vector<uint8_t>* body = nullptr;
        int64_t maxBytes = 0;
        bool tooLarge = false;
    };

    struct HeaderBuffer
    {
        HttpHeaderMap* headers = nullptr;
    };

    size_t WriteCb(void* ptr, size_t size, size_t nmemb, void* user)
    {
        WriteBuffer* w = static_cast<WriteBuffer*>(user);
        const size_t n = size * nmemb;
        if (w->maxBytes > 0 && (int64_t)w->body->size() + (int64_t)n > w->maxBytes)
        {
            w->tooLarge = true;
            return 0;
        }
        const size_t before = w->body->size();
        w->body->resize(before + n);
        memcpy(w->body->data() + before, ptr, n);
        return n;
    }

    size_t HeaderCb(void* ptr, size_t size, size_t nmemb, void* user)
    {
        HeaderBuffer* h = static_cast<HeaderBuffer*>(user);
        const size_t n = size * nmemb;
        const char* line = (const char*)ptr;

        // Headers come line-by-line, including a trailing "\r\n". Skip the
        // status line and any continuation/empty lines.
        size_t len = n;
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) --len;
        if (len == 0) return n;

        const char* end = line + len;
        const char* colon = (const char*)memchr(line, ':', len);
        if (colon == nullptr) return n;   // status line or malformed

        std::string name(line, colon);
        const char* val = colon + 1;
        while (val < end && (*val == ' ' || *val == '\t')) ++val;
        std::string value(val, end);

        auto it = h->headers->find(name);
        if (it != h->headers->end()) { it->second.append(", "); it->second.append(value); }
        else                          { h->headers->emplace(std::move(name), std::move(value)); }
        return n;
    }

    int XferInfoCb(void* user, long /*dltotal*/, long /*dlnow*/, long /*ultotal*/, long /*ulnow*/)
    {
        std::atomic<bool>* cancel = static_cast<std::atomic<bool>*>(user);
        return cancel != nullptr && cancel->load(std::memory_order_acquire) ? 1 : 0;
    }

    class LinuxBackend : public HttpBackend
    {
    public:
        bool Initialize() override
        {
            return LoadCurl(mCurl);
        }

        void Shutdown() override
        {
            if (mCurl.lib != nullptr)
            {
                if (mCurl.global_cleanup != nullptr) mCurl.global_cleanup();
                dlclose(mCurl.lib);
                mCurl.lib = nullptr;
                mCurl.available = false;
            }
        }

        bool        IsAvailable()                 const override { return mCurl.available; }
        const char* GetMissingDependencyMessage() const override
        {
            return mCurl.available ? "" : "libcurl4 is required for HTTP support. Install with: sudo apt install libcurl4";
        }

        void PerformRequest(const HttpRequest& req,
                            std::atomic<bool>& cancelFlag,
                            HttpResponse& outResponse) override
        {
            outResponse.SetFinalUrl(req.GetUrl());

            CURL* curl = mCurl.easy_init();
            if (curl == nullptr)
            {
                outResponse.SetError(HttpError::Network, "curl_easy_init failed");
                return;
            }

            WriteBuffer  writeBuf;  writeBuf.body = &outResponse.MutableBody(); writeBuf.maxBytes = req.GetMaxBodyBytes();
            HeaderBuffer headerBuf; headerBuf.headers = &outResponse.MutableHeaders();

            mCurl.easy_setopt(curl, CURLOPT_URL,             req.GetUrl().c_str());
            mCurl.easy_setopt(curl, CURLOPT_WRITEFUNCTION,   (void*)&WriteCb);
            mCurl.easy_setopt(curl, CURLOPT_WRITEDATA,       (void*)&writeBuf);
            mCurl.easy_setopt(curl, CURLOPT_HEADERFUNCTION,  (void*)&HeaderCb);
            mCurl.easy_setopt(curl, CURLOPT_HEADERDATA,      (void*)&headerBuf);
            mCurl.easy_setopt(curl, CURLOPT_TIMEOUT_MS,      (long)req.GetTimeoutMs());
            mCurl.easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,(long)req.GetTimeoutMs());
            mCurl.easy_setopt(curl, CURLOPT_USERAGENT,       "Polyphase/1.0");
            mCurl.easy_setopt(curl, CURLOPT_NOPROGRESS,      (long)0);
            mCurl.easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,(void*)&XferInfoCb);
            mCurl.easy_setopt(curl, CURLOPT_XFERINFODATA,    (void*)&cancelFlag);

            if (req.GetMaxRedirects() > 0)
            {
                mCurl.easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)1);
                mCurl.easy_setopt(curl, CURLOPT_MAXREDIRS,      (long)req.GetMaxRedirects());
            }
            else
            {
                mCurl.easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)0);
            }

            if (!req.GetVerifySsl())
            {
                mCurl.easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long)0);
                mCurl.easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (long)0);
            }

            // Verb selection.
            switch (req.GetVerb())
            {
            case HttpVerb::Get:
                // Default — no opt needed.
                break;
            case HttpVerb::Head:
                mCurl.easy_setopt(curl, CURLOPT_NOBODY, (long)1);
                break;
            case HttpVerb::Post:
                mCurl.easy_setopt(curl, CURLOPT_POST, (long)1);
                break;
            case HttpVerb::Put:
            case HttpVerb::Patch:
            case HttpVerb::Delete:
            case HttpVerb::Options:
                mCurl.easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HttpVerbToString(req.GetVerb()));
                break;
            default: break;
            }

            // Body
            const auto& body = req.GetBody();
            if (!body.empty())
            {
                mCurl.easy_setopt(curl, CURLOPT_POSTFIELDS,           (const void*)body.data());
                mCurl.easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,  (long long)body.size());
            }

            // Headers
            curl_slist* headers = nullptr;
            for (const auto& kv : req.GetHeaders())
            {
                std::string line = kv.first + ": " + kv.second;
                headers = mCurl.slist_append(headers, line.c_str());
            }
            if (headers != nullptr)
            {
                mCurl.easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }

            const CURLcode rc = mCurl.easy_perform(curl);

            if (rc == CURLE_OK)
            {
                long status = 0;
                mCurl.easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                outResponse.SetStatus((int)status);

                char* eff = nullptr;
                mCurl.easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff);
                if (eff != nullptr) outResponse.SetFinalUrl(eff);
            }
            else
            {
                if (writeBuf.tooLarge)
                {
                    outResponse.SetError(HttpError::TooLarge, "Response exceeded MaxBodyBytes");
                }
                else if (cancelFlag.load(std::memory_order_acquire))
                {
                    outResponse.SetError(HttpError::Cancelled, "Cancelled");
                }
                else
                {
                    const char* msg = mCurl.easy_strerror != nullptr ? mCurl.easy_strerror(rc) : "curl error";
                    outResponse.SetError(HttpError::Network, msg != nullptr ? msg : "");
                }
            }

            if (headers != nullptr && mCurl.slist_free_all != nullptr)
            {
                mCurl.slist_free_all(headers);
            }
            mCurl.easy_cleanup(curl);
        }

    private:
        CurlAPI mCurl;
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new LinuxBackend());
}

#endif
