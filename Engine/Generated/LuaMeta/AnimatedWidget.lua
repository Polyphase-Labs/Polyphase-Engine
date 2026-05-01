--- @meta

---@class AnimatedWidget : Quad
AnimatedWidget = {}

function AnimatedWidget:Play() end

function AnimatedWidget:Pause() end

function AnimatedWidget:Stop() end

---@param name string
function AnimatedWidget:PlayAnimation(name) end

---@return boolean
function AnimatedWidget:IsPlaying() end

---@param frame integer
function AnimatedWidget:SetFrame(frame) end

---@return number
function AnimatedWidget:GetProgress() end

---@param target integer
function AnimatedWidget:AnimateTo(target) end

---@param progress number
function AnimatedWidget:AnimateToProgress(progress) end

function AnimatedWidget:CancelAnimateTo() end

---@param v number
function AnimatedWidget:SetSpeed(v) end

---@return number
function AnimatedWidget:GetSpeed() end

---@param v boolean
function AnimatedWidget:SetAutoPlay(v) end

---@return boolean
function AnimatedWidget:GetAutoPlay() end

---@param v boolean
function AnimatedWidget:SetLoopOverride(v) end

---@return boolean
function AnimatedWidget:GetLoopOverride() end

---@param name string
function AnimatedWidget:SetDefaultAnimation(name) end

---@return string
function AnimatedWidget:GetDefaultAnimation() end

function AnimatedWidget:AddAnimation() end

---@param name string
function AnimatedWidget:CreateAnimation(name) end

---@param name string
function AnimatedWidget:AddImage(name) end

---@param name string
---@param arg2 table
function AnimatedWidget:AddImages(name, arg2) end

---@param name string
function AnimatedWidget:RemoveAnimation(name) end

---@param name string
---@return boolean
function AnimatedWidget:HasAnimation(name) end

---@return Asset
function AnimatedWidget:GetCurrentTexture() end

---@return string
function AnimatedWidget:GetCurrentAnimationName() end

---@return integer
function AnimatedWidget:GetCurrentFrameIndex() end
