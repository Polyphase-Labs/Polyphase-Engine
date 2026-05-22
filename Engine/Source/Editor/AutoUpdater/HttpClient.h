#pragma once

#if EDITOR

#include <string>
#include <functional>
#include <atomic>

/**
 * @brief HTTP response from a GET request.
 *
 * Distinct from engine-wide HttpResponse (Network/Http/HttpResponse.h). Both
 * lived in the global namespace under the same name — a textbook ODR
 * violation that caused heap corruption in DLL builds where the linker
 * COMDAT-folded the two implicit destructors. Renamed to keep this
 * self-contained class private to AutoUpdater.
 */
struct UpdaterHttpResponse
{
    int mStatusCode = 0;
    std::string mBody;
    std::string mError;

    bool IsSuccess() const
    {
        return mStatusCode >= 200 && mStatusCode < 300;
    }
};

/**
 * @brief Callback for download progress updates.
 * @param downloaded Bytes downloaded so far
 * @param total Total bytes to download (0 if unknown)
 */
using DownloadProgressCallback = std::function<void(size_t downloaded, size_t total)>;

/**
 * @brief Platform-agnostic HTTP client for update checking and downloading.
 *
 * Windows: Uses WinHTTP (built-in)
 * Linux: Uses libcurl via dlopen (optional dependency)
 */
class HttpClient
{
public:
    /**
     * @brief Check if HTTP functionality is available.
     * On Linux, this returns false if libcurl is not installed.
     * @return true if HTTP requests can be made
     */
    static bool IsAvailable();

    /**
     * @brief Get a message describing missing dependencies.
     * @return Install instructions if dependencies are missing, empty string otherwise
     */
    static std::string GetMissingDependencyMessage();

    /**
     * @brief Perform a synchronous HTTP GET request.
     * @param url The URL to fetch
     * @param timeoutMs Timeout in milliseconds (default 10 seconds)
     * @return HTTP response with status code, body, and any error message
     */
    static UpdaterHttpResponse Get(const std::string& url, int timeoutMs = 10000);

    /**
     * @brief Download a file from a URL to disk.
     * @param url The URL to download from
     * @param destPath Local file path to save to
     * @param progressCallback Called periodically with download progress
     * @param cancelFlag Atomic bool that can be set to true to cancel the download
     * @return true if download completed successfully
     */
    static bool DownloadFile(
        const std::string& url,
        const std::string& destPath,
        DownloadProgressCallback progressCallback,
        std::atomic<bool>& cancelFlag);

private:
    // Platform-specific initialization
    static bool InitializePlatform();
    static void ShutdownPlatform();

    static bool sInitialized;
    static bool sAvailable;
};

#endif // EDITOR
