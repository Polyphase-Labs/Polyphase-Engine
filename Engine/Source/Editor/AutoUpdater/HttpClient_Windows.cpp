#if EDITOR && PLATFORM_WINDOWS

#include "HttpClient.h"
#include "Network/Http/HttpClient.h"
#include "Network/Http/HttpRequest.h"
#include "Network/Http/HttpResponse.h"
#include "Network/Http/HttpTypes.h"
#include "Log.h"

#include <Windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

bool HttpClient::sInitialized = true;
bool HttpClient::sAvailable   = true;

// `Get` / `IsAvailable` / `GetMissingDependencyMessage` are thin wrappers around
// the engine-wide `Http::` API (Network/Http/HttpClient.h), so the AutoUpdater
// no longer ships its own duplicate HTTP stack.
//
// `DownloadFile` stays on WinHTTP for now because the engine's Http buffers
// the full response in memory before invoking the callback — fine for a 100 KB
// release manifest, but not for a multi-MB installer that wants a progress bar.
// Migrating DownloadFile would require adding streaming + progress support to
// `Http::Send`, which is its own task.

bool HttpClient::InitializePlatform() { return true; }
void HttpClient::ShutdownPlatform()   {}

bool HttpClient::IsAvailable()
{
    return Http::IsAvailable();
}

std::string HttpClient::GetMissingDependencyMessage()
{
    return Http::GetMissingDependencyMessage();
}

// Helper to convert UTF-8 to wide string (used by DownloadFile below).
static std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

UpdaterHttpResponse HttpClient::Get(const std::string& url, int timeoutMs)
{
    UpdaterHttpResponse out;

    HttpRequest req(HttpVerb::Get, url);
    req.TimeoutMs(timeoutMs);
    req.Header("Accept",     "application/vnd.github.v3+json");
    req.Header("User-Agent", "PolyphaseEngine/1.0");

    HttpResponse r = Http::SendSync(std::move(req));

    out.mStatusCode = r.GetStatus();
    const auto& body = r.GetBody();
    if (!body.empty())
    {
        out.mBody.assign(reinterpret_cast<const char*>(body.data()), body.size());
    }
    if (r.GetError() != HttpError::None)
    {
        out.mError = r.GetErrorMessage().empty()
            ? HttpErrorToString(r.GetError())
            : r.GetErrorMessage();
    }
    return out;
}

bool HttpClient::DownloadFile(
    const std::string& url,
    const std::string& destPath,
    DownloadProgressCallback progressCallback,
    std::atomic<bool>& cancelFlag)
{
    // Crack the URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    std::wstring wideUrl = Utf8ToWide(url);
    if (!WinHttpCrackUrl(wideUrl.c_str(), (DWORD)wideUrl.length(), 0, &urlComp))
    {
        LogError("HttpClient: Failed to parse download URL");
        return false;
    }

    // Open session
    HINTERNET hSession = WinHttpOpen(
        L"PolyphaseEngine/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!hSession)
    {
        LogError("HttpClient: Failed to open WinHTTP session");
        return false;
    }

    // Connect
    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Failed to connect for download");
        return false;
    }

    // Open request
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);

    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Failed to open download request");
        return false;
    }

    // Add user agent
    WinHttpAddRequestHeaders(hRequest,
        L"User-Agent: PolyphaseEngine/1.0\r\n",
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Failed to send download request");
        return false;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Failed to receive download response");
        return false;
    }

    // Check status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Download returned status %d", (int)statusCode);
        return false;
    }

    // Get content length
    DWORD contentLength = 0;
    DWORD contentLengthSize = sizeof(contentLength);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &contentLength,
        &contentLengthSize,
        WINHTTP_NO_HEADER_INDEX);

    // Open output file
    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile.is_open())
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LogError("HttpClient: Failed to open output file: %s", destPath.c_str());
        return false;
    }

    // Download loop
    size_t totalDownloaded = 0;
    std::vector<char> buffer(65536); // 64KB buffer
    DWORD bytesAvailable = 0;
    bool success = true;

    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        if (cancelFlag.load())
        {
            success = false;
            break;
        }

        DWORD toRead = (DWORD)std::min((size_t)bytesAvailable, buffer.size());
        DWORD bytesRead = 0;

        if (WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead))
        {
            outFile.write(buffer.data(), bytesRead);
            totalDownloaded += bytesRead;

            if (progressCallback)
            {
                progressCallback(totalDownloaded, (size_t)contentLength);
            }
        }
        else
        {
            success = false;
            break;
        }
    }

    outFile.close();

    // Cleanup handles
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Delete incomplete file on failure
    if (!success || cancelFlag.load())
    {
        DeleteFileA(destPath.c_str());
        return false;
    }

    return true;
}

#endif // EDITOR && PLATFORM_WINDOWS
