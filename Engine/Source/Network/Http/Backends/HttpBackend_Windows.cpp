#if PLATFORM_WINDOWS

#include "Network/Http/Backends/HttpBackend.h"
#include "Log.h"

#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#include <string>
#include <vector>

namespace
{
    // Convert a UTF-8 std::string to a wide string (WinHTTP wants wchar_t).
    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty()) return std::wstring();
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
        return out;
    }

    std::string WideToUtf8(const wchar_t* w, int len)
    {
        if (w == nullptr || len <= 0) return std::string();
        int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, len, out.data(), n, nullptr, nullptr);
        return out;
    }

    const wchar_t* VerbToWide(HttpVerb v)
    {
        switch (v)
        {
        case HttpVerb::Get:     return L"GET";
        case HttpVerb::Post:    return L"POST";
        case HttpVerb::Put:     return L"PUT";
        case HttpVerb::Patch:   return L"PATCH";
        case HttpVerb::Delete:  return L"DELETE";
        case HttpVerb::Head:    return L"HEAD";
        case HttpVerb::Options: return L"OPTIONS";
        default:                return L"GET";
        }
    }

    void ParseHeaderBlock(const std::wstring& raw, HttpHeaderMap& outHeaders, std::string& outFinalUrl)
    {
        // Raw header block from WinHTTP is "\0"-terminated lines separated by "\r\n".
        // First line is the status line ("HTTP/1.1 200 OK"); subsequent lines are
        // "Name: Value". We skip the status line and parse the rest.
        const std::string utf8 = WideToUtf8(raw.c_str(), (int)raw.size());

        size_t pos = 0;
        bool first = true;
        while (pos < utf8.size())
        {
            size_t eol = utf8.find("\r\n", pos);
            if (eol == std::string::npos) eol = utf8.size();
            const std::string line = utf8.substr(pos, eol - pos);
            pos = eol + 2;
            if (line.empty()) { first = false; continue; }
            if (first) { first = false; continue; }

            const size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string name  = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading whitespace in value
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
            while (!value.empty() && (value.back()  == ' ' || value.back()  == '\t')) value.pop_back();

            // Append (multi-value headers like Set-Cookie) by joining with ", ".
            auto it = outHeaders.find(name);
            if (it != outHeaders.end()) { it->second.append(", "); it->second.append(value); }
            else                         { outHeaders.emplace(std::move(name), std::move(value)); }
        }

        (void)outFinalUrl;
    }

    class WindowsBackend : public HttpBackend
    {
    public:
        bool Initialize() override
        {
            mSession = WinHttpOpen(
                L"Polyphase/1.0",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (mSession == nullptr)
            {
                LogError("WinHttpOpen failed: %lu", GetLastError());
                return false;
            }
            return true;
        }

        void Shutdown() override
        {
            if (mSession != nullptr)
            {
                WinHttpCloseHandle(mSession);
                mSession = nullptr;
            }
        }

        bool        IsAvailable()                 const override { return mSession != nullptr; }
        const char* GetMissingDependencyMessage() const override { return mSession != nullptr ? "" : "WinHTTP session not initialized"; }

        void PerformRequest(const HttpRequest& req,
                            std::atomic<bool>& cancelFlag,
                            HttpResponse& outResponse) override
        {
            outResponse.SetFinalUrl(req.GetUrl());

            std::wstring wurl = Utf8ToWide(req.GetUrl());

            URL_COMPONENTS uc = {};
            uc.dwStructSize     = sizeof(uc);
            wchar_t hostBuf[256] = {};
            wchar_t pathBuf[2048] = {};
            uc.lpszHostName     = hostBuf;  uc.dwHostNameLength     = 256;
            uc.lpszUrlPath      = pathBuf;  uc.dwUrlPathLength      = 2048;
            uc.lpszExtraInfo    = nullptr;  uc.dwExtraInfoLength    = (DWORD)-1;

            if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
            {
                outResponse.SetError(HttpError::InvalidUrl, "WinHttpCrackUrl failed");
                return;
            }

            const bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
            const INTERNET_PORT port = uc.nPort != 0 ? uc.nPort : (isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

            HINTERNET hConn = WinHttpConnect(mSession, hostBuf, port, 0);
            if (hConn == nullptr)
            {
                outResponse.SetError(HttpError::Network, "WinHttpConnect failed");
                return;
            }

            DWORD reqFlags = 0;
            if (isHttps) reqFlags |= WINHTTP_FLAG_SECURE;

            // Build path + query (CrackUrl gives us a contiguous chunk).
            std::wstring fullPath = pathBuf;
            // If extra info (?query) wasn't merged, append it.
            if (uc.lpszExtraInfo != nullptr && uc.dwExtraInfoLength > 0)
            {
                fullPath.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
            }

            HINTERNET hReq = WinHttpOpenRequest(
                hConn,
                VerbToWide(req.GetVerb()),
                fullPath.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                reqFlags);
            if (hReq == nullptr)
            {
                WinHttpCloseHandle(hConn);
                outResponse.SetError(HttpError::Network, "WinHttpOpenRequest failed");
                return;
            }

            // Apply timeouts (resolve, connect, send, receive).
            const int t = req.GetTimeoutMs();
            WinHttpSetTimeouts(hReq, t, t, t, t);

            // Redirect policy.
            DWORD redirectPolicy = (req.GetMaxRedirects() > 0)
                ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
                : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            // SSL verification toggle.
            if (!req.GetVerifySsl() && isHttps)
            {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                               | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }

            // Build the header block.
            std::wstring headerBlock;
            for (const auto& kv : req.GetHeaders())
            {
                headerBlock.append(Utf8ToWide(kv.first));
                headerBlock.append(L": ");
                headerBlock.append(Utf8ToWide(kv.second));
                headerBlock.append(L"\r\n");
            }

            // Body
            const auto& body = req.GetBody();
            const DWORD bodyLen = (DWORD)body.size();
            LPVOID bodyPtr = bodyLen > 0 ? const_cast<uint8_t*>(body.data()) : WINHTTP_NO_REQUEST_DATA;

            BOOL sent = WinHttpSendRequest(
                hReq,
                headerBlock.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBlock.c_str(),
                headerBlock.empty() ? 0 : (DWORD)-1,
                bodyPtr,
                bodyLen,
                bodyLen,
                0);

            if (!sent)
            {
                const DWORD err = GetLastError();
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConn);
                outResponse.SetError(HttpError::Network, "WinHttpSendRequest failed");
                outResponse.SetStatus(0);
                LogWarning("WinHttpSendRequest err=%lu", err);
                return;
            }

            if (!WinHttpReceiveResponse(hReq, nullptr))
            {
                WinHttpCloseHandle(hReq);
                WinHttpCloseHandle(hConn);
                outResponse.SetError(HttpError::Network, "WinHttpReceiveResponse failed");
                return;
            }

            // Status code.
            DWORD status = 0;
            DWORD statusLen = sizeof(status);
            WinHttpQueryHeaders(
                hReq,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statusLen,
                WINHTTP_NO_HEADER_INDEX);
            outResponse.SetStatus((int)status);

            // Response headers (raw, parse into map).
            DWORD rawSize = 0;
            WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &rawSize, WINHTTP_NO_HEADER_INDEX);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && rawSize > 0)
            {
                std::wstring raw(rawSize / sizeof(wchar_t), L'\0');
                if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &rawSize, WINHTTP_NO_HEADER_INDEX))
                {
                    std::string finalUrl;
                    ParseHeaderBlock(raw, outResponse.MutableHeaders(), finalUrl);
                }
            }

            // Final URL after redirects.
            DWORD urlSize = 0;
            WinHttpQueryOption(hReq, WINHTTP_OPTION_URL, nullptr, &urlSize);
            if (urlSize > 0)
            {
                std::wstring finalW(urlSize / sizeof(wchar_t), L'\0');
                if (WinHttpQueryOption(hReq, WINHTTP_OPTION_URL, finalW.data(), &urlSize))
                {
                    if (!finalW.empty() && finalW.back() == L'\0') finalW.pop_back();
                    outResponse.SetFinalUrl(WideToUtf8(finalW.c_str(), (int)finalW.size()));
                }
            }

            // Body stream.
            std::vector<uint8_t>& outBody = outResponse.MutableBody();
            outBody.clear();

            const int64_t maxBytes = req.GetMaxBodyBytes();

            for (;;)
            {
                if (cancelFlag.load(std::memory_order_acquire))
                {
                    outResponse.SetError(HttpError::Cancelled, "Cancelled during read");
                    break;
                }

                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(hReq, &avail))
                {
                    outResponse.SetError(HttpError::Network, "WinHttpQueryDataAvailable failed");
                    break;
                }
                if (avail == 0) break;

                if (maxBytes > 0 && (int64_t)outBody.size() + (int64_t)avail > maxBytes)
                {
                    outResponse.SetError(HttpError::TooLarge, "Response exceeded MaxBodyBytes");
                    break;
                }

                const size_t before = outBody.size();
                outBody.resize(before + avail);
                DWORD readN = 0;
                if (!WinHttpReadData(hReq, outBody.data() + before, avail, &readN))
                {
                    outResponse.SetError(HttpError::Network, "WinHttpReadData failed");
                    outBody.resize(before);
                    break;
                }
                outBody.resize(before + readN);
                if (readN == 0) break;
            }

            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
        }

    private:
        HINTERNET mSession = nullptr;
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new WindowsBackend());
}

#endif
