# WebSocket Client

A WebSocket client built into the engine, parallel to [HTTP](Http.md). Realtime,
bidirectional messaging between game scripts and a dedicated server.

Client only. No server, no `permessage-deflate`, no HTTP/2. It does not replace
`Network.*` UDP multiplayer, nor the web targets' UDP relay.

The Lua surface is documented in [Lua → WebSocket](../../Lua/Networking/WebSocket.md).

## Platform support

| Platform | ws:// | wss:// | Transport |
| --- | --- | --- | --- |
| Windows | yes | yes | `NET_` stream socket + engine framing; wss via the WinHTTP WebSocket API |
| Linux | yes | with libcurl ≥ 7.86 | Same; wss via dlopen'd libcurl (`CURLOPT_CONNECT_ONLY=2` + `curl_ws_*`) |
| Web (`web.webgl2` / `web.webgpu`) | yes | yes | Browser `WebSocket` via `EM_JS`, supplied by the build-target package |
| Android | yes | no | Same as Linux minus TLS |
| 3DS | yes | no | Non-blocking `soc:u` TCP via `NET_` |
| GameCube / Wii | yes | no | libogc TCP via `NET_` |
| Other Variant-2 addon targets (PSP, Dreamcast, …) | no | no | `WebSocket.IsAvailable()` returns false |

## Files

All flat in `Engine/Source/Network/` — deliberately. A new *subdirectory* would
have to be added to all four engine console Makefiles **and** to every
already-shipped Variant-2 build-target package's Makefile, and the latter is
impossible. Flat files in an already-enumerated directory are picked up
automatically everywhere except `Engine/Engine.vcxproj`, which is an explicit
file list.

| File | Role |
| --- | --- |
| `WebSocketTypes.h` | `WsState` / `WsError`, limits, `WsUrl`, `WsConnectOptions`, `WsMessage`. Header-only — the web package includes it |
| `WebSocketClient.h/.cpp` | Public `WebSocket::` namespace, the `WsConnection` interface, the three-arm seam, the connection state machine, and the `ws://` byte transport |
| `WebSocketFraming.h/.cpp` | RFC 6455 encode/decode, client masking, fragment reassembly, SHA-1, base64 |
| `WebSocketHandshake.h/.cpp` | URL parse, the HTTP/1.1 upgrade request, the 101 response parse |
| `WebSocketTransport.h` | Internal transport interface (byte vs message) and the secure-transport factory |
| `WebSocketWss_Windows.cpp` | wss:// via WinHTTP |
| `WebSocketWss_Linux.cpp` | wss:// via libcurl |
| `LuaBindings/WebSocket_Lua.h/.cpp` | The Lua binding |
| `Tools/ws-echo/` | Dependency-free Node echo server for the acceptance test |

## The compile seam

`WebSocketClient.cpp` has three arms:

```cpp
#if !defined(POLYPHASE_PLATFORM_ADDON)
    // Real implementation: desktop + engine-built consoles (3DS/GCN/Wii/Android).
#elif defined(POLYPHASE_WS_PROVIDED_BY_ADDON)
    // Nothing. The build-target package supplies the whole WebSocket:: namespace.
#else
    // Self-contained stubs: IsAvailable() -> false, Connect() -> null.
#endif
```

The middle arm keys on an **opt-in define**, not on `PLATFORM_WEB`. That is the
one deliberate improvement over `HttpClient.cpp`, whose `PLATFORM_WEB` arm means
a new engine paired with an older web package fails to link. Here, every
Variant-2 package that predates this feature compiles the stub arm and keeps
linking with zero edits — WebSocket simply reports itself unavailable. Only a
package that actually ships an implementation defines
`POLYPHASE_WS_PROVIDED_BY_ADDON` (the two web packages do, in their DEFINES
block).

`WebSocketFraming.cpp`, `WebSocketHandshake.cpp` and both `WebSocketWss_*.cpp`
are guarded the same way, so a stub-arm build compiles literally nothing new.

## Architecture

- **Byte transports** (`ws://` over `NET_`) hand raw bytes up; the engine layer
  owns the handshake and the framing.
- **Message transports** (WinHTTP, libcurl, and conceptually the browser) hand
  whole messages up; the platform owns the handshake and the framing.
  `WsTransport::IsMessageTransport()` is the switch.
