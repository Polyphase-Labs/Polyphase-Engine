#pragma once

#include <atomic>
#include <memory>

#include "Network/Http/HttpRequest.h"
#include "Network/Http/HttpResponse.h"

// Abstract HTTP backend interface — one concrete implementation per platform
// (WinHTTP on Windows, libcurl on Linux, httpc on 3DS, libogc TCP + mbedTLS on
// Wii/GCN, stub elsewhere). Backends are blocking: the worker thread inside
// HttpClient is what makes the public API async.
class HttpBackend
{
public:
    virtual ~HttpBackend() = default;

    // Backend-wide initialization. Called once during Http::Initialize.
    // Return false to disable the backend (the stub backend will be used
    // instead, and IsAvailable() will return false).
    virtual bool Initialize() = 0;

    // Symmetric teardown.
    virtual void Shutdown() = 0;

    // True when this backend is functional in the current environment.
    // libcurl-on-Linux returns false here when libcurl isn't installed.
    virtual bool IsAvailable() const = 0;

    // Diagnostic string when IsAvailable() is false.
    virtual const char* GetMissingDependencyMessage() const { return ""; }

    // Synchronous request execution. The cancel flag is checked periodically
    // by the backend during the request loop. Backends MUST populate
    // outResponse with a status, headers, body, and on failure an HttpError.
    virtual void PerformRequest(const HttpRequest& request,
                                std::atomic<bool>& cancelFlag,
                                HttpResponse& outResponse) = 0;
};

// Factory — returns the appropriate backend for the current platform.
// Implemented per-platform: only one of the HttpBackend_*.cpp files is
// compiled into a given build.
std::unique_ptr<HttpBackend> CreatePlatformHttpBackend();
