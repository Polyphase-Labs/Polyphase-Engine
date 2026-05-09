# External Library Integration

Bundle a third-party library with your addon and link it per-platform via `package.json` overrides. The VideoPlayer addon's FFmpeg integration is the canonical worked example.

## Outcome

- Third-party headers and libs vendored under your addon's `External/` directory.
- Windows links the prebuilt import libs and ships the runtime DLLs alongside the addon binary.
- Linux either pulls the same lib via the system package manager or vendors a copy.
- Console targets (GameCube / Wii / 3DS) skip the third-party lib entirely and fall back to a built-in implementation.
- Code paths gate on a single preprocessor flag (`MYADDON_WITH_FOO`) so the build link-completes on every target.

## Directory layout

```
Packages/com.example.myaddon/
    package.json
    Source/
        Backends/
            IFooBackend.h           (abstract interface)
            FooLibBackend.h/.cpp    (third-party implementation, gated by MYADDON_WITH_FOO)
            NativeBackend.h/.cpp    (no-deps fallback for consoles)
            BackendFactory.cpp      (chooses backend at runtime)
        MyAddon.cpp                 (entry point)
    External/
        foolib/
            include/                (third-party headers)
            lib/                    (prebuilt import libs: foo.lib, ...)
            bin/                    (runtime DLLs: foo-3.dll, ...)
            doc/                    (vendor docs, optional)
        .gitignore                  (ignore foolib/bin/* if licence permits)
```

## `package.json`

```json
{
    "name": "com.example.myaddon",
    "author": "Your Name",
    "description": "Demo addon with external lib integration",
    "version": "0.1.0",
    "native": {
        "target": "engine",
        "sourceDir": "Source",
        "binaryName": "com.example.myaddon",
        "entrySymbol": "PolyphasePlugin_GetDesc",
        "apiVersion": 3
    },
    "nativePerPlatform": {
        "Windows": {
            "extraDefines":     ["MYADDON_WITH_FOO=1"],
            "extraIncludeDirs": ["External/foolib/include"],
            "extraLibDirs":     ["External/foolib/lib"],
            "extraLibs":        ["foo.lib"],
            "copyBinaries":     ["External/foolib/bin"]
        },
        "Linux": {
            "extraDefines": ["MYADDON_WITH_FOO=1"],
            "extraLibs":    ["foo"]
        },
        "GameCube": {},
        "Wii": {},
        "3DS": {}
    }
}
```

What each field does is documented in `NativeAddon.md` *Per-Platform Build Configuration & External Libraries* — this file is the worked example for that section.

The empty objects on console targets are the point. They tell the build system "I know about this platform, I just don't need anything extra" — typos in the platform name are silently ignored, so an absent entry and a typo'd entry behave the same.

## Backend selection at compile time

The third-party backend should be guarded by the same flag the manifest defines:

```cpp
// Source/Backends/FooLibBackend.cpp
#if MYADDON_WITH_FOO

#include "FooLibBackend.h"
#include <foolib/foo.h>     // resolves via the per-platform extraIncludeDirs

bool FooLibBackend::Decode(const uint8_t* in, size_t inSize,
                           std::vector<uint8_t>& out)
{
    foo_context* ctx = foo_open(in, (int)inSize);
    if (!ctx) return false;
    // ... use foolib ...
    foo_close(ctx);
    return true;
}

#endif // MYADDON_WITH_FOO
```

```cpp
// Source/Backends/BackendFactory.cpp
#include "IFooBackend.h"
#if MYADDON_WITH_FOO
#include "FooLibBackend.h"
#endif
#include "NativeBackend.h"

std::unique_ptr<IFooBackend> CreateBackend()
{
#if MYADDON_WITH_FOO
    return std::make_unique<FooLibBackend>();
#else
    return std::make_unique<NativeBackend>();
#endif
}
```

`BackendFactory.cpp` is built on **every** platform; the `#if` decides which class is constructed. This is what keeps the link clean on consoles where `FooLibBackend.cpp` is excluded by its own `#if MYADDON_WITH_FOO`.

## Runtime DLL delivery (Windows only)

`copyBinaries` is post-build copy. On Windows, the build system places the listed files (or every file inside a listed directory) next to the built `.dll` so `LoadLibrary` finds them when the editor or shipped game loads your addon. Without it, the addon DLL loads but throws a delayed-load failure as soon as it tries to resolve a `foolib` import.

For Linux, runtime resolution typically goes through `pkg-config`'s rpath wiring or a system `LD_LIBRARY_PATH` — vendoring `.so` files inside the addon is rarely needed. For consoles, the third-party library is statically linked or absent.

## Don't commit binary blobs

Vendor's distributable terms vary. A common pattern is:

```
External/foolib/.gitignore
    bin/*
    lib/*
    !.gitkeep
```

…and a separate fetch script (PowerShell on Windows, bash on Linux) the developer runs once after cloning. The VideoPlayer addon's `External/ffmpeg/.gitignore` follows this convention.

## Verification

1. Place the third-party headers/libs under `External/foolib/{include,lib,bin}/`.
2. Reload addons (**Tools → Addons → Reload Native Addons**).
3. Confirm the build log shows the include directory and lib being passed to the compiler/linker for the active platform.
4. Confirm the runtime DLL ends up next to your addon's `.dll` (check the build output directory).
5. Switch the active platform target (e.g. via **File → Build Data → GameCube**) and confirm the build link-completes without `foolib`.

## See also

- `Documentation/Development/NativeAddon/NativeAddon.md` — *Per-Platform Build Configuration & External Libraries*.
- `Engine/Source/Editor/Addons/NativeAddonManager.cpp` — manifest parser (`extraDefines` / `nativePerPlatform` resolution around line 950).
- VideoPlayer addon `package.json` — production example of the FFmpeg-on-Windows pattern.
