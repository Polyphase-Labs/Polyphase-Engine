--- @meta

---@class BoneMaskAsset : Asset
BoneMaskAsset = {}

---@return Asset
function BoneMaskAsset:GetTargetMesh() end

---@param a? Asset
function BoneMaskAsset:SetTargetMesh(a) end

---@return boolean
function BoneMaskAsset:GetSelfOnly() end

---@param selfOnly boolean
function BoneMaskAsset:SetSelfOnly(selfOnly) end

---@return integer
function BoneMaskAsset:GetNumIncludeRoots() end

---@param index integer
---@return nil
function BoneMaskAsset:GetIncludeRoot(index) end

---@param name string
function BoneMaskAsset:AddIncludeRoot(name) end

---@param name string
function BoneMaskAsset:RemoveIncludeRoot(name) end

---@return integer
function BoneMaskAsset:GetNumExcludeRoots() end

---@param index integer
---@return nil
function BoneMaskAsset:GetExcludeRoot(index) end

---@param name string
function BoneMaskAsset:AddExcludeRoot(name) end

---@param name string
function BoneMaskAsset:RemoveExcludeRoot(name) end
