#pragma once

#if PLATFORM_WINDOWS
#define AUDIO_MAX_VOICES 8
#elif PLATFORM_LINUX
#define AUDIO_MAX_VOICES 8
#elif PLATFORM_ANDROID
#define AUDIO_MAX_VOICES 8
#elif PLATFORM_DOLPHIN
#define AUDIO_MAX_VOICES 8
#elif PLATFORM_3DS
#define AUDIO_MAX_VOICES 8
#elif defined(POLYPHASE_PLATFORM_ADDON)
// PSP via sceAudio has 8 hardware channels; pick that as the default for
// addon-provided platforms unless the addon overrides this value.
#define AUDIO_MAX_VOICES 8
#endif