--- @meta

---@class TweenModule
Tween = {}

---@param easingType integer
---@param from number
---@param to number
---@param duration number
---@param arg5 function
---@return integer
function Tween.Value(easingType, from, to, duration, arg5) end

---@param easingType integer
---@param from Vector
---@param to Vector
---@param duration number
---@param arg5 function
---@return integer
function Tween.Vector(easingType, from, to, duration, arg5) end

function Tween.Quaternion() end

---@param arg1 Node
---@param easingType integer
---@param to Vector
---@param duration number
---@return integer
function Tween.Position(arg1, easingType, to, duration) end

---@param arg1 Node
---@param easingType integer
---@param to Vector
---@param duration number
---@return integer
function Tween.Rotation(arg1, easingType, to, duration) end

---@param arg1 Node
---@param easingType integer
---@param to Vector
---@param duration number
---@return integer
function Tween.Scale(arg1, easingType, to, duration) end

---@param arg1 Node
---@param easingType integer
---@param to Vector
---@param duration number
---@return integer
function Tween.Color(arg1, easingType, to, duration) end

---@param id integer
function Tween.Cancel(id) end

function Tween.CancelAll() end

---@param id integer
function Tween.Pause(id) end

---@param id integer
function Tween.Resume(id) end
