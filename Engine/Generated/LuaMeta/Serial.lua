--- @meta

---@class SerialPortInfo
---@field name string Port name ("COM3", "/dev/ttyUSB0")
---@field description string Human-readable label ("USB Serial Port (COM3)")

---@class SerialConfig
---@field baud? integer Baud rate (default 9600)
---@field dataBits? integer Data bits 5-8 (default 8)
---@field stopBits? integer Stop bits 1 or 2 (default 1)
---@field parity? "none"|"odd"|"even"|"mark"|"space" Parity mode (default "none")
---@field flowControl? boolean RTS/CTS flow control (default false)

---@class SerialModule
Serial = {}

---@return SerialPortInfo[]
function Serial.EnumeratePorts() end

---@param portName string
---@param config? SerialConfig
---@return integer handle 0 on failure
function Serial.Connect(portName, config) end

---@param handle integer
function Serial.Disconnect(handle) end

---@param handle integer
---@return boolean
function Serial.IsConnected(handle) end

---@param handle integer
---@param data string Binary-safe byte string
---@return integer bytesWritten -1 on error
function Serial.Send(handle, data) end

---@param handle integer
function Serial.StartReceive(handle) end

---@param handle integer
function Serial.StopReceive(handle) end

---@param handle integer
---@return boolean
function Serial.IsReceiving(handle) end

---@param callback fun(handle: integer, data: string)
function Serial.SetMessageCallback(callback) end

---@param callback fun(handle: integer, portName: string)
function Serial.SetConnectCallback(callback) end

---@param callback fun(handle: integer)
function Serial.SetDisconnectCallback(callback) end
