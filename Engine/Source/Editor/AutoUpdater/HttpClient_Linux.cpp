#if EDITOR && PLATFORM_LINUX

#include "HttpClient.h"
#include "Network/Http/HttpClient.h"
#include "Network/Http/HttpRequest.h"
#include "Network/Http/HttpResponse.h"
#include "Network/Http/HttpTypes.h"
#include "Log.h"

#include <dlfcn.h>
#include <fstream>
#include <cstring>

// `Get` / `IsAvailable` / `GetMissingDependencyMessage` now delegate to the
// engine-wide `Http::` API (Network/Http/HttpClient.h). The libcurl
// dlopen-and-bind machinery below is retained only for `DownloadFile`, which
// the engine's Http stack can't replace yet — it buffers the full response in
// memory before invoking the callback, so a multi-MB installer download with
// a progress bar still has to drive libcurl directly. DownloadFile lazy-loads
// libcurl on first call via InitializePlatform().

// libcurl type definitions
typedef void CURL;
typedef int CURLcode;
typedef int CURLoption;
typedef int CURLINFO;

#define CURLE_OK 0
#define CURLOPT_URL 10002
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_TIMEOUT_MS 155
#define CURLOPT_USERAGENT 10018
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_FOLLOWLOCATION 52
#define CURLOPT_NOPROGRESS 43
#define CURLOPT_XFERINFOFUNCTION 20219
#define CURLOPT_XFERINFODATA 10057
#define CURLINFO_RESPONSE_CODE 0x200002

struct curl_slist;

// Function pointer types
typedef CURL* (*curl_easy_init_func)(void);
typedef void (*curl_easy_cleanup_func)(CURL*);
typedef CURLcode (*curl_easy_setopt_func)(CURL*, CURLoption, ...);
typedef CURLcode (*curl_easy_perform_func)(CURL*);
typedef CURLcode (*curl_easy_getinfo_func)(CURL*, CURLINFO, ...);
typedef const char* (*curl_easy_strerror_func)(CURLcode);
typedef curl_slist* (*curl_slist_append_func)(curl_slist*, const char*);
typedef void (*curl_slist_free_all_func)(curl_slist*);
typedef CURLcode (*curl_global_init_func)(long);
typedef void (*curl_global_cleanup_func)(void);

// Global function pointers
static curl_easy_init_func p_curl_easy_init = nullptr;
static curl_easy_cleanup_func p_curl_easy_cleanup = nullptr;
static curl_easy_setopt_func p_curl_easy_setopt = nullptr;
static curl_easy_perform_func p_curl_easy_perform = nullptr;
static curl_easy_getinfo_func p_curl_easy_getinfo = nullptr;
static curl_easy_strerror_func p_curl_easy_strerror = nullptr;
static curl_slist_append_func p_curl_slist_append = nullptr;
static curl_slist_free_all_func p_curl_slist_free_all = nullptr;
static curl_global_init_func p_curl_global_init = nullptr;
static curl_global_cleanup_func p_curl_global_cleanup = nullptr;

static void* sCurlHandle = nullptr;

bool HttpClient::sInitialized = false;
bool HttpClient::sAvailable = false;

bool HttpClient::InitializePlatform()
{
    if (sInitialized)
    {
        return sAvailable;
    }

    sInitialized = true;
    sAvailable = false;

    // Try to load libcurl
    sCurlHandle = dlopen("libcurl.so.4", RTLD_LAZY);
    if (!sCurlHandle)
    {
        // Try alternate names
        sCurlHandle = dlopen("libcurl.so", RTLD_LAZY);
    }
    if (!sCurlHandle)
    {
        sCurlHandle = dlopen("libcurl-gnutls.so.4", RTLD_LAZY);
    }

    if (!sCurlHandle)
    {
        LogWarning("HttpClient: libcurl not found. Auto-updates disabled.");
        LogWarning("HttpClient: Install with: sudo apt install libcurl4");
        return false;
    }

    // Load function pointers
    p_curl_easy_init = (curl_easy_init_func)dlsym(sCurlHandle, "curl_easy_init");
    p_curl_easy_cleanup = (curl_easy_cleanup_func)dlsym(sCurlHandle, "curl_easy_cleanup");
    p_curl_easy_setopt = (curl_easy_setopt_func)dlsym(sCurlHandle, "curl_easy_setopt");
    p_curl_easy_perform = (curl_easy_perform_func)dlsym(sCurlHandle, "curl_easy_perform");
    p_curl_easy_getinfo = (curl_easy_getinfo_func)dlsym(sCurlHandle, "curl_easy_getinfo");
    p_curl_easy_strerror = (curl_easy_strerror_func)dlsym(sCurlHandle, "curl_easy_strerror");
    p_curl_slist_append = (curl_slist_append_func)dlsym(sCurlHandle, "curl_slist_append");
    p_curl_slist_free_all = (curl_slist_free_all_func)dlsym(sCurlHandle, "curl_slist_free_all");
    p_curl_global_init = (curl_global_init_func)dlsym(sCurlHandle, "curl_global_init");
    p_curl_global_cleanup = (curl_global_cleanup_func)dlsym(sCurlHandle, "curl_global_cleanup");

    if (!p_curl_easy_init || !p_curl_easy_cleanup || !p_curl_easy_setopt ||
        !p_curl_easy_perform || !p_curl_easy_getinfo)
    {
        LogError("HttpClient: Failed to load libcurl functions");
        dlclose(sCurlHandle);
        sCurlHandle = nullptr;
        return false;
    }

    // Initialize curl
    if (p_curl_global_init)
    {
        p_curl_global_init(3); // CURL_GLOBAL_ALL
    }

    sAvailable = true;
    LogDebug("HttpClient: libcurl loaded successfully");
    return true;
}

