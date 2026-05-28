# Audio

System to play and manage sounds.

---
### PlaySound2D
Play a sound with no positional data.
TODO: Rename to PlaySound() if we add 2D support.

Sig: `Audio.PlaySound2D(sound, volume=1, pitch=1, startTime=0, loop=false, priority=0)`
 - Arg: `SoundWave sound` Sound wave to play
 - Arg: `number volume` Volume multiplier
 - Arg: `number pitch` Pitch multiplier
 - Arg: `number startTime` Start time offset
 - Arg: `boolean loop` Loop sound
 - Arg: `integer priority` Sound priority
---
### PlaySound3D
Play a sound at a position in 3D space.
TODO: Rename to PlaySoundAtPosition() to handle both 2D and 3D.

Sig: `Audio.PlaySound3D(sound, position, innerRadius, outerRadius, attenuationFunc=AttenuationFunc.Linear, volume=1, pitch=1, startTime=0, loop=false, priority=0)`
 - Arg: `SoundWave sound` Sound wave to play
 - Arg: `Vector position` Position of sound
 - Arg: `number innerRadius` Inner radius
 - Arg: `number outerRadius` Outer radius
 - Arg: `AttenuationFunc(integer) attenuationFunc` Attenuation function
 - Arg: `number volume` Volume multiplier
 - Arg: `number pitch` Pitch multiplier
 - Arg: `number startTime` Start time offset
 - Arg: `boolean loop` Loop sound
 - Arg: `integer priority` Sound priority
---
### StopSounds
Stop sounds using a particular sound wave.

Alias: `StopSound`

Sig: `Audio.StopSounds(sound)`
 - Arg: `SoundWave sound` Sound wave to stop
---
### StopAllSounds
Stop all sounds.

Sig: `Audio.StopAllSounds()`

---
### UpdateSound
Updates volume, pitch, and priority for a currently playing sound wave. Note: This will update volume/pitch/priority for all currently playing sounds of the same sound wave asset.

Sig: `Audio.UpdateSound(sound, volume, pitch, priority=0)`
- Arg: `SoundWave sound` Sound wave to update
- Arg: `number volume` New volume
- Arg: `number pitch` New pitch
- Arg: `integer priority` New priority

---
### IsSoundPlaying
Check if a sound wave is playing.

Sig: `playing = Audio.IsSoundPlaying(sound)`
 - Arg: `SoundWave sound` Sound wave to check
 - Ret: `boolean playing` Is the sound playing
---
### SetAudioClassVolume
Set the volume of an audio class. Audio classes can be used to control the volume and pitch of multiple sounds.

Sig: `Audio.SetAudioClassVolume(volume)`
 - Arg: `number volume` Volume multiplier
---
### GetAudioClassVolume
Get the volume of an audio class. Audio classes can be used to control the volume and pitch of multiple sounds.

Sig: `volume = Audio.GetAudioClassVolume()`
 - Ret: `number volume` Volume multiplier
---
### SetAudioClassPitch
Set the pitch of an audio class. Audio classes can be used to control the volume and pitch of multiple sounds.

Sig: `Audio.SetAudioClassPitch(pitch)`
 - Arg: `number pitch` Pitch multiplier
---
### GetAudioClassPitch
Get the pitch of an audio class. Audio classes can be used to control the volume and pitch of multiple sounds.

Sig: `pitch = Audio.GetAudioClassPitch()`
 - Ret: `number pitch` Pitch multiplier
---
### SetMasterVolume
Set the master volume.

Sig: `Audio.SetMasterVolume(volume)`
 - Arg: `number volume` Master volume
---
### GetMasterVolume
Get the master volume.

Sig: `volume = Audio.GetMasterVolume()`
 - Ret: `number volume` Master volume
---
### SetMasterPitch
Set the master pitch.

Sig: `Audio.SetMasterPitch(pitch)`
 - Arg: `number pitch` Master pitch
---
### GetMasterPitch
Get the master pitch.

Sig: `pitch = Audio.GetMasterPitch()`
 - Ret: `number pitch` Master pitch
---

## Switching the SoundWave on a playing `Audio3D`

`Audio3D:SetSoundWave(newWave)` automatically releases the live voice when the wave actually changes, so the intuitive pattern just works on every click:

```lua
self.audioPlayer:SetSoundWave(LoadAsset(nextTrack))
self.audioPlayer:PlayAudio()
```

Internally, if a different wave is passed in while the node is currently audible, `SetSoundWave` synchronously calls `AudioManager::StopComponent(this)` and zeroes `mPlayTime`. The next `AudioManager::Update` tick then sees `IsPlaying() && !IsAudible()` and spawns a fresh voice bound to the new wave. Passing the **same** wave is a no-op — playback is not interrupted.

You almost never need `ResetAudio()` in script code anymore — it's only useful if you want to hard-kill the voice without changing the wave.

| Goal | Call sequence |
|------|---------------|
| Start a track on an idle `Audio3D` | `SetSoundWave(w)` → `PlayAudio()` |
| Switch the track on a playing `Audio3D` | `SetSoundWave(w)` → `PlayAudio()` *(voice swap is automatic)* |
| Pause but keep position (resume later) | `PauseAudio()` |
| Stop and rewind | `StopAudio()` |
| Hard-kill the voice right now | `ResetAudio()` |

For one-shot sounds that don't need a node, prefer `Audio.PlaySound2D` / `Audio.PlaySound3D` — they always allocate a fresh voice.

---

## Signals

