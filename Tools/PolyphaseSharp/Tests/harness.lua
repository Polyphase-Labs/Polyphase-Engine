-- PolyphaseSharp M1 harness: emulates just enough of the engine's Lua
-- environment to load and drive a generated C# script outside the editor.
-- Run: lua.exe harness.lua <ScriptsRoot>

local scriptsRoot = arg[1] or "Scripts"

-- ---- engine stubs ----

DatumType = setmetatable({}, { __index = function(_, k) return k end })

Log = {
    Debug = function(msg) print("[Debug] " .. tostring(msg)) end,
    Warning = function(msg) print("[Warn ] " .. tostring(msg)) end,
    Error = function(msg) print("[Error] " .. tostring(msg)) end,
    Console = print,
}

-- Engine Vec userdata stand-in: table vec4 with the engine's metamethods.
local VecMeta = {}
VecMeta.__index = VecMeta
VecMeta.__add = function(a, b) return Vec(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w) end
VecMeta.__sub = function(a, b) return Vec(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w) end
VecMeta.__unm = function(a) return Vec(-a.x, -a.y, -a.z, -a.w) end
VecMeta.__mul = function(a, b)
    if type(b) == "number" then return Vec(a.x * b, a.y * b, a.z * b, a.w * b) end
    if type(a) == "number" then return Vec(b.x * a, b.y * a, b.z * a, b.w * a) end
    return Vec(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w)
end
VecMeta.__div = function(a, b)
    if type(b) == "number" then return Vec(a.x / b, a.y / b, a.z / b, a.w / b) end
    return Vec(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w)
end
function Vec(x, y, z, w)
    return setmetatable({ x = x or 0, y = y or 0, z = z or 0, w = w or 0 }, VecMeta)
end

-- Engine script loader stand-in (Script.Require semantics: load once, by
-- Scripts-relative path without extension).
local loadedScripts = {}
Script = {}
function Script.Require(path)
    if loadedScripts[path] then return end
    loadedScripts[path] = true
    local chunk, err = loadfile(scriptsRoot .. "/" .. path .. ".lua")
    assert(chunk, err)
    chunk()
end
Script.Load = Script.Require

-- ---- load the generated scripts exactly like the engine would ----

Script.Require("CSharp/Rotator")

assert(type(Rotator) == "table", "global Rotator class table missing")
assert(type(CSharpCore) == "table", "global CSharpCore class table missing")
assert(type(System) == "table", "CoreSystem System global missing")
assert(Game and Game.Rotator, "published Game.Rotator class missing")

-- ---- fake node: table emulating node userdata + uservalue + class fallback ----

local node = { __name = "TestNode", __rotDelta = Vec(0, 0, 0) }
function node:GetName() return self.__name end
function node:SetName(n) self.__name = n end
function node:AddRotation(v) self.__rotDelta = self.__rotDelta + v end
setmetatable(node, { __index = Rotator })

-- ---- drive the engine lifecycle ----

node:Create()
assert(node.__cs ~= nil, "companion instance missing after Create")
assert(node.AngularVelocity and node.AngularVelocity.y == 90, "default AngularVelocity not applied")
assert(node.Enabled == true, "default Enabled not applied")

local props = node:GatherProperties()
assert(#props == 2, "expected 2 gathered properties, got " .. #props)
assert(props[1].name == "AngularVelocity" and props[1].type == "Vector", "prop 1 mismatch")
assert(props[2].name == "Enabled" and props[2].display_name == "Spin Enabled", "prop 2 mismatch")

node:Start()

for _ = 1, 3 do
    node:Tick(0.25)
end
local expected = 90 * 0.25 * 3
assert(math.abs(node.__rotDelta.y - expected) < 0.001,
    string.format("rotation mismatch: got %.3f want %.3f", node.__rotDelta.y, expected))

-- Editor-toggled property flows into C# through the uservalue.
node.Enabled = false
node:Tick(0.25)
assert(math.abs(node.__rotDelta.y - expected) < 0.001, "Enabled=false should stop rotation")

-- ---- hot-reload simulation: re-run the generated chunk, restart the script ----

loadedScripts["CSharp/Rotator"] = nil
Script.Require("CSharp/Rotator")
node.__cs = nil
node:Create()
node.Enabled = true
node:Tick(0.25)
assert(math.abs(node.__rotDelta.y - (expected + 90 * 0.25)) < 0.001, "post-reload tick mismatch")

print("HARNESS OK - rotation y = " .. node.__rotDelta.y)
