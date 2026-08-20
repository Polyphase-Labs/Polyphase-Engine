# ws-echo

A dependency-free WebSocket echo server for exercising the engine's WebSocket
client (`Engine/Source/Network/WebSocket*`).

## Running

```bash
node Tools/ws-echo/echo.js
```

Listens on `ws://127.0.0.1:9002` by default. Node 16 or newer; no `npm install`
— it uses only `node:http` and `node:crypto`.

| Flag | Default | Meaning |
| --- | --- | --- |
| `--host H` | `127.0.0.1` | Bind address |
| `--port P` | `9002` | Bind port |
| `-v` | off | Log every frame |

## Behaviour

- Echoes every text and binary message back byte-identically, including
  messages the client fragmented.
- Answers `Ping` with `Pong`.
- Accepts the subprotocol `echo.v1`. If the client offers it, the negotiated
  value comes back in the handshake and `ws:GetSelectedProtocol()` reports it.
- On the text message `close-me`, initiates a server close with code `4000`
  and reason `requested`, so the client sees
  `Closed(4000, "requested", wasClean = true)`.
- Rejects an unmasked client frame outright — masking every client-to-server
  frame is required by RFC 6455 §5.3, and this is one of the things the test
  is here to catch.

## Test script

The matching self-checking Lua script lives in the Websocket test project at
`Scripts/WsEchoTest.lua`. It logs `WS-TEST PASS` or `WS-TEST FAIL <step>`.

## Not a production server

Plaintext `ws://` on loopback, no authentication, no origin checks, no limits.
It exists to test a client. Do not expose it.
