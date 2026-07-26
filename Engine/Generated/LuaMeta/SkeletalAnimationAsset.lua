--- @meta

---@class SkeletalAnimationAsset : Asset
SkeletalAnimationAsset = {}

---@return string
function SkeletalAnimationAsset:GetClipName() end

---@return number
function SkeletalAnimationAsset:GetDuration() end

---@return number
function SkeletalAnimationAsset:GetDurationSeconds() end

---@return number
function SkeletalAnimationAsset:GetTicksPerSecond() end

---@return string
function SkeletalAnimationAsset:GetSourceRigName() end

---@return integer
function SkeletalAnimationAsset:GetNumChannels() end

---@param index integer
---@return nil
function SkeletalAnimationAsset:GetChannelBoneName(index) end
