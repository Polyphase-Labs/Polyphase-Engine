# WebSocket (Lua API)

Realtime, bidirectional messaging with a dedicated server — the thing [`Http`](Http.md)
cannot do. Available globally as `WebSocket`. Every callback fires on the main
thread from the engine's per-frame pump, so you can touch node properties, asset
references and UI widgets directly from inside one without locking.

This is a **client**. It is not a replacement for the engine's UDP multiplayer
(`Network.*`), and on web builds it is unrelated to the WebGL2 target's UDP
relay — a dedicated server that speaks WebSocket serves native and browser
clients uniformly, with no relay in the middle.

```lua
local ws, err = WebSocket.Connect('ws://127.0.0.1:9002/', { protocols = { 'echo.v1' } })
if ws == nil then
    Log.Error('connect failed: ' .. err)
    return
end

ws:SetOpenCallback(function()
    ws:SendText('hello')
end)

ws:SetMessageCallback(function(data, isBinary)
    Log.Debug('got ' .. #data .. ' bytes, binary=' .. tostring(isBinary))
end)

ws:SetClosedCallback(function(code, reason, wasClean)
    Log.Debug('closed ' .. code .. ' ' .. reason)
end)
```

**Keep a reference to the connection.** The Lua handle owns it — if you drop it
and the garbage collector runs, the connection closes. Store it on `self`, not
in a local that goes out of scope.

## Top-level functions

### `WebSocket.IsAvailable() → bool`

False on platforms with no transport at all. Check it before showing a
"connect" button.

### `WebSocket.GetMissingDependencyMessage() → string`

Why not, when `IsAvailable()` is false. Empty string when available.

### `WebSocket.Connect(url [, options]) → connection` or `nil, error`

`url` is `ws://host[:port][/path]` or `wss://...`. The port defaults to 80 for
`ws` and 443 for `wss`.

Returns a connection handle immediately — it starts in the `Connecting` state
and reaches `Open` some frames later. On a malformed URL, or `wss://` on a build
with no TLS transport, it returns `nil` plus a message.

`options` is an optional table:

| Key | Type | Meaning |
| --- | --- | --- |
| `protocols` | array of strings | `Sec-WebSocket-Protocol` offers, most preferred first |
| `headers` | string→string map | Extra handshake headers. **Native only** — browsers cannot set them, and web builds log a warning and ignore this |
| `maxQueuedBytes` | integer | Per-direction queue cap (default 1 MiB) |

## Connection: callbacks

Passing `nil` clears a callback. Setting one replaces the previous.

### `ws:SetOpenCallback(fn)` — `fn()`

The handshake completed. `GetState()` is `Open` and `GetSelectedProtocol()` is
final by the time this fires.

### `ws:SetMessageCallback(fn)` — `fn(data, isBinary)`

`data` is a Lua string and is 8-bit clean, so binary payloads survive intact.

**While a message callback is set, messages are NOT queued for `GetPacket()`.**
The callback always wins.

### `ws:SetErrorCallback(fn)` — `fn(message)`

A transport or protocol error. A `Closed` callback always follows.

### `ws:SetClosedCallback(fn)` — `fn(code, reason, wasClean)`

Fires exactly once per connection. `wasClean` is true only when both sides
completed the close handshake.

**Ordering** is always `Open → Message* → Closed`. A failure before `Open`
produces `Error → Closed(1006, "", false)`.

## Connection: actions

### `ws:SendText(str) → bool`
### `ws:SendBinary(str) → bool`

Return false — never raise — when the connection is not `Open` or the outgoing
queue is full. A full queue also logs a warning once per connection.

Text frames are not UTF-8 validated. Send JSON and you will be fine; send raw
bytes and you should use `SendBinary`.

### `ws:Close([code [, reason]])`

Begins the close handshake. Defaults to `1000` and `""`. Safe to call in any
state, including twice.

## Connection: polling

An alternative to callbacks, mirroring Godot's `WebSocketPeer`. Only works when
no message callback is set.

### `ws:GetState() → WebSocketState`
### `ws:GetAvailablePacketCount() → integer`
### `ws:GetPacket() → data, isBinary` or `nil`

```lua
while ws:GetAvailablePacketCount() > 0 do
    local data, isBinary = ws:GetPacket()
    -- ...
end
```

### `ws:GetSelectedProtocol() → string`

The negotiated subprotocol, or `""` until `Open` / when none was negotiated.

### `ws:GetCloseCode() → integer`
### `ws:GetCloseReason() → string`

`0` and `""` until the connection closes.

### `ws:WasDowngraded() → bool`

True when the build was compiled with `POLYPHASE_WS_DOWNGRADE_WSS=1` and it
rewrote a `wss://` URL to `ws://`. Off everywhere by default — see
[Development → WebSocket Client](../../Development/Networking/WebSocket.md).

## `WebSocketState`

```lua
WebSocketState = { Connecting = 0, Open = 1, Closing = 2, Closed = 3 }
```

## Limits and behaviour

| | |
| --- | --- |
| Outgoing queue | 1 MiB (override with `options.maxQueuedBytes`) |
| Incoming queue | 1 MiB, only used when no message callback is set |
| Max single message | 16 MiB — larger is a protocol error, which closes the connection |
| Ping | Answered with Pong inside the transport; never surfaced to Lua |
| Fragmentation | Incoming fragments are reassembled; outgoing messages are never fragmented |

## Godot correspondence

| Godot `WebSocketPeer` | Polyphase |
| --- | --- |
| `connect_to_url(url)` | `WebSocket.Connect(url, options)` |
| `poll()` | implicit — the engine ticks the subsystem |
| `get_ready_state()` | `ws:GetState()` / `WebSocketState` |
| `send_text(t)` / `send(bytes)` | `ws:SendText(s)` / `ws:SendBinary(s)` |
| `get_available_packet_count()` | `ws:GetAvailablePacketCount()` |
| `get_packet()` + `was_string_packet()` | `ws:GetPacket()` → `data, isBinary` |
| `close(code, reason)` | `ws:Close(code, reason)` |
| `get_close_code()` / `get_close_reason()` | `ws:GetCloseCode()` / `ws:GetCloseReason()` |

## Testing locally

The engine ships a dependency-free echo server:

```bash
node Tools/ws-echo/echo.js
```

See `Tools/ws-echo/README.md`.
