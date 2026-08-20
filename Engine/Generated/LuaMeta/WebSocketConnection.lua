--- @meta

---@class WebSocketConnection
WebSocketConnection = {}

function WebSocketConnection:SetOpenCallback() end

function WebSocketConnection:SetMessageCallback() end

function WebSocketConnection:SetErrorCallback() end

function WebSocketConnection:SetClosedCallback() end

---@param data string
---@return boolean
function WebSocketConnection:SendText(data) end

---@param data string
---@return boolean
function WebSocketConnection:SendBinary(data) end

---@param reason string
function WebSocketConnection:Close(reason) end

---@return integer
function WebSocketConnection:GetState() end

---@return integer
function WebSocketConnection:GetAvailablePacketCount() end

---@return boolean
function WebSocketConnection:GetPacket() end

---@return string
function WebSocketConnection:GetSelectedProtocol() end

---@return integer
function WebSocketConnection:GetCloseCode() end

---@return string
function WebSocketConnection:GetCloseReason() end

---@return boolean
function WebSocketConnection:WasDowngraded() end
