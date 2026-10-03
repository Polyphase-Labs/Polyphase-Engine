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

-- =====================================================================
-- LedStrip: engine handle types, is/as on engine values, [Property] arrays
-- =====================================================================

Script.Require("CSharp/LedStrip")
assert(type(LedStrip) == "table", "global LedStrip class table missing")

-- Fake engine node: CheckType mirrors Node_Lua::CheckType (class flag chain).
local function FakeNode(classes, extra)
    local n = extra or {}
    local set = {}
    for _, c in ipairs(classes) do set[c] = true end
    function n:CheckType(name) return set[name] == true end
    return setmetatable(n, { __index = function(_, k) return nil end })
end

-- Fake asset userdata: class metatable with cf<Class> flags, __name, __index.
local function AssetClass(name, flags, methods)
    local mt = { __name = name, __index = methods }
    for _, f in ipairs(flags) do mt["cf" .. f] = true end
    return mt
end
local MaterialLiteMT = AssetClass("MaterialLite", { "Asset", "Material", "MaterialLite" }, {
    SetColor = function(self, c) self.color = c end,
    SetBlendMode = function(self, m) self.blend = m end,
    SetShadingModel = function(self, m) self.shading = m end,
    GetTypeName = function() return "MaterialLite" end,
})
local MaterialMT = AssetClass("Material", { "Asset", "Material" }, {
    GetTypeName = function() return "Material" end,
})

local liteMat = setmetatable({}, MaterialLiteMT)
local baseMat = setmetatable({}, MaterialMT)
local meshClasses = { "Node", "Node3D", "Primitive3D", "Mesh3D", "StaticMesh3D" }
local ledA = FakeNode(meshClasses, { GetMaterial = function() return liteMat end })
local ledB = FakeNode(meshClasses, { GetMaterial = function() return baseMat end })
local ledC = FakeNode(meshClasses, { GetMaterial = function() return liteMat end })
local splineNode = FakeNode({ "Node", "Node3D", "Spline3D" }, {
    GetSplineLength = function() return 42 end,
    GetSplinePointWorldPosition = function(self, i) return Vec(i, 0, 0) end, -- 1-based, like the engine
})

local strip = FakeNode(meshClasses)
setmetatable(strip, { __index = LedStrip })

