#include "Network/Http/HttpRequest.h"

#include <cctype>

HttpRequest::HttpRequest() = default;
HttpRequest::HttpRequest(HttpVerb verb, std::string url)
    : mVerb(verb), mUrl(std::move(url)) {}
HttpRequest::~HttpRequest() = default;
HttpRequest::HttpRequest(const HttpRequest&) = default;
HttpRequest::HttpRequest(HttpRequest&&) noexcept = default;
HttpRequest& HttpRequest::operator=(const HttpRequest&) = default;
HttpRequest& HttpRequest::operator=(HttpRequest&&) noexcept = default;

bool HttpHeaderLess::operator()(const std::string& a, const std::string& b) const
{
    const size_t na = a.size();
    const size_t nb = b.size();
    const size_t n  = na < nb ? na : nb;
    for (size_t i = 0; i < n; ++i)
    {
        const unsigned char ca = (unsigned char)std::tolower((unsigned char)a[i]);
        const unsigned char cb = (unsigned char)std::tolower((unsigned char)b[i]);
        if (ca != cb) return ca < cb;
    }
    return na < nb;
}
