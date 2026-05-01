--- @meta

---@class AnimatedSprite3D : Node3D
AnimatedSprite3D = {}

function AnimatedSprite3D:Play() end

function AnimatedSprite3D:Pause() end

function AnimatedSprite3D:Stop() end

---@param name string
function AnimatedSprite3D:PlayAnimation(name) end

---@return boolean
function AnimatedSprite3D:IsPlaying() end

---@param frame integer
function AnimatedSprite3D:SetFrame(frame) end

---@return number
function AnimatedSprite3D:GetProgress() end

---@param target integer
function AnimatedSprite3D:AnimateTo(target) end

---@param progress number
function AnimatedSprite3D:AnimateToProgress(progress) end

function AnimatedSprite3D:CancelAnimateTo() end

---@param v number
function AnimatedSprite3D:SetSpeed(v) end

---@return number
function AnimatedSprite3D:GetSpeed() end

---@param v boolean
function AnimatedSprite3D:SetAutoPlay(v) end

---@return boolean
function AnimatedSprite3D:GetAutoPlay() end

---@param v boolean
function AnimatedSprite3D:SetLoopOverride(v) end

---@return boolean
function AnimatedSprite3D:GetLoopOverride() end

---@param name string
function AnimatedSprite3D:SetDefaultAnimation(name) end

---@return string
function AnimatedSprite3D:GetDefaultAnimation() end

function AnimatedSprite3D:AddAnimation() end

---@param name string
function AnimatedSprite3D:CreateAnimation(name) end

---@param name string
function AnimatedSprite3D:AddImage(name) end

---@param name string
---@param arg2 table
function AnimatedSprite3D:AddImages(name, arg2) end

---@param name string
function AnimatedSprite3D:RemoveAnimation(name) end

---@param name string
---@return boolean
function AnimatedSprite3D:HasAnimation(name) end

---@return Asset
function AnimatedSprite3D:GetCurrentTexture() end

---@return string
function AnimatedSprite3D:GetCurrentAnimationName() end

---@return integer
function AnimatedSprite3D:GetCurrentFrameIndex() end

function AnimatedSprite3D:SetMaterial() end

---@return Asset
function AnimatedSprite3D:GetMaterial() end

---@param v boolean
function AnimatedSprite3D:SetAffectDiffuse(v) end

---@param v boolean
function AnimatedSprite3D:SetAffectAlpha(v) end

---@param v boolean
function AnimatedSprite3D:SetAffectEmission(v) end

---@return boolean
function AnimatedSprite3D:GetAffectDiffuse() end

---@return boolean
function AnimatedSprite3D:GetAffectAlpha() end

---@return boolean
function AnimatedSprite3D:GetAffectEmission() end
