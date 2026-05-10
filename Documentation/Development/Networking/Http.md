# HTTP / REST Client

Polyphase ships a runtime-callable HTTP/HTTPS client at `Engine/Source/Network/Http/`. It's the same API everywhere — C++, Lua, and addons all see one client. Async by default with synchronous escape hatch, full verb set, custom headers, request and response bodies, redirects, cancellation, per-request timeouts, response→`Texture`/`SoundWave` decoders, and built-in JSON parsing.

If you're writing Lua, jump to the [Lua reference](../../Lua/Networking/Http.md).

## Quick start

```cpp
#include "Network/Http/HttpClient.h"

Http::Get("https://api.example.com/status", [](const HttpResponse& r) {
    if (r.IsSuccess())
        LogDebug("HTTP %d, %zu bytes", r.GetStatus(), r.GetBody().size());
    else
        LogWarning("Request failed: %s", r.GetErrorMessage().c_str());
});
```

The callback runs **on the main thread** during `Http::Tick()`. You can touch any engine state from inside it without locking.

## Platforms and TLS

| Platform        | Backend                | TLS                          | Notes                                                                                                              |
|-----------------|-----------------------|------------------------------|--------------------------------------------------------------------------------------------------------------------|
| Windows         | WinHTTP                | SChannel (system)            | No external dependency. Built into the OS.                                                                         |
| Linux           | libcurl via `dlopen`   | system OpenSSL via libcurl   | `IsAvailable()` returns false if `libcurl.so.4` isn't installed (`sudo apt install libcurl4`).                     |
| 3DS             | libctru `httpc`        | Nintendo system stack        | Already linked via `-lctru`. No vendoring.                                                                         |
| Wii / GameCube  | libogc TCP + mbedTLS   | vendored mbedTLS             | Requires `External/mbedtls/lib-{wii,gcn}/` (built once via `build_dolphin_docker.sh`) + `External/CACerts/cacert.pem`. |
| Android         | Stub                   | n/a                          | `IsAvailable()` returns false; calls fail cleanly.                                                                 |

The same C++ / Lua call works on every platform. The platform-specific code lives behind `HttpBackend` and is never exposed to gameplay.

## Lifecycle

`Engine::Initialize()` calls `Http::Initialize()` after `NET_Initialize()`. That:

1. Creates the platform backend via `CreatePlatformHttpBackend()` (only one `HttpBackend_*.cpp` is compiled per build, gated by `#if PLATFORM_*`).
2. Spawns a single shared worker thread.
3. Begins accepting requests.

`Engine::Update()` calls `Http::Tick()` once per frame. Tick drains the main-thread completion queue and invokes each pending callback on the main thread. This is what makes Lua callbacks safe — they always run when no other Lua code is on the stack.

`Engine::Shutdown()` calls `Http::Shutdown()`, which:

1. Sets the cancel flag on every queued request.
2. Joins the worker thread (it'll abort whatever it's doing).
3. Drains and discards any leftover completions, logging a warning if there are any.
4. Tears down the backend.

## Public C++ API

### `Http::Send` and verb shortcuts

```cpp
namespace Http
{
    HttpHandle Send(HttpRequest req, HttpResponseCallback cb);
    HttpHandle Get   (const std::string& url, HttpResponseCallback cb);
    HttpHandle Post  (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    HttpHandle Put   (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    HttpHandle Patch (const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb);
    HttpHandle Delete(const std::string& url, HttpResponseCallback cb);
    HttpHandle PostString(const std::string& url, const std::string& body, HttpResponseCallback cb);
    HttpResponse SendSync(HttpRequest req);   // tools/tests only — never on main thread
    bool        IsAvailable();
    const char* GetMissingDependencyMessage();
}
```

### `HttpRequest` builder

```cpp
HttpRequest req(HttpVerb::Post, "https://api.example.com/items");
req.Header("Content-Type", "application/json")
   .Header("Authorization", "Bearer " + token)
   .Body(R"({"name":"test"})")
   .TimeoutMs(5000)
   .MaxRedirects(3)
   .MaxBodyBytes(2 * 1024 * 1024)
   .VerifySsl(true);

Http::Send(std::move(req), [](const HttpResponse& r) { /* ... */ });
```