- No exceptions, no RTTI, no threads in the core — web and console builds use
  `-fno-exceptions -fno-rtti`, and the sockets are non-blocking and polled from
  `WebSocket::Tick()`. The two desktop `wss://` backends are the sanctioned
  exception: they use worker threads, but those threads only ever push into a
  lock-protected handoff queue that `Tick()` drains. Nothing off the main thread
  ever touches Lua.
- Ownership: the Lua userdata holds a `shared_ptr<WsConnection>`; the module
  keeps only weak references and prunes them each `Tick`. Dropping the Lua
  handle closes the connection, matching Godot.

### Engine wiring

`Engine/Source/Engine/Engine.cpp`:

- `WebSocket::Initialize()` right after `Http::Initialize()`
- `WebSocket::Tick()` right after `Http::Tick()` — the **only** place Lua
  callbacks fire, inside `SCOPED_FRAME_STAT("WebSocketTick")`
- `WebSocket::Shutdown()` right after `Http::Shutdown()`, before `NET_Shutdown()`

## `NET_` additions

`NET_SocketConnect` is blocking-with-timeout on Windows/Linux and ignores the
timeout entirely on 3DS, Dolphin and Android — unusable from a per-frame pump.
Three purely additive functions were added to `Network/Network.h` and all five
platform backends:

```cpp
bool    NET_SocketConnectAsync(SocketHandle s, uint32_t ipAddr, uint16_t port);
int32_t NET_SocketConnectPoll(SocketHandle s);              // -1 fail, 0 pending, 1 connected
bool    NET_SocketWouldBlock(SocketHandle s, int32_t opResult);
```

`NET_SocketWouldBlock` exists because a non-blocking stream socket returns a
negative count both when it is merely out of data and when the connection is
dead, and the count alone cannot tell those apart. It takes the returned value
because libogc reports the error *in* the return value while the BSD platforms
use `errno` / `WSAGetLastError`.

Existing callers of `NET_SocketConnect` (only `HttpBackend_Dolphin.cpp`, on a
worker thread) are untouched. Variant-2 platform `Network_*.cpp` stubs do not
need the new symbols — only the real arm references them, and Variant-2 builds
compile the stub arm.

**Dolphin caveat:** libogc's `net_connect` always blocks and devkitPPC has no
portable non-blocking connect for it, so `NET_SocketConnectAsync` runs the
blocking connect and stores the result for `NET_SocketConnectPoll` to hand back.
A per-frame pump still works; it just eats one connect stall.

**DNS is still blocking.** `NET_ResolveHost` is synchronous on every platform, so
`WebSocket.Connect` with a hostname stalls for the lookup. Connect at menu or
level-load time, or pass a numeric IP — that bypasses the resolver everywhere.

## wss → ws auto-downgrade (per-build-target opt-in)

A game ships one `wss://` URL. Targets with no TLS can opt into rewriting it to
`ws://` rather than failing, accepting plaintext on those platforms.

Compile define: **`POLYPHASE_WS_DOWNGRADE_WSS=1`**. When set,
`WebSocket::Connect` rewrites the scheme, maps a default port 443 → 80 (an
explicit port is kept), logs a warning once per connection, and
`ws:WasDowngraded()` reports true.

**Default OFF everywhere.** Without the define the whole branch compiles out.
An un-downgraded `wss://` on a TLS-less transport fails loudly:
`WebSocket.Connect` returns `nil, "wss:// is not supported on this platform"`.

Opt-in points, documented but **not enabled**:

| Target kind | Where to add the define |
| --- | --- |
| Engine-built 3DS | `Engine/Makefile_3DS` CFLAGS block |
| Engine-built GameCube / Wii | `Engine/Makefile_GCN` / `Makefile_Wii` |
| Engine-built Linux | `Engine/Makefile_Linux` |
| Any Variant-2 build-target addon (PSP, Dreamcast, …) | the package's own Makefile DEFINES block |
| Web targets | **never** — browser TLS is free, and downgrading breaks https-served pages (mixed content) |
| Desktop Windows/Linux | not by default; they have real wss |

## Testing

```bash
node Tools/ws-echo/echo.js
```

Then run `WsEchoTest.lua` from the Websocket test project. It is self-checking
and logs `WS-TEST PASS` or `WS-TEST FAIL <step>`. The steps cover: availability,
connect + subprotocol negotiation, short text echo, 64 KiB binary (16-bit
extended length + masking), 200 KiB binary (multi-Tick reads), the polling path,
a server-initiated `Close(4000, "requested")`, a refused connection
(`Error → Closed(1006, false)`), and the `wss://` / downgrade branches.
