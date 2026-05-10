#pragma once

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include "PolyphaseAPI.h"
#include "Network/Http/HttpTypes.h"

// Case-insensitive comparator for header keys (HTTP/1.1 header names are case-insensitive).
struct POLYPHASE_API HttpHeaderLess
{
    bool operator()(const std::string& a, const std::string& b) const;
};

using HttpHeaderMap = std::map<std::string, std::string, HttpHeaderLess>;

class POLYPHASE_API HttpRequest
{
public:
    HttpRequest() = default;
    HttpRequest(HttpVerb verb, std::string url) : mVerb(verb), mUrl(std::move(url)) {}

    // Builder-style mutators. Each returns *this for chaining.
    HttpRequest& Verb(HttpVerb v)                              { mVerb = v;                                 return *this; }
    HttpRequest& Url(std::string u)                            { mUrl = std::move(u);                       return *this; }
    HttpRequest& Header(std::string k, std::string v)          { mHeaders[std::move(k)] = std::move(v);     return *this; }
    HttpRequest& Body(const std::string& s)                    { mBody.assign(s.begin(), s.end());          return *this; }
    HttpRequest& Body(std::vector<uint8_t> b)                  { mBody = std::move(b);                      return *this; }
    HttpRequest& Body(const uint8_t* data, size_t size)        { mBody.assign(data, data + size);           return *this; }
    HttpRequest& TimeoutMs(int32_t ms)                         { mTimeoutMs = ms;                           return *this; }
    HttpRequest& MaxRedirects(int32_t n)                       { mMaxRedirects = n;                         return *this; }
    HttpRequest& MaxBodyBytes(int64_t n)                       { mMaxBodyBytes = n;                         return *this; }
    HttpRequest& VerifySsl(bool v)                             { mVerifySsl = v;                            return *this; }

    HttpVerb               GetVerb()         const { return mVerb;         }
    const std::string&     GetUrl()          const { return mUrl;          }
    const HttpHeaderMap&   GetHeaders()      const { return mHeaders;      }
    const std::vector<uint8_t>& GetBody()    const { return mBody;         }
    int32_t                GetTimeoutMs()    const { return mTimeoutMs;    }
    int32_t                GetMaxRedirects() const { return mMaxRedirects; }
    int64_t                GetMaxBodyBytes() const { return mMaxBodyBytes; }
    bool                   GetVerifySsl()    const { return mVerifySsl;    }

private:
    HttpVerb              mVerb         = HttpVerb::Get;
    std::string           mUrl;
    HttpHeaderMap         mHeaders;
    std::vector<uint8_t>  mBody;
    int32_t               mTimeoutMs    = 10000;
    int32_t               mMaxRedirects = 5;
    int64_t               mMaxBodyBytes = 64ll * 1024 * 1024;   // 64 MiB
    bool                  mVerifySsl    = true;
};