Every setter returns `*this`, so you can chain. Bodies accept `std::string`, `std::vector<uint8_t>`, or `(const uint8_t*, size_t)`. Headers are case-insensitive on the way in and out.

### `HttpResponse` accessors

```cpp
const HttpResponse& r = ...;

r.GetStatus();                                  // 200, 404, ...
r.IsSuccess();                                  // 2xx and no transport error
r.GetError();                                   // HttpError enum
r.GetErrorMessage();                            // human-readable detail
r.GetHeader("Content-Type");                    // case-insensitive lookup
r.HasHeader("Location");
r.GetHeaders();                                 // const HttpHeaderMap&
r.GetBody();                                    // const std::vector<uint8_t>&
r.GetBodyAsString();                            // copies body into a std::string
r.GetStream();                                  // wraps body in a Stream (no copy)
r.GetFinalUrl();                                // URL after any redirects

// Asset decoders — return nullptr on failure.
Texture*   t = r.GetTexture();                  // PNG/JPG/TGA/BMP via stb_image
SoundWave* s = r.GetSoundWave();                // WAV today; OGG planned
```

### `HttpHandle` — cancellation

```cpp
HttpHandle h = Http::Get(url, callback);

// At any time, from any thread:
h.Cancel();
bool wasCancelled = h.IsCancelled();
```

`Cancel()` flips an `std::atomic<bool>` shared with the worker thread. Backends poll the flag during their request loop and abort cleanly. The callback still fires with `HttpError::Cancelled`.

### `HttpVerb` and `HttpError` enums

```cpp
enum class HttpVerb : uint8_t
{
    Get, Post, Put, Patch, Delete, Head, Options, Count
};

enum class HttpError : uint8_t
{
    None,
    NotInitialized,    // Http::Initialize() wasn't called
    Unavailable,       // backend disabled (e.g. missing libcurl)
    InvalidUrl,
    Network,           // connect / send / recv failure
    Tls,               // TLS handshake / cert error
    Timeout,
    TooLarge,          // response body exceeded MaxBodyBytes
    Cancelled,
    BadResponse,       // malformed HTTP response
    Unknown,
    Count
};

// Helpers:
const char* HttpVerbToString(HttpVerb verb);
HttpVerb    HttpVerbFromString(const char* s);
const char* HttpErrorToString(HttpError err);
bool        HttpStatusIsSuccess(int statusCode);
bool        HttpStatusIsRedirect(int statusCode);
```

## JSON

`JsonHelpers.h` exposes a thin `rapidjson` wrapper. The Lua bindings build on it.

```cpp
#include "Network/Http/JsonHelpers.h"
#include "document.h"   // rapidjson — bundled at External/Assimp/contrib/rapidjson

void HandleResponse(const HttpResponse& r)
{
    rapidjson::Document doc;
    std::string err;
    if (!ParseJsonBytes(r.GetBody().data(), r.GetBody().size(), doc, err))
    {
        LogWarning("JSON parse error: %s", err.c_str());
        return;
    }

    if (doc.IsObject() && doc.HasMember("status"))
    {
        const auto& status = doc["status"];
        if (status.IsString())
            LogDebug("status = %s", status.GetString());
    }
}
```

For Lua, `response:GetJson()` does the conversion to a Lua table directly — see the [Lua reference](../../Lua/Networking/Http.md).

## Defaults

| Field          | Default      | Override                                                         |
|----------------|--------------|------------------------------------------------------------------|
| `timeoutMs`    | 10000 (10s)  | `req.TimeoutMs(ms)` / Lua `request:Timeout(ms)`                  |
| `maxRedirects` | 5            | `req.MaxRedirects(n)`                                            |
| `maxBodyBytes` | 64 MiB       | `req.MaxBodyBytes(n)` — guard against runaway downloads          |
| `verifySsl`    | true         | `req.VerifySsl(false)` for self-signed / dev / pinned servers    |
| `User-Agent`   | `Polyphase/1.0` | `req.Header("User-Agent", "...")`                              |

