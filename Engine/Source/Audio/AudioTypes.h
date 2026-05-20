#pragma once

#include "Constants.h"

// Variant 2 platform-extension arm — see SystemTypes.h for the full rationale.
#if defined(POLYPHASE_PLATFORM_ADDON)
#include "PolyphasePlatform_AudioTypes.h"
#elif PLATFORM_WINDOWS
#include <xaudio2.h>
#endif