strip:Create()
assert(strip.count == 160, "count default")
assert(type(strip.leds) == "table" and #strip.leds == 0, "handle array should default to an empty table")
assert(#strip.weights == 3 and strip.weights[2] == 2, "weights initializer")
assert(#strip.labels == 2 and strip.labels[1] == "", "new string[2] should fill with empty strings")
assert(strip.tint.w == 1, "3-arg Color should be opaque")

local props = strip:GatherProperties()
local byName = {}
for _, p in ipairs(props) do byName[p.name] = p end
assert(byName.leds and byName.leds.array == true and byName.leds.type == "Node3D", "leds prop")
assert(byName.weights and byName.weights.array == true and byName.weights.type == "Float", "weights prop")
assert(byName.spline and byName.spline.type == "Spline3D" and not byName.spline.array, "spline prop")
assert(byName.count and byName.count.type == "Integer", "count prop")

-- Engine fills the array table (UploadDatum writes 1-based entries).
strip.leds[1] = ledA
strip.leds[2] = ledB
strip.leds[3] = ledC

assert(strip:CountMeshes() == 3, "is Mesh3D on engine nodes")
assert(strip:SetLEDColor(0, Vec(1, 0, 0, 1)) == true, "lite material should be recolored")
assert(liteMat.color.x == 1 and liteMat.blend == 3 and liteMat.shading == 0, "SetColor/BlendMode.Additive/ShadingModel.Unlit")
assert(strip:SetLEDColor(1, Vec(0, 1, 0, 1)) == false, "`as MaterialLite` on a base Material must be null")
assert(strip:SetLEDColor(7, Vec(0, 1, 0, 1)) == false, "out of range index")
assert(strip:SetLEDColor(-1, Vec(0, 1, 0, 1)) == false, "negative index")
assert(strip:GetColoredCount() == 1, "colored count")
assert(strip:MaterialKind(0) == "lite" and strip:MaterialKind(1) == "Material", "MaterialKind")
assert(strip:SumWeights() == 6, "foreach over float[] property")
assert(strip:SelfIsStaticMesh() == true, "Node is StaticMesh3D")

-- Spline handle + 0-based index translation.
assert(strip:SplineLength() == -1, "no spline yet")
strip:SetSpline(splineNode)
assert(strip.spline == splineNode, "SetSpline wrote through to the node")
assert(strip:SplineLength() == 42, "GetSplineLength")
assert(strip:PointAt(0).x == 1, "C# index 0 must map to Lua index 1")

-- Array assigned from Lua (another script) replaces the table.
strip:SetLEDs({ ledC })
assert(strip.leds[1] == ledC and strip:CountMeshes() == 1, "SetLEDs")

-- Hole tolerance: a nil slot reads back as null instead of throwing.
strip.leds[2] = nil
strip.leds[3] = ledA
assert(strip:SetLEDColor(1, Vec(0, 0, 1, 1)) == false, "nil slot must read as null")

print("HARNESS OK - LedStrip")

-- =====================================================================
-- Hud: ScriptWidget, generated statics, callbacks, [Replicated], [NetFunc]
-- =====================================================================

NetFuncType = setmetatable({}, { __index = function(_, k) return k end })
local timers = {}
TimerManager = {
    SetTimer = function(fn, seconds, loop) timers[#timers + 1] = { fn = fn, seconds = seconds, loop = loop }; return #timers end,
    ClearTimer = function(id) timers[id] = false end,
}
Engine = { GetTime = function() return 12.5 end }
local constructed = {}
Node = {
    Construct = function(className)
        local n = FakeNode({ "Node", "Widget", className }, {
            SetAnchorMode = function(self, m) self.anchor = m end,
            SetPosition = function(self, x, y) self.px, self.py = x, y end,
            SetText = function(self, t) self.text = t end,
            Attach = function(self, parent) self.parent = parent end,
        })
        constructed[#constructed + 1] = n
        return n
    end,
}

Script.Require("CSharp/Hud")
assert(type(Hud) == "table", "global Hud class table missing")

local hud = FakeNode({ "Node", "Widget", "Canvas" }, {
    SetOpacityFloat = function(self, v) self.opacity = v end,
    ConnectSignal = function(self, name, listener, fn) self.signals = self.signals or {}; self.signals[name] = { listener = listener, fn = fn } end,
    InvokeNetFunc = function(self, name, ...) return self[name](self, ...) end,
})
setmetatable(hud, { __index = Hud })

hud:Create()
assert(hud.offset.x == 10 and hud.offset.y == 20, "Vector2 default")
assert(hud.score == 0 and hud.title == "HUD", "replicated defaults")

local hprops = {}
for _, p in ipairs(hud:GatherProperties()) do hprops[p.name] = p end
assert(hprops.offset and hprops.offset.type == "Vector2D", "Vector2 -> DatumType.Vector2D")
assert(hprops.title and not hprops.score, "only [Property] fields are inspector rows")

local rep = {}
for _, r in ipairs(hud:GatherReplicatedData()) do rep[r.name] = r end
assert(rep.score and rep.score.type == "Integer" and rep.score.onRep == "OnScoreChanged", "score replicated row")
assert(rep.title and rep.title.type == "String" and rep.title.onRep == nil, "title replicated row")

local nf = {}
for _, f in ipairs(hud:GatherNetFuncs()) do nf[f.name] = f end
assert(nf.S_AddScore and nf.S_AddScore.type == "Server" and nf.S_AddScore.reliable == true, "S_AddScore net func")
assert(nf.M_Flash and nf.M_Flash.type == "Multicast" and nf.M_Flash.reliable == false, "M_Flash net func")

hud:Start()
assert(#constructed == 1 and constructed[1].anchor == 0 and constructed[1].px == 10 and constructed[1].py == 20,
    "Node.Construct + generated widget calls")
assert(constructed[1].text == "HUD" and constructed[1].parent == hud, "SetText / Attach")
assert(#timers == 1 and timers[1].seconds == 0.5 and timers[1].loop == true, "SetTimer overload with loop")
assert(type(timers[1].fn) == "function", "C# lambda must reach Lua as a plain function")
timers[1].fn(); timers[1].fn()
assert(hud.__cs.ticks == 2, "timer lambda ran")

assert(hud.signals and hud.signals.Scored, "ConnectSignal")
hud.signals.Scored.fn(hud.signals.Scored.listener, 7)   -- engine passes the listener first
assert(hud.__cs.lastPoints == 7 and hud.score == 7, "signal handler with (listener, arg)")

hud:RequestScore(5)
assert(hud.score == 12, "InvokeNetFunc dispatched to the [NetFunc] method")
hud:OnScoreChanged()
assert(hud.__cs.repCount == 1, "OnRep method reachable by name")

hud:Tick(0.1)
assert(hud.opacity == 1, "bare widget call from ScriptWidget")
assert(hud:Elapsed() == 12.5, "generated static Engine.GetTime")
assert(hud:Kind() == "widget", "Node is Widget")
hud:Stop()
assert(timers[1] == false, "ClearTimer")

-- ---- Door: Script<StaticMesh3D> ----
Script.Require("CSharp/Door")
local door = FakeNode(meshClasses, {
    GetMaterial = function() return liteMat end,
    GetUseTriangleCollision = function() return true end,
})
setmetatable(door, { __index = Door })
door:Create()
assert(door.openAngle == 90, "Door default")
assert(door:MaterialKind() == "lite", "Script<T>.Node typed access")
assert(door:HasTriangleCollision() == true, "hand-written property over generated getter")

print("HARNESS OK - Hud/Door")
