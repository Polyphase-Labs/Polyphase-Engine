--- @meta

---@class SkeletalMesh : Asset
SkeletalMesh = {}

---@return Asset
function SkeletalMesh:GetMaterial() end

---@param material? Material
function SkeletalMesh:SetMaterial(material) end

---@return integer
function SkeletalMesh:GetNumIndices() end

---@return integer
function SkeletalMesh:GetNumFaces() end

---@return integer
function SkeletalMesh:GetNumVertices() end

---@param name string
---@return integer
function SkeletalMesh:FindBoneIndex(name) end

---@return integer
function SkeletalMesh:GetNumBones() end

---@param index integer
---@return string
function SkeletalMesh:GetAnimationName(index) end

---@return integer
function SkeletalMesh:GetNumAnimations() end

---@param name string
---@return number
function SkeletalMesh:GetAnimationDuration(name) end

---@return integer
function SkeletalMesh:GetNumSections() end

---@param index integer
---@return nil
function SkeletalMesh:GetSectionName(index) end

---@param index integer
---@return Asset
function SkeletalMesh:GetSectionMaterial(index) end

---@param index integer
---@param material? Material
function SkeletalMesh:SetSectionMaterial(index, material) end

---@param name string
---@return integer
function SkeletalMesh:FindSectionIndex(name) end

---@return table
function SkeletalMesh:GetBounds() end

---@return any
function SkeletalMesh:GetAABB() end
