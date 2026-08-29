--- @meta

---@class SpotLight3D : PointLight3D
SpotLight3D = {}

---@param value number
function SpotLight3D:SetInnerAngle(value) end

---@return number
function SpotLight3D:GetInnerAngle() end

---@param value number
function SpotLight3D:SetOuterAngle(value) end

---@return number
function SpotLight3D:GetOuterAngle() end

---@return Vector
function SpotLight3D:GetDirection() end

---@param value Vector
function SpotLight3D:SetDirection(value) end
