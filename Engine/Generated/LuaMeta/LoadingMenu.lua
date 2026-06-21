--- @meta

---@class LoadingMenuModule
LoadingMenu = {}

---@param sceneName string
function LoadingMenu.SetMenuScene(sceneName) end

---@return string
function LoadingMenu.GetMenuScene() end

---@param targetSceneName string
---@return boolean
function LoadingMenu.Open(targetSceneName) end

function LoadingMenu.Close() end

---@return boolean
function LoadingMenu.IsActive() end

---@return string
function LoadingMenu.GetState() end

---@return string
function LoadingMenu.GetTargetScene() end
