# Audio Visualizer

The engine exposes a small platform-independent audio-analysis API on top of `Audio.h` so you can build real-time visualizers — VU meters, bass/mid/treble bars, full spectrum displays — from either C++ or Lua, on any Polyphase target.

The math runs above the platform audio backend (XAudio2, ALSA/PulseAudio, ASND on Wii/GameCube, NDSP on 3DS, custom console-addon backends). Static SoundWave voices (anything you play with `AudioManager` / `Audio3D`) work everywhere without backend changes; streaming voices (push-PCM from the VideoPlayer addon and similar) need a tiny one-line hook in the platform backend, which the engine ships for Windows / Linux / Wii / 3DS out of the box.

## What you get

| Operation             | Lua (free)                                  | Lua (`Audio3D`)                 | C++                                          |
| --------------------- | ------------------------------------------- | ------------------------------- | -------------------------------------------- |
| RMS                   | `Audio.GetRMS(voice)`                       | `audio3d:GetRMS()`              | `AUD_GetRMS(voice)`                          |
| Normalized loudness   | `Audio.GetLoudness(voice)`                  | `audio3d:GetLoudness()`         | `AUD_GetLoudness(voice)`                     |
| Loudness in dBFS      | `Audio.GetLoudnessDb(voice)`                | `audio3d:GetLoudnessDb()`       | `AUD_GetLoudnessDb(voice)`                   |
| Band magnitude        | `Audio.GetFrequencies(voice, s, e)`         | `audio3d:GetFrequencies(s, e)`  | `AUD_GetFrequencies(voice, s, e)`            |
| Binned spectrum       | `Audio.GetSpectrum(voice, s, e, n)`         | `audio3d:GetSpectrum(s, e, n)`  | `AUD_GetSpectrum(voice, s, e, bins, n)`      |
| Streaming variant     | `Audio.GetStream*(streamId, ...)`           | —                               | `AUD_GetStream*(streamId, ...)`              |

All functions return `0.0` (or zero-filled arrays) when the queried voice is idle or unknown. You never have to gate visualizer code on `IsPlaying()`.

Loudness scales:

- `GetLoudness` returns `[0, 1]` mapped from a -60 dB floor (i.e. `1.0 - (dB / -60.0)`). Drop-in for bar heights.
- `GetLoudnessDb` returns the raw dBFS value clamped to `[-60, 0]`.

`GetSpectrum`'s output array is averaged across the requested frequency range, so visualisers don't need to know the underlying FFT size.

## Quick recipe — Lua bass / mid / treble bars

Drop this on any Lua script attached to a node that holds an `Audio3D` reference (or any pawn that exposes one):

```lua
function MyVis:Tick(dt)
    local bass = self.audio:GetFrequencies(20, 250)
    local mid  = self.audio:GetFrequencies(250, 2000)
    local high = self.audio:GetFrequencies(2000, 16000)

    -- draw three bars however your UI does it
    self.barBass:SetSize(self.barBass:GetWidth(),  bass * 400)
    self.barMid:SetSize (self.barMid:GetWidth(),   mid  * 400)
    self.barHigh:SetSize(self.barHigh:GetWidth(),  high * 400)
end
```

## Quick recipe — Lua full-spectrum bar chart

```lua
function MySpectrum:Tick(dt)
    local bins = self.audio:GetSpectrum(20, 20000, 64) -- 64 magnitudes across the audible band
    for i = 1, #bins do
        self.bars[i]:SetSize(self.bars[i]:GetWidth(), bins[i] * 300)
    end
end
```

`GetSpectrum` returns a regular Lua table — `#bins` is the array length you asked for and `bins[i]` is `1`-indexed.

## C++ recipe

```cpp
#include "Audio/Audio.h"

void MyVis::Tick(float dt)
{
    const uint32_t voice = mVoiceIndex;     // however you stash it
    const float    rms    = AUD_GetRMS(voice);
    const float    bass   = AUD_GetFrequencies(voice,   20.0f,   250.0f);
    const float    mid    = AUD_GetFrequencies(voice,  250.0f,  2000.0f);

    float spectrum[64] = {};
    AUD_GetSpectrum(voice, 20.0f, 20000.0f, spectrum, 64);
    // … draw …
}
```

If your code already holds an `Audio3D*`, prefer the per-node methods — voice-index lookup is automatic:

```cpp
audio3d->GetFrequencies(20.0f, 250.0f);
audio3d->GetSpectrum(20.0f, 20000.0f, spectrum, 64);
```

