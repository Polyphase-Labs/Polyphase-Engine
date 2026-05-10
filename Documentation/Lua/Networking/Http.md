# Http (Lua API)

Async HTTP/HTTPS client. Available globally as `Http`. All callbacks fire on the main thread, so you can touch any engine state — node properties, asset references, UI widgets — directly from inside without locking.

For architecture, platform support, and the C++ surface, see [Networking → HTTP Client](../../Development/Networking/Http.md).

```lua
Http.Get("https://api.example.com/status", function(r)
    if r:IsSuccess() then
        print("OK", r:GetStatus(), #r:GetBody(), "bytes")
    else
        print("Error", r:GetStatus(), r:GetError())
    end
end)
```

## Top-level functions

### `Http.Get(url, callback) → handle`

Performs a GET. The callback fires on the main thread when the response arrives.

```lua
Http.Get("https://example.com/foo", function(r) print(r:GetStatus()) end)
```

### `Http.Post(url, body, callback) → handle`

Performs a POST with `body` as raw bytes. Set `Content-Type` via the [Request builder](#request-builder) if needed.

```lua
Http.Post("https://example.com/items", "name=test&count=3", function(r) ... end)
```

### `Http.Put(url, body, callback) → handle`
### `Http.Patch(url, body, callback) → handle`
### `Http.Delete(url, callback) → handle`

Same shapes as `Post` (DELETE has no body).

### `Http.Request(verb, url) → request`

Returns a chainable [request builder](#request-builder). Use this when you need headers, body content-type control, custom timeout, or SSL toggle.

`verb` is `"GET"`, `"POST"`, `"PUT"`, `"PATCH"`, `"DELETE"`, `"HEAD"`, or `"OPTIONS"` (case-insensitive).

```lua
Http.Request("POST", "https://example.com/api")
    :Header("Content-Type", "application/json")
    :Body('{"foo":"bar"}')
    :Send(function(r) print(r:GetStatus()) end)
```

### `Http.IsAvailable() → bool`

Returns `false` on platforms without HTTP support (Android), or on Linux when `libcurl4` isn't installed. Always `true` on Windows / 3DS / Wii / GCN with the standard build.

### `Http.GetMissingDependencyMessage() → string`

Diagnostic string when `IsAvailable()` is `false`. Empty string otherwise.

```lua
if not Http.IsAvailable() then
    Log.Warning("HTTP unavailable: " .. Http.GetMissingDependencyMessage())
end
```

## Request builder

Returned by `Http.Request(verb, url)`. Every method except `:Send` returns the same request, so they chain.

| Method                 | Purpose                                                                              |
|------------------------|--------------------------------------------------------------------------------------|
| `:Header(name, value)` | Add (or replace) a request header. Names are case-insensitive.                       |
| `:Body(string)`        | Set the request body. Raw bytes — set `Content-Type` separately if needed.           |
| `:Timeout(ms)`         | Per-request timeout in milliseconds. Default 10000.                                  |
| `:VerifySsl(bool)`     | Toggle TLS certificate verification. Default `true`.                                 |
| `:Send(callback)`      | Sends the request and returns a handle. Callback fires on main thread on completion. |

```lua
local req = Http.Request("POST", "https://api.example.com/login")
    :Header("Content-Type", "application/json")
    :Header("Accept", "application/json")
    :Body('{"user":"alice","password":"hunter2"}')
    :Timeout(3000)

local handle = req:Send(function(r)
    if r:IsSuccess() then
        local data = r:GetJson()
        SaveToken(data.token)
    else
        print("Login failed:", r:GetStatus(), r:GetError())
    end
end)
```

## Response object

Passed to your callback. Every method below is non-blocking and safe to call multiple times.

| Method                  | Returns                                                                                                  |
|-------------------------|----------------------------------------------------------------------------------------------------------|
| `:IsSuccess()`          | `true` when status is in `[200, 300)` and there was no transport / TLS / cancel / parse error.           |
| `:GetStatus()`          | HTTP status code as integer (200, 404, 500, …). `0` when the request never reached the server.          |
| `:GetError()`           | Error string (`"Cancelled"`, `"Timeout"`, `"Network"`, …) or `nil` when none.                            |
| `:GetBody()`            | Response body as a Lua string (raw bytes — may contain nulls).                                          |
| `:GetHeader(name)`      | Header value (case-insensitive lookup), or `nil` if absent.                                              |
| `:GetHeaders()`         | Lua table of all response headers, name → value.                                                         |
| `:GetFinalUrl()`        | URL after following any redirects.                                                                       |
| `:GetJson()`            | Lua table parsed from a JSON body. Returns `nil, errMessage` on parse failure.                           |
| `:GetTexture()`         | New `Texture` asset decoded from a PNG/JPG body. `nil` on failure (incl. non-power-of-two dimensions).   |
| `:GetSoundWave()`       | New `SoundWave` asset decoded from a WAV body. `nil` on failure. (OGG support is planned.)               |

### `:GetJson()` shapes

JSON values map to Lua as follows:

| JSON                  | Lua                                                |
|-----------------------|----------------------------------------------------|
| `null`                | `nil`                                              |
| `true` / `false`      | `true` / `false`                                   |
| number (integer)      | integer                                            |
| number (float)        | number                                             |
| string                | string                                             |
| array `[a, b, c]`     | table with integer keys `1..n`                     |
| object `{"k": v}`     | table with string keys                             |

Nested structures recurse. JSON booleans / nulls round-trip cleanly; numeric precision matches Polyphase's Lua build (single-precision float on console builds).

```lua
Http.Get("https://api.example.com/scores", function(r)
    local data, err = r:GetJson()
    if data == nil then
        Log.Error("JSON parse failed: " .. tostring(err))
        return
    end

    -- e.g. data = { scores = { { name="alice", value=1200 }, ... } }
    for i, entry in ipairs(data.scores) do
        print(i, entry.name, entry.value)
    end
end)
```

## Request handle

Returned by every send variant. Lets you cancel an in-flight request and check whether it was cancelled.

| Method            | Purpose                                                                                  |
|-------------------|------------------------------------------------------------------------------------------|
| `:Cancel()`       | Mark the request for cancellation. Safe to call from anywhere, multiple times. The callback still fires with `IsSuccess()` false and `GetError()` set. |
| `:IsCancelled()`  | `true` if `:Cancel()` has been called.                                                   |

```lua
local handle = Http.Get("https://example.com/slow-endpoint", onResponse)

-- Later, when the player abandons the operation:
handle:Cancel()
```

## Defaults

| Field          | Default      |
|----------------|--------------|
| Timeout        | 10000 ms (10s) |
| Max redirects  | 5            |
| Max body size  | 64 MiB       |
| SSL verify     | on           |
| User-Agent     | `Polyphase/1.0` |

Override via the [request builder](#request-builder).

## Worked examples

### Fetch JSON and update a node

```lua
Http.Get("https://api.example.com/status", function(r)
    if not r:IsSuccess() then return end
    local data = r:GetJson()
    if data and data.online then
        statusLabel:SetText("Server online")
    else
        statusLabel:SetText("Server down")
    end
end)
```

### Send a JSON body

Most APIs that take JSON expect a `Content-Type: application/json` header. Use the builder so you can set it.

```lua
local payload = '{"event":"player_died","level":3}'
Http.Request("POST", "https://api.example.com/telemetry")
    :Header("Content-Type", "application/json")
    :Body(payload)
    :Send(function(r)
        if not r:IsSuccess() then
            Log.Warning("Telemetry failed: " .. r:GetStatus())
        end
    end)
```

### Download an avatar texture

```lua
Http.Get(player.avatarUrl, function(r)
    local tex = r:GetTexture()
    if tex == nil then
        Log.Warning("Avatar decode failed for " .. player.name)
        return
    end
    avatarWidget:SetTexture(tex)
end)
```

> Texture decode requires power-of-two image dimensions. For arbitrary user uploads, host them as 64×64, 128×128, 256×256, etc., or run them through a resize pipeline before serving.

### Stream a sound effect

```lua
Http.Get("https://cdn.example.com/sfx/bell.wav", function(r)
    local snd = r:GetSoundWave()
    if snd then Audio.PlaySound2D(snd, 1.0, 1.0) end
end)
```

WAV decode supports 8/16-bit PCM, mono and stereo, any sample rate. OGG runtime decode is planned.

### Cancel on scene change

```lua
local MyScript = {}

function MyScript:Start()
    self.handle = Http.Get(longPollUrl, function(r)
        if r:IsSuccess() and r:GetError() ~= "Cancelled" then
            self:OnDataReceived(r:GetJson())
        end
    end)
end

function MyScript:Destroy()
    if self.handle then self.handle:Cancel() end
end

return MyScript
```

### Custom timeout for a slow endpoint

```lua
Http.Request("GET", "https://api.example.com/heavy-report")
    :Timeout(60000)               -- 60 seconds
    :Send(function(r)
        if r:GetError() == "Timeout" then
            print("Report took too long, giving up")
            return
        end
        ProcessReport(r:GetBody())
    end)
```

### Check that HTTP is wired up before kicking off a flow

```lua
if not Http.IsAvailable() then
    Log.Error("HTTP unavailable: " .. Http.GetMissingDependencyMessage())
    return
end
```

## Common pitfalls

- **Capturing `self` in callbacks.** Define your callback as a closure that captures `self` from the enclosing function — Lua doesn't auto-bind methods, so passing `self.OnReply` directly drops the `self` argument.

   ```lua
   function MyScript:Fetch()
       Http.Get(url, function(r) self:OnReply(r) end)   -- ✅
       Http.Get(url, self.OnReply)                       -- ❌ self lost
   end
   ```

- **Using the response after the callback returns.** The response object is alive as long as your Lua state holds a reference. If you stash `r` in `self.lastResponse`, that's fine. If you let the callback exit and the local goes out of scope, the response is GC'd.

- **Forgetting `Content-Type` on POST.** Many APIs return 415 (Unsupported Media Type) without it. Use the builder and `:Header("Content-Type", "application/json")`.

- **Treating cancelled callbacks as failures.** A cancelled request still calls your callback. Check `r:GetError() == "Cancelled"` before logging or retrying — it's not a real failure.

- **Power-of-two textures.** `GetTexture()` rejects non-PoT dimensions for runtime safety on consoles. Pre-resize content to PoT or use square 256/512/1024 source images.

## See also

- [Networking → HTTP Client](../../Development/Networking/Http.md) — architecture, C++ API, platform / TLS details, JSON helpers.
- [Lua Audio API](../Systems/Audio.md) — pair `:GetSoundWave()` with `Audio.PlaySound2D()`.
- [Texture Lua API](../Assets/Texture.md) — what you can do with the `Texture` returned by `:GetTexture()`.
- `Polyphase-Examples/HTTP-Client-Demo/Scripts/` — runnable verification tests for every feature documented here.