Disabling SSL verification is per-request and only honoured by backends that allow it (Windows / Linux / Wii / GCN). 3DS routes through Nintendo's system services, which always verify; you can't disable it there.

## Threading and async model

```
┌─ main thread ───────────────────────────────────────────────┐
│  Http::Send(req, cb)                                        │
│    → enqueue { id, req, cb, cancelFlag } to queue           │
│    → return HttpHandle                                       │
│                                                              │
│  Http::Tick() each frame                                    │
│    → drain completion queue                                  │
│    → invoke each (cb, response) pair                         │
└─────────────────────────────────────────────────────────────┘
                  ▲
                  │ atomic completion queue
                  │
┌─ worker thread (one shared) ────────────────────────────────┐
│  loop:                                                       │
│    pull next request from queue (cv.wait if empty)           │
│    backend->PerformRequest(req, cancelFlag, response)        │
│       (synchronous call into WinHTTP / libcurl / httpc /     │
│        mbedTLS / ...)                                        │
│    push (cb, response) onto completion queue                 │
└─────────────────────────────────────────────────────────────┘
```

Key implications:

- Requests run **sequentially**. A long-running download will queue up shorter requests behind it. Two concurrent downloads need either a v2 multi-worker change or different `HttpClient` instances (planned).
- Callbacks always fire on the main thread. **No mutexes needed** when touching engine state from inside.
- Cancellation works mid-flight — the backend checks the flag during its read loop.
- `SendSync` runs the request inline on the calling thread. Use it from worker threads or tools, never from the main thread (it would block the frame).

This mirrors the existing `GitOperationQueue` (`Engine/Source/Editor/Git/GitOperationQueue.h`) and `ControllerServer::QueueCommand` (`Engine/Source/Editor/ControllerServer/ControllerServer.cpp:173`) patterns.

## Recipes

### Fetch JSON, dispatch to gameplay

```cpp
Http::Get("https://api.example.com/leaderboard", [](const HttpResponse& r) {
    if (!r.IsSuccess()) return;
    rapidjson::Document doc;
    std::string err;
    if (!ParseJsonBytes(r.GetBody().data(), r.GetBody().size(), doc, err)) return;

    auto* world = GetWorld(0);
    if (auto* node = world->FindNode<LeaderboardNode>("Leaderboard"))
        node->ApplyJson(doc);
});
```

### POST a binary blob

```cpp
std::vector<uint8_t> snapshotBytes = CaptureScreenshotPng();

HttpRequest req(HttpVerb::Post, "https://api.example.com/snapshots");
req.Header("Content-Type", "image/png")
   .Header("Authorization", "Bearer " + token)
   .Body(std::move(snapshotBytes));

Http::Send(std::move(req), [](const HttpResponse& r) {
    LogDebug("Upload status: %d", r.GetStatus());
});
```

### Download an avatar texture

```cpp
Http::Get(player.avatarUrl, [&](const HttpResponse& r) {
    Texture* tex = r.GetTexture();
    if (tex == nullptr) {
        LogWarning("Avatar decode failed");
        return;
    }
    avatarWidget->SetTexture(tex);
});
```

The texture will be freed via the normal asset lifecycle when nothing references it. Hold an `AssetRef<Texture>` to keep it alive.

### Cancellable request driven by gameplay

```cpp
class MyNode : public Node3D {
    HttpHandle mPendingRequest;
public:
    void StartFetch() {
        mPendingRequest = Http::Get(url, [this](const HttpResponse& r) {
            if (r.GetError() == HttpError::Cancelled) return;
            // ... use r ...
        });
    }
    void Destroy() override {
        mPendingRequest.Cancel();   // safe whether or not it's still running
        Node3D::Destroy();
    }
};
```

### Synchronous from a tool / test

```cpp
// Editor command-line tools, unit tests — DO NOT call from gameplay.
HttpResponse r = Http::SendSync(HttpRequest(HttpVerb::Get, url).TimeoutMs(2000));
if (r.IsSuccess())
    std::cout << r.GetBodyAsString() << '\n';
```

