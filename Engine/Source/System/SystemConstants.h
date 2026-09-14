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

// POLYPHASE_THREAD_LOCAL — `thread_local`, except on targets with no threads.
//
// `thread_local` is not free: it requires the C runtime to establish a thread
// pointer (r2 on PowerPC32, r13 on PPC64, %fs on x86-64) and an initialised
// TLS block. A bare-metal SDK that never spawns threads generally does not
// bother, and then the FIRST access to any thread_local dereferences an
// uninitialised thread pointer.
//
// That is not hypothetical. On Xbox 360 (libxenon) there is no TLS setup
// anywhere in the SDK -- its `_start` never touches r2 -- yet the ELF still
// carries a PT_TLS segment for the engine's own thread_locals. Boot died
// inside Widget::MarkDirty, whose depth guard is a `static thread_local`, on
// the very first widget constructed. It cost a long bisect because the failure
// is a silent memory fault with no diagnostic, and it affects real hardware
// exactly as much as the emulator.
//
// On a platform where SYS_CreateThread cannot create a thread, a plain static
// is semantically identical to a thread_local -- there is only ever one
// thread -- so this degrades cleanly rather than disabling a safety check.
//
// Keep this list in sync with platforms whose SYS_CreateThread returns null.
#if PLATFORM_XBOX360 || PLATFORM_WEB
#define POLYPHASE_THREAD_LOCAL
#else
#define POLYPHASE_THREAD_LOCAL thread_local
#endif