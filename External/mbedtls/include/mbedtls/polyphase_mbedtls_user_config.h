// Embedded-friendly mbedTLS config for Wii / GameCube (libogc).
//
// AFTER VENDORING mbedTLS source, copy this file to:
//   External/mbedtls/include/mbedtls/polyphase_mbedtls_user_config.h
//
// It's referenced by build_dolphin.sh via -DMBEDTLS_USER_CONFIG_FILE.
// mbedTLS's default mbedtls_config.h ends with:
//
//     #if defined(MBEDTLS_USER_CONFIG_FILE)
//     #include MBEDTLS_USER_CONFIG_FILE
//     #endif
//
// so anything we #undef here overrides the default.

// ---- POSIX-y bits libogc doesn't supply ----------------------------------
// Networking: HttpBackend_Dolphin uses NET_Socket* + mbedtls_ssl_set_bio,
// not mbedTLS's own net_sockets.c (which expects sys/socket.h semantics
// that libogc only partially matches).
#undef MBEDTLS_NET_C

// Filesystem: libogc has libfat fopen but mbedTLS's helpers expect POSIX
// stat/fseeko, which devkitPPC's newlib doesn't expose cleanly.
#undef MBEDTLS_FS_IO

// Threading: HttpClient already runs requests on a single dedicated worker
// thread — mbedTLS contexts are owned by it for their lifetime, no need for
// internal mutexes.
#undef MBEDTLS_THREADING_C
#undef MBEDTLS_THREADING_PTHREAD

// Real-time clock: console without RTC. Disabling these lets cert chain
// validation skip the not-before / not-after window check and prevents
// platform_util.c from trying to compile mbedtls_ms_time(), which devkitPPC's
// newlib can't satisfy (no _POSIX_TIMERS, no clock_gettime).
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_HAVE_TIME
#undef MBEDTLS_HAVE_TIME_DATE

// ---- Footprint trimming --------------------------------------------------
// Modern HTTPS endpoints negotiate AES-GCM + ECDHE + RSA/ECDSA + a couple
// of curves. The defaults pull in DES / Blowfish / Camellia / ARIA / ChaCha
// / Poly1305 — none of which a Polyphase Wii game would negotiate first.
// Drop them to shave ~300+ KiB off the linked image.
#undef MBEDTLS_DES_C
#undef MBEDTLS_BLOWFISH_C
#undef MBEDTLS_CAMELLIA_C
#undef MBEDTLS_ARIA_C
// Keep ChaCha20-Poly1305 if you talk to APIs that prefer it on mobile-style
// negotiations — comment these two #undefs back out if you see TLS handshake
// failures with cipher_suite errors.
#undef MBEDTLS_CHACHAPOLY_C
#undef MBEDTLS_CHACHA20_C
#undef MBEDTLS_POLY1305_C

// DTLS isn't used by HttpBackend_Dolphin.
#undef MBEDTLS_SSL_PROTO_DTLS
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID
#undef MBEDTLS_SSL_DTLS_SRTP

// We're a TLS client, never a server.
#undef MBEDTLS_SSL_SRV_C

// PSA crypto stays ENABLED — TLS 1.3 in mbedTLS 3.x routes its primitives
// through PSA, and the LMS module also requires it. Disabling PSA cascades
// into check_config.h failures. The PSA file-storage backend, on the other
// hand, requires MBEDTLS_FS_IO (which we turned off), so it must go.
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C

// LMS (Leighton-Micali Hash-Based Signatures) is a post-quantum signature
// scheme. Not used by HTTPS — drop it to save the table data.
#undef MBEDTLS_LMS_C
#undef MBEDTLS_LMS_PRIVATE

// ---- Entropy: platform doesn't have /dev/urandom or BCryptGenRandom ------
// mbedTLS's built-in entropy_poll.c only knows how to read entropy on Unix
// and Windows, and refuses to compile on anything else. We replace it with
// our own hardware_poll callback (defined in HttpBackend_Dolphin.cpp) that
// derives entropy from libogc's gettime/gettick counters.
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
