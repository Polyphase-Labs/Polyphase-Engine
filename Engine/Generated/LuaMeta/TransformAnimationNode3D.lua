--- @meta

---@class TransformAnimationNode3D : Node
TransformAnimationNode3D = {}

---@param arg1? Asset
function TransformAnimationNode3D:Play(arg1) end

function TransformAnimationNode3D:Pause() end

function TransformAnimationNode3D:Stop() end

---@param arg1? Asset
function TransformAnimationNode3D:SetAnimation(arg1) end

---@return Asset
function TransformAnimationNode3D:GetAnimation() end

function TransformAnimationNode3D:SetKeyframes() end

---@param n? Node
function TransformAnimationNode3D:SetTargetNode(n) end

---@return Node
function TransformAnimationNode3D:GetTargetNode() end

function TransformAnimationNode3D:ApplyKeyframe() end

function TransformAnimationNode3D:SampleNow() end

function TransformAnimationNode3D:SetTime() end

---@return number
function TransformAnimationNode3D:GetTime() end

---@return number
function TransformAnimationNode3D:GetDuration() end

---@return boolean
function TransformAnimationNode3D:IsPlaying() end

---@return boolean
function TransformAnimationNode3D:IsPaused() end

---@return number
function TransformAnimationNode3D:GetProgress() end

---@param loop boolean
function TransformAnimationNode3D:SetLoop(loop) end

---@return boolean
function TransformAnimationNode3D:IsLooping() end

function TransformAnimationNode3D:SetPlayRate() end

---@return number
function TransformAnimationNode3D:GetPlayRate() end

---@param play boolean
function TransformAnimationNode3D:SetPlayOnStart(play) end

---@return boolean
function TransformAnimationNode3D:GetPlayOnStart() end
