--- @meta

---@class SpriteAnimator : Node
SpriteAnimator = {}

function SpriteAnimator:Play() end

function SpriteAnimator:Pause() end

function SpriteAnimator:Stop() end

---@param name string
function SpriteAnimator:PlayAnimation(name) end

---@return boolean
function SpriteAnimator:IsPlaying() end

---@param frame integer
function SpriteAnimator:SetFrame(frame) end

---@return number
function SpriteAnimator:GetProgress() end

---@param target integer
function SpriteAnimator:AnimateTo(target) end

---@param progress number
function SpriteAnimator:AnimateToProgress(progress) end

function SpriteAnimator:CancelAnimateTo() end

---@param v number
function SpriteAnimator:SetSpeed(v) end

---@return number
function SpriteAnimator:GetSpeed() end

---@param v boolean
function SpriteAnimator:SetAutoPlay(v) end

---@return boolean
function SpriteAnimator:GetAutoPlay() end

---@param v boolean
function SpriteAnimator:SetLoopOverride(v) end

---@return boolean
function SpriteAnimator:GetLoopOverride() end

---@param name string
function SpriteAnimator:SetDefaultAnimation(name) end

---@return string
function SpriteAnimator:GetDefaultAnimation() end

function SpriteAnimator:AddAnimation() end

---@param name string
function SpriteAnimator:CreateAnimation(name) end

---@param name string
function SpriteAnimator:AddImage(name) end

---@param name string
---@param arg2 table
function SpriteAnimator:AddImages(name, arg2) end

---@param name string
function SpriteAnimator:RemoveAnimation(name) end

---@param name string
---@return boolean
function SpriteAnimator:HasAnimation(name) end

---@return Asset
function SpriteAnimator:GetCurrentTexture() end

---@return number, number
function SpriteAnimator:GetCurrentUVScale() end

---@return number, number
function SpriteAnimator:GetCurrentUVOffset() end

---@return number, number, number, number
function SpriteAnimator:GetCurrentUVRect() end

---@return string
function SpriteAnimator:GetCurrentAnimationName() end

---@return integer
function SpriteAnimator:GetCurrentFrameIndex() end
