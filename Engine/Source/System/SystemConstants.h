#pragma once

// ENDIAN_SWAP — file format (asset .oct) is little-endian per editor-host
// convention. Big-endian platforms (GameCube/Wii via libogc, N64, PS3/Cell
// PPU) need to swap on read/write through Stream<T>. Enable for any platform
// whose CPU is big-endian: the explicit platform list PLUS a compiler-detected
// big-endian check (covers PS3, which is a POLYPHASE_PLATFORM_ADDON with no
// Platform enum value). The `defined(__BYTE_ORDER__)` guard keeps MSVC (which
// doesn't define it) on the little-endian default.
#if PLATFORM_DOLPHIN || PLATFORM_N64 || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define ENDIAN_SWAP 1
#else
#define ENDIAN_SWAP 0
#endif