// External/mbedtls/include/mbedtls/mbedtls_user_config.h
  //
  // Embedded-friendly mbedTLS config for Wii/GCN (libogc).
  // Applied on top of the default mbedtls_config.h.

  // Networking — we provide our own BIO via mbedtls_ssl_set_bio in
  // HttpBackend_Dolphin.cpp, so the built-in POSIX net layer (which doesn't
  // match libogc's `net_*` API) is removed.
  #undef MBEDTLS_NET_C

  // Filesystem — libogc doesn't expose fopen/fseek the way mbedTLS expects.
  #undef MBEDTLS_FS_IO

  // Threading — single-threaded HTTP worker; no mutex layer needed.
  #undef MBEDTLS_THREADING_C
  #undef MBEDTLS_THREADING_PTHREAD

  // Timing helpers — mbedTLS's timing.c uses POSIX clocks not present on libogc.
  #undef MBEDTLS_TIMING_C
  #undef MBEDTLS_HAVE_TIME_DATE   // lets cert verify run without a real-time clock

  // Trim the cipher / curve menu to what current public APIs actually present:
  // AES-GCM + ECDHE + RSA + a couple of curves. Drop legacy / niche stuff.
  #undef MBEDTLS_DES_C
  #undef MBEDTLS_BLOWFISH_C
  #undef MBEDTLS_CAMELLIA_C
  #undef MBEDTLS_ARIA_C
  #undef MBEDTLS_CHACHAPOLY_C      // optional: keep if you talk to APIs that prefer ChaCha
  #undef MBEDTLS_CHACHA20_C
  #undef MBEDTLS_POLY1305_C

  // DTLS isn't used.
  #undef MBEDTLS_SSL_PROTO_DTLS
  #undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
  #undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
  #undef MBEDTLS_SSL_DTLS_CONNECTION_ID