--- @meta

---@class TransformAnimationAsset : Asset
TransformAnimationAsset = {}

function TransformAnimationAsset:Sample() end

---@return number
function TransformAnimationAsset:GetDuration() end

function TransformAnimationAsset:SetDuration() end

---@return integer
function TransformAnimationAsset:GetKeyframeCount() end

---@return nil
function TransformAnimationAsset:GetKeyframe() end

---@return boolean
function TransformAnimationAsset:IsLooping() end

function TransformAnimationAsset:SetLooping() end

---@return number
function TransformAnimationAsset:GetPlayRate() end

function TransformAnimationAsset:SetPlayRate() end

function TransformAnimationAsset:AddKeyframe() end

function TransformAnimationAsset:RemoveKeyframe() end

function TransformAnimationAsset:ClearKeyframes() end