## Streaming visualizers (VideoPlayer, push-PCM)

Push-PCM sources call `AUD_OpenStream` and get back a `streamId`. Pass that id to the `Stream` variants:

```cpp
uint32_t streamId = AUD_OpenStream(48000, 2, 16);
// … decode + AUD_SubmitStreamBuffer(streamId, pcm, size) per video frame …
float magnitude = AUD_GetStreamFrequencies(streamId, 20.0f, 250.0f);
```

From Lua:

```lua
local rms = Audio.GetStreamRMS(streamId)
```

The engine keeps roughly the last second of PCM in a per-stream ring buffer (see memory notes below) so analysis follows the live audio.

## Performance & memory notes

The visualizer pipeline trades a small amount of memory and per-frame CPU for full cross-platform support. Tunables live in `Engine/Source/Engine/Constants.h`; per-platform overrides go in your platform's `Constants_<Plat>.h` (same idiom as `LUA_ENABLED`):

```cpp
#define AUDIO_ANALYSIS_ENABLED            1        // master switch
#define AUDIO_ANALYSIS_STREAMS_ENABLED    1        // streaming ring buffers
#define AUDIO_FFT_SIZE                    512      // power of two
#define AUDIO_ANALYSIS_STREAM_SECONDS     1.0f     // ring depth per stream
#define AUDIO_ANALYSIS_MAX_STREAMS        4
```

Memory budget at defaults:

| Slot                              | Size                          | Notes                                                          |
| --------------------------------- | ----------------------------- | -------------------------------------------------------------- |
| Hann window LUT                   | `~2 KB`                       | Lazy-init, static.                                             |
| Per-voice / per-stream cache      | `~36 KB`                      | Stores last computed spectrum + key tuple.                     |
| FFT scratch                       | `~4 KB` (on stack)            | No persistent cost.                                            |
| Streaming ring (per active stream)| `~192 KB` (48 kHz / stereo / 16-bit / 1 s) | **Biggest item.** Sized by sample-rate × channels × seconds. |

### Low-RAM consoles

| Platform              | Suggested overrides                                                                                                  |
| --------------------- | -------------------------------------------------------------------------------------------------------------------- |
| **PSP** (32 MB)       | `AUDIO_FFT_SIZE 256`, `AUDIO_ANALYSIS_STREAM_SECONDS 0.25f` → static voices ~9 KB, streams ~12 KB each. Or `AUDIO_ANALYSIS_STREAMS_ENABLED 0`. |
| **3DS** (64–128 MB)   | Defaults usually fine; `AUDIO_FFT_SIZE 256` for cache savings.                                                       |
| **GameCube** (24 + 16 MB) | `AUDIO_FFT_SIZE 256`, `AUDIO_ANALYSIS_STREAM_SECONDS 0.25f`.                                                     |
| **Dreamcast** (16 MB) | `AUDIO_ANALYSIS_ENABLED 0` is the safe default until budget audit.                                                   |
| Desktop / Android     | Defaults.                                                                                                            |

Disabling the subsystem keeps every call signature intact — they just resolve to constant-`0` inline stubs. Lua scripts that call `Audio.GetRMS` etc. continue to load and run unchanged across configurations.

## Platform support matrix

| Platform                       | Static voices | Streaming voices                                                              |
| ------------------------------ | ------------- | ----------------------------------------------------------------------------- |
| Windows / Linux                | ✅            | ✅                                                                            |
| Wii / GameCube (Dolphin)       | ✅            | ✅                                                                            |
| 3DS                            | ✅            | ✅                                                                            |
| Android                        | ✅            | ❌ — backend has no streaming                                                 |
| PSP / custom build-target addons | ✅          | ✅ if the addon wires `AudioAnalysis::OnStreamOpened/Closed/Submitted`        |

Authors of new build-target addons: the three required stream hooks are documented in `.claude/skills/polyphase-buildtarget/SKILL.md` under "Audio analysis hook (mandatory for streaming voices)".

## Limitations

- The loudness map is `20*log10(rms)` clamped to `[-60, 0]` — not ITU-R BS.1770 LUFS or A-weighted SPL. Fine for visualizers; do not use as a mastering reference.
- FFT size is a compile-time constant. Different sizes need a rebuild.
- The per-frame FFT cache lives between calls *within* the same frame (so two `GetFrequencies` calls in one tick share one FFT) but recomputes on the next frame.
- DSP-accelerated FFT on Wii AX / 3DS DSP is out of scope — the pure-C reference runs well inside per-frame budget at typical visualizer rates.
