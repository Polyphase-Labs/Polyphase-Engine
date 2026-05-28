--- @meta

---@class SignalBusModule
SignalBus = {}

---@param signalName string
---@param listenerNode Node
---@param arg3 function
function SignalBus.Subscribe(signalName, listenerNode, arg3) end

---@param signalName string
---@param listenerNode Node
function SignalBus.Unsubscribe(signalName, listenerNode) end

---@param signalName string
function SignalBus.Emit(signalName) end

function SignalBus.Clear() end