## Adding a new backend

To support a new platform:

1. Create `Engine/Source/Network/Http/Backends/HttpBackend_<Platform>.cpp`.
2. Wrap everything in `#if PLATFORM_*`.
3. Subclass `HttpBackend` and implement `Initialize` / `Shutdown` / `IsAvailable` / `PerformRequest` / `GetMissingDependencyMessage`.
4. Implement `CreatePlatformHttpBackend()` returning your backend. Exactly one of these is compiled per build (the others' `#if` guards skip them).
5. Register the file in `Engine.vcxproj` + `.filters` and the platform's `Makefile_*`.

See `HttpBackend_Windows.cpp` (cleanest reference) and `HttpBackend_Dolphin.cpp` (most involved — manual HTTP/1.1 framing + mbedTLS).

## Streaming

Not supported in v1. The current API buffers the whole response into `HttpResponse::body` and respects `MaxBodyBytes` to bound memory.

V2 will add an `HttpRequest::OnChunk(callback)` that streams body chunks as they arrive instead of buffering. The C++ surface will stay backwards-compatible — callers who don't set `OnChunk` get the buffered behaviour they have today.

## Migration notes

If your code currently uses `Editor/AutoUpdater/HttpClient.h`:

- That file is editor-only (`#if EDITOR`) and stays where it is. It's a deliberately tiny WinHTTP-vs-libcurl shim for the auto-updater and won't be promoted to runtime.
- For new gameplay-side HTTP, use this client (`Network/Http/HttpClient.h`) — full verb set, async, available at runtime.
- AutoUpdater's `HttpClient::Get(url, timeoutMs)` becomes `Http::SendSync(HttpRequest(HttpVerb::Get, url).TimeoutMs(timeoutMs))` — but only on a worker thread, never the main thread.

## Troubleshooting

| Symptom                                                | Likely cause                                                                                                                      |
|--------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| `Http.IsAvailable()` returns false on Linux            | `libcurl.so.4` not installed. `sudo apt install libcurl4`.                                                                        |
| HTTPS fails with TLS error on Wii / GCN                | `External/mbedtls/lib-{wii,gcn}/*.a` not built or `External/CACerts/cacert.pem` missing. See `External/mbedtls/README.md`.        |
| Callback never fires                                   | `Http::Tick()` not called this frame. Must run from the main loop after `Http::Initialize()`.                                     |
| Custom REST routes return 404                          | This client makes outbound requests. To **serve** routes from the editor, use the controller server (`polyphase-controller` skill). |
| Wii cert verification fails on every site              | CA bundle missing or stale. `curl -o External/CACerts/cacert.pem https://curl.se/ca/cacert.pem`.                                  |
| HTTP works, HTTPS fails on Wii                         | mbedTLS initialised but `mbedtls_hardware_poll` returned no entropy. Confirm `HttpBackend_Dolphin.cpp` is compiled with TLS enabled. |
| Response body unexpectedly small / truncated           | Hit `maxBodyBytes` cap. Bump it via `req.MaxBodyBytes(n)` for known-large responses.                                              |
| `GetTexture()` returns nullptr on a valid PNG          | Texture::LoadFromMemory rejects non-power-of-two dimensions for runtime safety. Provide a PoT image (64×64, 128×128, …).           |
| Memory grows when running thousands of requests        | Each in-flight request holds its own `cancelFlag` shared_ptr + buffered body. Use `MaxBodyBytes` and let handles drop out of scope. |

## See also

- [Lua HTTP API](../../Lua/Networking/Http.md) — full Lua reference with examples.
- [Native Addons](../NativeAddon/NativeAddon.md) — addons can call the same `Http::*` API.
- [polyphase-controller skill](../../../.claude/skills/polyphase-controller/SKILL.md) — for the editor's *inbound* REST server (different direction).
- `Engine/Source/Network/Http/` — the source. `HttpBackend.h` for the backend contract.
- `External/mbedtls/README.md` — Wii/GCN TLS setup.
- `Polyphase-Examples/HTTP-Client-Demo/Scripts/` — runnable verification tests.
