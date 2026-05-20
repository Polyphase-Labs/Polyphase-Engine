#pragma once

#define TEXT_VERTS_PER_CHAR 6

#if API_VULKAN
#define MAX_FRAMES 2
#define MAX_MESH_VERTEX_COUNT 4294967295
#define SYNC_ON_END_FRAME 1
#define SUPPORTS_SECOND_SCREEN 0
#define MAX_GPU_BONES 64
#elif API_GX
#define MAX_FRAMES 1
#define MAX_MESH_VERTEX_COUNT 65535
#define SYNC_ON_END_FRAME 1
#define SUPPORTS_SECOND_SCREEN 0
#define MAX_GPU_BONES 10
#elif API_C3D
#define MAX_FRAMES 2
#define MAX_MESH_VERTEX_COUNT 65535
#define SYNC_ON_END_FRAME 0
#define SUPPORTS_SECOND_SCREEN 1
#define MAX_GPU_BONES 16
#elif API_PSPGU
// PSP (sceGu): 65535-vertex limit per draw call (16-bit indices), 8-bone
// matrices fit in the VFPU vector registers used for hardware skinning,
// single 480×272 screen, single-buffered front-and-back with vblank sync.
#define MAX_FRAMES 1
#define MAX_MESH_VERTEX_COUNT 65535
#define SYNC_ON_END_FRAME 1
#define SUPPORTS_SECOND_SCREEN 0
#define MAX_GPU_BONES 8
#elif defined(POLYPHASE_PLATFORM_ADDON)
// Fallback for addon-provided platforms that haven't set an API_* macro:
// pick conservative limits. Real addons should `#define API_<NAME>` and
// add an arm above.
#define MAX_FRAMES 1
#define MAX_MESH_VERTEX_COUNT 65535
#define SYNC_ON_END_FRAME 1
#define SUPPORTS_SECOND_SCREEN 0
#define MAX_GPU_BONES 8
#endif
