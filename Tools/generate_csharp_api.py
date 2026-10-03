#!/usr/bin/env python3
"""
Generate the Polyphase C# scripting API from the engine's Lua bindings.

Reads Engine/Source/LuaBindings/*_Lua.cpp with the same parser as
generate_lua_stubs.py and writes one C# file per Lua class / module / enum to
Tools/PolyphaseSharp/Polyphase.Engine/Polyphase/Generated/. Every member is an
`extern` carrying a @CSharpLua.Template doc comment onto the Lua binding, so the
generated surface is exactly what the engine exposes to Lua scripts.

Hand-written files in Tools/PolyphaseSharp/Polyphase.Engine/Polyphase/*.cs add
ergonomics (properties, phantom table classes, 0-based indices, typed
callbacks). Generated classes are `partial`; a member whose C# name already
exists in the hand-written part is skipped, so hand-written members win.

Also generates the bare-call mirrors (Script <- Node, Script3D <- Node3D,
ScriptWidget <- Widget) that make `AddRotation(v)` inside a script mean
`self:AddRotation(v)`.

Usage:
    python Tools/generate_csharp_api.py [--check]

--check exits 1 (and prints the files) when the output would differ from what
is on disk, for CI.
"""

import argparse
import itertools
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_lua_stubs as gls  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INPUT_DIR = os.path.join(REPO_ROOT, "Engine", "Source", "LuaBindings")
ENGINE_SRC = os.path.join(REPO_ROOT, "Engine", "Source")
API_DIR = os.path.join(REPO_ROOT, "Tools", "PolyphaseSharp", "Polyphase.Engine", "Polyphase")
OUT_DIR = os.path.join(API_DIR, "Generated")

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Lua types that are hand-written in full (or meaningless in C#) and never generated.
SKIP_TYPES = {"Vector", "Property", "DatumType", "PropertyTracker"}

# Lua global -> C# class name when they must differ.
CLASS_RENAME = {
    "Script": "LuaScript",       # `Script` is the C# script base class
    "System": "Sys",             # `System` is the BCL namespace
    "ToolTipManager": "ToolTip",  # parser fallback name; the Lua global is ToolTip
    "UIDOCUMENT_LUA_NAME": "UIDocument",
}

# Lua global table used for static calls when it is not the class name.
STATIC_GLOBAL = {
    "UIDocument": "UI",      # UI.Load / UI.LoadFromString
    "ToolTipManager": "ToolTip",
}

# Lifecycle names the wrapper dispatches; never mirrored onto Script bases.
LIFECYCLE = {"Create", "Awake", "Start", "Tick", "EditorTick", "Stop", "Destroy",
             "BeginOverlap", "EndOverlap", "OnCollision", "GatherProperties",
             "GatherReplicatedData", "GatherNetFuncs"}

# Script base class -> Lua class whose instance methods it mirrors as bare calls.
MIRRORS = [("Script", "Node"), ("Script3D", "Node3D"), ("ScriptWidget", "Widget")]

CS_KEYWORDS = {
    "abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "checked",
    "class", "const", "continue", "decimal", "default", "delegate", "do", "double", "else",
    "enum", "event", "explicit", "extern", "false", "finally", "fixed", "float", "for",
    "foreach", "goto", "if", "implicit", "in", "int", "interface", "internal", "is", "lock",
    "long", "namespace", "new", "null", "object", "operator", "out", "override", "params",
    "private", "protected", "public", "readonly", "ref", "return", "sbyte", "sealed",
    "short", "sizeof", "stackalloc", "static", "string", "struct", "switch", "this", "throw",
    "true", "try", "typeof", "uint", "ulong", "unchecked", "unsafe", "ushort", "using",
    "virtual", "void", "volatile", "while",
}

LUA_TO_CS = {
    "number": "float", "integer": "int", "string": "string", "boolean": "bool",
    "table": "object", "function": "object", "userdata": "object", "nil": "object",
    "any": "object", "Vector": "Vector3", "Rect": "Rect",
}

# CoreSystem class path for array element types (CSharpCore.Array needs it).
ELEM_PATH = {"string": "System.String", "float": "System.Single", "int": "System.Int32",
             "bool": "System.Boolean", "object": "System.Object"}

# (LuaClass, func) -> C# return type (phantom table classes live in Tables.cs).
RETURN_OVERRIDES = {
    ("Node3D", "GetAABB"): "AABB", ("Node3D", "GetHierarchyAABB"): "AABB",
    ("Primitive3D", "GetAABB"): "AABB", ("Primitive3D", "GetLocalAABB"): "AABB",
    ("Primitive3D", "GetBounds"): "Bounds",
    ("StaticMesh", "GetBounds"): "Bounds", ("StaticMesh", "GetAABB"): "AABB",
    ("SkeletalMesh", "GetBounds"): "Bounds", ("SkeletalMesh", "GetAABB"): "AABB",
    ("Primitive3D", "SweepToWorldPosition"): "HitResult", ("Primitive3D", "SweepToPosition"): "HitResult",
    ("World", "RayTest"): "HitResult", ("World", "SweepTest"): "HitResult",
    ("World", "RayTestMulti"): "MultiHitResult",
    ("World", "FindNavPath"): "NavPath",
    ("World", "GetFogSettings"): "FogSettings", ("World", "GetFog"): "FogSettings",
    ("World", "FindRandomNavPoint"): "Vector3", ("World", "FindClosestNavPoint"): "Vector3",
    ("World", "GetActiveCamera"): "Camera3D",
    ("World", "SpawnParticle"): "Particle3D",
    ("Network", "GetSession"): "NetSessionInfo", ("Network", "FindNetClient"): "NetClientInfo",
    ("Camera3D", "TraceScreenToWorld"): "ScreenTrace",
    ("Scene", "Instantiate"): "Node",
    ("Widget", "GetRect"): "Rect",
    ("DataAsset", "Get"): "object",
    ("Http", "Request"): "HttpRequest", ("HttpRequest", "Send"): "HttpHandle",
    ("WebSocket", "Connect"): "WebSocketConnection",
    ("SkeletalMesh3D", "GetMaterialSlot"): "Material",
    ("SkeletalMesh", "GetSectionMaterial"): "Material",
    ("Terrain3D", "GetMaterialSlot"): "Material",
    ("ListViewWidget", "GetItem"): "ListViewItemWidget",
    ("ListViewItemWidget", "GetListView"): "ListViewWidget",
    ("UIDocument", "GetRootWidget"): "Widget", ("UIDocument", "FindById"): "Widget",
    ("UIDocument", "Instantiate"): "Widget", ("Canvas", "GetUIDocument"): "UIDocument",
    ("UIDocument", "Load"): "UIDocument", ("UIDocument", "LoadFromString"): "UIDocument",
    ("Tween", "Quaternion"): "int",
    ("Http", "Get"): "HttpHandle", ("Http", "Post"): "HttpHandle", ("Http", "Put"): "HttpHandle",
    ("Http", "Patch"): "HttpHandle", ("Http", "Delete"): "HttpHandle",
    ("HttpRequest", "Header"): "HttpRequest", ("HttpRequest", "Body"): "HttpRequest",
    ("HttpRequest", "Timeout"): "HttpRequest", ("HttpRequest", "VerifySsl"): "HttpRequest",
    ("HttpResponse", "GetBody"): "string",
    ("SkeletalMesh", "GetSectionName"): "string", ("SkeletalMesh3D", "GetSocketName"): "string",
    ("SkeletalAnimationAsset", "GetChannelBoneName"): "string",
    ("ListViewWidget", "GetScrollContainer"): "ScrollContainer", ("ListViewWidget", "GetArrayWidget"): "Widget",
    ("ListViewItemWidget", "GetContentWidget"): "Widget",
    ("Box3D", "SetExtents"): "void",
}

# (LuaClass, func) -> element type of the returned Lua array (C# T[] via CSharpCore.Array).
ARRAY_RETURNS = {
    ("World", "FindNodesWithTag"): "Node", ("World", "FindNodesWithName"): "Node",
    ("World", "GetSeenByCamera"): "Node3D", ("World", "GetLoadedSceneNames"): "string",
    ("Network", "GetSessions"): "NetSessionInfo", ("Network", "GetClients"): "NetClientInfo",
    ("System", "ListDirectory"): "string",
    ("ComboBox", "GetOptions"): "string",
    ("StaticMesh", "GetIndices"): "int", ("StaticMesh", "GetVertices"): "object",
    ("Serial", "EnumeratePorts"): "string",
    ("UIDocument", "FindByClass"): "Widget",
    ("Voxel3D", "GetVoxelsInSphere"): "object", ("Voxel3D", "GetVoxelsInBox"): "object",
    ("Voxel3D", "GetVoxelsInCylinder"): "object", ("Voxel3D", "GetVoxelNeighbors"): "object",
    ("TileMap2D", "FindAllTilesWithTag"): "object", ("TileMap2D", "FindAllTiles"): "object",
    ("TileMap2D", "GetCellsInRect"): "object", ("TileMap2D", "GetNeighborCells"): "object",
    ("TileMap2D", "GetClosestTilesWithTag"): "object", ("TileMap2D", "GetClosestTilesOfType"): "object",
    ("TileMap2D", "GetReachableCells"): "object", ("TileMap2D", "GetTileTags"): "string",
    ("Input", "GetKeysJustDown"): "int", ("Input", "GetKeysPressed"): "int",
    ("Audio", "GetSpectrum"): "float", ("Audio", "GetFrequencies"): "float",
    ("Audio", "GetStreamSpectrum"): "float", ("Audio", "GetStreamFrequencies"): "float",
    ("Audio3D", "GetSpectrum"): "float", ("Audio3D", "GetFrequencies"): "float",
}

# Callback parameters: (LuaClass, func) -> {paramIndex: [C# delegate types]}.
# Each listed type yields an overload. Engine signal handlers always receive
# the listener node first (Signals.cpp), hence Action<Node, ...>.
_SIGNAL = ["Action<Node>", "Action<Node, object>", "Action<Node, object, object>",
           "Action<Node, object, object, object>"]
CALLBACK_TYPES = {
    ("Node", "ConnectSignal"): {2: _SIGNAL},
    ("Signal", "Connect"): {1: _SIGNAL},
    ("SignalBus", "Subscribe"): {1: _SIGNAL},
    ("Node", "Traverse"): {0: ["Func<Node, bool>"]},
    ("Node", "ForEach"): {0: ["Func<Node, bool>"]},
    ("TimerManager", "SetTimer"): {0: ["Action"]},
    ("Tween", "Value"): {4: ["Action<float, float, bool>", "Action<float>"], 5: ["Action"]},
    ("Tween", "Vector"): {4: ["Action<Vector3, float, bool>", "Action<Vector3>"], 5: ["Action"]},
    ("Tween", "Quaternion"): {4: ["Action<Vector3, float, bool>", "Action<Vector3>"], 5: ["Action"]},
    ("Tween", "Position"): {4: ["Action"]}, ("Tween", "Rotation"): {4: ["Action"]},
    ("Tween", "Scale"): {4: ["Action"]}, ("Tween", "Color"): {4: ["Action"]},
    ("Log", "SetCallback"): {0: ["Action<int, string>"]},
    ("Http", "Get"): {1: ["Action<HttpResponse>"]}, ("Http", "Delete"): {1: ["Action<HttpResponse>"]},
    ("Http", "Post"): {2: ["Action<HttpResponse>"]}, ("Http", "Put"): {2: ["Action<HttpResponse>"]},
    ("Http", "Patch"): {2: ["Action<HttpResponse>"]},
    ("HttpRequest", "Send"): {0: ["Action<HttpResponse>"]},
    ("WebSocketConnection", "SetOpenCallback"): {0: ["Action"]},
    ("WebSocketConnection", "SetMessageCallback"): {0: ["Action<string, bool>"]},
    ("WebSocketConnection", "SetErrorCallback"): {0: ["Action<string>"]},
    ("WebSocketConnection", "SetClosedCallback"): {0: ["Action<int, string, bool>"]},
    ("Network", "SetConnectCallback"): {0: ["Action<NetClientInfo>"]},
    ("Network", "SetDisconnectCallback"): {0: ["Action<NetClientInfo>"]},
    ("Network", "SetAcceptCallback"): {0: ["Action"]},
    ("Network", "SetRejectCallback"): {0: ["Action<string>"]},
    ("Network", "SetKickCallback"): {0: ["Action<string>"]},
    ("SkeletalMesh3D", "AddAnimationNotify"): {2: ["Action"]},
    ("SkeletalMesh3D", "SetAnimEventHandler"): {0: ["Action<object>"]},
    ("SpriteAnimator", "AnimateTo"): {1: ["Action"]}, ("SpriteAnimator", "AnimateToProgress"): {1: ["Action"]},
    ("AnimatedWidget", "AnimateTo"): {1: ["Action"]}, ("AnimatedWidget", "AnimateToProgress"): {1: ["Action"]},
    ("AnimatedSprite3D", "AnimateTo"): {1: ["Action"]}, ("AnimatedSprite3D", "AnimateToProgress"): {1: ["Action"]},
    ("UIDocument", "SetCallback"): {2: ["Action<Widget>", "string"]},
    ("ToolTipManager", "SetOnShowCallback"): {0: ["Action<Widget>"]},
    ("ToolTipManager", "SetOnHideCallback"): {0: ["Action<Widget>"]},
    ("Serial", "RegisterMessageFunction"): {0: ["Action<string>"]},
    ("Serial", "RegisterREGEXMessageFunction"): {1: ["Action<string>"]},
    ("Serial", "SetMessageCallback"): {0: ["Action<string>"]},
    ("Serial", "SetConnectCallback"): {0: ["Action"]},
    ("Serial", "SetDisconnectCallback"): {0: ["Action"]},
}

# Parameters the C++ reads without a CHECK_ macro (lua_isboolean/lua_isfunction...).
EXTRA_PARAMS = {
    ("TimerManager", "SetTimer"): [("loop", "boolean", True)],
    ("Node", "FindChild"): [("recurse", "boolean", True)],
    ("Http", "Get"): [("callback", "function", True)],
    ("Http", "Delete"): [("callback", "function", True)],
    ("Http", "Post"): [("callback", "function", True)],
    ("Http", "Put"): [("callback", "function", True)],
    ("Http", "Patch"): [("callback", "function", True)],
    ("HttpRequest", "Send"): [("callback", "function", True)],
    ("Tween", "Value"): [("onComplete", "function", True)],
    ("Tween", "Vector"): [("onComplete", "function", True)],
    ("Tween", "Position"): [("onComplete", "function", True)],
    ("Tween", "Rotation"): [("onComplete", "function", True)],
    ("Tween", "Scale"): [("onComplete", "function", True)],
    ("Tween", "Color"): [("onComplete", "function", True)],
}

# Whole parameter lists the parser cannot see (forwarding bodies, lua_gettop loops).
PARAMS_OVERRIDE = {
    ("Tween", "Quaternion"): [("easingType", "integer", False), ("from", "Vector", False), ("to", "Vector", False),
                              ("duration", "number", False), ("onUpdate", "function", False),
                              ("onComplete", "function", True)],
    ("Rect", "Create"): [("x", "number", True), ("y", "number", True), ("w", "number", True), ("h", "number", True)],
}

# Integer parameters that are enums but read without a `(Enum)CHECK_INTEGER` cast.
_EASE0, _EASE1 = {0: "Easing"}, {1: "Easing"}
PARAM_ENUMS = {
    ("Text", "SetHorizontalJustification"): {0: "Justification"},
    ("Text", "SetVerticalJustification"): {0: "Justification"},
    ("TextMesh3D", "SetHorizontalJustification"): {0: "Justification"},
    ("TextMesh3D", "SetVerticalJustification"): {0: "Justification"},
    ("Tween", "Value"): _EASE0, ("Tween", "Vector"): _EASE0, ("Tween", "Quaternion"): _EASE0,
    ("Tween", "Position"): _EASE1, ("Tween", "Rotation"): _EASE1, ("Tween", "Scale"): _EASE1,
    ("Tween", "Color"): _EASE1,
    ("SkeletalMesh3D", "SetSlotLayerMode"): {1: "AnimLayerMode"},
    ("System", "SetScreenOrientation"): {0: "ScreenOrientation"},
}
RETURN_ENUMS = {
    ("Text", "GetHorizontalJustification"): "Justification", ("Text", "GetVerticalJustification"): "Justification",
    ("TextMesh3D", "GetHorizontalJustification"): "Justification", ("TextMesh3D", "GetVerticalJustification"): "Justification",
    ("System", "GetScreenOrientation"): "ScreenOrientation",
}

# Functions on a class table that are static despite checking an engine type at slot 1.
STATIC_FUNCS = {("UIDocument", "Load"), ("UIDocument", "LoadFromString")}

PRIMITIVE_LUA = {"number", "integer", "string", "boolean", "table", "function", "userdata", "nil"}

# Alternative whole parameter lists (extra overloads), e.g. Vector instead of (x, y).
EXTRA_OVERLOADS = {
    ("Widget", "SetPosition"): [[("position", "Vector")]],
    ("Widget", "SetDimensions"): [[("dimensions", "Vector")]],
    ("Widget", "SetSize"): [[("size", "Vector")]],
    ("Widget", "SetOffset"): [[("offset", "Vector")]],
    ("Widget", "SetPivot"): [[("pivot", "Vector")]],
    ("Widget", "SetScale"): [[("scale", "Vector")]],
    ("World", "SetFogSettings"): [[("settings", "FogSettings")]],
    ("World", "SetFog"): [[("settings", "FogSettings")]],
    ("Network", "OpenSession"): [[], [("options", "SessionOptions")]],
    ("Network", "JoinSession"): [[("session", "NetSessionInfo")]],
    ("UIDocument", "Load"): [[("path", "string")]],
}

# Vararg functions: emit overloads with 0..N trailing `object` args.
VARARGS = {("Node", "EmitSignal"): 6, ("Node", "InvokeNetFunc"): 8,
           ("Signal", "Emit"): 6, ("SignalBus", "Emit"): 6}

# 1-based Lua integer parameters exposed 0-based in C#: (LuaClass, func) -> {paramIndex}.
INDEX_ADJUST = {("Node", "GetChild"): {0}, ("Network", "GetSession"): {0}}

# Return type refinement for functions the parser only sees as Node / Asset.
NODE_RETURN_RULES = [
    (re.compile(r"Camera$"), "Camera3D"), (re.compile(r"Button$"), "Button"),
    (re.compile(r"(Title)?Text$"), "Text"), (re.compile(r"Quad$"), "Quad"),
    (re.compile(r"InputField$"), "InputField"), (re.compile(r"ScrollContainer$"), "ScrollContainer"),
    (re.compile(r"(Widget|Background|Grabber|Caret|Track|Scrollbar|Handle|Container|TitleBar|ButtonBar|Bar)$"), "Widget"),
    (re.compile(r"TargetNode$"), "Node3D"),
]
ASSET_RETURN_RULES = [
    (re.compile(r"Material"), "Material"), (re.compile(r"Texture$"), "Texture"),
    (re.compile(r"StaticMesh$"), "StaticMesh"), (re.compile(r"SkeletalMesh$"), "SkeletalMesh"),
    (re.compile(r"SoundWave$"), "SoundWave"), (re.compile(r"Font$"), "Font"),
    (re.compile(r"Scene$"), "Scene"), (re.compile(r"ParticleSystem$"), "ParticleSystem"),
    (re.compile(r"Timeline$"), "Timeline"), (re.compile(r"UIDocument$"), "UIDocument"),
    (re.compile(r"NodeGraphAsset$"), "Asset"), (re.compile(r"ItemTemplate$"), "UIDocument"),
    (re.compile(r"^GetAnimation$"), "TransformAnimationAsset"),
]


# ---------------------------------------------------------------------------
# Hand-written API scan
# ---------------------------------------------------------------------------

_CLASS_DECL_RE = re.compile(
    r'(?:public|internal)\s+(?:static\s+|sealed\s+|abstract\s+|partial\s+)*(class|enum)\s+(\w+)(?:<\w+>)?')
_MEMBER_RE = re.compile(
    r'^\s*public\s+(?:static\s+|extern\s+|virtual\s+|new\s+|override\s+)*'
    r'[\w<>\[\],\.\s]+?\s+(\w+)\s*(?:\(|\{|;|=)', re.MULTILINE)


def scan_handwritten(api_dir):
    """Returns {csClassName: set(memberNames)} for the hand-written (non-generated) API."""
    result = {}
    for fname in sorted(os.listdir(api_dir)):
        if not fname.endswith(".cs"):
            continue
        text = gls.read_file(os.path.join(api_dir, fname))
        for m in _CLASS_DECL_RE.finditer(text):
            name = m.group(2)
            brace = text.find("{", m.end())
            body = gls._extract_brace_block(text, brace) if brace != -1 else None
            members = result.setdefault(name, set())
            if body is None:
                continue
            for mm in _MEMBER_RE.finditer(body):
                members.add(mm.group(1))
            # operators / constructors never collide with generated names
    return result


# ---------------------------------------------------------------------------
# C++ enum values
# ---------------------------------------------------------------------------

def parse_cpp_enums(src_dir):
    """{EnumName: {Member: intValue}} for every resolvable enum under Engine/Source."""
    enums = {}
    enum_re = re.compile(r'enum\s+(?:class\s+)?(\w+)\s*(?::\s*[\w:]+\s*)?\{(.*?)\}\s*;', re.DOTALL)
    for root, _dirs, files in os.walk(src_dir):
        if "External" in root:
            continue
        for f in files:
            if not f.endswith(".h"):
                continue
            text = gls.read_file(os.path.join(root, f))
            text = re.sub(r'//[^\n]*', '', text)
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
            for m in enum_re.finditer(text):
                name, body = m.group(1), m.group(2)
                body = re.sub(r'#[^\n]*', '', body)
                values = {}
                next_val = 0
                ok = True
                for entry in body.split(','):
                    entry = entry.strip()
                    if not entry:
                        continue
                    if '=' in entry:
                        member, expr = [s.strip() for s in entry.split('=', 1)]
                        expr = expr.strip()
                        if re.fullmatch(r'0[xX][0-9a-fA-F]+', expr):
                            val = int(expr, 16)
                        elif re.fullmatch(r'-?\d+', expr):
                            val = int(expr)
                        elif expr in values:
                            val = values[expr]
                        else:
                            ok = False
                            break
                    else:
                        member, val = entry, next_val
                    if not re.fullmatch(r'\w+', member):
                        ok = False
                        break
                    values[member] = val
                    next_val = val + 1
                if ok and values and name not in enums:
                    enums[name] = values
    return enums


def parse_lua_enum_tables(input_dir, cpp_enums, name_map):
    """
    Every `lua_setglobal(L, "X")` table filled with lua_pushinteger/lua_setfield.
    Returns [(globalName, [(field, value_or_None)])] — value resolved through
    the C++ enum when the pushed expression is `(int)Enum::Member`.
    """
    tables = {}
    for fname in sorted(os.listdir(input_dir)):
        if not fname.endswith("_Lua.cpp"):
            continue
        source = gls.read_file(os.path.join(input_dir, fname))
        for _func, body in gls.extract_function_bodies(source).items():
            gm = re.search(r'lua_setglobal\s*\(\s*L\s*,\s*(?:"(\w+)"|(\w+_LUA_NAME))\s*\)', body)
            if not gm or 'lua_pushinteger' not in body:
                continue
            global_name = gm.group(1) or name_map.get(gm.group(2))
            if not global_name:
                continue
            entries = []
            cpp_name = None
            pairs = [(m.group(1).strip(), m.group(2)) for m in re.finditer(
                r'lua_pushinteger\s*\(\s*L\s*,\s*([^;]+?)\)\s*;\s*lua_setfield\s*\(\s*L\s*,\s*[-\w]+\s*,\s*"([^"]+)"\s*\)', body)]
            pairs += [(m.group(2).strip(), m.group(1)) for m in re.finditer(
                r'lua_pushstring\s*\(\s*L\s*,\s*"([^"]+)"\s*\)\s*;\s*lua_pushinteger\s*\(\s*L\s*,\s*([^;]+?)\)\s*;\s*lua_rawset', body)]
            for expr, field in pairs:
                value = None
                em = re.search(r'\(\s*(?:int|int32_t|uint32_t|uint8_t|lua_Integer)\s*\)\s*(\w+)::(\w+)', expr)
                if em and em.group(1) in cpp_enums:
                    cpp_name = em.group(1)
                    value = cpp_enums[em.group(1)].get(em.group(2))
                elif re.fullmatch(r'-?\d+', expr):
                    value = int(expr)
                elif re.fullmatch(r'0[xX][0-9a-fA-F]+', expr):
                    value = int(expr, 16)
                elif re.fullmatch(r'\w+', expr):
                    for ename, members in cpp_enums.items():
                        if expr in members:
                            cpp_name, value = ename, members[expr]
                            break
                entries.append((field, value))
            if not entries:
                continue
            seen = set()
            deduped = []
            for field, value in entries:
                if field not in seen:
                    seen.add(field)
                    deduped.append((field, value))
            tables.setdefault(global_name, (deduped, cpp_name))
    return tables


# ---------------------------------------------------------------------------
# Binding model
# ---------------------------------------------------------------------------

class Method:
    def __init__(self, lua_name, params, returns, is_alias, body):
        self.lua_name = lua_name
        self.params = params        # [(name, luaType, optional)]
        self.returns = returns      # [luaType]
        self.is_static = False
        self.is_alias = is_alias
        self.body = body


class LuaType:
    def __init__(self, lua_name, kind, parent, source):
        self.lua_name = lua_name
        self.kind = kind            # class | value | module
        self.parent = parent
        self.methods = []
        self.source = source

    @property
    def cs_name(self):
        return CLASS_RENAME.get(self.lua_name, self.lua_name)


def _related(by_lua, a, b):
    """True when Lua class a is b, derives from b, or b derives from a."""
    def chain(name):
        out = []
        t = by_lua.get(name)
        while t is not None:
            out.append(t.lua_name)
            t = by_lua.get(t.parent) if t.parent else None
        return out
    return b in chain(a) or a in chain(b)


def _is_static(by_lua, t, m):
    """A class-table function is static unless it checks `self` at stack slot 1."""
    key = (t.lua_name, m.lua_name)
    if key in STATIC_FUNCS:
        return True
    body = m.body
    if body is None:
        return False
    if re.search(r'(?:lua_touserdata|luaL_checkudata|lua_isuserdata)\s*\(\s*L\s*,\s*1\b', body):
        return False
    for cm in re.finditer(r'(\w*[Cc][Hh][Ee][Cc][Kk]\w*)\s*\(\s*L\s*,\s*1\b', body):
        macro = cm.group(1)
        lt = gls._macro_to_lua_type(macro) if macro.startswith("CHECK_") else None
        if lt is None:
            return False                # custom checker (CheckResponse...) -> self
        if lt in PRIMITIVE_LUA:
            continue                    # a plain argument in slot 1 -> static
        if _related(by_lua, t.lua_name, lt):
            return False
    return True


def load_types(input_dir):
    name_map, _flags, _ = gls.parse_headers(input_dir)
    check_map = gls.build_check_map_from_headers(input_dir)
    gls.ENGINE_CHECK_MAP.update(check_map)

    types = []
    for fname in sorted(f for f in os.listdir(input_dir) if f.endswith("_Lua.cpp")):
        path = os.path.join(input_dir, fname)
        infos, _enums = gls.process_binding_file(path, name_map, gls.ENGINE_CHECK_MAP)
        if not infos:
            continue
        source = gls.read_file(path)
        prefix_m = re.search(r'(\w+_Lua)::Bind\b', source)
        prefix = prefix_m.group(1) if prefix_m else None
        bodies = gls.extract_function_bodies(source)

        for info in infos:
            lua_name = info.lua_name
            if lua_name.endswith("_LUA_NAME"):
                lua_name = name_map.get(lua_name) or CLASS_RENAME.get(lua_name, lua_name)
            if lua_name in SKIP_TYPES:
                continue
            t = LuaType(lua_name, info.kind, info.parent, fname)
            for m in info.methods:
                body = bodies.get(f"{prefix}::{m.cpp_func}") if prefix else None
                params = [(p.name, p.lua_type, p.optional) for p in m.params]
                t.methods.append(Method(m.name, params, list(m.returns), m.is_alias, body))
            types.append(t)

    by_lua = {t.lua_name: t for t in types}
    for t in types:
        for m in t.methods:
            key = (t.lua_name, m.lua_name)
            m.is_static = t.kind == "module" or _is_static(by_lua, t, m)
            if key in PARAMS_OVERRIDE:
                m.params = list(PARAMS_OVERRIDE[key])
            elif m.is_static and t.kind != "module" and m.body is not None:
                m.params = [(p.name, p.lua_type, p.optional)
                            for p in gls.parse_check_macros_from_body(m.body, False)]
    return types


# ---------------------------------------------------------------------------
# C# emission
# ---------------------------------------------------------------------------

class Emitter:
    def __init__(self, types, handwritten, enum_cs):
        self.types = types
        self.by_lua = {t.lua_name: t for t in types}
        self.hand = handwritten
        self.known_cs = {t.cs_name for t in types} | set(handwritten.keys())
        self.enum_cs = enum_cs      # C++ enum name -> C# enum name (real enums only)

    def enum_param_types(self, m, offset):
        """{paramIndex: C# enum} for integer params the binding casts to a C++ enum."""
        out = {}
        if not m.body:
            return out
        for cm in re.finditer(r'\(\s*(\w+)\s*\)\s*CHECK_(?:INTEGER|INDEX)\s*\(\s*L\s*,\s*(\d+)\s*\)', m.body):
            cs = self.enum_cs.get(cm.group(1))
            if cs:
                out[int(cm.group(2)) - 1 - offset] = cs
        return out

    def enum_return_type(self, m):
        if not m.body or m.returns != ["integer"]:
            return None
        rm = re.search(r'\b(\w+)\s+ret\s*=', m.body)
        if rm and self.enum_cs.get(rm.group(1)) and re.search(
                r'lua_pushinteger\s*\(\s*L\s*,\s*\(\s*(?:int|int32_t|uint32_t)\s*\)\s*ret\s*\)', m.body):
            return self.enum_cs[rm.group(1)]
        return None

    # ---- type mapping ----

    def cs_type(self, lua_type, hint=""):
        if lua_type in LUA_TO_CS:
            t = LUA_TO_CS[lua_type]
            if t == "Vector3" and "color" in hint.lower():
                return "Color"
            return t
        t = self.by_lua.get(lua_type)
        if t is not None:
            return t.cs_name
        if lua_type in self.known_cs:
            return lua_type
        return "object"

    def return_type(self, cls, m):
        key = (cls.lua_name, m.lua_name)
        if key in ARRAY_RETURNS:
            return self.cs_type(ARRAY_RETURNS[key]) + "[]"
        if key in RETURN_OVERRIDES:
            return RETURN_OVERRIDES[key]
        if m.is_static and cls.kind != "module" and m.lua_name in ("Create", "Construct", "New"):
            return cls.cs_name
        if not m.returns:
            return "void"
        multi = self.multi_return(cls, m)
        if multi:
            return multi[0]
        first = m.returns[0]
        if first == "Node":
            for rx, name in NODE_RETURN_RULES:
                if rx.search(m.lua_name) and name in self.known_cs:
                    return name
            return "Node"
        if first == "Asset":
            for rx, name in ASSET_RETURN_RULES:
                if rx.search(m.lua_name) and name in self.known_cs:
                    return name
            return "Asset"
        if first == "table":
            return "object"
        enum_ret = self.enum_return_type(m) or RETURN_ENUMS.get(key)
        if enum_ret and enum_ret in self.enum_cs.values():
            return enum_ret
        return self.cs_type(first, m.lua_name)

    def multi_return(self, cls, m):
        """(csType, wrapperTemplateFormat) for bindings that return several Lua values:
        2-4 numbers -> Vector2/Vector3 (or Rect when the name says so); N values of one
        handle type -> T[] via CSharpCore.Pack. None when the first value is exposed alone."""
        key = (cls.lua_name, m.lua_name)
        if len(m.returns) < 2 or key in RETURN_OVERRIDES or key in ARRAY_RETURNS:
            return None
        rets = m.returns
        if all(r in ("number", "integer") for r in rets) and len(rets) <= 4:
            if len(rets) == 4 and "Rect" in m.lua_name:
                return ("Rect", "Rect.Create({call})")
            return ("Vector2" if len(rets) == 2 else "Vector3", "Vec({call})")
        first = rets[0]
        if all(r == first for r in rets) and first in ("Node", "Asset", "Vector"):
            cs = {"Node": "Node", "Asset": "Asset", "Vector": "Vector3"}[first]
            return (cs + "[]", "CSharpCore.Array(CSharpCore.Pack({call}), Polyphase." + cs + ")")
        return None

    # ---- members ----

    def method_lines(self, cls, m, receiver, hand_members, seen_sigs):
        """Emit every overload of one Lua function. receiver: "{this}" | "{this}.__node" | None (static)."""
        key = (cls.lua_name, m.lua_name)
        cs_name = m.lua_name
        if cs_name in hand_members or cs_name == cls.cs_name or cs_name in CS_KEYWORDS:
            return []

        params = list(m.params)
        for extra in EXTRA_PARAMS.get(key, []):
            if extra[0] not in [p[0] for p in params]:
                params.append(extra)

        ret = self.return_type(cls, m)
        lines = []

        param_lists = [params]
        for alt in EXTRA_OVERLOADS.get(key, []):
            param_lists.append([(n, t, False) for n, t in alt])

        for plist in param_lists:
            first_opt = len(plist)
            for i, p in enumerate(plist):
                if p[2]:
                    first_opt = i
                    break
            counts = list(range(first_opt, len(plist) + 1))
            if len(counts) > 6:
                counts = counts[:1] + counts[-5:]
            if key in VARARGS:
                counts = [len(plist) + n for n in range(VARARGS[key] + 1)]
                plist = plist + [(f"arg{n + 1}", "any", False) for n in range(VARARGS[key])]

            cb = CALLBACK_TYPES.get(key, {})
            enum_params = self.enum_param_types(m, 0 if receiver is None else 1) if plist is params else {}
            for idx, enum_name in PARAM_ENUMS.get(key, {}).items():
                if enum_name in self.enum_cs.values():
                    enum_params[idx] = enum_name
            for n in counts:
                sub = plist[:n]
                alternatives = []
                for i, (pname, ptype, _opt) in enumerate(sub):
                    if i in cb:
                        alternatives.append(cb[i])
                    elif ptype == "function":
                        alternatives.append(["object"])
                    elif ptype == "integer" and i in enum_params:
                        alternatives.append([enum_params[i]])
                    else:
                        alternatives.append([self.cs_type(ptype, pname)])
                for combo in itertools.product(*alternatives) if sub else [()]:
                    names = self.param_names([p[0] for p in sub])
                    sig = tuple(combo)
                    if sig in seen_sigs.get(cs_name, set()):
                        continue
                    seen_sigs.setdefault(cs_name, set()).add(sig)
                    args = []
                    adjust = INDEX_ADJUST.get(key, set())
                    for i in range(n):
                        args.append(f"(({{{i}}}) + 1)" if i in adjust else f"{{{i}}}")
                    call = ", ".join(args)
                    if receiver is None:
                        glob = STATIC_GLOBAL.get(cls.lua_name, cls.lua_name)
                        template = f"{glob}.{m.lua_name}({call})"
                    else:
                        template = f"{receiver}:{m.lua_name}({call})"
                    if key in ARRAY_RETURNS:
                        elem = ARRAY_RETURNS[key]
                        elem_cs = self.cs_type(elem)
                        elem_path = ELEM_PATH.get(elem_cs, "Polyphase." + elem_cs)
                        template = f"CSharpCore.Array({template}, {elem_path})"
                    elif RETURN_OVERRIDES.get(key) == "ScreenTrace":
                        template = f"CSharpCore.Pack({template})"
                    elif self.multi_return(cls, m):
                        template = self.multi_return(cls, m)[1].replace("{call}", template)
                    decl = ", ".join(f"{t} {nm}" for t, nm in zip(combo, names))
                    static = "static " if receiver is None else ""
                    note = ""
                    if len(m.returns) > 1 and key not in RETURN_OVERRIDES and not self.multi_return(cls, m):
                        note = f"        /// <summary>Lua returns {len(m.returns)} values; only the first is exposed here.</summary>\n"
                    lines.append(note +
                                 f'        /// @CSharpLua.Template = "{template}"\n'
                                 f"        public {static}extern {ret} {cs_name}({decl});")
        return lines

    @staticmethod
    def param_names(names):
        out = []
        for n in names:
            n = re.sub(r'\W', '_', n) or "arg"
            if n in CS_KEYWORDS or n[0].isdigit():
                n = n + "_"
            base, k = n, 2
            while n in out:
                n = f"{base}{k}"
                k += 1
            out.append(n)
        return out

    # ---- classes ----

    def class_file(self, t):
        hand_members = self.hand.get(t.cs_name, set())
        is_hand = t.cs_name in self.hand
        lines = [
            "// <auto-generated> by Tools/generate_csharp_api.py from " + t.source + " - DO NOT EDIT.",
            "#pragma warning disable CS0626, CS0108, CS0109",
            "using System;",
            "",
            "namespace Polyphase",
            "{",
        ]
        if t.kind == "module":
            lines.append(f"    /// <summary>Lua global `{STATIC_GLOBAL.get(t.lua_name, t.lua_name)}`.</summary>")
            lines.append(f"    public static partial class {t.cs_name}")
        else:
            base = ""
            if t.parent:
                pt = self.by_lua.get(t.parent)
                base = " : " + (pt.cs_name if pt else t.parent)
            lines.append(f"    /// <summary>Engine `{t.lua_name}` ({t.kind}); the Lua value is the engine userdata.</summary>")
            lines.append(f"    public partial class {t.cs_name}{base}")
        lines.append("    {")
        if t.kind != "module" and not is_hand:
            lines.append(f"        protected {t.cs_name}() {{ }}")
            lines.append("")
        seen = {}
        for m in t.methods:
            receiver = None if (m.is_static or t.kind == "module") else "{this}"
            for l in self.method_lines(t, m, receiver, hand_members, seen):
                lines.append(l)
                lines.append("")
        if lines[-1] == "":
            lines.pop()
        lines.append("    }")
        lines.append("}")
        return "\n".join(lines) + "\n"

    def mirror_file(self, script_cs, lua_class):
        t = self.by_lua[lua_class]
        hand_members = set(self.hand.get(script_cs, set()))
        # Script<T>/Script3D inherit Script's mirror: skip what a base mirror already emits.
        for base_script, base_lua in MIRRORS:
            if base_script != script_cs and lua_class != base_lua and self.inherits(lua_class, base_lua):
                hand_members |= {m.lua_name for m in self.by_lua[base_lua].methods}
                hand_members |= self.hand.get(base_script, set())
        lines = [
            "// <auto-generated> by Tools/generate_csharp_api.py - bare-call mirror of " + lua_class + " - DO NOT EDIT.",
            "#pragma warning disable CS0626, CS0108, CS0109",
            "using System;",
            "",
            "namespace Polyphase",
            "{",
            f"    public partial class {script_cs}",
            "    {",
        ]
        seen = {}
        for m in t.methods:
            if m.is_static or m.lua_name in LIFECYCLE:
                continue
            for l in self.method_lines(t, m, "{this}.__node", hand_members, seen):
                lines.append(l)
                lines.append("")
        if lines[-1] == "":
            lines.pop()
        lines.append("    }")
        lines.append("}")
        return "\n".join(lines) + "\n"

    def inherits(self, lua_class, base):
        t = self.by_lua.get(lua_class)
        while t is not None:
            if t.lua_name == base:
                return True
            t = self.by_lua.get(t.parent) if t.parent else None
        return False


def is_real_enum(global_name, entries, handwritten):
    cs_name = CLASS_RENAME.get(global_name, global_name)
    valid = [(f, v) for f, v in entries if re.fullmatch(r'[A-Za-z_]\w*', f) and f not in CS_KEYWORDS]
    return bool(valid) and all(v is not None for _f, v in valid) and cs_name not in handwritten


def enum_file(global_name, entries, handwritten):
    cs_name = CLASS_RENAME.get(global_name, global_name)
    valid = [(f, v) for f, v in entries if re.fullmatch(r'[A-Za-z_]\w*', f) and f not in CS_KEYWORDS]
    lines = [
        "// <auto-generated> by Tools/generate_csharp_api.py - DO NOT EDIT.",
        "#pragma warning disable CS0626",
        "",
        "namespace Polyphase",
        "{",
    ]
    if is_real_enum(global_name, entries, handwritten):
        lines.append(f"    /// <summary>Same values as the Lua global `{global_name}`.</summary>")
        lines.append(f"    public enum {cs_name}")
        lines.append("    {")
        for f, v in valid:
            lines.append(f"        {f} = {v},")
        lines.append("    }")
    else:
        # Values are platform constants (key codes) or unresolved: read them from Lua.
        lines.append(f"    /// <summary>Lua global `{global_name}`; values come from the engine at runtime.</summary>")
        lines.append(f"    public static partial class {cs_name}")
        lines.append("    {")
        hand = handwritten.get(cs_name, set())
        for f, _v in valid:
            if f in hand:
                continue
            lines.append(f'        /// @CSharpLua.Get = "{global_name}.{f}"')
            lines.append(f"        public static extern int {f} {{ get; }}")
        lines.append("    }")
    lines.append("}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Generate the Polyphase C# API from the Lua bindings")
    ap.add_argument("--check", action="store_true", help="verify generated files are up to date")
    args = ap.parse_args()

    handwritten = scan_handwritten(API_DIR)
    types = load_types(INPUT_DIR)
    cpp_enums = parse_cpp_enums(ENGINE_SRC)
    name_map, _flags, _ = gls.parse_headers(INPUT_DIR)
    enum_tables = parse_lua_enum_tables(INPUT_DIR, cpp_enums, name_map)

    type_names = {t.cs_name for t in types}
    enum_cs = {}
    for global_name, (entries, cpp_name) in enum_tables.items():
        if cpp_name and global_name not in SKIP_TYPES and global_name not in type_names \
                and is_real_enum(global_name, entries, handwritten):
            enum_cs[cpp_name] = CLASS_RENAME.get(global_name, global_name)

    types = [t for t in types if t.methods or t.lua_name not in enum_tables]
    type_names = {t.cs_name for t in types}
    enum_cs = {}
    for global_name, (entries, cpp_name) in enum_tables.items():
        if cpp_name and global_name not in SKIP_TYPES and global_name not in type_names \
                and is_real_enum(global_name, entries, handwritten):
            enum_cs[cpp_name] = CLASS_RENAME.get(global_name, global_name)

    emitter = Emitter(types, handwritten, enum_cs)
    outputs = {}
    for t in types:
        outputs[f"{t.cs_name}.cs"] = emitter.class_file(t)
    for script_cs, lua_class in MIRRORS:
        if lua_class in emitter.by_lua:
            outputs[f"{script_cs}.Mirror.cs"] = emitter.mirror_file(script_cs, lua_class)
    for global_name, (entries, _cpp) in sorted(enum_tables.items()):
        if global_name in SKIP_TYPES or global_name in type_names:
            continue
        outputs[f"{CLASS_RENAME.get(global_name, global_name)}.cs"] = enum_file(global_name, entries, handwritten)

    os.makedirs(OUT_DIR, exist_ok=True)
    existing = {f for f in os.listdir(OUT_DIR) if f.endswith(".cs")}
    changed = []
    for fname, content in outputs.items():
        path = os.path.join(OUT_DIR, fname)
        old = gls.read_file(path) if os.path.exists(path) else None
        if old != content:
            changed.append(fname)
            if not args.check:
                with open(path, "w", encoding="utf-8", newline="\n") as f:
                    f.write(content)
    stale = sorted(existing - set(outputs.keys()))
    for fname in stale:
        changed.append(fname + " (stale)")
        if not args.check:
            os.remove(os.path.join(OUT_DIR, fname))

    unresolved = [(g, [f for f, v in e if v is None]) for g, (e, _c) in enum_tables.items()
                  if any(v is None for _f, v in e) and g not in SKIP_TYPES]
    print(f"generate_csharp_api: {len(types)} types, {len(enum_tables)} enum tables -> {len(outputs)} files "
          f"({len(changed)} changed)")
    for g, fields in unresolved:
        print(f"  note: {g} emitted as int getters (unresolved: {', '.join(fields[:4])}{'...' if len(fields) > 4 else ''})")
    if args.check and changed:
        print("generate_csharp_api: OUT OF DATE: " + ", ".join(changed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
