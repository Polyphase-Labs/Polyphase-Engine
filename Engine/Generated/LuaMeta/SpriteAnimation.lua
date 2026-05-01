--- @meta

---@class SpriteAnimation : Asset
SpriteAnimation = {}

---@return string
function SpriteAnimation:GetAnimationName() end

---@param name string
function SpriteAnimation:SetAnimationName(name) end

---@return integer
function SpriteAnimation:GetMode() end

---@param mode integer
function SpriteAnimation:SetMode(mode) end

---@return number
function SpriteAnimation:GetFps() end

---@param fps number
function SpriteAnimation:SetFps(fps) end

---@return boolean
function SpriteAnimation:GetLoop() end

---@param loop boolean
function SpriteAnimation:SetLoop(loop) end

---@return integer
function SpriteAnimation:GetFrameCount() end

function SpriteAnimation:AddFrame() end

function SpriteAnimation:ClearFrames() end

---@return Asset
function SpriteAnimation:GetAtlasTexture() end

function SpriteAnimation:SetAtlasTexture() end

---@param cols integer
---@param rows integer
function SpriteAnimation:SetAtlasGrid(cols, rows) end

---@param x integer
---@param y integer
function SpriteAnimation:SetAtlasMargin(x, y) end

---@param x integer
---@param y integer
function SpriteAnimation:SetAtlasSpacing(x, y) end
