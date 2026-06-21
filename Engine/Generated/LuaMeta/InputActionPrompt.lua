--- @meta

---@class InputActionPrompt : Widget
InputActionPrompt = {}

---@param category string
---@param name string
function InputActionPrompt:SetAction(category, name) end

---@return string
function InputActionPrompt:GetActionCategory() end

---@return string
function InputActionPrompt:GetActionName() end

---@param arg1? Asset
function InputActionPrompt:SetPromptMap(arg1) end

---@param arg1? Asset
function InputActionPrompt:SetPromptStyle(arg1) end

---@return string
function InputActionPrompt:GetResolvedLabel() end
