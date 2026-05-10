# CA Certificates

Trusted root CA bundle for HTTPS verification on Wii and GameCube
(Windows / Linux / 3DS use the host or system trust store).

`HttpBackend_Dolphin::Initialize()` looks for `cacert.pem` here at runtime,
parses it via mbedTLS, and uses the resulting chain to verify every HTTPS
peer.

## How to populate

```bash
curl -o External/CACerts/cacert.pem https://curl.se/ca/cacert.pem
```

The Mozilla CA bundle (curl maintains a clean export) is the canonical
source. Re-run periodically — the bundle expires roots over time.

## Asset shipping

The build packager copies `External/CACerts/cacert.pem` into the shipped
game's asset bundle at the same path. If you change the location, update
`HttpBackend_Dolphin::Initialize` to match.
