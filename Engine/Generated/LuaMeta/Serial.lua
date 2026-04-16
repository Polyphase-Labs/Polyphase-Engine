--- @meta

---@class SerialModule
Serial = {}

---@return table
function Serial.EnumeratePorts() end

---@param portName string
---@return integer
function Serial.Connect(portName) end

---@param h integer
function Serial.Disconnect(h) end

---@param h integer
---@return boolean
function Serial.IsConnected(h) end

---@param h integer
---@param data string
---@return integer
function Serial.Send(h, data) end

---@param h integer
---@param data string
---@return integer
function Serial.SendLine(h, data) end

---@param h integer
function Serial.StartReceive(h) end

---@param h integer
function Serial.StopReceive(h) end

---@param h integer
---@return boolean
function Serial.IsReceiving(h) end

---@param h integer
---@param pattern string
---@param arg3 function
---@return integer
function Serial.RegisterMessageFunction(h, pattern, arg3) end

---@param h integer
---@param pattern string
---@param arg3 function
---@return integer
function Serial.RegisterREGEXMessageFunction(h, pattern, arg3) end

---@param h integer
---@param matcherId integer
function Serial.UnregisterMessageFunction(h, matcherId) end

---@param arg1 function
function Serial.SetMessageCallback(arg1) end

---@param arg1 function
function Serial.SetConnectCallback(arg1) end

---@param arg1 function
function Serial.SetDisconnectCallback(arg1) end
