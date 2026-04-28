--- @meta

---@class Voxel3D : Mesh3D
Voxel3D = {}

---@param x integer
---@param y integer
---@param z integer
---@param value integer
function Voxel3D:SetVoxel(x, y, z, value) end

---@param x integer
---@param y integer
---@param z integer
---@return integer
function Voxel3D:GetVoxel(x, y, z) end

---@param x integer
---@param y integer
---@param z integer
---@return Vector
function Voxel3D:GetVoxelWorldPosition(x, y, z) end

---@param rayOrigin Vector
---@param rayDir Vector
---@param maxDist? number
---@return boolean, integer, integer, integer, Vector, integer, integer, integer, integer
function Voxel3D:RayTest(rayOrigin, rayDir, maxDist) end

---@param camera Camera3D
---@param screenX integer
---@param screenY integer
---@param maxDist? number
function Voxel3D:RayTestScreen(camera, screenX, screenY, maxDist) end

---@param camera Camera3D
---@param maxDist? number
function Voxel3D:RayTestCenterCamera(camera, maxDist) end

---@param worldPos Vector
---@return boolean, integer, integer, integer, Vector, integer
function Voxel3D:GetVoxelAtWorldPosition(worldPos) end

---@param value integer
function Voxel3D:Fill(value) end

---@param x0 integer
---@param y0 integer
---@param z0 integer
---@param x1 integer
---@param y1 integer
---@param z1 integer
---@param value integer
function Voxel3D:FillRegion(x0, y0, z0, x1, y1, z1, value) end

---@param center Vector
---@param radius number
---@param value integer
function Voxel3D:FillSphere(center, radius, value) end

---@param center Vector
---@param radius number
---@param height number
---@param axis integer
---@param value integer
function Voxel3D:FillCylinder(center, radius, height, axis, value) end

---@param center Vector
---@param radius number
function Voxel3D:GetVoxelsInSphere(center, radius) end

---@param worldMin Vector
---@param worldMax Vector
function Voxel3D:GetVoxelsInBox(worldMin, worldMax) end

---@param center Vector
---@param radius number
---@param height number
---@param axis integer
function Voxel3D:GetVoxelsInCylinder(center, radius, height, axis) end

---@param x integer
---@param y integer
---@param z integer
function Voxel3D:GetVoxelNeighbors(x, y, z) end

function Voxel3D:MarkDirty() end

function Voxel3D:RebuildMesh() end

---@return boolean
function Voxel3D:IsDirty() end

---@return Vector
function Voxel3D:GetDimensions() end

---@param texture? Texture
function Voxel3D:SetAtlasTexture(texture) end

---@return Asset
function Voxel3D:GetAtlasTexture() end

function Voxel3D:SetAtlasEnabled() end

---@return boolean
function Voxel3D:IsAtlasEnabled() end

function Voxel3D:SetMaterialTexture() end

---@param tint Vector
function Voxel3D:SetMaterialTint(tint) end

function Voxel3D:DisableMaterialTexture() end
