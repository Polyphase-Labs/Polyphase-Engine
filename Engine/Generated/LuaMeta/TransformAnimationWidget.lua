--- @meta

---@class TransformAnimationWidget : Node
TransformAnimationWidget = {}

---@param arg1? Asset
function TransformAnimationWidget:Play(arg1) end

function TransformAnimationWidget:Pause() end

function TransformAnimationWidget:Stop() end

---@param arg1? Asset
function TransformAnimationWidget:SetAnimation(arg1) end

---@return Asset
function TransformAnimationWidget:GetAnimation() end

function TransformAnimationWidget:SetKeyframes() end

---@param n? Node
function TransformAnimationWidget:SetTargetWidget(n) end

---@return Node
function TransformAnimationWidget:GetTargetWidget() end

function TransformAnimationWidget:ApplyKeyframe() end

function TransformAnimationWidget:SampleNow() end

function TransformAnimationWidget:SetTime() end

---@return number
function TransformAnimationWidget:GetTime() end

---@return number
function TransformAnimationWidget:GetDuration() end

---@return boolean
function TransformAnimationWidget:IsPlaying() end

---@return boolean
function TransformAnimationWidget:IsPaused() end

---@return number
function TransformAnimationWidget:GetProgress() end

---@param loop boolean
function TransformAnimationWidget:SetLoop(loop) end

---@return boolean
function TransformAnimationWidget:IsLooping() end

function TransformAnimationWidget:SetPlayRate() end

---@return number
function TransformAnimationWidget:GetPlayRate() end

---@param play boolean
function TransformAnimationWidget:SetPlayOnStart(play) end

---@return boolean
function TransformAnimationWidget:GetPlayOnStart() end
