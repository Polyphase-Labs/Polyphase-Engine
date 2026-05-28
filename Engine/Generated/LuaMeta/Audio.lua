--- @meta

---@class AudioModule
Audio = {}

---@param soundWave SoundWave
---@param volume? number
---@param pitch? number
---@param startTime? number
---@param loop? boolean
---@param priority? integer
function Audio.PlaySound2D(soundWave, volume, pitch, startTime, loop, priority) end

---@param soundWave SoundWave
---@param pos Vector
---@param innerRadius number
---@param outerRadius number
---@param attenFunc? integer
---@param volume? number
---@param pitch? number
---@param startTime? number
---@param loop? boolean
---@param priority? integer
function Audio.PlaySound3D(soundWave, pos, innerRadius, outerRadius, attenFunc, volume, pitch, startTime, loop, priority) end

---@param soundWave SoundWave
function Audio.StopSounds(soundWave) end

---@param soundWave SoundWave
---@param volume number
---@param pitch number
---@param priority integer
function Audio.UpdateSound(soundWave, volume, pitch, priority) end

function Audio.StopAllSounds() end

---@param soundWave SoundWave
---@return boolean
function Audio.IsSoundPlaying(soundWave) end

---@param audioClass integer
---@param volume number
function Audio.SetAudioClassVolume(audioClass, volume) end

---@param audioClass integer
---@return number
function Audio.GetAudioClassVolume(audioClass) end

---@param audioClass integer
---@param pitch number
function Audio.SetAudioClassPitch(audioClass, pitch) end

---@param audioClass integer
---@return number
function Audio.GetAudioClassPitch(audioClass) end

---@param value number
function Audio.SetMasterVolume(value) end

---@return number
function Audio.GetMasterVolume() end

---@param value number
function Audio.SetMasterPitch(value) end

---@return number
function Audio.GetMasterPitch() end

---@param voice integer
---@return number
function Audio.GetDuration(voice) end

---@param voice integer
---@return number
function Audio.GetPlayTimeNormalized(voice) end

---@param voice integer
---@return number
function Audio.GetRMS(voice) end

---@param voice integer
---@return number
function Audio.GetLoudness(voice) end

---@param voice integer
---@return number
function Audio.GetLoudnessDb(voice) end

---@param voice integer
---@param startHz number
---@param endHz number
---@return number
function Audio.GetFrequencies(voice, startHz, endHz) end

---@param voice integer
---@param startHz number
---@param endHz number
---@param numBins integer
---@return table
function Audio.GetSpectrum(voice, startHz, endHz, numBins) end

---@param streamId integer
---@return number
function Audio.GetStreamRMS(streamId) end

---@param streamId integer
---@return number
function Audio.GetStreamLoudness(streamId) end

---@param streamId integer
---@return number
function Audio.GetStreamLoudnessDb(streamId) end

---@param streamId integer
---@param startHz number
---@param endHz number
---@return number
function Audio.GetStreamFrequencies(streamId, startHz, endHz) end

---@param streamId integer
---@param startHz number
---@param endHz number
---@param numBins integer
---@return table
function Audio.GetStreamSpectrum(streamId, startHz, endHz, numBins) end

---@param soundWave SoundWave
function Audio.StopSound(soundWave) end
