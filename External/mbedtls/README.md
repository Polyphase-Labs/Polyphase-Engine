# mbedTLS (vendored)

This directory holds the [mbedTLS](https://github.com/Mbed-TLS/mbedtls) source
tree plus the Polyphase build wrapper. It's used by `HttpBackend_Dolphin.cpp`
to provide HTTPS on Wii and GameCube — libogc TCP doesn't ship a TLS layer.

mbedTLS is **not** used on Windows (WinHTTP/SChannel), Linux (libcurl/system
OpenSSL), or 3DS (libctru httpc/system TLS). Those backends bring their own.

## Layout

After vendoring + building, this directory looks like:

```
External/mbedtls/
    README.md                                       ← this file
    LICENSE                                         (from mbedTLS)
    build_dolphin.sh                                ← Polyphase wrapper
    polyphase_mbedtls_user_config.h.template        ← template for the user config
    include/                                        (from mbedTLS — public headers)
        mbedtls/
            polyphase_mbedtls_user_config.h         ← copy of the template, see below
            ssl.h, x509_crt.h, ...                  (mbedTLS originals)
        psa/...
    library/                                        (from mbedTLS — .c files + Makefile)
    lib-wii/                                        ← created by build_dolphin.sh wii
        libmbedtls.a, libmbedx509.a, libmbedcrypto.a
    lib-gcn/                                        ← created by build_dolphin.sh gcn
        libmbedtls.a, libmbedx509.a, libmbedcrypto.a
```

Only `README.md`, `build_dolphin.sh`, and `polyphase_mbedtls_user_config.h.template`
are committed to the Polyphase repo. Everything else (mbedTLS source +
toolchain-specific .a files) is produced locally by the steps below.

## Step 1 — Vendor the mbedTLS source

```bash
# from the Polyphase repo root
cd External/mbedtls
git clone --depth 1 --branch v3.6.2 https://github.com/Mbed-TLS/mbedtls.git _clone

# Move the bits we need into place, drop everything else.
mv _clone/include  ./include
mv _clone/library  ./library
mv _clone/LICENSE  ./LICENSE
rm -rf _clone

# Drop the user config into the include path so build_dolphin.sh can find it.
cp polyphase_mbedtls_user_config.h.template include/mbedtls/polyphase_mbedtls_user_config.h
```

(In PowerShell, use `Move-Item` / `Copy-Item` / `Remove-Item -Recurse -Force`.)

Replace `v3.6.2` with the current mbedTLS LTS at vendoring time.

## Step 2 — Build for devkitPPC

`build_dolphin.sh` wraps mbedTLS's own `library/Makefile` with the right
cross-compile environment.

```bash
chmod +x build_dolphin.sh
./build_dolphin.sh wii      # → External/mbedtls/lib-wii/
./build_dolphin.sh gcn      # → External/mbedtls/lib-gcn/
```

You should end up with three archives in each output directory. Total size
should be under ~400 KiB combined; if it's bigger, the user config wasn't
applied — re-check the include/mbedtls/polyphase_mbedtls_user_config.h copy.

## Step 3 — Linker path

`Engine/Makefile_Wii` and `Engine/Makefile_GCN` build `libEngine.a` (a static
archive) — they don't link executables, so `LIBS` / `LIBDIRS` aren't consumed
there. The actual link happens in `Standalone/Makefile_Wii` and
`Standalone/Makefile_GCN`, which already add the mbedTLS lib directories to
`LIBDIRS` and `-lmbedtls -lmbedx509 -lmbedcrypto` to `LIBS`. As long as
`lib-wii/` and `lib-gcn/` contain the three archives, `make -f Makefile_Wii`
in `Standalone/` will pick them up.

## Step 4 — CA bundle

`HttpBackend_Dolphin::Initialize()` reads a CA bundle from
`Engine/CACerts/cacert.pem` (resolved as a Polyphase asset path). Drop the
Mozilla CA bundle there:

```bash
curl -o External/CACerts/cacert.pem https://curl.se/ca/cacert.pem
```

Without it, HTTPS requests on Wii/GCN fall back to no-verify mode and log a
warning at startup.

## Disabling TLS for early bring-up

If you want a Wii / GCN build that compiles *without* mbedTLS (HTTP only),
add `-DPOLYPHASE_DOLPHIN_TLS_ENABLED=0` to `CFLAGS` in `Engine/Makefile_Wii`
and `Engine/Makefile_GCN`. The Dolphin backend will then return
`HttpError::Tls` for any `https://` URL but compile and link clean without the
mbedTLS archives.

In that case, also remove `-lmbedtls -lmbedx509 -lmbedcrypto` from the LIBS
lines and the `MBEDTLS_LIB_DIR` entry from LIBDIRS in the Standalone makefile
to avoid `cannot find -lmbedtls` link errors.
