--- @meta

---@class Terrain3D : Mesh3D
Terrain3D = {}

---@param x integer
---@param z integer
---@param height number
function Terrain3D:SetHeight(x, z, height) end

---@param x integer
---@param z integer
---@return number
function Terrain3D:GetHeight(x, z) end

---@param worldX number
---@param worldZ number
---@return number
function Terrain3D:GetHeightAtWorldPos(worldX, worldZ) end

---@param texture? Texture
function Terrain3D:SetHeightFromTexture(texture) end

---@param height number
function Terrain3D:FlattenAll(height) end

---@param resX integer
---@param resZ integer
---@param worldW number
---@param worldD number
function Terrain3D:Resize(resX, resZ, worldW, worldD) end

---@return integer, integer
function Terrain3D:GetResolution() end

---@return number, number
function Terrain3D:GetWorldSize() end

---@return number
function Terrain3D:GetHeightScale() end

---@param scale number
function Terrain3D:SetHeightScale(scale) end

function Terrain3D:MarkDirty() end

function Terrain3D:RebuildMesh() end

---@return boolean
function Terrain3D:IsDirty() end

---@param slot integer
---@param mat? MaterialLite
function Terrain3D:SetMaterialSlot(slot, mat) end

---@param slot integer
---@return Asset
function Terrain3D:GetMaterialSlot(slot) end

---@param x integer
---@param z integer
---@param slot integer
---@param weight number
function Terrain3D:SetMaterialWeight(x, z, slot, weight) end

---@param x integer
---@param z integer
---@param slot integer
---@return number
function Terrain3D:GetMaterialWeight(x, z, slot) end

---@param size number
function Terrain3D:SetSnapGridSize(size) end

---@return number
function Terrain3D:GetSnapGridSize() end
