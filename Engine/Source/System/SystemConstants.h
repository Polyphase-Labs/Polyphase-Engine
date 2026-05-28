#pragma once

// ENDIAN_SWAP — file format (asset .oct) is little-endian per editor-host
// convention. Big-endian platforms (GameCube/Wii via libogc, N64) need to
// swap on read/write through Stream<T>. Enable for any platform whose CPU
// is big-endian.
#if PLATFORM_DOLPHIN || PLATFORM_N64
#define ENDIAN_SWAP 1
#else
#define ENDIAN_SWAP 0
#endif