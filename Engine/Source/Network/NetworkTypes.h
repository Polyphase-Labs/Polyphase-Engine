#pragma once

#include <stdint.h>

// Variant 2 platform-extension arm — see SystemTypes.h for the full rationale.
// NetworkTypes_Platform.h is optional; ActionManager generates a stub bridge
// when the addon doesn't supply one (engine code then degrades to "no
// networking" via standard SocketHandle == -1 sentinels).
#if defined(POLYPHASE_PLATFORM_ADDON)
#include "PolyphasePlatform_NetworkTypes.h"
#elif PLATFORM_WINDOWS
#include <winsock.h>
#elif PLATFORM_LINUX
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#elif PLATFORM_ANDROID
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#elif PLATFORM_3DS
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#elif PLATFORM_DOLPHIN
#include <network.h>
#elif PLATFORM_ANDROID
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#endif

#if defined(POLYPHASE_PLATFORM_ADDON)
// SocketHandle typedef supplied by PolyphasePlatform_NetworkTypes.h above.
// (Addons that stub networking should `typedef int32_t SocketHandle;`.)
#elif PLATFORM_WINDOWS
    typedef SOCKET SocketHandle;
#elif PLATFORM_LINUX
    typedef int32_t SocketHandle;
#elif PLATFORM_ANDROID
    typedef int32_t SocketHandle;
#elif PLATFORM_3DS
    typedef int32_t SocketHandle;
#elif PLATFORM_DOLPHIN
    typedef int32_t SocketHandle;
#endif

