#pragma once

#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

#include "PolyphaseAPI.h"
#include "Network/Http/HttpHandle.h"
#include "Network/Http/HttpRequest.h"
#include "Network/Http/HttpResponse.h"

using HttpResponseCallback = std::function<void(const HttpResponse&)>;

namespace Http
{
    // Lifecycle. Initialize is called from Engine::Initialize after NET_Initialize.
    POLYPHASE_API void  Initialize();
    POLYPHASE_API void  Shutdown();

    // Pumps the main-thread completion queue. Called once per frame from
    // Engine::Update so callbacks fire on the main thread (Lua-safe).
    POLYPHASE_API void  Tick();

    // Returns false on platforms with no working backend (Android, or Linux
    // without libcurl present at runtime).
    POLYPHASE_API bool  IsAvailable();

    // Diagnostic message for environments where IsAvailable() returns false
    // (e.g. "Install libcurl4 to enable HTTP support."). Empty string when
    // available.
    POLYPHASE_API const char* GetMissingDependencyMessage();

    // Async send. Callback is invoked on the main thread once the response is
    // complete (or on error). Returns a handle that can be used to Cancel().
    POLYPHASE_API HttpHandle Send(HttpRequest req, HttpResponseCallback cb);

    POLYPHASE_API HttpHandle Get   (const std::string& url, HttpResponseCallback cb);
    POLYPHASE_API HttpHandle Post  (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    POLYPHASE_API HttpHandle Put   (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    POLYPHASE_API HttpHandle Patch (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    POLYPHASE_API HttpHandle Delete(const std::string& url, HttpResponseCallback cb);

    // Convenience POST with a string body (utf-8, content-type unchanged).
    POLYPHASE_API HttpHandle PostString(const std::string& url, const std::string& body, HttpResponseCallback cb);

    // Synchronous send. Blocks the calling thread until the response is
    // available. Intended for tests/tools — DO NOT call from gameplay code or
    // the main thread.
    POLYPHASE_API HttpResponse SendSync(HttpRequest req);
}