The audio system broadcasts the following signal through the global [SignalBus](SignalBus.md). Subscribe to it from any node — looping sounds do **not** emit (they never naturally end); only sounds that play to completion fire it. User-initiated stops (`StopSound`, `StopSounds`, `StopAllSounds`, `audio3d:StopAudio`) do **not** fire it either.

| Signal | Emitted When | Args |
|--------|--------------|------|
| `SoundFinished` | A non-looping voice plays to its natural end. Fires for both `Audio.PlaySound2D` / `PlaySound3D` voices and `Audio3D`-driven voices. | `SoundWave soundWave` — the asset that just finished. |

Example:

```lua
function MyMusicController:Start()
    SignalBus.Subscribe("SoundFinished", self, function(self, soundWave)
        if soundWave == self.musicTrack then
            self:PlayNextTrack()
        end
    end)
end
```

For per-node notifications on an `Audio3D` (without bus filtering), connect to that node's `OnFinished` signal instead — see [Audio3D Signals](../Nodes/3D/Audio3D.md#signals).

---

## Audio analysis

Real-time analysis helpers used to build visualizers (bass/mid/treble bars, spectrum graphs, loudness meters). All work on every Polyphase target — Windows, Linux, Android, GameCube/Wii, 3DS, plus any console addon that wires the stream hooks. They return `0` (or zero-filled tables) when nothing is playing on the queried voice, so visualizer code never has to special-case idle audio.

**Voice index vs. streamId.** A "voice index" is a slot in the engine's audio mixer pool — voices spawned through `Audio.PlaySound2D`/`PlaySound3D` and `Audio3D` nodes consume one. If your script already holds an `Audio3D` reference, prefer `audio3d:GetRMS()` etc. — they look up the voice index for you. A "streamId" is the value returned by `AUD_OpenStream`, used by push-PCM sources like the VideoPlayer addon.

---
### GetRMS
Root-mean-square amplitude of the voice's most recent playback window. Returns `0.0` when the voice is idle.

Sig: `rms = Audio.GetRMS(voiceIndex)`
 - Arg: `integer voiceIndex` Voice slot
 - Ret: `number rms` RMS amplitude in `[0, 1]`
---
### GetLoudness
Perceptual loudness in `[0, 1]`, computed from RMS with a -60 dB floor. Drop-in for bar heights.

Sig: `loudness = Audio.GetLoudness(voiceIndex)`
 - Arg: `integer voiceIndex` Voice slot
 - Ret: `number loudness` Normalized loudness in `[0, 1]`
---
### GetLoudnessDb
Raw dBFS loudness clamped to `[-60, 0]`. Use when you need an engineering-grade signal level.

Sig: `dB = Audio.GetLoudnessDb(voiceIndex)`
 - Arg: `integer voiceIndex` Voice slot
 - Ret: `number dB` Loudness in dBFS, clamped `[-60, 0]`
---
### GetFrequencies
Average FFT magnitude in the band `[startHz, endHz]`. Perfect for 3-bar bass/mid/treble visualizers.

Sig: `magnitude = Audio.GetFrequencies(voiceIndex, startHz, endHz)`
 - Arg: `integer voiceIndex` Voice slot
 - Arg: `number startHz` Lower edge of the band (Hz)
 - Arg: `number endHz` Upper edge of the band (Hz)
 - Ret: `number magnitude` Average band magnitude
---
### GetSpectrum
FFT magnitudes binned across `[startHz, endHz]`. Returns a Lua array of `numBins` floats for spectrum-style displays.

Sig: `bins = Audio.GetSpectrum(voiceIndex, startHz, endHz, numBins)`
 - Arg: `integer voiceIndex` Voice slot
 - Arg: `number startHz` Lower edge of the range (Hz)
 - Arg: `number endHz` Upper edge of the range (Hz)
 - Arg: `integer numBins` Number of output bins (1..1024)
 - Ret: `{ number, ... } bins` Array of magnitudes, length `numBins`
---
### GetStreamRMS
RMS for a streaming voice. Same semantics as `GetRMS` but indexed by the streamId returned from `AUD_OpenStream`.

Sig: `rms = Audio.GetStreamRMS(streamId)`
 - Arg: `integer streamId` Streaming voice id
 - Ret: `number rms` RMS amplitude in `[0, 1]`
---
### GetStreamLoudness
Normalized `[0, 1]` loudness for a streaming voice.

Sig: `loudness = Audio.GetStreamLoudness(streamId)`
 - Arg: `integer streamId` Streaming voice id
 - Ret: `number loudness` Normalized loudness
---
### GetStreamLoudnessDb
Raw dBFS loudness for a streaming voice (clamped `[-60, 0]`).

Sig: `dB = Audio.GetStreamLoudnessDb(streamId)`
 - Arg: `integer streamId` Streaming voice id
 - Ret: `number dB` Loudness in dBFS
---
### GetStreamFrequencies
Average band magnitude for a streaming voice.

Sig: `magnitude = Audio.GetStreamFrequencies(streamId, startHz, endHz)`
 - Arg: `integer streamId` Streaming voice id
 - Arg: `number startHz` Lower edge of the band (Hz)
 - Arg: `number endHz` Upper edge of the band (Hz)
 - Ret: `number magnitude` Average band magnitude
---
### GetStreamSpectrum
Binned spectrum for a streaming voice.

Sig: `bins = Audio.GetStreamSpectrum(streamId, startHz, endHz, numBins)`
 - Arg: `integer streamId` Streaming voice id
 - Arg: `number startHz` Lower edge of the range (Hz)
 - Arg: `number endHz` Upper edge of the range (Hz)
 - Arg: `integer numBins` Number of output bins (1..1024)
 - Ret: `{ number, ... } bins` Array of magnitudes, length `numBins`
---