void HttpClient::ShutdownPlatform()
{
    if (sCurlHandle)
    {
        if (p_curl_global_cleanup)
        {
            p_curl_global_cleanup();
        }
        dlclose(sCurlHandle);
        sCurlHandle = nullptr;
    }
    sInitialized = false;
    sAvailable = false;
}

bool HttpClient::IsAvailable()
{
    return Http::IsAvailable();
}

std::string HttpClient::GetMissingDependencyMessage()
{
    return Http::GetMissingDependencyMessage();
}

// Write callback for file download
struct FileWriteData
{
    std::ofstream* file;
    size_t* downloaded;
    size_t total;
    DownloadProgressCallback* callback;
    std::atomic<bool>* cancelFlag;
};

static size_t FileWriteCallback(void* contents, size_t size, size_t nmemb, FileWriteData* data)
{
    if (data->cancelFlag && data->cancelFlag->load())
    {
        return 0; // Abort
    }

    size_t totalSize = size * nmemb;
    data->file->write((char*)contents, totalSize);
    *(data->downloaded) += totalSize;

    if (data->callback && *data->callback)
    {
        (*data->callback)(*(data->downloaded), data->total);
    }

    return totalSize;
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
    // IsAvailable() now delegates to the engine, so it no longer lazy-loads
    // AutoUpdater's private libcurl handle. Do it explicitly here.
    if (!sInitialized)
    {
        InitializePlatform();
    }
    if (!sAvailable || sCurlHandle == nullptr)
    {
        LogError("HttpClient: libcurl not loaded — install libcurl4 to enable downloads");
        return false;
    }

    CURL* curl = p_curl_easy_init();
    if (!curl)
    {
        LogError("HttpClient: Failed to initialize curl for download");
        return false;
    }

    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile.is_open())
    {
        p_curl_easy_cleanup(curl);
        LogError("HttpClient: Failed to open output file: %s", destPath.c_str());
        return false;
    }

    size_t downloaded = 0;
    FileWriteData writeData;
    writeData.file = &outFile;
    writeData.downloaded = &downloaded;
    writeData.total = 0;
    writeData.callback = &progressCallback;
    writeData.cancelFlag = &cancelFlag;

    p_curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    p_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void*)FileWriteCallback);
    p_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
    p_curl_easy_setopt(curl, CURLOPT_USERAGENT, "PolyphaseEngine/1.0");
    p_curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    p_curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    CURLcode res = p_curl_easy_perform(curl);
    outFile.close();

    bool success = (res == CURLE_OK);

    if (!success)
    {
        if (p_curl_easy_strerror)
        {
            LogError("HttpClient: Download failed: %s", p_curl_easy_strerror(res));
        }
    }

    // Check status code
    if (success)
    {
        long httpCode = 0;
        p_curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        if (httpCode != 200)
        {
            LogError("HttpClient: Download returned status %ld", httpCode);
            success = false;
        }
    }

    p_curl_easy_cleanup(curl);

    // Delete incomplete file on failure
    if (!success || cancelFlag.load())
    {
        std::remove(destPath.c_str());
        return false;
    }

    return true;
}

#endif // EDITOR && PLATFORM_LINUX
