--- @meta

---@class ComboBox : Widget
ComboBox = {}

---@param option string
function ComboBox:AddOption(option) end

---@param index integer
function ComboBox:RemoveOption(index) end

function ComboBox:ClearOptions() end

---@param arg1 table
function ComboBox:SetOptions(arg1) end

---@return table
function ComboBox:GetOptions() end

---@return integer
function ComboBox:GetOptionCount() end

---@param index integer
function ComboBox:SetSelectedIndex(index) end

---@return integer
function ComboBox:GetSelectedIndex() end

---@return string
function ComboBox:GetSelectedOption() end

---@return boolean
function ComboBox:IsOpen() end

function ComboBox:Open() end

function ComboBox:Close() end

function ComboBox:Toggle() end

---@param color Vector
function ComboBox:SetBackgroundColor(color) end

---@return Vector
function ComboBox:GetBackgroundColor() end

---@param color Vector
function ComboBox:SetTextColor(color) end

---@return Vector
function ComboBox:GetTextColor() end

---@param color Vector
function ComboBox:SetDropdownColor(color) end

---@return Vector
function ComboBox:GetDropdownColor() end

---@param color Vector
function ComboBox:SetHoveredColor(color) end

---@return Vector
function ComboBox:GetHoveredColor() end

---@param count integer
function ComboBox:SetMaxVisibleItems(count) end

---@return integer
function ComboBox:GetMaxVisibleItems() end

---@return Node
function ComboBox:GetBackground() end

---@return Node
function ComboBox:GetTextWidget() end
