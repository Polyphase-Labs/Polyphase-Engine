--- @meta

---@class PlayerInputModule
PlayerInput = {}

---@param category string
---@param name string
---@param playerIndex? integer
---@return boolean
function PlayerInput.IsActive(category, name, playerIndex) end

---@param category string
---@param name string
---@param playerIndex? integer
---@return boolean
function PlayerInput.WasJustActivated(category, name, playerIndex) end

---@param category string
---@param name string
---@param playerIndex? integer
---@return boolean
function PlayerInput.WasJustDeactivated(category, name, playerIndex) end

---@param category string
---@param name string
---@param playerIndex? integer
---@return number
function PlayerInput.GetValue(category, name, playerIndex) end

---@param category string
---@param name string
function PlayerInput.RegisterAction(category, name) end

---@param category string
---@param name string
function PlayerInput.UnregisterAction(category, name) end

---@return integer
function PlayerInput.GetPlayersConnected() end

function PlayerInput.SetEnabled() end

---@return boolean
function PlayerInput.IsEnabled() end

---@param arg1 Asset
function PlayerInput.LoadActions(arg1) end
