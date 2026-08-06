-- ~PolyphaseSharp~ CSharpCore runtime bundle - DO NOT EDIT, regenerated on every C# build.
-- CSharp.lua CoreSystem (Apache-2.0, https://github.com/yanghuan/CSharp.lua)

CSharpCore = CSharpCore or {}

if rawget(_G, "__POLYPHASE_CSHARP_CORE__") then return end
__POLYPHASE_CSHARP_CORE__ = true

CSharpLuaSingleFile = true

-- CoreSystemLib: Core.lua
do
local setmetatable = setmetatable
local getmetatable = getmetatable
local type = type
local pairs  = pairs
local assert = assert
local table = table
local tremove = table.remove
local tconcat = table.concat
local floor = math.floor
local ceil = math.ceil
local error = error
local select = select
local xpcall = xpcall
local rawget = rawget
local rawset = rawset
local rawequal = rawequal
local tostring = tostring
local string = string
local sfind = string.find
local ssub = string.sub
local debug = debug
local global = _G
local prevSystem = rawget(global, "System")
local emptyFn = function() end
local nilFn = function() return nil end
local falseFn = function() return false end
local trueFn = function() return true end
local identityFn = function(x) return x end
local lengthFn = function (t) return #t end
local zeroFn = function() return 0 end
local oneFn = function() return 1 end
local equals = function(x, y) return x == y end
local getCurrent = function(t) return t.current end
local assembly, metadatas
local System, Object, ValueType
local function new(cls, ...)
  local this = setmetatable({}, cls)
  return this, cls.__ctor__(this, ...)
end
local function throw(e, lv)
  if e == nil then e = System.NullReferenceException() end
  e:traceback(lv)
  error(e)
end
local function xpcallErr(e)
  if e == nil then
    e = System.Exception("script error")
    e:traceback()
  elseif type(e) == "string" then
    if sfind(e, "attempt to index") then
      e = System.NullReferenceException(e)
    elseif sfind(e, "attempt to divide by zero") then
      e = System.DivideByZeroException(e)
    else
      e = System.Exception(e)
    end
    e:traceback()
  end
  return e
end
local function try(tryFn, catch, finally)
  local ok, status, result = xpcall(tryFn, xpcallErr)
  if not ok then
    if catch then
      if finally then
        ok, status, result = xpcall(catch, xpcallErr, status)
      else
        ok, status, result = true, catch(status)
      end
      if ok then
        if status == 1 then
          ok = false
          status = result
        end
      end
    end
  end
  if finally then
    finally()
  end
  if not ok then
    error(status)
  end
  return status, result
end
local function set(className, cls)
  local scope = global
  local starIndex = 1
  while true do
    local pos = sfind(className, "[%.+]", starIndex) or 0
    local name = ssub(className, starIndex, pos -1)
    if pos ~= 0 then
      local t = rawget(scope, name)
      if t == nil then
        if cls then
          t = {}
          rawset(scope, name, t)
        else
          return nil
        end
      end
      scope = t
      starIndex = pos + 1
    else
      if cls then
        assert(rawget(scope, name) == nil, className)
        rawset(scope, name, cls)
        return cls
      else
        return rawget(scope, name)
      end
    end
  end
end
local function multiKey(t, f, ...)
  local n, i, k = select("#", ...), 1
  while true do
    local arg = select(i, ...)
    if not arg then error(i .. " is nil") end
    if f then
      k = f(arg)
    else
      k = arg
    end
    if i == n then
      break
    end
    local tk = t[k]
    if tk == nil then
      tk = {}
      t[k] = tk
    end
    t = tk
    i = i + 1
  end
  return t, k
end
local function genericName(name, ...)
  local n = select("#", ...)
  local t = { name, "`", n, "[" }
  local count = 5
  local hascomma
  for i = 1, n do
    local cls = select(i, ...)
    if hascomma then
      t[count] = ","
      count = count + 1
    else
      hascomma = true
    end
    t[count] = cls.__name__
    count = count + 1
  end
  t[count] = "]"
  return tconcat(t)
end
local enumMetatable = { class = "E", default = zeroFn, __index = false, interface = false, __call = function (_, v) return v or 0 end }
enumMetatable.__index = enumMetatable
local interfaceMetatable = { class = "I", default = nilFn, __index = false }
interfaceMetatable.__index = interfaceMetatable
local ctorMetatable = { __call = function (ctor, ...) return ctor[1](...) end }
local function applyExtends(cls)
  local extends = cls.base
  if extends then
    if type(extends) == "function" then
      extends = extends(global, cls)
    end
    cls.base = nil
  end
  return extends
end
local function applyMetadata(cls)
  local metadata = cls.__metadata__
  if metadata then
    if metadatas then
      metadatas[#metadatas + 1] = function (global)
        cls.__metadata__ = metadata(global)
      end
    else
      cls.__metadata__ = metadata(global)
    end
  end
end
local function setInterface(cls, interfaces)
  cls.interface = interfaces
  for  i = 1, #interfaces do
    local extern = interfaces[i].extern
    if extern then
      for k, v in pairs(extern) do
        if cls[k] == nil then
          cls[k] = v
        end
      end
    end
  end
end
local function setBase(cls, kind)
  local ctor = cls.__ctor__
  if ctor and type(ctor) == "table" then
    setmetatable(ctor, ctorMetatable)
  end
  local extends = applyExtends(cls)
  applyMetadata(cls)
  cls.__index = cls
  cls.__call = new
  if extends then
    local base = extends[1]
    if not base then error(cls.__name__ .. "'s base is nil") end
    if base.class == "I" then
      setmetatable(cls, kind ~= "S" and Object or ValueType)
      setInterface(cls, extends)
    else
      setmetatable(cls, base)
      if #extends > 1 then
        tremove(extends, 1)
        setInterface(cls, extends)
      end
    end
  else
    setmetatable(cls, kind ~= "S" and Object or ValueType)
  end
end
local function staticCtorSetBase(cls)
  setmetatable(cls, nil)
  local t = cls[cls]
  for k, v in pairs(t) do
    cls[k] = v
  end
  cls[cls] = nil
  local kind = cls.class
  cls.class = nil
  setBase(cls, kind)
  cls:static()
  cls.static = nil
end
local staticCtorMetatable = {
  __index = function(cls, key)
    staticCtorSetBase(cls)
    return cls[key]
  end,
  __newindex = function(cls, key, value)
    staticCtorSetBase(cls)
    cls[key] = value
  end,
  __call = function(cls, ...)
    staticCtorSetBase(cls)
    return new(cls, ...)
  end
}
local function setHasStaticCtor(cls, kind)
  local name = cls.__name__
  cls.__name__ = nil
  local t = {}
  for k, v in pairs(cls) do
    t[k] = v
    cls[k] = nil
  end
  cls[cls] = t
  cls.__name__ = name
  cls.class = kind
  cls.__call = new
  cls.__index = cls
  setmetatable(cls, staticCtorMetatable)
end
local function defCore(name, kind, cls, generic)
  cls = cls or {}
  cls.__name__ = name
  cls.__assembly__ = assembly
  if not generic then
    set(name, cls)
  end
  if kind == "C" or kind == "S" then
    if cls.static == nil then
      setBase(cls, kind)
    else
      setHasStaticCtor(cls, kind)
    end
  elseif kind == "I" then
    local extends = applyExtends(cls)
    if extends then
      cls.interface = extends
    end
    applyMetadata(cls)
    setmetatable(cls, interfaceMetatable)
  elseif kind == "E" then
    applyMetadata(cls)
    setmetatable(cls, enumMetatable)
  else
    assert(false, kind)
  end
  return cls
end
local genericClassKey = {}
local function getGenericClass(cls)
  return cls[genericClassKey]
end
local function getDefGenericClass(name, kind, generic, genericArgumentCount)
  local genericClass, genericBaseName
  if generic then
    generic.__index = generic
    generic.__call = new
    genericClass = generic
  else
    genericClass = {}
  end
  genericClass[genericClassKey] = genericClass
  if kind == 'I' then
    genericClass.class = 'I'
  end
  local _, i = name:find('.*_')
  if i then
    genericBaseName = name:sub(1, i - 1)
    genericArgumentCount = name:sub(i + 1, i + 1)
  else
    genericBaseName = name
    if not genericArgumentCount then error(name .. ' has not pass genericArgumentCount') end
  end
  genericClass.__name__ = genericBaseName .. '`' .. genericArgumentCount
  return genericClass, genericBaseName
end
local function defGeneric(name, kind, cls, generic, ...)
  local genericClass, genericBaseName = getDefGenericClass(name, kind, generic, ...)
  local mt = {}
  local fn = function(_, ...)
    local gt, gk = multiKey(mt, nil, ...)
    local t = gt[gk]
    if t == nil then
      local class, super  = cls(...)
      t = class or {}
      t[genericClassKey] = genericClass
      defCore(genericName(genericBaseName, ...), kind, t, true)
      if generic then
        setmetatable(t, super or generic)
      end
      gt[gk] = t
    end
    return t
  end
  local base = kind ~= "S" and Object or ValueType
  local caller = setmetatable({ __call = fn, __index = base }, base)
  return set(name, setmetatable(genericClass, caller))
end
local function def(name, kind, cls, ...)
  if type(cls) == "function" then
    return defGeneric(name, kind, cls, ...)
  else
    return defCore(name, kind, cls, ...)
  end
end
local function defCls(name, cls, ...)
  return def(name, "C", cls, ...)
end
local function defInf(name, cls, ...)
  return def(name, "I", cls, ...)
end
local function defStc(name, cls, ...)
  return def(name, "S", cls, ...)
end
local function defEnum(name, cls)
  return def(name, "E", cls)
end
local function defArray(name, cls, Array, MultiArray)
  Array.__index = Array
  MultiArray.__index =  MultiArray
  setmetatable(MultiArray, Array)
  local mt = {}
  local function create(Array, T)
    local ArrayT = mt[T]
    if ArrayT == nil then
      ArrayT = defCore(T.__name__ .. "[]", "C", cls(T), true)
      setmetatable(ArrayT, Array)
      mt[T] = ArrayT
    end
    return ArrayT
  end
  local mtMulti = {}
  local function createMulti(MultiArray, T, dimension)
    local gt, gk = multiKey(mtMulti, nil, T, dimension)
    local ArrayT = gt[gk]
    if ArrayT == nil then
      local name = T.__name__ .. "[" .. (","):rep(dimension - 1) .. "]"
      ArrayT = defCore(name, "C", cls(T), true)
      setmetatable(ArrayT, MultiArray)
      gt[gk] = ArrayT
    end
    return ArrayT
  end
  return set(name, setmetatable(Array, {
    __index = Object,
    __call = function (Array, T, dimension)
      if not dimension then
        return create(Array, T)
      else
        return createMulti(MultiArray, T, dimension)
      end
    end
  }))
end
local function trunc(num)
  return num > 0 and floor(num) or ceil(num)
end
local function when(f, ...)
  local ok, r = pcall(f, ...)
  return ok and r
end
System = {
  emptyFn = emptyFn,
  falseFn = falseFn,
  trueFn = trueFn,
  identityFn = identityFn,
  lengthFn = lengthFn,
  nilFn = nilFn,
  zeroFn = zeroFn,
  oneFn = oneFn,
  equals = equals,
  getCurrent = getCurrent,
  try = try,
  when = when,
  throw = throw,
  getClass = set,
  multiKey = multiKey,
  getGenericClass = getGenericClass,
  define = defCls,
  defInf = defInf,
  defStc = defStc,
  defEnum = defEnum,
  defArray = defArray,
  enumMetatable = enumMetatable,
  trunc = trunc,
  global = global
}
if prevSystem then
  setmetatable(System, { __index = prevSystem })
end
global.System = System
local debugsetmetatable = debug and debug.setmetatable
System.debugsetmetatable = debugsetmetatable
local _, _, version = sfind(_VERSION, "^Lua (.*)$")
version = tonumber(version)
System.luaVersion = version
if version < 5.3 then
  local bnot, band, bor, xor, sl, sr
  local bit = rawget(global, "bit")
  if not bit then
    local b32 = rawget(global, "bit32")
    if not b32 then
      local ok, b = pcall(require, "bit")
      if ok then
        bit = b
      end
    else
      bit = b32
    end
  end
  if bit then
    bnot, band, bor, xor, sl, sr = bit.bnot, bit.band, bit.bor, bit.bxor, bit.lshift, bit.rshift
  else
    local function disable()
      throw(System.NotSupportedException("bit operation is not enabled."))
    end
    bnot, band, bor, xor, sl, sr  = disable, disable, disable, disable, disable, disable
  end
  System.bnot = bnot
  System.band = band
  System.bor = bor
  System.xor = xor
  System.sl = sl
  System.sr = sr
  function System.div(x, y)
    if y == 0 then throw(System.DivideByZeroException(), 1) end
    return trunc(x / y)
  end
  function System.mod(x, y)
    if y == 0 then throw(System.DivideByZeroException(), 1) end
    local v = x % y
    if v ~= 0 and x * y < 0 then
      return v - y
    end
    return v
  end
  function System.modf(x, y)
    local v = x % y
    if v ~= 0 and x * y < 0 then
      return v - y
    end
    return v
  end
  function System.toUInt(v, max, mask, checked)
    if v >= 0 and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return band(v, mask)
  end
  function System.ToUInt(v, max, mask, checked)
    v = trunc(v)
    if v >= 0 and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    if v < -2147483648 or v > 2147483647 then
      return 0
    end
    return band(v, mask)
  end
  local function toInt(v, mask, umask)
    v = band(v, mask)
    local uv = band(v, umask)
    if uv ~= v then
      v = xor(uv - 1, umask)
      if uv ~= 0 then
        v = -v
      end
    end
    return v
  end
  function System.toInt(v, min, max, mask, umask, checked)
    if v >= min and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return toInt(v, mask, umask)
  end
  function System.ToInt(v, min, max, mask, umask, checked)
    v = trunc(v)
    if v >= min and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    if v < -2147483648 or v > 2147483647 then
      return 0
    end
    return toInt(v, mask, umask)
  end
  local function toUInt32(v)
    if v <= -2251799813685248 or v >= 2251799813685248 then
      throw(System.InvalidCastException())
    end
    v = band(v, 0xffffffff)
    local uv = band(v, 0x7fffffff)
    if uv ~= v then
      return uv + 0x80000000
    end
    return v
  end
  function System.toUInt32(v, checked)
    if v >= 0 and v <= 4294967295 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return toUInt32(v)
  end
  function System.ToUInt32(v, checked)
    v = trunc(v)
    if v >= 0 and v <= 4294967295 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return toUInt32(v)
  end
  function System.toInt32(v, checked)
    if v >= -2147483648 and v <= 2147483647 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    if v <= -2251799813685248 or v >= 2251799813685248 then
      throw(System.InvalidCastException())
    end
    return band(v, 0xffffffff)
  end
  function System.toInt64(v, checked)
    if v >= (-9223372036854775807 - 1) and v <= 9223372036854775807 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    throw(System.InvalidCastException())
  end
  function System.toUInt64(v, checked)
    if v >= 0 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    if v >= -2147483648 then
      return band(v, 0x7fffffff) + 0xffffffff80000000
    end
    throw(System.InvalidCastException())
  end
  function System.ToUInt64(v, checked)
    v = trunc(v)
    if v >= 0 and v <= 18446744073709551615 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    if v >= -2147483648 and v <= 2147483647 then
      v = band(v, 0xffffffff)
      local uv = band(v, 0x7fffffff)
      if uv ~= v then
        return uv + 0xffffffff80000000
      end
      return v
    end
    throw(System.InvalidCastException())
  end
  if table.pack == nil then
    table.pack = function(...)
      return { n = select("#", ...), ... }
    end
  end
  if table.unpack == nil then
    table.unpack = assert(unpack)
  end
  if table.move == nil then
    table.move = function(a1, f, e, t, a2)
      if a2 == nil then a2 = a1 end
      if t > f then
        t = e - f + t
        while e >= f do
          a2[t] = a1[e]
          t = t - 1
          e = e - 1
        end
      else
        while f <= e do
          a2[t] = a1[f]
          t = t + 1
          f = f + 1
        end
      end
    end
  end
else
  load[[
  local System = System
  local throw = System.throw
  local trunc = System.trunc
  function System.bnot(x) return ~x end
  function System.band(x, y) return x & y end
  function System.bor(x, y) return x | y end
  function System.xor(x, y) return x ~ y end
  function System.sl(x, y) return x << y end
  function System.sr(x, y) return x >> y end
  function System.div(x, y) if x ~ y < 0 then return -(-x // y) end return x // y end
  function System.mod(x, y)
    local v = x % y
    if v ~= 0 and 1.0 * x * y < 0 then
      return v - y
    end
    return v
  end
  function System.modf(x, y)
    local v = x % y
    if v ~= 0 and x * y < 0 then
      return v - y
    end
    return v
  end
  local function toUInt(v, max, mask, checked)
    if v >= 0 and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 2)
    end
    return v & mask
  end
  System.toUInt = toUInt
  function System.ToUInt(v, max, mask, checked)
    v = trunc(v)
    if v >= 0 and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 2)
    end
    if v < -2147483648 or v > 2147483647 then
      return 0
    end
    return v & mask
  end
  local function toSingedInt(v, mask, umask)
    v = v & mask
    local uv = v & umask
    if uv ~= v then
      v = (uv - 1) ~ umask
      if uv ~= 0 then
        v = -v
      end
    end
    return v
  end
  local function toInt(v, min, max, mask, umask, checked)
    if v >= min and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 2)
    end
    return toSingedInt(v, mask, umask)
  end
  System.toInt = toInt
  function System.ToInt(v, min, max, mask, umask, checked)
    v = trunc(v)
    if v >= min and v <= max then
      return v
    end
    if checked then
      throw(System.OverflowException(), 2)
    end
    if v < -2147483648 or v > 2147483647 then
      return 0
    end
    return toSingedInt(v, mask, umask)
  end
  function System.toUInt32(v, checked)
    return toUInt(v, 4294967295, 0xffffffff, checked)
  end
  function System.ToUInt32(v, checked)
    v = trunc(v)
    if v >= 0 and v <= 4294967295 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return v & 0xffffffff
  end
  function System.toInt32(v, checked)
    return toInt(v, -2147483648, 2147483647, 0xffffffff, 0x7fffffff, checked)
  end
  function System.toInt64(v, checked)
    return toInt(v, (-9223372036854775807 - 1), 9223372036854775807, 0xffffffffffffffff, 0x7fffffffffffffff, checked)
  end
  function System.toUInt64(v, checked)
    if v >= 0 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    return (v & 0x7fffffffffffffff) + 0x8000000000000000
  end
  function System.ToUInt64(v, checked)
    v = trunc(v)
    if v >= 0 and v <= 18446744073709551615 then
      return v
    end
    if checked then
      throw(System.OverflowException(), 1)
    end
    v = v & 0xffffffffffffffff
    local uv = v & 0x7fffffffffffffff
    if uv ~= v then
      return uv + 0x8000000000000000
    end
    return v
  end
  ]]()
end
local toUInt = System.toUInt
local toInt = System.toInt
local ToUInt = System.ToUInt
local ToInt = System.ToInt
function System.toByte(v, checked)
  return toUInt(v, 255, 0xff, checked)
end
function System.toSByte(v, checked)
  return toInt(v, -128, 127, 0xff, 0x7f, checked)
end
function System.toInt16(v, checked)
  return toInt(v, -32768, 32767, 0xffff, 0x7fff, checked)
end
function System.toUInt16(v, checked)
  return toUInt(v, 65535, 0xffff, checked)
end
function System.ToByte(v, checked)
  return ToUInt(v, 255, 0xff, checked)
end
function System.ToSByte(v, checked)
  return ToInt(v, -128, 127, 0xff, 0x7f, checked)
end
function System.ToInt16(v, checked)
  return ToInt(v, -32768, 32767, 0xffff, 0x7fff, checked)
end
function System.ToUInt16(v, checked)
  return ToUInt(v, 65535, 0xffff, checked)
end
function System.ToInt32(v, checked)
  v = trunc(v)
  if v >= -2147483648 and v <= 2147483647 then
    return v
  end
  if checked then
    throw(System.OverflowException(), 1)
  end
  return -2147483648
end
function System.ToInt64(v, checked)
  v = trunc(v)
  if v >= (-9223372036854775807 - 1) and v <= 9223372036854775807 then
    return v
  end
  if checked then
    throw(System.OverflowException(), 1)
  end
  return (-9223372036854775807 - 1)
end
function System.ToSingle(v, checked)
  if v >= -3.40282347E+38 and v <= 3.40282347E+38 then
    return v
  end
  if checked then
    throw(System.OverflowException(), 1)
  end
  if v > 0 then
    return 1 / 0
  else
    return -1 / 0
  end
end
function System.using(t, f)
  local dispose = t and t.Dispose
  if dispose ~= nil then
    local ok, status, ret = xpcall(f, xpcallErr, t)
    dispose(t)
    if not ok then
      error(status)
    end
    return status, ret
  else
    return f(t)
  end
end
function System.usingX(f, ...)
  local ok, status, ret = xpcall(f, xpcallErr, ...)
  for i = 1, select("#", ...) do
    local t = select(i, ...)
    if t ~= nil then
      local dispose = t.Dispose
      if dispose ~= nil then
        dispose(t)
      end
    end
  end
  if not ok then
    error(status)
  end
  return status, ret
end
function System.apply(t, f)
  f(t)
  return t
end
function System.default(T)
  return T:default()
end
function System.property(name, onlyget)
  local function g(this)
    return this[name]
  end
  if onlyget then
    return g
  end
  local function s(this, v)
    this[name] = v
  end
  return g, s
end
function System.new(cls, index, ...)
  local this = setmetatable({}, cls)
  return this, cls.__ctor__[index](this, ...)
end
function System.base(this)
  return getmetatable(getmetatable(this))
end
local equalsObj, compareObj, toString
if debugsetmetatable then
  equalsObj = function (x, y)
    if x == y then
      return true
    end
    if x == nil or y == nil then
      return false
    end
    local ix = x.EqualsObj
    if ix ~= nil then
      return ix(x, y)
    end
    local iy = y.EqualsObj
    if iy ~= nil then
      return iy(y, x)
    end
    return false
  end
  compareObj = function (a, b)
    if a == b then return 0 end
    if a == nil then return -1 end
    if b == nil then return 1 end
    local ia = a.CompareToObj
    if ia ~= nil then
      return ia(a, b)
    end
    local ib = b.CompareToObj
    if ib ~= nil then
      return -ib(b, a)
    end
    throw(System.ArgumentException("Argument_ImplementIComparable"))
  end
  toString = function (t, f, a)
    local s = t ~= nil and t:ToString(f) or ""
    if a then
      return ("%" .. a .. "s"):format(s)
    end
    return s
  end
  debugsetmetatable(nil, {
    __concat = function(a, b)
      if a == nil then
        if b == nil then
          return ""
        else
          return b
        end
      else
        return a
      end
    end,
    __add = function (a, b)
      if a == nil then
        if b == nil or type(b) == "number" then
          return nil
        end
        return b
      end
      return nil
    end,
    __sub = nilFn,
    __mul = nilFn,
    __div = nilFn,
    __mod = nilFn,
    __unm = nilFn,
    __lt = falseFn,
    __le = falseFn,
    __idiv = nilFn,
    __band = nilFn,
    __bor = nilFn,
    __bxor = nilFn,
    __bnot = nilFn,
    __shl = nilFn,
    __shr = nilFn,
  })
else
  equalsObj = function (x, y)
    if x == y then
      return true
    end
    if x == nil or y == nil then
      return false
    end
    local t = type(x)
    if t == "table" then
      local ix = x.EqualsObj
      if ix ~= nil then
        return ix(x, y)
      end
    elseif t == "number" then
      return System.Number.EqualsObj(x, y)
    end
    t = type(y)
    if t == "table" then
      local iy = y.EqualsObj
      if iy ~= nil then
        return iy(y, x)
      end
    end
    return false
  end
  compareObj = function (a, b)
    if a == b then return 0 end
    if a == nil then return -1 end
    if b == nil then return 1 end
    local t = type(a)
    if t == "number" then
      return System.Number.CompareToObj(a, b)
    elseif t == "boolean" then
      return System.Boolean.CompareToObj(a, b)
    else
      local ia = a.CompareToObj
      if ia ~= nil then
        return ia(a, b)
      end
    end
    t = type(b)
    if t == "number" then
      return -System.Number.CompareToObj(b, a)
    elseif t == "boolean" then
      return -System.Boolean.CompareToObj(a, b)
    else
      local ib = b.CompareToObj
      if ib ~= nil then
        return -ib(b, a)
      end
    end
    throw(System.ArgumentException("Argument_ImplementIComparable"))
  end
  toString = function (obj, f, a)
    local s
    if obj ~= nil then
      local t = type(obj)
      if t == "number" then
        if f then
          s = System.Number.ToString(obj, f)
        else
          s = tostring(obj)
        end
      elseif t == "table" then
        s = obj:ToString(f)
      elseif t == "boolean" then
        s = obj and "True" or "False"
      elseif t == "function" then
        s = "System.Delegate"
      else
        s = tostring(obj)
      end
    else
      s = ""
    end
    if a then
      return ("%" .. a .. "s"):format(s)
    end
    return s
  end
end
local addr
if version <= 5.1 then
  addr = function (t, i)
    return ssub(tostring(t), i or 8) + 0
  end
else
  addr = function (t, i)
    return tonumber("0x" .. ssub(tostring(t), i or 8))
  end
end
local function hash(v)
  if v == nil then return 0 end
  local t = type(v)
  if t == "number" then
    if v % 1 == 0 and v >= -2147483648 and v <= 2147483647 then
      return v
    end
    local s = tostring(v)
    return s:GetHashCode()
 elseif t == "string" then
    local c = 0
    for i = 1, #v do
      local b = v:byte(i)
      c = 31 * c + b
      c = System.toInt32(c)
    end
    return c
  elseif t == "boolean" then
    return v and 1 or 0
  elseif t == "function" then
    return addr(v, 11)
  end
  return addr(v)
end
local function hashObj(obj)
  if obj == nil then return 0 end
  local t = type(obj)
  if t == "table" then
    return obj:GetHashCode()
  end
  return hash(obj)
end
System.hasHash = function (t)
  return t.GetHashCode ~= hash
end
System.equalsObj = equalsObj
System.compareObj = compareObj
System.hash = hash
System.hashObj = hashObj
System.toString = toString
Object = defCls("System.Object", {
  __call = new,
  __ctor__ = emptyFn,
  default = nilFn,
  class = "C",
  EqualsObj = equals,
  ReferenceEquals = rawequal,
  GetHashCode = hash,
  EqualsStatic = equalsObj,
  GetType = false,
  ToString = function(this) return this.__name__ end
})
setmetatable(Object, { __call = new })
ValueType = defCls("System.ValueType", {
  class = "S",
  default = function(T)
    return T()
  end,
  __clone__ = function(this)
    if type(this) == "table" then
      local cls = getmetatable(this)
      local t = {}
      for k, v in pairs(this) do
        if type(v) == "table" and v.class == "S" then
          t[k] = v:__clone__()
        else
          t[k] = v
        end
      end
      return setmetatable(t, cls)
    end
    return this
  end,
  __copy__ = function (this, obj)
    for k, v in pairs(obj) do
      if type(v) == "table" and v.class == "S" then
        this[k] = v:__clone__()
      else
        this[k] = v
      end
    end
    for k, v in pairs(this) do
      if v ~= nil and rawget(obj, k) == nil then
        this[k] = nil
      end
    end
  end,
  EqualsObj = function (this, obj)
    if this == obj then return true end
    if getmetatable(this) ~= getmetatable(obj) then return false end
    for k, v in pairs(this) do
      if not equalsObj(v, obj[k]) then
        return false
      end
    end
    return true
  end,
  GetHashCode = function (this)
    local c = 17
    for _, v in pairs(this) do
      c = c * 31 + hash(v)
      c = System.toInt32(c)
    end
    return c
  end
})
local AnonymousType
AnonymousType = defCls("System.AnonymousType", {
  EqualsObj = function (this, obj)
    if getmetatable(obj) ~= AnonymousType then return false end
    for k, v in pairs(this) do
      if not equalsObj(v, obj[k]) then
        return false
      end
    end
    return true
  end
})
local function anonymousTypeCreate(T, t)
  return setmetatable(t, T)
end
local anonymousTypeMetaTable = setmetatable({ __index = Object, __call = anonymousTypeCreate }, Object)
setmetatable(AnonymousType, anonymousTypeMetaTable)
local pack, unpack = table.pack, table.unpack
local function tupleDeconstruct(t)
  return unpack(t, 1, t.n)
end
local function tupleEquals(t, other)
  for i = 1, t.n do
    if not equalsObj(t[i], other[i]) then
      return false
    end
  end
  return true
end
local function tupleEqualsObj(t, obj)
  if getmetatable(obj) ~= getmetatable(t) or t.n ~= obj.n then
    return false
  end
  return tupleEquals(t, obj)
end
local function tupleCompareTo(t, other)
  for i = 1, t.n do
    local v = compareObj(t[i], other[i])
    if v ~= 0 then
      return v
    end
  end
  return 0
end
local function tupleCompareToObj(t, obj)
  if obj == nil then return 1 end
  if getmetatable(obj) ~= getmetatable(t) or t.n ~= obj.n then
    throw(System.ArgumentException())
  end
  return tupleCompareTo(t, obj)
end
local function tupleToString(t)
  local a = { "(" }
  local count = 2
  for i = 1, t.n do
    if i ~= 1 then
      a[count] = ", "
      count = count + 1
    end
    local v = t[i]
    if v ~= nil then
      a[count] = v:ToString()
      count = count + 1
    end
  end
  a[count] = ")"
  return tconcat(a)
end
local function tupleLength(t)
  return t.n
end
local function tupleGet(t, index)
  if index < 0 or index >= t.n then
    throw(System.IndexOutOfRangeException())
  end
  return t[index + 1]
end
local function tupleGetRest(t)
  return t[8]
end
local function tupleCreate(T, ...)
  return setmetatable(pack(...), T)
end
local Tuple = defCls("System.Tuple", {
  Deconstruct = tupleDeconstruct,
  ToString = tupleToString,
  EqualsObj = tupleEqualsObj,
  CompareToObj = tupleCompareToObj,
  getLength = tupleLength,
  get = tupleGet,
  getRest = tupleGetRest
})
local tupleMetaTable = setmetatable({ __index  = Object, __call = tupleCreate }, Object)
setmetatable(Tuple, tupleMetaTable)
local ValueTuple = {
  Deconstruct = tupleDeconstruct,
  ToString = tupleToString,
  Equals = tupleEquals,
  EqualsObj = tupleEqualsObj,
  CompareTo = tupleCompareTo,
  CompareToObj = tupleCompareToObj,
  getLength = tupleLength,
  get = tupleGet,
  default = function (T)
    local genericT = T.__genericT__
    local t, n = {}, #genericT
    for i = 1, n do
      t[i] = genericT[i]:default()
    end
    t.n = n
    return setmetatable(t, T)
  end
}
local ValueTupleFn = defStc("System.ValueTuple", function (...)
  return {
    __eq = tupleEquals,
    __genericT__ = { ... },
  }
end, ValueTuple, '')
ValueTuple.__call = tupleCreate
System.ValueTuple = ValueTupleFn
local function recordEquals(t, other)
  if t == other then return true end
  if getmetatable(t) == getmetatable(other) then
    for k, v in pairs(t) do
      if not equalsObj(v, other[k]) then
        return false
      end
    end
    return true
  end
  return false
end
local function recordNotEquals(t, other)
  return not recordEquals(t, other)
end
local function recordPrintMembers(this, builder)
  local p = pack(this.__members__())
  local n = p.n
  for i = 2, n do
    local k = p[i]
    local v = this[k]
    builder:Append(k)
    builder:Append(" = ")
    if v ~= nil then
    builder:Append(toString(v))
    end
    if i ~= n then
    builder:Append(", ")
    end
  end
  return true
end
local function recordToString(this)
  local p = pack(this.__members__())
  local n = p.n
  local t = { p[1], "{" }
  local count = 3
  for i = 2, n do
    local k = p[i]
    local v = this[k]
    t[count] = k
    t[count + 1] = "="
    if v ~= nil then
      if i ~= n then
        t[count + 2] = toString(v) .. ','
      else
        t[count + 2] = toString(v)
      end
    else
      if i ~= n then
        t[count + 2] = ','
      end
    end
    if v == nil and i == n then
      count = count + 2
    else
      count = count + 3
    end
  end
  t[count] = "}"
  return tconcat(t, ' ')
end
local function recordDeconstruct(this)
  local t = pack(this.__members__())
  for i = 2, t.n do
    t[i] = this[t[i]]
  end
  return unpack(t, 2)
end
defCls("System.RecordType", {
  __eq = recordEquals,
  __clone__ = ValueType.__clone__,
  op_Equality = recordEquals,
  op_Inequality = recordNotEquals,
  GetHashCode = ValueType.GetHashCode,
  Equals = recordEquals,
  PrintMembers = recordPrintMembers,
  ToString = recordToString,
  Deconstruct = recordDeconstruct
})
defStc("System.RecordValueType", {
  __eq = recordEquals,
  op_Equality = recordEquals,
  op_Inequality = recordNotEquals,
  Equals = recordEquals,
  PrintMembers = recordPrintMembers,
  ToString = recordToString,
  Deconstruct = recordDeconstruct
})
local Attribute = defCls("System.Attribute")
defCls("System.FlagsAttribute", { base = { Attribute } })
local Nullable = {
  default = nilFn,
  Value = function (this)
    if this == nil then
      throw(System.InvalidOperationException("Nullable object must have a value."))
    end
    return this
  end,
  EqualsObj = equalsObj,
  GetHashCode = function (this)
    if this == nil then
      return 0
    end
    if type(this) == "table" then
      return this:GetHashCode()
    end
    return hash(this)
  end,
  clone = function (t)
    if type(t) == "table" then
      return t:__clone__()
    end
    return t
  end
}
defStc("System.Nullable", function (T)
  return {
    __genericT__ = T
  }
end, Nullable, 1)
function System.isNullable(T)
  return getmetatable(T) == Nullable
end
local Index = defStc("System.Index", {
  End = -0.0,
  Start = 0,
  IsFromEnd = function (this)
    return 1 / this < 0
  end,
  GetOffset = function (this, length)
    if 1 / this < 0 then
      return length + this
    end
    return this
  end,
  ToString = function (this)
    return ((1 / this < 0) and '^' or '') .. this
  end
})
setmetatable(Index, {
  __call = function (value, fromEnd)
    if value < 0 then
      throw(System.ArgumentOutOfRangeException("Non-negative number required."))
    end
    if fromEnd then
      if value == 0 then
        return -0.0
      end
      return -value
    end
    return value
  end
})
local function pointerAddress(p)
  local address = p[3]
  if address == nil then
    address = addr(p)
    p[3] = address
  end
  return address + p[2]
end
local Pointer
local function newPointer(t, i)
  return setmetatable({ t, i }, Pointer)
end
Pointer = {
  __index = false,
  get = function(this)
    local t, i = this[1], this[2]
    return t[i]
  end,
  set = function(this, value)
    local t, i = this[1], this[2]
    t[i] = value
  end,
  __add = function(this, count)
    return newPointer(this[1], this[2] + count)
  end,
  __sub = function(this, count)
    return newPointer(this[1], this[2] - count)
  end,
  __lt = function(t1, t2)
    return pointerAddress(t1) < pointerAddress(t2)
  end,
  __le = function(t1, t2)
    return pointerAddress(t1) <= pointerAddress(t2)
  end
}
Pointer.__index = Pointer
function System.stackalloc(t)
  return newPointer(t, 1)
end
local modules, imports = {}, {}
function System.getRegisteredModuleNames()
  local t = {}
  for k in pairs(modules) do t[#t + 1] = k end
  return t
end
function System.import(f)
  imports[#imports + 1] = f
end
local namespace
local function defIn(kind, name, f)
  local namespaceName, isClass = namespace[1], namespace[2]
  if #namespaceName > 0 then
    name = namespaceName .. (isClass and "+" or ".") .. name
  end
  assert(modules[name] == nil, name)
  namespace[1], namespace[2] = name, kind == "C" or kind == "S"
  local t = f(assembly, global)
  namespace[1], namespace[2] = namespaceName, isClass
  modules[isClass and name:gsub("+", ".") or name] = function()
    return def(name, kind, t)
  end
end
namespace = {
  "",
  false,
  __index = false,
  class = function(name, f) defIn("C", name, f) end,
  struct = function(name, f) defIn("S", name, f) end,
  interface = function(name, f) defIn("I", name, f) end,
  enum = function(name, f) defIn("E", name, f) end,
  namespace = function(name, f)
    local namespaceName = namespace[1]
    name = namespaceName .. "." .. name
    namespace[1] = name
    f(namespace)
    namespace[1] = namespaceName
  end
}
namespace.__index = namespace
function System.namespace(name, f)
  if not assembly then assembly = setmetatable({}, namespace) end
  namespace[1] = name
  f(namespace)
  namespace[1], namespace[2] = "", false
end
function System.init(t)
  local path, files = t.path, t.files
  if files then
    path = (path and #path > 0) and (path .. '.') or ""
    for i = 1, #files do
      require(path .. files[i])
    end
  end
  metadatas = {}
  local types = t.types
  if types then
    local classes = {}
    for i = 1, #types do
      local name = types[i]
      local cls = assert(modules[name], name)()
      classes[i] = cls
    end
    assembly.classes = classes
  end
  for i = 1, #imports do
    imports[i](global)
  end
  local b, e = 1, #metadatas
  while true do
    for i = b, e do
      metadatas[i](global)
    end
    local len = #metadatas
    if len == e then
      break
    end
    b, e = e + 1, len
  end
  local main = t.Main
  if main then
    assembly.entryPoint = main
    System.entryAssembly = assembly
  end
  local attributes = t.assembly
  if attributes then
    if type(attributes) == "function" then
      attributes = attributes(global)
    end
    for k, v in pairs(attributes) do
      assembly[k] = v
    end
  end
  local current = assembly
  modules, imports, assembly, metadatas = {}, {}, nil, nil
  return current
end
System.config = rawget(global, "CSharpLuaSystemConfig") or {}
local isSingleFile = rawget(global, "CSharpLuaSingleFile")
if not isSingleFile then
  return function (config)
    if config then
      System.config = config
    end
  end
end

end

-- CoreSystemLib: Interfaces.lua
do
local System = System
local defInf = System.defInf
local emptyFn = System.emptyFn
local IComparable = defInf("System.IComparable")
local IFormattable = defInf("System.IFormattable")
local IConvertible = defInf("System.IConvertible")
defInf("System.IFormatProvider")
defInf("System.ICloneable")
defInf("System.IComparable_1", emptyFn)
defInf("System.IEquatable_1", emptyFn)
defInf("System.IPromise")
defInf("System.IDisposable")
defInf("System.IStructuralComparable")
defInf("System.IStructuralEquatable")
local IEnumerable = defInf("System.IEnumerable")
local IEnumerator = defInf("System.IEnumerator")
local ICollection = defInf("System.ICollection", {
  base = { IEnumerable }
})
defInf("System.IList", {
  base = { ICollection }
})
defInf("System.IDictionary", {
  base = { ICollection }
})
local IEnumerator_1 = defInf("System.Collections.Generic.IEnumerator_1", function(T)
  return {
    base = { IEnumerator }
  }
end)
System.IEnumerator_1 = IEnumerator_1
local IEnumerable_1 = defInf("System.Collections.Generic.IEnumerable_1", function(T)
  return {
    base = { IEnumerable }
  }
end)
System.IEnumerable_1 = IEnumerable_1
local ICollection_1 = defInf("System.ICollection_1", function(T)
  return {
    base = { IEnumerable_1(T) }
  }
end)
local IReadOnlyCollection_1 = defInf("System.IReadOnlyCollection_1", function (T)
  return {
    base = { IEnumerable_1(T) }
  }
end)
defInf("System.IReadOnlyList_1", function (T)
  return {
    base = { IReadOnlyCollection_1(T) }
  }
end)
defInf('System.IDictionary_2', function(TKey, TValue)
  return {
    base = { ICollection_1(System.KeyValuePair(TKey, TValue)) }
  }
end)
defInf("System.IReadOnlyDictionary_2", function(TKey, TValue)
  return {
    base = { IReadOnlyCollection_1(System.KeyValuePair(TKey, TValue)) }
  }
end)
defInf("System.IList_1", function(T)
  return {
    base = { ICollection_1(T) }
  }
end)
defInf("System.ISet_1", function(T)
  return {
    base = { ICollection_1(T) }
  }
end)
defInf("System.IReadOnlySet_1", function(T)
  return {
    base = { IReadOnlyCollection_1(T) }
  }
end)
defInf("System.IComparer")
defInf("System.IComparer_1", emptyFn)
defInf("System.IEqualityComparer")
defInf("System.IEqualityComparer_1", emptyFn)
System.enumMetatable.interface = { IComparable, IFormattable, IConvertible }

end

-- CoreSystemLib: Exception.lua
do
local System = _G.System
local define = System.define
local Object = System.Object
local toStr = System.toString
local tconcat = table.concat
local type = type
local debug = debug
local assert = assert
local select = select
local traceback = (debug and debug.traceback) or System.config.traceback or function () return "" end
System.traceback = traceback
local resource = {
  Arg_KeyNotFound = "The given key was not present in the dictionary.",
  Arg_KeyNotFoundWithKey = "The given key '%s' was not present in the dictionary.",
  Arg_WrongType = "The value '%s' is not of type '%s' and cannot be used in this generic collection.",
  Arg_ParamName_Name = "(Parameter '%s')",
  Argument_AddingDuplicate = "An item with the same key has already been added. Key: %s",
  ArgumentOutOfRange_SmallCapacity = "capacity was less than the current size.",
  InvalidOperation_EmptyQueue = "Queue empty.",
  ArgumentOutOfRange_NeedNonNegNum = "Non-negative number required.",
}
local function getResource(t, k)
  local s = resource[k]
  assert(s, k)
  return function (...)
	local n = select("#", ...)
    local f
    if n == 0 then
      f = function () return s end
    elseif n == 1 then
      f = function (x1) return s:format(toStr(x1)) end
    elseif n == 2 then
      f = function (x1, x2) return s:format(toStr(x1), toStr(x2)) end
    elseif n == 3 then
      f = function (x1, x2, x3) return s:format(toStr(x1), toStr(x2), toStr(x3)) end
    else
      assert(false)
    end
    t[k] = f
    return f(...)
  end
end
System.er = setmetatable({}, { __index = getResource })
local function getMessage(this)
  return this.message or ("Exception of type '%s' was thrown."):format(this.__name__)
end
local function toString(this)
  local t = { this.__name__ }
  local count = 2
  local message, innerException, stackTrace = getMessage(this), this.innerException, this.errorStack
  t[count] = ": "
  t[count + 1] = message
  count = count + 2
  if innerException then
    t[count] = "---> "
    t[count + 1] = innerException:ToString()
    count = count + 2
  end
  if stackTrace then
    t[count] = stackTrace
  end
  return tconcat(t)
end
local function ctorOfException(this, message, innerException)
  this.message = message
  this.innerException = innerException
end
local Exception = define("System.Exception", {
  __tostring = toString,
  __ctor__ = ctorOfException,
  ToString = toString,
  getMessage = getMessage,
  getInnerException = function(this)
    return this.innerException
  end,
  getStackTrace = function(this)
    return this.errorStack
  end,
  getData = function (this)
    local data = this.data
    if not data then
      data = System.Dictionary(Object, Object)()
      this.data = data
    end
    return data
  end,
  traceback = function(this, lv)
    this.errorStack = traceback("", lv and lv + 3 or 3)
  end
})
local SystemException = define("System.SystemException", {
  __tostring = toString,
  base = { Exception },
  __ctor__ = function (this, message, innerException)
    ctorOfException(this, message or "System error.", innerException)
  end
})
local ArgumentException = define("System.ArgumentException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, paramName, innerException)
    if type(paramName) == "table" then
      paramName, innerException = nil, paramName
    end
    ctorOfException(this, message or "Value does not fall within the expected range.", innerException)
    this.paramName = paramName
    if paramName and #paramName > 0 then
      this.message = this.message .. " " .. resource.Arg_ParamName_Name:format(paramName)
    end
  end,
  getParamName = function(this)
    return this.paramName
  end
})
define("System.ArgumentNullException", {
  __tostring = toString,
  base = { ArgumentException },
  __ctor__ = function(this, paramName, message, innerException)
    ArgumentException.__ctor__(this, message or "Value cannot be null.", paramName, innerException)
  end
})
define("System.ArgumentOutOfRangeException", {
  __tostring = toString,
  base = { ArgumentException },
  __ctor__ = function(this, paramName, message, innerException, actualValue)
    ArgumentException.__ctor__(this, message or "Specified argument was out of the range of valid values.", paramName, innerException)
    this.actualValue = actualValue
  end,
  getActualValue = function(this)
    return this.actualValue
  end
})
define("System.IndexOutOfRangeException", {
   __tostring = toString,
   base = { SystemException },
   __ctor__ = function (this, message, innerException)
    ctorOfException(this, message or "Index was outside the bounds of the array.", innerException)
  end
})
define("System.CultureNotFoundException", {
  __tostring = toString,
  base = { ArgumentException },
  __ctor__ = function(this, paramName, invalidCultureName, message, innerException, invalidCultureId)
    if not message then
      message = "Culture is not supported."
      if paramName then
        message = message .. "\nParameter name = " .. paramName
      end
      if invalidCultureName then
        message = message .. "\n" .. invalidCultureName .. " is an invalid culture identifier."
      end
    end
    ArgumentException.__ctor__(this, message, paramName, innerException)
    this.invalidCultureName = invalidCultureName
    this.invalidCultureId = invalidCultureId
  end,
  getInvalidCultureName = function(this)
    return this.invalidCultureName
  end,
  getInvalidCultureId = function(this)
    return this.invalidCultureId
  end
})
local KeyNotFoundException = define("System.Collections.Generic.KeyNotFoundException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or resource.Arg_KeyNotFound, innerException)
  end
})
System.KeyNotFoundException = KeyNotFoundException
local ArithmeticException = define("System.ArithmeticException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Overflow or underflow in the arithmetic operation.", innerException)
  end
})
define("System.DivideByZeroException", {
  __tostring = toString,
  base = { ArithmeticException },
  __ctor__ = function(this, message, innerException)
    ArithmeticException.__ctor__(this, message or "Attempted to divide by zero.", innerException)
  end
})
define("System.OverflowException", {
  __tostring = toString,
  base = { ArithmeticException },
  __ctor__ = function(this, message, innerException)
    ArithmeticException.__ctor__(this, message or "Arithmetic operation resulted in an overflow.", innerException)
  end
})
define("System.FormatException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Invalid format.", innerException)
  end
})
define("System.InvalidCastException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Specified cast is not valid.", innerException)
  end
})
local InvalidOperationException = define("System.InvalidOperationException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Operation is not valid due to the current state of the object.", innerException)
  end
})
define("System.NotImplementedException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "The method or operation is not implemented.", innerException)
  end
})
define("System.NotSupportedException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Specified method is not supported.", innerException)
  end
})
define("System.NullReferenceException", {
  __tostring = toString,
  base = { SystemException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Object reference not set to an instance of an object.", innerException)
  end
})
define("System.RankException", {
  __tostring = toString,
  base = { Exception },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Attempted to operate on an array with the incorrect number of dimensions.", innerException)
  end
})
define("System.TypeLoadException", {
  __tostring = toString,
  base = { Exception },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Failed when load type.", innerException)
  end
})
define("System.ObjectDisposedException", {
  __tostring = toString,
  base = { InvalidOperationException },
  __ctor__ = function(this, objectName, message, innerException)
    ctorOfException(this, message or "Cannot access a disposed object.", innerException)
    this.objectName = objectName
    if objectName and #objectName > 0 then
      this.message = this.message .. "\nObject name: '" .. objectName .. "'."
    end
  end
})
local function toStringOfAggregateException(this)
  local t = { toString(this) }
  local count = 2
  for i = 0, this.innerExceptions:getCount() - 1 do
    t[count] = "\n---> (Inner Exception #"
    t[count + 1] = i
    t[count + 2] = ") "
    t[count + 3] = this.innerExceptions:get(i):ToString()
    t[count + 4] = "<---\n"
    count = count + 5
  end
  return tconcat(t)
end
define("System.AggregateException", {
  ToString = toStringOfAggregateException,
  __tostring = toStringOfAggregateException,
  base = { Exception },
  __ctor__ = function (this, message, innerExceptions)
    if type(message) == "table" then
      message, innerExceptions = nil, message
    end
    Exception.__ctor__(this, message or "One or more errors occurred.")
    local ReadOnlyCollection = System.ReadOnlyCollection(Exception)
    if innerExceptions then
      if System.is(innerExceptions, Exception) then
        local list = System.List(Exception)()
        list:Add(innerExceptions)
        this.innerExceptions = ReadOnlyCollection(list)
      else
        if not System.isArrayLike(innerExceptions) then
          innerExceptions = System.Array.toArray(innerExceptions)
        end
        this.innerExceptions = ReadOnlyCollection(innerExceptions)
      end
    else
      this.innerExceptions = ReadOnlyCollection(System.Array.Empty(Exception))
    end
  end,
  getInnerExceptions = function (this)
    return this.innerExceptions
  end
})
System.SwitchExpressionException = define("System.Runtime.CompilerServices", {
  __tostring = toString,
  base = { InvalidOperationException },
  __ctor__ = function(this, message, innerException)
    ctorOfException(this, message or "Non-exhaustive switch expression failed to match its input.", innerException)
  end
})

end

-- CoreSystemLib: Math.lua
do
local System = System
local trunc = System.trunc
local math = math
local floor = math.floor
local ceil = math.ceil
local min = math.min
local max = math.max
local abs = math.abs
local log = math.log
local sqrt = math.sqrt
local ln2 = log(2)
local function acosh(a)
  return log(a + sqrt(a ^ 2 - 1))
end
local function asinh(a)
  return log(a + sqrt(a ^ 2 + 1))
end
local function atanh(a)
  return 0.5 * log((1 + a) / (1 - a))
end
local function cbrt(a)
  if a >= 0 then
    return a ^ (1 / 3)
  else
    return -abs(a) ^ (1 / 3)
  end
end
local function copySign(a, b)
  if b >= 0 then
    return a >= 0 and a or -a
  else
    return a >= 0 and -a or a
  end
end
local function fusedMultiplyAdd(a, b, c)
  return a * b + c
end
local function ilogB(a)
  return a == 0 and -2147483648 or floor(log(abs(a)) / ln2)
end
local function log2(a)
  return log(a) / ln2
end
local function maxMagnitude(a, b)
  local x = abs(a)
  local y = abs(b)
  if x > y then
    return a
  elseif x < y then
    return b
  else
    return a > b and a or b
  end
end
local function minMagnitude(a, b)
  local x = abs(a)
  local y = abs(b)
  if x < y then
    return a
  elseif x > y then
    return b
  else
    return a < b and a or b
  end
end
local function reciprocalEstimate(a)
  return 1 / a
end
local function reciprocalSqrtEstimate(a)
  return sqrt(1 / a)
end
local function scaleB(a, b)
  return a * 2 ^ b
end
local function sinCos(a)
  local Double = System.Double
  return System.ValueTuple(Double, Double)(math.sin(a), math.cos(a))
end
local function bigMul(a, b)
  return a * b
end
local function divRem(a, b)
  local remainder = a % b
  return (a - remainder) / b, remainder
end
local function round(value, digits, mode)
  local mult = 10 ^ (digits or 0)
  local i = value * mult
  if mode == 1 then
    value = trunc(i + (value >= 0 and 0.5 or -0.5))
  elseif mode == 2 then
    value = i >= 0 and floor(i) or ceil(i)
  elseif mode == 3 then
    value = floor(i)
  elseif mode == 4 then
    value = ceil(i)
  else
    value = trunc(i)
    if value ~= i then
      local dif = i - value
      if i >= 0 then
        if dif > 0.5 or (dif == 0.5 and value % 2 ~= 0) then
          value = value + 1
        end
      else
        if dif < -0.5 or (dif == -0.5 and value % 2 ~= 0) then
          value = value - 1
        end
      end
    end
  end
  return value / mult
end
local function sign(v)
  return v == 0 and 0 or (v > 0 and 1 or -1)
end
local function IEEERemainder(x, y)
  if x ~= x then
    return x
  end
  if y ~= y then
    return y
  end
  local regularMod = System.mod(x, y)
  if regularMod ~= regularMod then
    return regularMod
  end
  if regularMod == 0 and x < 0 then
    return -0.0
  end
  local alternativeResult = regularMod - abs(y) * sign(x)
  local i, j = abs(alternativeResult), abs(regularMod)
  if i == j then
    local divisionResult = x / y
    local roundedResult = round(divisionResult)
    if abs(roundedResult) > abs(divisionResult) then
      return alternativeResult
    else
      return regularMod
    end
  end
  if i < j then
    return alternativeResult
  else
    return regularMod
  end
end
local function clamp(a, b, c)
  return min(max(a, b), c)
end
local function truncate(d)
  return trunc(d) * 1.0
end
local log10 = math.log10
if not log10 then
  log10 = function (x) return log(x, 10) end
  math.log10 = log10
end
local exp = math.exp
local cosh = math.cosh or function (x) return (exp(x) + exp(-x)) / 2.0 end
local pow = math.pow or function (x, y) return x ^ y end
local sinh = math.sinh or function (x) return (exp(x) - exp(-x)) / 2.0 end
local tanh = math.tanh or function (x) return sinh(x) / cosh(x) end
local Math = math
Math.Abs = abs
Math.Acos = math.acos
Math.Acosh = acosh
Math.Asin = math.asin
Math.Asinh = asinh
Math.Atan = math.atan
Math.Atanh = atanh
Math.Atan2 = math.atan2 or math.atan
Math.BigMul = bigMul
Math.Cbrt = cbrt
Math.Ceiling = ceil
Math.Clamp = clamp
Math.CopySign = copySign
Math.Cos = math.cos
Math.Cosh = cosh
Math.DivRem = divRem
Math.Exp = exp
Math.Floor = floor
Math.FusedMultiplyAdd = fusedMultiplyAdd
Math.IEEERemainder = IEEERemainder
Math.ILogB = ilogB
Math.Log = log
Math.Log10 = log10
Math.Log2 = log2
Math.Max = max
Math.MaxMagnitude = maxMagnitude
Math.Min = min
Math.MinMagnitude = minMagnitude
Math.Pow = pow
Math.ReciprocalEstimate = reciprocalEstimate
Math.ReciprocalSqrtEstimate = reciprocalSqrtEstimate
Math.Round = round
Math.ScaleB = scaleB
Math.Sign = sign
Math.Sin = math.sin
Math.SinCos = sinCos
Math.Sinh = sinh
Math.Sqrt = sqrt
Math.Tan = math.tan
Math.Tanh = tanh
Math.Truncate = truncate
System.define("System.Math", Math)
System.define("System.MathF", Math)

end

-- CoreSystemLib: Number.lua
do
local System = System
local throw = System.throw
local define = System.defStc
local equals = System.equals
local zeroFn = System.zeroFn
local hash = System.hash
local debugsetmetatable = System.debugsetmetatable
local IComparable = System.IComparable
local IComparable_1 = System.IComparable_1
local IEquatable_1 = System.IEquatable_1
local IConvertible = System.IConvertible
local IFormattable = System.IFormattable
local ArgumentException = System.ArgumentException
local ArgumentNullException = System.ArgumentNullException
local FormatException = System.FormatException
local OverflowException = System.OverflowException
local type = type
local tonumber = tonumber
local floor = math.floor
local setmetatable = setmetatable
local tostring = tostring
local function hexForamt(x, n)
  return n == "" and "%" .. x or "%0" .. n .. x
end
local function floatForamt(x, n)
  return n == "" and "%.f" or "%." .. n .. 'f'
end
local function integerFormat(x, n)
  return n == "" and "%d" or "%0" .. n .. 'd'
end
local function exponentialFormat(x, n)
  return n == "" and "%" .. x or "%." .. n .. x
end
local formats = {
  ['x'] = hexForamt,
  ['X'] = hexForamt,
  ['f'] = floatForamt,
  ['F'] = floatForamt,
  ['d'] = integerFormat,
  ['D'] = integerFormat,
  ['e'] = exponentialFormat,
  ['E'] = exponentialFormat
}
local function toStringWithFormat(this, format)
  if #format ~= 0 then
    local i, j, x, n = format:find("^%s*([xXdDfFeE])(%d?)%s*$")
    if i then
      local f = formats[x]
      if f then
        format = f(x, n)
      end
      return format:format(this)
    end
  end
  return tostring(this)
end
local function toString(this, format)
  if format then
    return toStringWithFormat(this, format)
  end
  return tostring(this)
end
local function compareInt(this, v)
  if this < v then return -1 end
  if this > v then return 1 end
  return 0
end
local function inherits(_, T)
  return { IComparable, IComparable_1(T), IEquatable_1(T), IConvertible, IFormattable }
end
local Int = define("System.Int", {
  base = inherits,
  default = zeroFn,
  CompareTo = compareInt,
  Equals = equals,
  ToString = toString,
  GetHashCode = hash,
  CompareToObj = function (this, v)
    if v == nil then return 1 end
    if type(v) ~= "number" then
      throw(ArgumentException("Arg_MustBeInt"))
    end
    return compareInt(this, v)
  end,
  EqualsObj = function (this, v)
    if type(v) ~= "number" then
      return false
    end
    return this == v
  end
})
Int.__call = zeroFn
local function parseInt(s, min, max)
  if s == nil then
    return nil, 1
  end
  local v = tonumber(s)
  if v == nil or v ~= floor(v) then
    return nil, 2
  end
  if v < min or v > max then
    return nil, 3
  end
  return v
end
local function tryParseInt(s, min, max)
  local v = parseInt(s, min, max)
  if v then
    return true, v
  end
  return false, 0
end
local function parseIntWithException(s, min, max)
  local v, err = parseInt(s, min, max)
  if v then
    return v
  end
  if err == 1 then
    throw(ArgumentNullException())
  elseif err == 2 then
    throw(FormatException())
  else
    throw(OverflowException())
  end
end
local SByte = define("System.SByte", {
  Parse = function (s)
    return parseIntWithException(s, -128, 127)
  end,
  TryParse = function (s)
    return tryParseInt(s, -128, 127)
  end
})
setmetatable(SByte, Int)
local Byte = define("System.Byte", {
  Parse = function (s)
    return parseIntWithException(s, 0, 255)
  end,
  TryParse = function (s)
    return tryParseInt(s, 0, 255)
  end
})
setmetatable(Byte, Int)
local Int16 = define("System.Int16", {
  Parse = function (s)
    return parseIntWithException(s, -32768, 32767)
  end,
  TryParse = function (s)
    return tryParseInt(s, -32768, 32767)
  end
})
setmetatable(Int16, Int)
local UInt16 = define("System.UInt16", {
  Parse = function (s)
    return parseIntWithException(s, 0, 65535)
  end,
  TryParse = function (s)
    return tryParseInt(s, 0, 65535)
  end
})
setmetatable(UInt16, Int)
local Int32 = define("System.Int32", {
  Parse = function (s)
    return parseIntWithException(s, -2147483648, 2147483647)
  end,
  TryParse = function (s)
    return tryParseInt(s, -2147483648, 2147483647)
  end
})
setmetatable(Int32, Int)
local UInt32 = define("System.UInt32", {
  Parse = function (s)
    return parseIntWithException(s, 0, 4294967295)
  end,
  TryParse = function (s)
    return tryParseInt(s, 0, 4294967295)
  end
})
setmetatable(UInt32, Int)
local Int64 = define("System.Int64", {
  Parse = function (s)
    return parseIntWithException(s, (-9223372036854775807 - 1), 9223372036854775807)
  end,
  TryParse = function (s)
    return tryParseInt(s, (-9223372036854775807 - 1), 9223372036854775807)
  end
})
setmetatable(Int64, Int)
local UInt64 = define("System.UInt64", {
  Parse = function (s)
    return parseIntWithException(s, 0, 18446744073709551615.0)
  end,
  TryParse = function (s)
    return tryParseInt(s, 0, 18446744073709551615.0)
  end
})
setmetatable(UInt64, Int)
local nan = 0 / 0
local posInf = 1 / 0
local negInf = - 1 / 0
local function isNaN(v)
  return v ~= v
end
local function compareDouble(this, v)
  if this < v then return -1 end
  if this > v then return 1 end
  if this == v then return 0 end
  if isNaN(this) then
    return isNaN(v) and 0 or -1
  else
    return 1
  end
end
local function equalsDouble(this, v)
  if this == v then return true end
  return isNaN(this) and isNaN(v)
end
local function equalsObj(this, v)
  if type(v) ~= "number" then
    return false
  end
  return equalsDouble(this, v)
end
local Number = define("System.Number", {
  base = inherits,
  default = zeroFn,
  CompareTo = compareDouble,
  Equals = equalsDouble,
  ToString = toString,
  NaN = nan,
  IsNaN = isNaN,
  NegativeInfinity = negInf,
  PositiveInfinity = posInf,
  EqualsObj = equalsObj,
  GetHashCode = hash,
  CompareToObj = function (this, v)
    if v == nil then return 1 end
    if type(v) ~= "number" then
      throw(ArgumentException("Arg_MustBeNumber"))
    end
    return compareDouble(this, v)
  end,
  IsFinite = function (v)
    return v ~= posInf and v ~= negInf and not isNaN(v)
  end,
  IsInfinity = function (v)
    return v == posInf or v == negInf
  end,
  IsNegativeInfinity = function (v)
    return v == negInf
  end,
  IsPositiveInfinity = function (v)
    return v == posInf
  end
})
Number.__call = zeroFn
if debugsetmetatable then
  debugsetmetatable(0, Number)
end
local function parseDouble(s)
  if s == nil then
    return nil, 1
  end
  local v = tonumber(s)
  if v == nil then
    return nil, 2
  end
  return v
end
local function parseDoubleWithException(s)
  local v, err = parseDouble(s)
  if v then
    return v
  end
  if err == 1 then
    throw(ArgumentNullException())
  else
    throw(FormatException())
  end
end
local Single = define("System.Single", {
  Parse = function (s)
    local v = parseDoubleWithException(s)
    if v < -3.40282347E+38 or v > 3.40282347E+38 then
      throw(OverflowException())
    end
    return v
  end,
  TryParse = function (s)
    local v = parseDouble(s)
    if v and v >= -3.40282347E+38 and v < 3.40282347E+38 then
      return true, v
    end
    return false, 0
  end
})
setmetatable(Single, Number)
local Double = define("System.Double", {
  Parse = parseDoubleWithException,
  TryParse = function (s)
    local v = parseDouble(s)
    if v then
      return true, v
    end
    return false, 0
  end
})
setmetatable(Double, Number)
if not debugsetmetatable then
  local NullReferenceException = System.NullReferenceException
  local systemToString = System.toString
  function System.ObjectEqualsObj(this, obj)
    if this == nil then throw(NullReferenceException()) end
    local t = type(this)
    if t == "number" then
      return equalsObj(this, obj)
    elseif t == "table" then
      return this:EqualsObj(obj)
    end
    return this == obj
  end
  function System.ObjectGetHashCode(this)
    if this == nil then throw(NullReferenceException()) end
    if type(this) == "table" then
      return this:GetHashCode()
    end
    return hash(this)
  end
  function System.ObjectToString(this)
    if this == nil then throw(NullReferenceException()) end
    return systemToString(this)
  end
  function System.IComparableCompareTo(this, other)
    if this == nil then throw(NullReferenceException()) end
    local t = type(this)
    if t == "number" then
      return compareDouble(this, other)
    elseif t == "boolean" then
      return System.Boolean.CompareTo(this, other)
    end
    return this:CompareTo(other)
  end
  function System.IEquatableEquals(this, other)
    if this == nil then throw(NullReferenceException()) end
    local t = type(this)
    if t == "number" then
      return equalsDouble(this, other)
    elseif t == "boolean" then
      return System.Boolean.Equals(this, other)
    end
    return this:Equals(other)
  end
  function System.IFormattableToString(this, format, formatProvider)
    if this == nil then throw(NullReferenceException()) end
    local t = type(this)
    if t == "number" then
      return toString(this, format, formatProvider)
    end
    return this:ToString(format, formatProvider)
  end
end

end

-- CoreSystemLib: Char.lua
do
local System = System
local throw = System.throw
local Int = System.Int
local ArgumentNullException = System.ArgumentNullException
local ArgumentOutOfRangeException = System.ArgumentOutOfRangeException
local setmetatable = setmetatable
local byte = string.byte
local isSeparatorTable = {
  [0x2028] = true,
  [0x2029] = true,
  [0x0020] = true,
  [0x00A0] = true,
  [0x1680] = true,
  [0x180E] = true,
  [0x202F] = true,
  [0x205F] = true,
  [0x3000] = true,
}
local isSymbolTable = {
  [36] = true,
  [43] = true,
  [60] = true,
  [61] = true,
  [62] = true,
  [94] = true,
  [96] = true,
  [124] = true,
  [126] = true,
  [172] = true,
  [180] = true,
  [182] = true,
  [184] = true,
  [215] = true,
  [247] = true,
}
local isWhiteSpace = {
  [0x0020] = true,
  [0x00A0] = true,
  [0x1680] = true,
  [0x202F] = true,
  [0x205F] = true,
  [0x3000] = true,
  [0x2028] = true,
  [0x2029] = true,
  [0x0085] = true,
}
local function get(s, index)
  if s == nil then throw(ArgumentNullException("s")) end
  local c = byte(s, index + 1)
  if not c then throw(ArgumentOutOfRangeException("index")) end
  return c
end
local function isAsciiLetter(c)
  return (c >= 65 and c <= 90) or (c >= 97 and c <= 122)
end
local function isDigit(c, index)
  if index then
    c = get(c, index)
  end
  return (c >= 48 and c <= 57)
end
local function isLetter(c, index)
  if index then
    c = get(c, index)
  end
  if c < 128 then
    return isAsciiLetter(c)
  else
    return (c >= 0x0400 and c <= 0x042F)
      or (c >= 0x03AC and c <= 0x03CE)
      or (c == 0x01C5 or c == 0x1FFC)
      or (c >= 0x02B0 and c <= 0x02C1)
      or (c >= 0x1D2C and c <= 0x1D61)
      or (c >= 0x05D0 and c <= 0x05EA)
      or (c >= 0x0621 and c <= 0x063A)
      or (c >= 0x4E00 and c <= 0x9FC3)
  end
end
local Char = System.defStc("System.Char", {
  ToString = string.char,
  CompareTo = Int.CompareTo,
  CompareToObj = Int.CompareToObj,
  Equals = Int.Equals,
  EqualsObj = Int.EqualsObj,
  GetHashCode = Int.GetHashCode,
  default = Int.default,
  IsAsciiLetter = isAsciiLetter,
  IsControl = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >=0 and c <= 31) or (c >= 127 and c <= 159)
  end,
  IsDigit = isDigit,
  IsLetter = isLetter,
  IsLetterOrDigit = function (c, index)
    if index then
      c = get(c, index)
    end
    return isDigit(c) or isLetter(c)
  end,
  IsLower = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >= 97 and c <= 122) or (c >= 945 and c <= 969)
  end,
  IsNumber = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >= 48 and c <= 57) or c == 178 or c == 179 or c == 185 or c == 188 or c == 189 or c == 190
  end,
  IsPunctuation = function (c, index)
    if index then
      c = get(c, index)
    end
    if c < 256 then
      return (c >= 0x0021 and c <= 0x0023)
        or (c >= 0x0025 and c <= 0x002A)
        or (c >= 0x002C and c <= 0x002F)
        or (c >= 0x003A and c <= 0x003B)
        or (c >= 0x003F and c <= 0x0040)
        or (c >= 0x005B and c <= 0x005D)
        or c == 0x5F or c == 0x7B or c == 0x007D or c == 0x00A1 or c == 0x00AB or c == 0x00AD or c == 0x00B7 or c == 0x00BB or c == 0x00BF
    end
    return false
  end,
  IsSeparator = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >= 0x2000 and c <= 0x200A) or isSeparatorTable[c] == true
  end,
  IsSymbol = function (c, index)
    if index then
      c = get(c, index)
    end
    if c < 256 then
      return (c >= 162 and c <= 169) or (c >= 174 and c <= 177) or isSymbolTable(c) == true
    end
    return false
  end,
  IsUpper = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >= 65 and c <= 90) or (c >= 913 and c <= 937)
  end,
  IsWhiteSpace = function (c, index)
    if index then
      c = get(c, index)
    end
    return (c >= 0x2000 and c <= 0x200A) or (c >= 0x0009 and c <= 0x000d) or isWhiteSpace[c] == true
  end,
  Parse = function (s)
    if s == nil then
      throw(System.ArgumentNullException())
    end
    if #s ~= 1 then
      throw(System.FormatException())
    end
    return s:byte()
  end,
  TryParse = function (s)
    if s == nil or #s ~= 1 then
      return false, 0
    end
    return true, s:byte()
  end,
  ToLower = function (c)
    if (c >= 65 and c <= 90) or (c >= 913 and c <= 937) then
      return c + 32
    end
    return c
  end,
  ToUpper = function (c)
    if (c >= 97 and c <= 122) or (c >= 945 and c <= 969) then
      return c - 32
    end
    return c
  end,
  IsHighSurrogate = function (c, index)
    if index then
      c = get(c, index)
    end
    return c >= 0xD800 and c <= 0xDBFF
  end,
  IsLowSurrogate = function (c, index)
    if index then
      c = get(c, index)
    end
    return c >= 0xDC00 and c <= 0xDFFF
  end,
  IsSurrogate = function (c, index)
    if index then
      c = get(c, index)
    end
    return c >= 0xD800 and c <= 0xDFFF
  end,
  base = function (_, T)
    return { System.IComparable, System.IComparable_1(T), System.IEquatable_1(T) }
  end
})
local ValueType = System.ValueType
local charMetaTable = setmetatable({ __index = ValueType, __call = Char.default }, ValueType)
setmetatable(Char, charMetaTable)

end

-- CoreSystemLib: String.lua
do
local System = System
local Char = System.Char
local throw = System.throw
local emptyFn = System.emptyFn
local lengthFn = System.lengthFn
local systemToString = System.toString
local debugsetmetatable = System.debugsetmetatable
local ArgumentException = System.ArgumentException
local ArgumentNullException = System.ArgumentNullException
local ArgumentOutOfRangeException = System.ArgumentOutOfRangeException
local FormatException = System.FormatException
local IndexOutOfRangeException = System.IndexOutOfRangeException
local string = string
local char = string.char
local rep = string.rep
local lower = string.lower
local upper = string.upper
local byte = string.byte
local sub = string.sub
local find = string.find
local gsub = string.gsub
local table = table
local tconcat = table.concat
local unpack = table.unpack
local getmetatable = getmetatable
local setmetatable = setmetatable
local select = select
local type = type
local tonumber = tonumber
local String
local function toString(t, isch, format)
  if isch then return char(t) end
  return systemToString(t, format)
end
local function checkIndex(value, startIndex, count)
  if value == nil then throw(ArgumentNullException("value")) end
  local len = #value
  if not startIndex then
    startIndex, count = 0, len
  elseif not count then
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    count = len - startIndex
  else
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    if count < 0 or count > len - startIndex then
      throw(ArgumentOutOfRangeException("count"))
    end
  end
  return startIndex, count, len
end
local bytemarkers = { {0x7FF,192}, {0xFFFF,224}, {0x1FFFFF,240} }
local function utf8(decimal)
  if decimal < 128 then return char(decimal) end
  local charbytes = {}
  for i = 1, #bytemarkers do
    local vals = bytemarkers[i]
    if decimal<=vals[1] then
      for b = i + 1, 2, -1 do
        local mod = decimal%64
        decimal = (decimal-mod)/64
        charbytes[b] = char(128+mod)
      end
      charbytes[1] = char(vals[2]+decimal)
      break
    end
  end
  return tconcat(charbytes)
end
local function ctor(String, value, startIndex, count)
  if type(value) == "number" then
    if startIndex <= 0 then throw(ArgumentOutOfRangeException("count")) end
    return rep(char(value), startIndex)
  end
  startIndex, count = checkIndex(value, startIndex, count)
  local found
  for i = startIndex + 1, startIndex + count do
    local c = value[i]
    if c >= 128 then
      found = true
      break
    end
  end
  if not found then
    return char(unpack(value, startIndex + 1, startIndex + count))
  end
  local t, index = {}, 1
  for i = startIndex + 1, startIndex + count do
    local c = value[i]
    t[index] = utf8(c)
    index = index + 1
  end
  return tconcat(t)
end
local function get(this, index)
  local c = byte(this, index + 1)
  if not c then
    throw(IndexOutOfRangeException())
  end
  return c
end
local function compare(strA, strB, ignoreCase)
  if strA == nil then
    if strB == nil then
      return 0
    end
    return -1
  elseif strB == nil then
    return 1
  end
  if ignoreCase then
    strA, strB = lower(strA), lower(strB)
  end
  if strA < strB then return -1 end
  if strA > strB then return 1 end
  return 0
end
local function compareFull(...)
  local n = select("#", ...)
  if n == 2 then
    return compare(...)
  elseif n == 3 then
    local strA, strB, ignoreCase = ...
    if type(ignoreCase) == "number" then
      ignoreCase = ignoreCase % 2 ~= 0
    end
    return compare(strA, strB, ignoreCase)
  elseif n == 4 then
    local strA, strB, ignoreCase, options = ...
    if type(options) == "number" then
      ignoreCase = options == 1 or options == 268435456
    end
    return compare(strA, strB, ignoreCase)
  else
    local strA, indexA, strB, indexB, length, ignoreCase, options = ...
    if type(ignoreCase) == "number" then
      ignoreCase = ignoreCase % 2 ~= 0
    elseif type(options) == "number" then
      ignoreCase = options == 1 or options == 268435456
    end
    checkIndex(strA, indexA, length)
    checkIndex(strB, indexB, length)
    strA, strB = sub(strA, indexA + 1, indexA +  length), sub(strB, indexB + 1, indexB + length)
    return compare(strA, strB, ignoreCase)
  end
end
local function concat(...)
  local t = {}
  local count = 1
  local len = select("#", ...)
  if len == 1 then
    local v = ...
    if System.isEnumerableLike(v) then
      local isch = v.__genericT__ == Char
      for _, v in System.each(v) do
        t[count] = toString(v, isch)
        count = count + 1
      end
    else
      return toString(v)
    end
  else
    for i = 1, len do
      local v = select(i, ...)
      t[count] = toString(v)
      count = count + 1
    end
  end
  return tconcat(t)
end
local function equals(this, value, comparisonType)
  if not comparisonType then
    return this == value
  end
  return compare(this, value, comparisonType % 2 ~= 0) == 0
end
local function throwFormatError()
  throw(FormatException("Input string was not in a correct format."))
end
local function formatBuild(format, len, select, ...)
  local t, count = {}, 1
  local i, j, s = 1
  while true do
    local startPos  = i
    while true do
      i, j, s = find(format, "([{}])", i)
      if not i then
        if count == 1 then
          return format
        end
        t[count] = sub(format, startPos)
        return table.concat(t)
      end
      local pos = i - 1
      i = i + 1
      local c = byte(format, i)
      if not c then throwFormatError() end
      if s == '{' then
        if c == 123 then
          i = i + 1
        else
          pos = i - 2
          if pos >= startPos then
            t[count] = sub(format, startPos, pos)
            count = count + 1
          end
          break
        end
      else
        if c == 125 then
          i = i + 1
        else
          throwFormatError()
        end
      end
      if pos >= startPos then
        t[count] = sub(format, startPos, pos)
        count = count + 1
      end
      t[count] = s
      count = count + 1
      startPos = i
    end
    local r, alignment, formatString
    i, j, s, r = find(format, "^(%d+)(.-)}", i)
    if not i then throwFormatError() end
    s = tonumber(s) + 1
    if s > len then throwFormatError() end
    if r ~= "" then
      local i, j, c, d = find(r, "^,([-]?)(%d+)")
      if i then
        alignment = tonumber(d)
        if c == '-' then alignment = -alignment end
        i = j + 1
      end
      i, j, c = find(r, "^:(.*)$", i)
      if i then
        formatString = c
      elseif not alignment then
        throwFormatError()
      end
    end
    s = select(s, ...)
    if s ~= nil and s ~= System.null then
      s = toString(s, false, formatString)
      if alignment then
        s = ("%" .. alignment .. "s"):format(s)
      end
      t[count] = s
      count = count + 1
    end
    i = j + 1
  end
end
local function selectTable(i, t)
  return t[i]
end
local function format(format, ...)
  if format == nil then throw(ArgumentNullException()) end
  local len = select("#", ...)
  if len == 1 then
    local args = ...
    if System.isArrayLike(args) then
      return formatBuild(format, #args, selectTable, args)
    end
  end
  return formatBuild(format, len, select, ...)
end
local function isNullOrEmpty(value)
  return value == nil or #value == 0
end
local function isNullOrWhiteSpace(value)
  return value == nil or find(value, "^%s*$") ~= nil
end
local function joinEnumerable(separator, values)
  if values == nil then throw(ArgumentNullException("values")) end
  if type(separator) == "number" then
    separator = char(separator)
  end
  local isch = values.__genericT__ == Char
  local t = {}
  local len = 1
  for _, v in System.each(values) do
    if v ~= nil then
      t[len] = toString(v, isch)
      len = len + 1
    end
  end
  return tconcat(t, separator)
end
local function joinParams(separator, ...)
  if type(separator) == "number" then
    separator = char(separator)
  end
  local t = {}
  local len = 1
  local n = select("#", ...)
  if n == 1 then
    local values = ...
    if System.isArrayLike(values) then
      for i = 0, #values - 1 do
        local v = values:get(i)
        if v ~= nil then
          t[len] = toString(v)
          len = len + 1
        end
      end
      return tconcat(t, separator)
    end
  end
  for i = 1, n do
    local v = select(i, ...)
    if v ~= nil then
      t[len] = toString(v)
      len = len + 1
    end
  end
  return tconcat(t, separator)
end
local function join(separator, value, startIndex, count)
  if type(separator) == "number" then
    separator = char(separator)
  end
  local t = {}
  local len = 1
  if startIndex then
    checkIndex(value, startIndex, count)
    for i = startIndex + 1, startIndex + count do
      local v = value[i]
      if v ~= System.null then
        t[len] = v
        len = len + 1
      end
    end
  else
    for _, v in System.each(value) do
      if v ~= nil then
        t[len] = v
        len = len + 1
      end
    end
  end
  return tconcat(t, separator)
end
local function compareToObj(this, v)
  if v == nil then return 1 end
  if type(v) ~= "string" then
    throw(ArgumentException("Arg_MustBeString"))
  end
  return compare(this, v)
end
local function escape(s)
  return gsub(s, "([%%%^%.])", "%%%1")
end
local function contains(this, value, comparisonType)
  if value == nil then throw(ArgumentNullException("value")) end
  if type(value) == "number" then
    value = char(value)
  end
  if comparisonType then
    local ignoreCase = comparisonType % 2 ~= 0
    if ignoreCase then
      this, value = lower(this), lower(value)
    end
  end
  return find(this, escape(value)) ~= nil
end
local function copyTo(this, sourceIndex, destination, destinationIndex, count)
  if destination == nil then throw(ArgumentNullException("destination")) end
  if count < 0 then throw(ArgumentOutOfRangeException("count")) end
  local len = #this
  if sourceIndex < 0 or count > len - sourceIndex then throw(ArgumentOutOfRangeException("sourceIndex")) end
  if destinationIndex > #destination - count or destinationIndex < 0 then throw(ArgumentOutOfRangeException("destinationIndex")) end
  if count > 0 then
    destinationIndex = destinationIndex + 1
    for i = sourceIndex + 1, sourceIndex + count do
      destination[destinationIndex] = byte(this, i)
      destinationIndex = destinationIndex + 1
    end
  end
end
local function endsWith(this, suffix)
  return suffix == "" or sub(this, -#suffix) == suffix
end
local function equalsObj(this, v)
  if type(v) == "string" then
    return this == v
  end
  return false
end
local CharEnumerator = System.define("System.CharEnumerator", {
  base = { System.IEnumerator_1(System.Char), System.IDisposable, System.ICloneable },
  getCurrent = System.getCurrent,
  Dispose = emptyFn,
  MoveNext = function (this)
    local index, s = this.index, this.s
    if index <= #s then
      this.current = byte(s, index)
      this.index = index + 1
      return true
    end
    return false
  end
})
local function getEnumerator(this)
  return setmetatable({ s = this, index = 1 }, CharEnumerator)
end
local function getTypeCode()
  return 18
end
local function indexOf(this, value, startIndex, count, comparisonType)
  if value == nil then throw(ArgumentNullException("value")) end
  startIndex, count = checkIndex(this, startIndex, count)
  if type(value) == "number" then value = char(value) end
  local ignoreCase = comparisonType and comparisonType % 2 ~= 0
  if ignoreCase then
    this, value = lower(this), lower(value)
  end
  local i, j = find(this, escape(value), startIndex + 1)
  if i then
    local e = startIndex + count
    if j <= e then
      return i - 1
    end
    return - 1
  end
  return -1
end
local function indexOfAny(this, anyOf, startIndex, count)
  if anyOf == nil then throw(ArgumentNullException("chars")) end
  startIndex, count = checkIndex(this, startIndex, count)
  anyOf = "[" .. escape(char(unpack(anyOf))) .. "]"
  local i, j = find(this, anyOf, startIndex + 1)
  if i then
    local e = startIndex + count
    if j <= e then
      return i - 1
    end
    return - 1
  end
  return -1
end
local function insert(this, startIndex, value)
  if value == nil then throw(ArgumentNullException("value")) end
  if startIndex < 0 or startIndex > #this then throw(ArgumentOutOfRangeException("startIndex")) end
  return sub(this, 1, startIndex) .. value .. sub(this, startIndex + 1)
end
local function chechLastIndexOf(value, startIndex, count)
  if value == nil then throw(ArgumentNullException("value")) end
  local len = #value
  if not startIndex then
    startIndex, count = len - 1, len
  elseif not count then
    count = len == 0 and 0 or (startIndex + 1)
  end
  if len == 0 then
    if startIndex ~= -1 and startIndex ~= 0 then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    if count ~= 0 then
      throw(ArgumentOutOfRangeException("count"))
    end
  end
  if startIndex < 0 or startIndex >= len then
    throw(ArgumentOutOfRangeException("startIndex"))
  end
  if count < 0 or startIndex - count + 1 < 0 then
    throw(ArgumentOutOfRangeException("count"))
  end
  return startIndex, count, len
end
local function lastIndexOf(this, value, startIndex, count, comparisonType)
  if value == nil then throw(ArgumentNullException("value")) end
  startIndex, count = chechLastIndexOf(this, startIndex, count)
  if type(value) == "number" then value = char(value) end
  local ignoreCase = comparisonType and comparisonType % 2 ~= 0
  if ignoreCase then
    this, value = lower(this), lower(value)
  end
  value = escape(value)
  local e = startIndex + 1
  local f = e - count + 1
  local index = -1
  while true do
    local i, j = find(this, value, f)
    if not i or j > e then
      break
    end
    index = i - 1
    f = j + 1
  end
  return index
end
local function lastIndexOfAny(this, anyOf, startIndex, count)
  if anyOf == nil then throw(ArgumentNullException("chars")) end
  startIndex, count = chechLastIndexOf(this, startIndex, count)
  anyOf = "[" .. escape(char(unpack(anyOf))) .. "]"
  local f, e = startIndex - count + 1, startIndex + 1
  local index = -1
  while true do
    local i, j = find(this, anyOf, f)
    if not i or j > e then
      break
    end
    index = i - 1
    f = j + 1
  end
  return index
end
local function padLeft(this, totalWidth, paddingChar)
  local len = #this;
  if len >= totalWidth then
    return this
  else
    paddingChar = paddingChar or 0x20
    return rep(char(paddingChar), totalWidth - len) .. this
  end
end
local function padRight(this, totalWidth, paddingChar)
  local len = #this
  if len >= totalWidth then
    return this
  else
    paddingChar = paddingChar or 0x20
    return this .. rep(char(paddingChar), totalWidth - len)
  end
end
local function remove(this, startIndex, count)
  startIndex, count = checkIndex(this, startIndex, count)
  return sub(this, 1, startIndex) .. sub(this, startIndex + 1 + count)
end
local function replace(this, a, b)
  if type(a) == "number" then
    a, b = char(a), char(b)
  end
  return gsub(this, escape(a), b)
end
local function findAny(s, strings, startIndex)
  local findBegin, findEnd
  for i = 1, #strings do
    local posBegin, posEnd = find(s, escape(strings[i]), startIndex)
    if posBegin then
      if not findBegin or posBegin < findBegin then
        findBegin, findEnd = posBegin, posEnd
      else
        break
      end
    end
  end
  return findBegin, findEnd
end
local function split(this, strings, count, options)
  local t = {}
  local find = find
  if type(strings) == "table" then
    if #strings == 0 then
      return t
    end
    if type(strings[1]) == "string" then
      find = findAny
    else
      strings = char(unpack(strings))
      strings = escape(strings)
      strings = "[" .. strings .. "]"
    end
  elseif type(strings) == "string" then
    strings = escape(strings)
  else
    strings = char(strings)
    strings = escape(strings)
  end
  local len = 1
  local startIndex = 1
  while true do
    local posBegin, posEnd = find(this, strings, startIndex)
    posBegin = posBegin or 0
    local subStr = sub(this, startIndex, posBegin -1)
    if options ~= 1 or #subStr > 0 then
      t[len] = subStr
      len = len + 1
      if count then
        count = count -1
        if count == 0 then
          if posBegin ~= 0 then
            t[len - 1] = sub(this, startIndex)
          end
          break
        end
      end
    end
    if posBegin == 0 then
      break
    end
    startIndex = posEnd + 1
  end
  return System.arrayFromTable(t, String)
end
local function startsWith(this, prefix)
  return sub(this, 1, #prefix) == prefix
end
local function substring(this, startIndex, count)
  startIndex, count = checkIndex(this, startIndex, count)
  return sub(this, startIndex + 1, startIndex + count)
end
local function toCharArray(str, startIndex, count)
  startIndex, count = checkIndex(str, startIndex, count)
  local t = {}
  local len = 1
  for i = startIndex + 1, startIndex + count do
    t[len] = byte(str, i)
    len = len + 1
  end
  return System.arrayFromTable(t, System.Char)
end
local function trim(this, chars, ...)
  if not chars then
    chars = "^%s*(.-)%s*$"
  else
    if type(chars) == "table" then
      chars = char(unpack(chars))
    else
      chars = char(chars, ...)
    end
    chars = escape(chars)
    chars = "^[" .. chars .. "]*(.-)[" .. chars .. "]*$"
  end
  return (gsub(this, chars, "%1"))
end
local function trimEnd(this, chars, ...)
  if not chars then
    chars = "(.-)%s*$"
  else
    if type(chars) == "table" then
      chars = char(unpack(chars))
    else
      chars = char(chars, ...)
    end
    chars = escape(chars)
    chars = "(.-)[" .. chars .. "]*$"
  end
  return (gsub(this, chars, "%1"))
end
local function trimStart(this, chars, ...)
  if not chars then
    chars = "^%s*(.-)"
  else
    if type(chars) == "table" then
      chars = char(unpack(chars))
    else
      chars = char(chars, ...)
    end
    chars = escape(chars)
    chars = "^[" .. chars .. "]*(.-)"
  end
  return (gsub(this, chars, "%1"))
end
local function inherits(_, T)
  return { System.IEnumerable_1(System.Char), System.IComparable, System.IComparable_1(T), System.IConvertible, System.IEquatable_1(T), System.ICloneable }
end
string.traceback = emptyFn
string.getLength = lengthFn
string.getCount = lengthFn
string.get = get
string.Compare = compareFull
string.CompareOrdinal = compareFull
string.Concat = concat
string.Copy = System.identityFn
string.Equals = equals
string.Format = format
string.IsNullOrEmpty = isNullOrEmpty
string.IsNullOrWhiteSpace = isNullOrWhiteSpace
string.JoinEnumerable = joinEnumerable
string.JoinParams = joinParams
string.Join = join
string.CompareTo = compare
string.CompareToObj = compareToObj
string.Contains = contains
string.CopyTo = copyTo
string.EndsWith = endsWith
string.EqualsObj = equalsObj
string.GetEnumerator = getEnumerator
string.GetTypeCode = getTypeCode
string.IndexOf = indexOf
string.IndexOfAny = indexOfAny
string.Insert = insert
string.LastIndexOf = lastIndexOf
string.LastIndexOfAny = lastIndexOfAny
string.PadLeft = padLeft
string.PadRight = padRight
string.Remove = remove
string.Replace = replace
string.Split = split
string.StartsWith = startsWith
string.Substring = substring
string.ToCharArray = toCharArray
string.ToLower = lower
string.ToLowerInvariant = lower
string.ToString = System.identityFn
string.ToUpper = upper
string.ToUpperInvariant = upper
string.Trim = trim
string.TrimEnd = trimEnd
string.TrimStart = trimStart
if debugsetmetatable then
  String = string
  String.__genericT__ = System.Char
  String.base = inherits
  System.define("System.String", String)
  debugsetmetatable("", String)
  local Object = System.Object
  local StringMetaTable = setmetatable({ __index = Object, __call = ctor }, Object)
  setmetatable(String, StringMetaTable)
else
  string.__call = ctor
  string.__index = string
  String = getmetatable("")
  String.__genericT__ = System.Char
  String.base = inherits
  System.define("System.String", String)
  String.__index = string
  setmetatable(String, string)
  setmetatable(string, System.Object)
end

end

-- CoreSystemLib: Boolean.lua
do
local System = System
local throw = System.throw
local debugsetmetatable = System.debugsetmetatable
local ArgumentException = System.ArgumentException
local ArgumentNullException = System.ArgumentNullException
local FormatException = System.FormatException
local type = type
local setmetatable = setmetatable
local function compareTo(this, v)
  if this == v then
    return 0
  elseif this == false then
    return -1
  end
  return 1
end
local falseString = "False"
local trueString = "True"
local function parse(s)
  if s == nil then
    return nil, 1
  end
  local i, j, value = s:find("^[%s%c%z]*(%a+)[%s%c%z]*$")
  if value then
    s = value:lower()
    if s == "true" then
      return true
    elseif s == "false" then
      return false
    end
  end
  return nil, 2
end
local function toString(this)
  return this and trueString or falseString
end
local Boolean = System.defStc("System.Boolean", {
  default = System.falseFn,
  Equals = System.equals,
  CompareTo = compareTo,
  ToString = toString,
  FalseString = falseString,
  TrueString = trueString,
  GetHashCode = function (this)
    return this and 1 or 0
  end,
  CompareToObj = function (this, v)
    if v == nil then return 1 end
    if type(v) ~= "boolean" then
      throw(ArgumentException("Arg_MustBeBoolean"))
    end
    return compareTo(this, v)
  end,
  EqualsObj = function (this, v)
    if type(v) ~= "boolean" then
      return false
    end
    return this == v
  end,
  __concat = function (a, b)
    if type(a) == "boolean" then
      return toString(a) .. b
    else
      return a .. toString(b)
    end
  end,
  __tostring = toString,
  Parse = function (s)
    local v, err = parse(s)
    if v == nil then
      if err == 1 then
        throw(ArgumentNullException())
      else
        throw(FormatException())
      end
    end
    return v
  end,
  TryParse = function (s)
    local v = parse(s)
    if v ~= nil then
      return true, v
    end
    return false, false
  end,
  base = function (_, T)
    return { System.IComparable, System.IConvertible, System.IComparable_1(T), System.IEquatable_1(T) }
  end
})
if debugsetmetatable then
  debugsetmetatable(false, Boolean)
end
local ValueType = System.ValueType
local boolMetaTable = setmetatable({ __index = ValueType, __call = Boolean.default }, ValueType)
setmetatable(Boolean, boolMetaTable)

end

-- CoreSystemLib: Delegate.lua
do
local System = System
local throw = System.throw
local Object = System.Object
local debugsetmetatable = System.debugsetmetatable
local ArgumentNullException = System.ArgumentNullException
local setmetatable = setmetatable
local assert = assert
local select = select
local type = type
local unpack = table.unpack
local tmove = table.move
local Delegate
local multicast
local function appendFn(t, count, f)
  if type(f) == "table" then
    for i = 1, #f do
      t[count] = f[i]
      count = count + 1
    end
  else
    t[count] = f
    count = count + 1
  end
  return count
end
local function combineImpl(fn1, fn2)
  local t = setmetatable({}, multicast)
  local count = 1
  count = appendFn(t, count, fn1)
  appendFn(t, count, fn2)
  return t
end
local function combine(fn1, fn2)
  if fn1 ~= nil then
    if fn2 ~= nil then
      return combineImpl(fn1, fn2)
    end
    return fn1
  end
  if fn2 ~= nil then return fn2 end
  return nil
end
local function equalsMulticast(fn1, fn2, start, count)
  for i = 1, count do
    if fn1[start + i] ~= fn2[i] then
      return false
    end
  end
  return true
end
local function delete(fn, count, deleteIndex, deleteCount)
  local t =  setmetatable({}, multicast)
  local len = 1
  for i = 1, deleteIndex - 1 do
    t[len] = fn[i]
    len = len + 1
  end
  for i = deleteIndex + deleteCount, count do
    t[len] = fn[i]
    len = len + 1
  end
  return t
end
local function removeImpl(fn1, fn2)
  if type(fn2) ~= "table" then
    if type(fn1) ~= "table" then
      if fn1 == fn2 then
        return nil
      end
    else
      local count = #fn1
      for i = count, 1, -1 do
        if fn1[i] == fn2 then
          if count == 2 then
            return fn1[3 - i]
          else
            return delete(fn1, count, i, 1)
          end
        end
      end
    end
  elseif type(fn1) == "table" then
    local count1, count2 = #fn1, # fn2
    local diff = count1 - count2
    for i = diff + 1, 1, -1 do
      if equalsMulticast(fn1, fn2, i - 1, count2) then
        if diff == 0 then
          return nil
        elseif diff == 1 then
          return fn1[i ~= 1 and 1 or count1]
        else
          return delete(fn1, count1, i, count2)
        end
      end
    end
  end
  return fn1
end
local function remove(fn1, fn2)
  if fn1 ~= nil then
    if fn2 ~= nil then
      return removeImpl(fn1, fn2)
    end
    return fn1
  end
  return nil
end
local multiKey = System.multiKey
local mt = {}
local function makeGenericTypes(...)
  local gt, gk = multiKey(mt, nil, ...)
  local t = gt[gk]
  if t == nil then
    t = setmetatable({ ... }, Delegate)
    gt[gk] = t
  end
  return t
end
Delegate = System.define("System.Delegate", {
  __add = combine,
  __sub = remove,
  EqualsObj = System.equals,
  Combine = combine,
  Remove = remove,
  RemoveAll = function (source, value)
    local newDelegate
    repeat
      newDelegate = source
      source = remove(source, value)
    until newDelegate == source
    return newDelegate
  end,
  DynamicInvoke = function (this, ...)
    return this(...)
  end,
  GetType = function ()
    return System.typeof(Delegate)
  end,
  GetInvocationList = function (this)
    local t
    if type(this) == "table" then
      t = {}
      tmove(this, 1, #this, 1, t)
    else
      t = { this }
    end
    return System.arrayFromTable(t, Delegate)
  end
})
local delegateMetaTable = setmetatable({ __index = Object, __call = makeGenericTypes }, Object)
setmetatable(Delegate, delegateMetaTable)
if debugsetmetatable then
  debugsetmetatable(System.emptyFn, Delegate)
  function System.event(name)
    local function a(this, v)
      this[name] = this[name] + v
    end
    local function r(this, v)
      this[name] = this[name] - v
    end
    return a, r
  end
else
  System.DelegateCombine = combine
  System.DelegateRemove = remove
  function System.event(name)
    local function a(this, v)
      this[name] = combine(this[name], v)
    end
    local function r(this, v)
      this[name] = remove(this[name], v)
    end
    return a, r
  end
end
multicast = setmetatable({
  __index = Delegate,
  __add = combine,
  __sub = remove,
  __call = function (t, ...)
    local result
    for i = 1, #t do
      result = t[i](...)
    end
    return result
  end,
  __eq = function (fn1, fn2)
    local len1, len2 = #fn1, #fn2
    if len1 ~= len2 then
      return false
    end
    for i = 1, len1 do
      if fn1[i] ~= fn2[i] then
        return false
      end
    end
    return true
  end
}, Delegate)
function System.fn(target, method)
  assert(method)
  if target == nil then throw(ArgumentNullException()) end
  local f = target[method]
  if f == nil then
    f = function (...)
      return method(target, ...)
    end
    target[method] = f
  end
  return f
end
local binds = setmetatable({}, { __mode = "k" })
function System.bind(f, n, ...)
  assert(f)
  local gt, gk = multiKey(binds, nil, f, ...)
  local fn = gt[gk]
  if fn == nil then
    local args = { ... }
    fn = function (...)
      local len = select("#", ...)
      if len == n then
        return f(..., unpack(args))
      else
        assert(len > n)
        local t = { ... }
        for i = 1, #args do
          local j = args[i]
          if type(j) == "number" then
            j = select(n + j, ...)
            assert(j)
          end
          t[n + i] = j
        end
        return f(unpack(t, 1, n + #args))
      end
    end
    gt[gk] = fn
  end
  return fn
end
local function bind(f, create, ...)
  assert(f)
  local gt, gk = multiKey(binds, nil, f, create)
  local fn = gt[gk]
  if fn == nil then
    fn = create(f, ...)
    gt[gk] = fn
  end
  return fn
end
local function create1(f, a)
  return function (...)
    return f(..., a)
  end
end
function System.bind1(f, a)
  return bind(f, create1, a)
end
local function create2(f, a, b)
  return function (...)
    return f(..., a, b)
  end
end
function System.bind2(f, a, b)
  return bind(f, create2, a, b)
end
local function create3(f, a, b, c)
  return function (...)
    return f(..., a, b, c)
  end
end
function System.bind3(f, a, b, c)
 return bind(f, create3, a, b, c)
end
local function create2_1(f)
  return function(x1, x2, T1, T2)
    return f(x1, x2, T2, T1)
  end
end
function System.bind2_1(f)
  return bind(f, create2_1)
end
local function create0_2(f)
  return function(x1, x2, T1, T2)
    return f(x1, x2, T2)
  end
end
function System.bind0_2(f)
  return bind(f, create0_2)
end
local EventArgs = System.define("System.EventArgs")
EventArgs.Empty = setmetatable({}, EventArgs)

end

-- CoreSystemLib: Enum.lua
do
local System = System
local throw = System.throw
local Int = System.Int
local Number = System.Number
local band = System.band
local bor = System.bor
local ArgumentNullException = System.ArgumentNullException
local ArgumentException = System.ArgumentException
local assert = assert
local pairs = pairs
local tostring = tostring
local type = type
local function toString(this, cls)
  if this == nil then return "" end
  if cls then
    for k, v in pairs(cls) do
      if v == this then
        return k
      end
    end
  end
  return tostring(this)
end
local function hasFlag(this, flag)
  if this == flag then
    return true
  end
  return band(this, flag) == flag
end
Number.EnumToString = toString
Number.HasFlag = hasFlag
System.EnumToString = toString
System.EnumHasFlag = hasFlag
local function tryParseEnum(enumType, value, ignoreCase)
  if enumType == nil then throw(ArgumentNullException("enumType")) end
  local cls = enumType[1] or enumType
  if cls.class ~= "E" then throw(ArgumentException("Arg_MustBeEnum")) end
  if value == nil then
    return
  end
  if ignoreCase then
    value = value:lower()
  end
  local i, j, s, r = 1
  while true do
    i, j, s = value:find("%s*(%a+)%s*", i)
    if not i then
      return
    end
    for k, v in pairs(cls) do
      local k = k
      if ignoreCase then
        k = k:lower()
      end
      if k == s then
        if not r then
          r = v
        else
          r = bor(r, v)
        end
        break
      end
    end
    i = value:find(',', j + 1)
    if not i then
      break
    end
    i = i + 1
  end
  return r
end
System.define("System.Enum", {
  CompareToObj = Int.CompareToObj,
  EqualsObj = Int.EqualsObj,
  default = Int.default,
  ToString = toString,
  HasFlag = hasFlag,
  GetHashCode = Int.GetHashCode,
  GetName = function (enumType, value)
    if enumType == nil then throw(ArgumentNullException("enumType")) end
    if value == nil then throw(ArgumentNullException("value")) end
    local cls = enumType[1] or enumType
    if cls.class ~= "E" then throw(ArgumentException("Arg_MustBeEnum")) end
    for k, v in pairs(cls) do
      if v == value then
        return k
      end
    end
  end,
  GetNames = function (enumType)
    if enumType == nil then throw(ArgumentNullException("enumType")) end
    local cls = enumType[1] or enumType
    if cls.class ~= "E" then throw(ArgumentException("Arg_MustBeEnum")) end
    local t = {}
    local count = 1
    for k, v in pairs(cls) do
      if type(v) == "number" then
        t[count] = k
        count = count + 1
      end
    end
    return System.arrayFromTable(t, System.String)
  end,
  GetValues = function (enumType)
    if enumType == nil then throw(ArgumentNullException("enumType")) end
    local cls = enumType[1] or enumType
    if cls.class ~= "E" then throw(ArgumentException("Arg_MustBeEnum")) end
    local t = {}
    local count = 1
    for _, v in pairs(cls) do
      if type(v) == "number" then
        t[count] = v
        count = count + 1
      end
    end
    return System.arrayFromTable(t, System.Int32)
  end,
  IsDefined = function (enumType, value)
    if enumType == nil then throw(ArgumentNullException("enumType")) end
    if value == nil then throw(ArgumentNullException("value")) end
    local cls = enumType[1] or enumType
    if cls.class ~= "E" then throw(ArgumentException("Arg_MustBeEnum")) end
    local t = type(value)
    if t == "string" then
      return cls[value] ~= nil
    elseif t == "number" then
      for _, v in pairs(cls) do
        if v == value then
          return true
        end
      end
      return false
    end
    throw(System.InvalidOperationException())
  end,
  Parse = function (enumType, value, ignoreCase)
    local result = tryParseEnum(enumType, value, ignoreCase)
    if result == nil then
      throw(ArgumentException("Requested value '" .. value .. "' was not found."))
    end
    return result
  end,
  TryParse = function (type, value, ignoreCase)
    local result = tryParseEnum(type, value, ignoreCase)
    if result == nil then
      return false, 0
    end
    return true, result
  end
})

end

-- CoreSystemLib: EqualityComparer.lua
do
local System = System
local define = System.define
local throw = System.throw
local equalsObj = System.equalsObj
local compareObj = System.compareObj
local hashObj = System.hashObj
local ArgumentException = System.ArgumentException
local ArgumentNullException = System.ArgumentNullException
local type = type
local EqualityComparer
EqualityComparer = define("System.EqualityComparer", function (T)
  local equals, getHashCode
  if T.class == 'S' then
    equals = T.Equals or equalsObj
    getHashCode = T.GetHashCode
  else
    equals = T.Equals and function (x, y) return x:Equals(y) end or equalsObj
    getHashCode = hashObj
  end
  local defaultComparer
  return {
    __genericT__ = T,
    base = { System.IEqualityComparer_1(T), System.IEqualityComparer },
    getDefault = function ()
      local comparer = defaultComparer
      if comparer == nil then
        comparer = EqualityComparer(T)()
        defaultComparer = comparer
      end
      return comparer
    end,
    EqualsOf = function (this, x, y)
      if x ~= nil then
        if y ~= nil then return equals(x, y) end
        return false
      end
      if y ~= nil then return false end
      return true
    end,
    GetHashCodeOf = function (this, obj)
      if obj == nil then return 0 end
      return getHashCode(obj)
    end,
    GetHashCodeObjOf = function (this, obj)
      if obj == nil then return 0 end
      if System.is(obj, T) then return getHashCode(obj) end
      throw(ArgumentException("Type of argument is not compatible with the generic comparer."))
    end,
    EqualsObjOf = function (this, x, y)
      if x == y then return true end
      if x == nil or y == nil then return false end
      local is = System.is
      if is(x, T) and is(y, T) then return equals(x, y) end
      throw(ArgumentException("Type of argument is not compatible with the generic comparer."))
    end
  }
end, nil, 1)
local function compare(this, a, b)
  return compareObj(a, b)
end
define("System.Comparer", (function ()
  local Comparer
  Comparer = {
    base = { System.IComparer },
    static = function (this)
      local default = Comparer()
      this.Default = default
      this.DefaultInvariant = default
    end,
    Compare = compare
  }
  return Comparer
end)())
local Comparer, ComparisonComparer
ComparisonComparer = define("System.ComparisonComparer", function (T)
  return {
    base = { Comparer(T) },
    __ctor__ = function (this, comparison)
      this.comparison = comparison
    end,
    Compare = function (this, x, y)
      return this.comparison(x, y)
    end
  }
end, nil, 1)
Comparer = define("System.Comparer_1", function (T)
  local Compare
  local compareTo = T.CompareTo
  if compareTo then
    if T.class ~= 'S' then
      compareTo = function (x, y)
        return x:CompareTo(y)
      end
    end
    Compare = function (this, x, y)
      if x ~= nil then
        if y ~= nil then
          return compareTo(x, y)
        end
        return 1
      end
      if y ~= nil then return -1 end
      return 0
    end
  else
    Compare = compare
  end
  local defaultComparer
  local function getDefault()
    local comparer = defaultComparer
    if comparer == nil then
      comparer = Comparer(T)()
      defaultComparer = comparer
    end
    return comparer
  end
  local function Create(comparison)
    if comparison == nil then throw(ArgumentNullException("comparison")) end
    return ComparisonComparer(T)(comparison)
  end
  return {
    __genericT__ = T,
    base = { System.IComparer_1(T), System.IComparer },
    getDefault = getDefault,
    getDefaultInvariant = getDefault,
    Compare = Compare,
    Create = Create
  }
end)

end

-- CoreSystemLib: Array.lua
do
local System = _G.System
local define = System.define
local throw = System.throw
local div = System.div
local bnot = System.bnot
local trueFn = System.trueFn
local falseFn = System.falseFn
local lengthFn = System.lengthFn
local er = System.er
local InvalidOperationException = System.InvalidOperationException
local NullReferenceException = System.NullReferenceException
local ArgumentException = System.ArgumentException
local ArgumentNullException = System.ArgumentNullException
local ArgumentOutOfRangeException = System.ArgumentOutOfRangeException
local IndexOutOfRangeException = System.IndexOutOfRangeException
local NotSupportedException = System.NotSupportedException
local EqualityComparer = System.EqualityComparer
local Comparer_1 = System.Comparer_1
local IEnumerable_1 = System.IEnumerable_1
local IEnumerator_1 = System.IEnumerator_1
local assert = assert
local select = select
local getmetatable = getmetatable
local setmetatable = setmetatable
local type = type
local table = table
local tinsert = table.insert
local tremove = table.remove
local tmove = table.move
local tsort = table.sort
local pack = table.pack
local unpack = table.unpack
local error = error
local coroutine = coroutine
local ccreate
local cresume
local cyield
if coroutine ~= nil then
  ccreate = coroutine.create
  cresume = coroutine.resume
  cyield = coroutine.yield
end
local null = { GetHashCode = System.zeroFn }
local arrayEnumerator
local arrayFromTable
local versions = setmetatable({}, { __mode = "k" })
System.versions = versions
local function throwFailedVersion()
  throw(InvalidOperationException("Collection was modified; enumeration operation may not execute."))
end
local function checkIndex(t, index)
  if index < 0 or index >= #t then
    throw(ArgumentOutOfRangeException("index"))
  end
end
local function checkIndexAndCount(t, index, count)
  if t == nil then throw(ArgumentNullException("array")) end
  if index < 0 or count < 0 or index + count > #t then
    throw(ArgumentOutOfRangeException("index or count"))
  end
end
local function wrap(v)
  if v == nil then
    return null
  end
  return v
end
local function unWrap(v)
  if v == null then
    return nil
  end
  return v
end
local function ipairs(t)
  local version = versions[t]
  return function (t, i)
    if version ~= versions[t] then
      throwFailedVersion()
    end
    local v = t[i]
    if v ~= nil then
      if v == null then
        v = nil
      end
      return i + 1, v
    end
  end, t, 1
end
local function eachFn(en)
  if en:MoveNext() then
    return true, en:getCurrent()
  end
  return nil
end
local function each(t)
  if t == nil then throw(NullReferenceException(), 1) end
  local getEnumerator = t.GetEnumerator
  if getEnumerator == arrayEnumerator then
    return ipairs(t)
  end
  local en = getEnumerator(t)
  return eachFn, en
end
function System.isArrayLike(t)
  return type(t) == "table" and t.GetEnumerator == arrayEnumerator
end
function System.isEnumerableLike(t)
  return type(t) == "table" and t.GetEnumerator ~= nil
end
function System.toLuaTable(array)
  local t = {}
  for i = 1, #array do
    local item = array[i]
    if item ~= null then
      t[i] = item
    end
  end
  return t
end
System.null = null
System.Void = null
System.each = each
System.ipairs = ipairs
System.throwFailedVersion = throwFailedVersion
System.wrap = wrap
System.unWrap = unWrap
System.checkIndex = checkIndex
System.checkIndexAndCount = checkIndexAndCount
local Array
local emptys = {}
local function get(t, index)
  local v = t[index + 1]
  if v == nil then
    throw(ArgumentOutOfRangeException("index"))
  end
  if v ~= null then
    return v
  end
  return nil
end
local function set(t, index, v)
  index = index + 1
  if t[index] == nil then
    throw(ArgumentOutOfRangeException("index"))
  end
  t[index] = v == nil and null or v
  versions[t] = (versions[t] or 0) + 1
end
local function add(t, v)
  local n = #t
  t[n + 1] = v == nil and null or v
  versions[t] = (versions[t] or 0) + 1
  return n
end
local function addRange(t, collection)
  if collection == nil then throw(ArgumentNullException("collection")) end
  local count = #t + 1
  if collection.GetEnumerator == arrayEnumerator then
    tmove(collection, 1, #collection, count, t)
  else
    for _, v in each(collection) do
      t[count] = v == nil and null or v
      count = count + 1
    end
  end
  versions[t] = (versions[t] or 0) + 1
end
local function clear(t)
  local size = #t
  if size > 0 then
    for i = 1, size do
      t[i] = nil
    end
    versions[t] = (versions[t] or 0) + 1
  end
end
local function first(t)
  local v = t[1]
  if v == nil then throw(InvalidOperationException())  end
  if v ~= null then
    return v
  end
  return nil
end
local function last(t)
  local n = #t
  if n == 0 then throw(InvalidOperationException()) end
  local v = t[n]
  if v ~= null then
    return v
  end
  return nil
end
local function unset()
  throw(NotSupportedException("Collection is read-only."))
end
local function fill(t, f, e, v)
  while f <= e do
    t[f] = v
    f = f + 1
  end
end
local function buildArray(ArrayT, n, t)
  if type(n) == "table" then
    t = n
  elseif t ~= nil then
    for i = 1, n  do
      if t[i] == nil then
        t[i] = null
      end
    end
  else
    t = {}
    if n > 0 then
      local T = ArrayT.__genericT__
      local default = T:default()
      if default == nil then
        fill(t, 1, n, null)
      elseif type(default) ~= "table" then
        fill(t, 1, n, default)
      else
        for i = 1, n do
          t[i] = T:default()
        end
      end
    end
  end
  return setmetatable(t, ArrayT)
end
local function indexOf(t, v, startIndex, count)
  if t == nil then throw(ArgumentNullException("array")) end
  local len = #t
  if not startIndex then
    startIndex, count = 0, len
  elseif not count then
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    count = len - startIndex
  else
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    if count < 0 or count > len - startIndex then
      throw(ArgumentOutOfRangeException("count"))
    end
  end
  local comparer = EqualityComparer(t.__genericT__).getDefault()
  local equals = comparer.EqualsOf
  for i = startIndex + 1, startIndex + count do
    local item = t[i]
    if item == null then item = nil end
    if equals(comparer, item, v) then
      return i - 1
    end
  end
  return -1
end
local function findIndex(t, startIndex, count, match)
  if t == nil then throw(ArgumentNullException("array")) end
  local len = #t
  if not count then
    startIndex, count, match = 0, len, startIndex
  elseif not match then
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    count, match = len - startIndex, count
  else
    if startIndex < 0 or startIndex > len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    if count < 0 or count > len - startIndex then
      throw(ArgumentOutOfRangeException("count"))
    end
  end
  if match == nil then throw(ArgumentNullException("match")) end
  local endIndex = startIndex + count
  for i = startIndex + 1, endIndex  do
    local item = t[i]
    if item == null then item = nil end
    if match(item) then
      return i - 1
    end
  end
  return -1
end
local function copy(sourceArray, sourceIndex, destinationArray, destinationIndex, length, reliable)
  if not reliable then
    checkIndexAndCount(sourceArray, sourceIndex, length)
    checkIndexAndCount(destinationArray, destinationIndex, length)
  end
  tmove(sourceArray, sourceIndex + 1, sourceIndex + length, destinationIndex + 1, destinationArray)
end
local function removeRange(t, index, count)
  local n = #t
  if count < 0 or index > n - count then
    throw(ArgumentOutOfRangeException("index or count"))
  end
  if count > 0 then
    if index + count < n then
      tmove(t, index + count + 1, n, index + 1)
    end
    fill(t, n - count + 1, n, nil)
    versions[t] = (versions[t] or 0) + 1
  end
end
local function findAll(t, match)
  if t == nil then throw(ArgumentNullException("array")) end
  if match == nil then throw(ArgumentNullException("match")) end
  local list = {}
  local count = 1
  for i = 1, #t do
    local item = t[i]
    if (item == null and match(nil)) or match(item) then
      list[count] = item
      count = count + 1
    end
  end
  return list
end
local function binarySearch(t, ...)
  if t == nil then throw(ArgumentNullException("array")) end
  local index, count, v, comparer
  local n = select("#", ...)
  if n == 1 or n == 2 then
    index, count, v, comparer = 0, #t, ...
  else
    index, count, v, comparer = ...
  end
  checkIndexAndCount(t, index, count)
  local compare
  if type(comparer) == "function" then
    compare = comparer
  elseif comparer == nil then
    comparer = Comparer_1(t.__genericT__).getDefault()
    compare = comparer.Compare
  else
    compare = comparer.Compare
  end
  local lo = index
  local hi = index + count - 1
  while lo <= hi do
    local i = lo + div(hi - lo, 2)
    local item = t[i + 1]
    if item == null then item = nil end
    local order = compare(comparer, item, v);
    if order == 0 then return i end
    if order < 0 then
      lo = i + 1
    else
      hi = i - 1
    end
  end
  return bnot(lo)
end
local function getSortComp(t, comparer)
  local compare
  if comparer then
    if type(comparer) == "function" then
      compare = comparer
    else
      local Compare = comparer.Compare
      if Compare then
        compare = function (x, y) return Compare(comparer, x, y) end
      else
        compare = comparer
      end
    end
  else
    comparer = Comparer_1(t.__genericT__).getDefault()
    local Compare = comparer.Compare
    compare = function (x, y) return Compare(comparer, x, y) end
  end
  return function(x, y)
    if x == null then x = nil end
    if y == null then y = nil end
    return compare(x, y) < 0
  end
end
local function sort(t, comparer)
  if #t > 1 then
    tsort(t, getSortComp(t, comparer))
    versions[t] = (versions[t] or 0) + 1
  end
end
local function addOrder(t, v, comparer)
  local i = binarySearch(t, v, comparer)
  if i >= 0 then return false end
  tinsert(t, bnot(i) + 1, v == nil and null or v)
  return true
end
local function addOrderRange(t, collection)
  if collection == nil then throw(ArgumentNullException("collection")) end
  local comparer, n = t.comparer, #t
  if collection.GetEnumerator == arrayEnumerator then
    for i = 1, #collection do
      local v = collection[i]
      if v == null then v = nil end
      addOrder(t, v, comparer)
    end
  else
    for _, v in each(collection) do
      addOrder(t, v, comparer)
    end
  end
  if #t ~= n then
    versions[t] = (versions[t] or 0) + 1
  end
end
local function exceptWithOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then return end
  if t == other then
    clear(t)
    return
  end
  local comparer = t.comparer
  if other.GetEnumerator == arrayEnumerator then
    for i = 1, #other do
      local v = other[i]
      if v == null then v = nil end
      local j = binarySearch(t, 0, n, v, comparer)
      if j >= 0 then
        tremove(t, j + 1)
      end
    end
  else
    for _, v in each(other) do
      local i = binarySearch(t, 0, n, v, comparer)
      if i >= 0 then
        tremove(t, i + 1)
      end
    end
  end
  if #t ~= n then
    versions[t] = (versions[t] or 0) + 1
  end
end
local function intersectWithOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then return end
  if t == other then return end
  local comparer = t.comparer
  local array = arrayFromTable({}, t.__genericT__)
  for _, v in each(other) do
    local i = binarySearch(t, 0, n, v, comparer)
    if i >= 0 then
      array[#array + 1] = v == nil and null or v
    end
  end
  clear(t)
  addOrderRange(t, array)
end
local function isSubsetOfOrderWithSameEC(t, n, comparer, t1)
  local n1 = #t1
  for i = 1, n do
    local v = t[i]
    if v == null then v = nil end
    local j = binarySearch(t1, 0, n1, v, comparer)
    if j < 0 then
      return false
    end
  end
  return true
end
local function checkOrderUniqueAndUnfoundElements(t, n, comparer, other, returnIfUnfound)
  if n == 0 then
    local numElementsInOther = 0
    if other:GetEnumerator():MoveNext() then
      numElementsInOther = 1
    end
    return 0, numElementsInOther
  end
  local uniqueFoundCount, unfoundCount, mark = 0, 0, {}
  for _, v in each(other) do
    local i = binarySearch(t, v, comparer)
    if i >= 0 then
      if not mark[i] then
        mark[i] = true
        uniqueFoundCount = uniqueFoundCount + 1
      end
    else
      unfoundCount = unfoundCount + 1
      if returnIfUnfound then
        break
      end
    end
  end
  return uniqueFoundCount, unfoundCount
end
local function isProperSubsetOfOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  if t == other then return false end
  local comparer, n = t.comparer, #t
  if System.is(other, System.SortedSet(t.__genericT__)) then
    if comparer == other.comparer then
      if n >= #other then return false end
      return isSubsetOfOrderWithSameEC(t, n, comparer, other)
    end
  end
  local uniqueCount, unfoundCount = checkOrderUniqueAndUnfoundElements(t, n, comparer, other)
  return uniqueCount == n and unfoundCount > 0
end
local function isProperSupersetOfOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then return false end
  if System.is(other, System.ICollection) then
    if other:getCount() == 0 then
      return true
    end
  end
  local comparer = t.comparer
  if System.is(other, System.SortedSet(t.__genericT__)) then
    if comparer == other.comparer then
      local n1 = #other
      if n1 >= n then return false end
      return isSubsetOfOrderWithSameEC(other, n1, comparer, t)
    end
  end
  local uniqueCount, unfoundCount = checkOrderUniqueAndUnfoundElements(t, n, comparer, other, true)
  return uniqueCount < n and unfoundCount == 0
end
local function isSubsetOfOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then return true end
  local comparer = t.comparer
  if System.is(other, System.SortedSet(t.__genericT__)) then
    if comparer == other.comparer then
      if n > #other then return false end
      return isSubsetOfOrderWithSameEC(t, n, comparer, set)
    end
  end
  local uniqueCount, unfoundCount = checkOrderUniqueAndUnfoundElements(t, n, comparer, other)
  return uniqueCount == n and unfoundCount >= 0
end
local function isSupersetOfOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  if System.is(other, System.ICollection) then
    if other:getCount() == 0 then
      return true
    end
  end
  local comparer, n = t.comparer, #t
  if System.is(other, System.SortedSet(t.__genericT__)) then
    if comparer == other.comparer then
      local n1 = #other
      if n < n1 then return false end
      return isSubsetOfOrderWithSameEC(other, n1, comparer, t)
    end
  end
  for _, v in each(other) do
    local i = binarySearch(t, v, comparer)
    if i < 0 then
      return false
    end
  end
  return true
end
local function isOverlapsOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then return false end
  if System.is(other, System.ICollection) and other:getCount() == 0 then return false end
  local comparer = t.comparer
  if System.is(other, System.SortedSet(t.__genericT__)) and comparer == other.comparer then
    local c = comparer.Compare
    if c(comparer, first(t), last(other)) > 0 or c(comparer, last(t), first(other)) < 0 then
      return false
    end
  end
  for _, v in each(other) do
    local i = binarySearch(t, v, comparer)
    if i >= 0 then
      return true
    end
  end
  return false
end
local function equalsOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local comparer, n = t.comparer, #t
  if System.is(other, System.SortedSet(t.__genericT__)) and comparer == other.comparer then
    local n1 = #other
    if n ~= n1 then return false end
    local c = comparer.Compare
    for i = 1, n do
      local v, v1 = t[i], other[i]
      if v == null then v = nil end
      if v1 == null then v1 = nil end
      if c(comparer, v, v1) ~= 0 then
        return false
      end
    end
    return true
  end
  local uniqueCount, unfoundCount = checkOrderUniqueAndUnfoundElements(t, n, comparer, other, true)
  return uniqueCount == n and unfoundCount == 0
end
local function symmetricExceptWithOrder(t, other)
  if other == nil then throw(ArgumentNullException("other")) end
  local n = #t
  if n == 0 then
    addOrderRange(other)
    return
  end
  if t == other then
    clear(t)
    return
  end
  local comparer = t.comparer
  for _, v in each(other) do
    local i = binarySearch(t, v, comparer)
    if i >= 0 then
      tremove(t, i + 1)
    else
      addOrder(t, v, comparer)
    end
  end
end
local function tryGetValueOrder(t, equalValue)
  local i = binarySearch(t, equalValue, t.comparer)
  if i >= 0 then
    local v = t[i + 1]
    if v == null then v = nil end
    return true, v
  end
  return false, t.__genericT__:default()
end
local function checkOrderDictKeyValueObj(t, k, v)
  if k == nil then throw(ArgumentNullException("key")) end
  local TValue = t.__genericTValue__
  if v == nil and TValue:default() ~= nil then throw(ArgumentNullException("value")) end
  local TKey = t.__genericTKey__
  if not System.is(k, TKey) then
    throw(ArgumentException(er.Arg_WrongType(k, TKey.__name__), "key"))
  end
  if not System.is(v, TValue) then
    throw(ArgumentException(er.Arg_WrongType(v, TValue.__name__), "value"))
  end
end
local function getOrderDictIndex(t, k, isObj)
  if k == nil then throw(ArgumentNullException("key")) end
  if isObj and not System.is(k, t.__genericTKey__) then
    return -1
  end
  return binarySearch(t, k, t.keyComparer)
end
local function addOrderDict(t, k, v, keyComparer, T, version)
  local i = binarySearch(t, k, keyComparer)
  if i >= 0 then
    throw(ArgumentException(er.Argument_AddingDuplicate(k)))
  end
  tinsert(t, bnot(i) + 1, setmetatable({ k, v }, T))
  if version then
    versions[t] = (versions[t] or 0) + 1
  end
end
local function getOrderDict(t, k, isObj)
  local i = getOrderDictIndex(t, k)
  if i >= 0 then
    return t[i + 1][2]
  end
  if isObj then
    return nil
  end
  throw(System.KeyNotFoundException(er.Arg_KeyNotFoundWithKey(k)))
end
local function setOrderDict(t, k, v)
  local i = getOrderDictIndex(t, k)
  if i >= 0 then
    t[i + 1][2] = v
  else
    tinsert(t, bnot(i) + 1, setmetatable({ k, v }, t.__genericT__))
    versions[t] = (versions[t] or 0) + 1
  end
end
local function getViewBetweenOrder(t, lowerValue, upperValue)
  local comparer = t.comparer
  if comparer:Compare(lowerValue, upperValue) > 0 then
    throw(ArgumentException("lowerBound is greater than upperBound"))
  end
  local n = #t
  local i = binarySearch(t, lowerValue, comparer)
  if i < 0 then i = bnot(i) end
  local s = { comparer = comparer }
  if i < n then
    local j = binarySearch(t, upperValue, comparer)
    if j < 0 then j = bnot(j) - 1 end
    if i <= j then
      tmove(t, i + 1, j + 1, 1, s)
    end
  end
  return setmetatable(s, System.SortedSet(t.__genericT__))
end
local function heapDown(t, k, n, c)
  local j
  while true do
    j = k * 2
    if j <= n and j > 0 then
      if j < n and c(t[j], t[j + 1]) > 0 then
        j = j + 1
      end
      if c(t[k], t[j]) <= 0 then
        break
      end
      t[j], t[k] = t[k], t[j]
      k = j
    else
      break
    end
  end
end
local function heapUp(t, k, n, c)
  while k > 1 do
    local j = div(k, 2)
    if c(t[j], t[k]) <= 0 then
      break
    end
    t[j], t[k] = t[k], t[j]
    k = j
  end
end
local function heapify(t, c)
  local n = #t
  for i = div(n, 2), 1, -1 do
    heapDown(t, i, n, c)
  end
end
local function heapAdd(t, v, c)
  local n = #t + 1
  t[n] = v
  heapUp(t, n, n, c)
end
local function heapPop(t, c)
  local n = #t
  if n == 0 then return end
  local v = t[1]
  t[1] = t[n]
  t[n] = nil
  heapDown(t, 1, n - 1, c)
  return v
end
local SortedSetEqualityComparerFn
local SortedSetEqualityComparer = {
  __ctor__ = function (this, equalityComparer, comparer)
    local T = this.__genericT__
    this.comparer = comparer or Comparer_1(T).getDefault()
    this.equalityComparer = equalityComparer or EqualityComparer(T).getDefault()
  end,
  EqualsOf = function (_, x, y)
    if x == nil then return y == nil end
    if y == nil then return false end
    return equalsOrder(x, y)
  end,
  GetHashCodeOf = function (this, t)
    local hashCode = 0
    if t ~= nil then
      for i = 1, #t do
        local v = t[i]
        if v == null then v = nil end
        hashCode = System.xor(hashCode, System.band(this.equalityComparer:GetHashCodeOf(v), 0x7FFFFFFF))
      end
    end
    return hashCode
  end,
  EqualsObj = function (this, obj)
    if System.is(obj, SortedSetEqualityComparerFn(this.__genericT__)) then
      return this.comparer == obj.comparer
    end
    return false
  end,
  GetHashCode = function (this)
    return System.xor(this.comparer:GetHashCode(), this.equalityComparer:GetHashCode())
  end
}
SortedSetEqualityComparerFn = define("System.SortedSetEqualityComparer", function (T)
  return {
    base = { System.IEqualityComparer_1(T) },
    __genericT__ = T
  }
end, SortedSetEqualityComparer, 1)
local ArrayEnumerator = define("System.ArrayEnumerator", function (T)
  return {
    base = { IEnumerator_1(T) }
  }
end, {
  getCurrent = System.getCurrent,
  Dispose = System.emptyFn,
  Reset = function (this)
    this.index = 1
    this.current = nil
  end,
  MoveNext = function (this)
    local t = this.list
    if this.version ~= versions[t] then
      throwFailedVersion()
    end
    local index = this.index
    local v = t[index]
    if v ~= nil then
      if v == null then
        this.current = nil
      else
        this.current = v
      end
      this.index = index + 1
      return true
    end
    this.current = nil
    return false
  end
}, 1)
arrayEnumerator = function (t, T)
  if not T then T = t.__genericT__ end
  return setmetatable({ list = t, index = 1, version = versions[t], currnet = T:default() }, ArrayEnumerator(T))
end
local ArrayReverseEnumerable
local function reverseEnumerator(t)
  local T = t.__genericT__
  return setmetatable({ list = t, index = #t, version = versions[t], currnet = T:default() }, ArrayReverseEnumerable(T))
end
ArrayReverseEnumerable = define("System.ArrayReverseEnumerable", function (T)
  return {
    base = { IEnumerable_1(T), IEnumerator_1(T) }
  }
end, {
  getCurrent = System.getCurrent,
  Dispose = System.emptyFn,
  GetEnumerator = function (this)
    return reverseEnumerator(this.list)
  end,
  Reset = function (this)
    this.index = #this.list
    this.current = nil
  end,
  MoveNext = function (this)
    local t = this.list
    if this.version ~= versions[t] then
      throwFailedVersion()
    end
    local index = this.index
    local v = t[index]
    if v ~= nil then
      if v == null then
        this.current = nil
      else
        this.current = v
      end
      this.index = index - 1
      return true
    end
    this.current = nil
    return false
  end
}, 1)
local function reverseEnumerable(t)
  return setmetatable({ list = t }, ArrayReverseEnumerable(t.__genericT__))
end
local function checkArrayIndex(index1, index2)
  if index2 then
    throw(ArgumentException("Indices length does not match the array rank."))
  elseif type(index1) == "table" then
    if #index1 ~= 1 then
      throw(ArgumentException("Indices length does not match the array rank."))
    else
      index1 = index1[1]
    end
  end
  return index1
end
Array = {
  version = 0,
  __call = buildArray,
  set = set,
  get = get,
  setCapacity = function (t, len)
    if len < #t then throw(ArgumentOutOfRangeException("Value", er.ArgumentOutOfRange_SmallCapacity())) end
  end,
  ctorList = function (t, ...)
    local n = select("#", ...)
    if n == 0 then return end
    local collection = ...
    if type(collection) == "number" then return end
    addRange(t, collection)
  end,
  ctorOrderSet = function(t, ...)
    local n = select("#", ...)
    if n == 0 then
      t.comparer = Comparer_1(t.__genericT__).getDefault()
    else
      local collection, comparer = ...
      if collection == nil then throw(ArgumentNullException("collection")) end
      t.comparer = comparer or Comparer_1(t.__genericT__).getDefault()
      if collection then
        addOrderRange(t, collection)
      end
    end
  end,
  ctorOrderDict = function (t, ...)
    local n = select("#", ...)
    local dictionary, comparer
    if n ~= 0 then
      dictionary, comparer = ...
      if dictionary == nil then throw(ArgumentNullException("dictionary")) end
    end
    if comparer == nil then comparer = Comparer_1(t.__genericT__).getDefault() end
    local c = comparer.Compare
    local keyComparer = function (_, p, v)
      return c(comparer, p[1], v)
    end
    t.comparer, t.keyComparer = comparer, keyComparer
    if type(dictionary) == "table" then
      local T = t.__genericT__
      for _, p in each(dictionary) do
        local k, v = p[1], p[2]
        addOrderDict(t, k, v, keyComparer, T)
      end
    end
  end,
  add = add,
  addObj = function (this, item)
    if not System.is(item, this.__genericT__) then
      throw(ArgumentException())
    end
    return add(this, item)
  end,
  addRange = addRange,
  addOrder = function (t, v)
    local success = addOrder(t, v, t.comparer)
    if success then
      versions[t] = (versions[t] or 0) + 1
    end
    return success
  end,
  addOrderRange = addOrderRange,
  addOrderDict = function (t, k, v)
    if k == nil then throw(ArgumentNullException("key")) end
    addOrderDict(t, k, v, t.keyComparer, t.__genericT__, true)
  end,
  addPairOrderDict = function (t, ...)
    local k, v
    if select("#", ...) == 1 then
      local pair = ...
      k, v = pair[1], pair[2]
    else
      k, v = ...
    end
    if k == nil then throw(ArgumentNullException("key")) end
    addOrderDict(t, k ,v, t.keyComparer, t.__genericT__, true)
  end,
  addOrderDictObj = function (t, k, v)
    checkOrderDictKeyValueObj(t, k, v)
    addOrderDict(t, k ,v, t.keyComparer, t.__genericT__, true)
  end,
  AsReadOnly = function (t)
    return System.ReadOnlyCollection(t.__genericT__)(t)
  end,
  clear = clear,
  containsOrder = function (t, v)
    return binarySearch(t, v, t.comparer) >= 0
  end,
  containsOrderDict = function (t, k)
    return getOrderDictIndex(t, k) >= 0
  end,
  containsOrderDictObj = function (t, k)
    return getOrderDictIndex(t, k, true) >= 0
  end,
  createSetComparer = function (T, equalityComparer)
    return SortedSetEqualityComparerFn(T)(equalityComparer)
  end,
  exceptWithOrder = exceptWithOrder,
  findAll = function (t, match)
    return setmetatable(findAll(t, match), System.List(t.__genericT__))
  end,
  first = first,
  firstOrDefault = function (t)
    local v = t[1]
    if v == nil then
      return t.__genericT__:default()
    elseif v == null then
      return nil
    else
      return v
    end
  end,
  insert = function (t, index, v)
    if index < 0 or index > #t then
      throw(ArgumentOutOfRangeException("index"))
    end
    tinsert(t, index + 1, v == nil and null or v)
    versions[t] = (versions[t] or 0) + 1
  end,
  insertRange = function (t, index, collection)
    if collection == nil then throw(ArgumentNullException("collection")) end
    local len = #t
    if index < 0 or index > len then
      throw(ArgumentOutOfRangeException("index"))
    end
    if t.GetEnumerator == arrayEnumerator then
      local count = #collection
      if count > 0 then
        if index < len then
          tmove(t, index + 1, len, index + 1 + count, t)
        end
        if t == collection then
          tmove(t, 1, index, index + 1, t)
          tmove(t, index + 1 + count, count * 2, index * 2 + 1, t)
        else
          tmove(collection, 1, count, index + 1, t)
        end
      end
    else
      for _, v in each(collection) do
        index = index + 1
        tinsert(t, index, v == nil and null or v)
      end
    end
    versions[t] = (versions[t] or 0) + 1
  end,
  getViewBetweenOrder = getViewBetweenOrder,
  getOrderComparer = function (t) return t.comparer end,
  intersectWithOrder = intersectWithOrder,
  isProperSubsetOfOrder = isProperSubsetOfOrder,
  isProperSupersetOfOrder = isProperSupersetOfOrder,
  isSubsetOfOrder = isSubsetOfOrder,
  isSupersetOfOrder = isSupersetOfOrder,
  isOverlapsOrder = isOverlapsOrder,
  equalsOrder = equalsOrder,
  symmetricExceptWithOrder = symmetricExceptWithOrder,
  heapDown = heapDown,
  heapUp = heapUp,
  heapify = heapify,
  heapAdd = heapAdd,
  heapPop = heapPop,
  last = last,
  lastOrDefault = function (t)
    local n = #t
    local v = t[n]
    if v == nil then
      return t.__genericT__:default()
    elseif v == null then
      return nil
    else
      return v
    end
  end,
  popFirst = function (t)
    if #t == 0 then throw(InvalidOperationException()) end
    local v = t[1]
    tremove(t, 1)
    versions[t] = (versions[t] or 0) + 1
    if v ~= null then
      return v
    end
    return nil
  end,
  popLast = function (t)
    local n = #t
    if n == 0 then throw(InvalidOperationException()) end
    local v = t[n]
    t[n] = nil
    if v ~= null then
      return v
    end
    return nil
  end,
  removeRange = removeRange,
  remove = function (t, v)
    local index = indexOf(t, v)
    if index >= 0 then
      tremove(t, index + 1)
      versions[t] = (versions[t] or 0) + 1
      return true
    end
    return false
  end,
  removeAll = function (t, match)
    if match == nil then throw(ArgumentNullException("match")) end
    local size = #t
    local freeIndex = 1
    while freeIndex <= size do
      local item = t[freeIndex]
      if item == null then  item = nil end
      if match(item) then
        break
      end
      freeIndex = freeIndex + 1
    end
    if freeIndex > size then return 0 end
    local current = freeIndex + 1
    while current <= size do
      while current <= size do
        local item = t[current]
        if item == null then item = nil end
        if not match(item) then
          break
        end
        current = current + 1
      end
      if current <= size then
        t[freeIndex] = t[current]
        freeIndex = freeIndex + 1
        current = current + 1
      end
    end
    freeIndex = freeIndex -1
    local count = size - freeIndex
    removeRange(t, freeIndex, count)
    return count
  end,
  removeAt = function (t, index)
    index = index + 1
    if t[index] == nil then throw(ArgumentOutOfRangeException("index"))  end
    tremove(t, index)
    versions[t] = (versions[t] or 0) + 1
  end,
  removeOrder = function (t, v)
    local i = binarySearch(t, v, t.comparer)
    if i >= 0 then
      tremove(t, i + 1)
      versions[t] = (versions[t] or 0) + 1
      return true
    end
    return false
  end,
  removeOrderDict = function (t, k)
    local i = getOrderDictIndex(t, k)
    if i >= 0 then
      tremove(t, i + 1)
      versions[t] = (versions[t] or 0) + 1
      return true
    end
    return false
  end,
  removePairOrderDict = function (t, p)
    local i = getOrderDictIndex(t, p[1])
    if i >= 0 then
      local v = t[i + 1][2]
      local comparer = EqualityComparer(t.__genericTValue__).getDefault()
      if comparer:EqualsOf(p[2], v) then
        tremove(t, i)
        return true
      end
    end
    return false
  end,
  tryGetValueOrder = tryGetValueOrder,
  tryGetValueOrderDict = function (t, k)
    local i = getOrderDictIndex(t, k)
    if i >= 0 then
      local p = t[i + 1]
      return true, p[2]
    end
    return false, t.__genericTValue__:default()
  end,
  getRange = function (t, index, count)
    if count < 0 or index > #t - count then
      throw(ArgumentOutOfRangeException("index or count"))
    end
    local list = {}
    tmove(t, index + 1, index + count, 1, list)
    return setmetatable(list, System.List(t.__genericT__))
  end,
  getOrderDict = getOrderDict,
  getOrderDictObj = function (t, k)
    return getOrderDict(t, k, true)
  end,
  setOrderDict = setOrderDict,
  setOrderDictObj = function (t, k, v)
    checkOrderDictKeyValueObj(t, k, v)
    setOrderDict(t, k, v)
  end,
  indexKeyOrderDict = function (t, k)
    local i = getOrderDictIndex(t, k)
    if i < 0 then i = -1 end
    return i
  end,
  indexOfValue = function (t, v)
    local len = #t
    if len > 0 then
      local comparer = EqualityComparer(t.__genericTValue__).getDefault()
      local equals = comparer.EqualsOf
      for i = 1, len do
        if equals(comparer, v, t[i][2]) then
          return i - 1
        end
      end
    end
    return -1
  end,
  reverseEnumerator = reverseEnumerator,
  reverseEnumerable = reverseEnumerable,
  getCount = lengthFn,
  getSyncRoot = System.identityFn,
  getLongLength = lengthFn,
  getLength = lengthFn,
  getIsSynchronized = falseFn,
  getIsReadOnly = falseFn,
  getIsFixedSize = trueFn,
  getRank = System.oneFn,
  Add = unset,
  Clear = unset,
  Insert = unset,
  Remove = unset,
  RemoveAt = unset,
  BinarySearch = binarySearch,
  ClearArray = function (t, index, length)
    if t == nil then throw(ArgumentNullException("array")) end
    if index < 0 or length < 0 or index + length > #t then
      throw(IndexOutOfRangeException())
    end
    local default = t.__genericT__:default()
    if default == nil then default = null end
    fill(t, index + 1, index + length, default)
  end,
  Contains = function (t, v)
    return indexOf(t, v) ~= -1
  end,
  Copy = function (t, ...)
    local len = select("#", ...)
    if len == 2 then
      local array, length = ...
      copy(t, 0, array, 0, length)
    else
      copy(t, ...)
    end
  end,
  CreateInstance = function (elementType, length)
    return Array(elementType[1])(length)
  end,
  Empty = function (T)
    local t = emptys[T]
    if t == nil then
      t = Array(T){}
      emptys[T] = t
    end
    return t
  end,
  Exists = function (t, match)
    return findIndex(t, match) ~= -1
  end,
  Fill = function (t, value, startIndex, count)
    if t == nil then throw(ArgumentNullException("array")) end
    local len = #t
    if not startIndex then
      startIndex, count = 0, len
    else
      if startIndex < 0 or startIndex > len then
        throw(ArgumentOutOfRangeException("startIndex"))
      end
      if count < 0 or count > len - startIndex then
        throw(ArgumentOutOfRangeException("count"))
      end
    end
    fill(t, startIndex + 1, startIndex + count, value)
  end,
  Find = function (t, match)
    if t == nil then throw(ArgumentNullException("array")) end
    if match == nil then throw(ArgumentNullException("match")) end
    for i = 1, #t do
      local item = t[i]
      if item == null then item = nil end
      if match(item) then
        return item
      end
    end
    return t.__genericT__:default()
  end,
  FindAll = function (t, match)
    return setmetatable(findAll(t, match), Array(t.__genericT__))
  end,
  FindIndex = findIndex,
  FindLast = function (t, match)
    if t == nil then throw(ArgumentNullException("array")) end
    if match == nil then throw(ArgumentNullException("match")) end
    for i = #t, 1, -1 do
      local item = t[i]
      if item == null then item = nil end
      if match(item) then
        return item
      end
    end
    return t.__genericT__:default()
  end,
  FindLastIndex = function (t, startIndex, count, match)
    if t == nil then throw(ArgumentNullException("array")) end
    local len = #t
    if not count then
      startIndex, count, match = len - 1, len, startIndex
    elseif not match then
      count, match = startIndex + 1, count
    end
    if match == nil then throw(ArgumentNullException("match")) end
    if count < 0 or startIndex - count + 1 < 0 then
      throw(ArgumentOutOfRangeException("count"))
    end
    local endIndex = startIndex - count + 1
    for i = startIndex + 1, endIndex + 1, -1 do
      local item = t[i]
      if item == null then
        item = nil
      end
      if match(item) then
        return i - 1
      end
    end
    return -1
  end,
  ForEach = function (t, action)
    if action == nil then throw(ArgumentNullException("action")) end
    for i = 1, #t do
      local item = t[i]
      if item == null then item = nil end
      action(item)
    end
  end,
  IndexOf = indexOf,
  LastIndexOf = function (t, value, startIndex, count)
    if t == nil then throw(ArgumentNullException("array")) end
    local len = #t
    if not startIndex then
      startIndex, count = len - 1, len
    elseif not count then
      count = len == 0 and 0 or (startIndex + 1)
    end
    if len == 0 then
      if startIndex ~= -1 and startIndex ~= 0 then
        throw(ArgumentOutOfRangeException("startIndex"))
      end
      if count ~= 0 then
        throw(ArgumentOutOfRangeException("count"))
      end
    end
    if startIndex < 0 or startIndex >= len then
      throw(ArgumentOutOfRangeException("startIndex"))
    end
    if count < 0 or startIndex - count + 1 < 0 then
      throw(ArgumentOutOfRangeException("count"))
    end
    local comparer = EqualityComparer(t.__genericT__).getDefault()
    local equals = comparer.EqualsOf
    local endIndex = startIndex - count + 1
    for i = startIndex + 1, endIndex + 1, -1 do
      local item = t[i]
      if item == null then item = nil end
      if equals(comparer, item, value) then
        return i - 1
      end
    end
    return -1
  end,
  Resize = function (t, newSize, T)
    if newSize < 0 then throw(ArgumentOutOfRangeException("newSize")) end
    if t == nil then
      return Array(T)(newSize)
    end
    local len = #t
    if len > newSize then
      fill(t, newSize + 1, len, nil)
    elseif len < newSize then
      local default = t.__genericT__:default()
      if default == nil then default = null end
      fill(t, len + 1, newSize, default)
    end
    return t
  end,
  Reverse = function (t, index, count)
    if not index then
      index = 0
      count = #t
    else
      if count < 0 or index > #t - count then
        throw(ArgumentOutOfRangeException("index or count"))
      end
    end
    local i, j = index + 1, index + count
    while i < j do
      t[i], t[j] = t[j], t[i]
      i = i + 1
      j = j - 1
    end
    versions[t] = (versions[t] or 0) + 1
  end,
  Sort = function (t, ...)
    if t == nil then throw(ArgumentNullException("array")) end
    local len = select("#", ...)
    if len == 0 then
      sort(t)
    elseif len == 1 then
      local comparer = ...
      sort(t, comparer)
    else
      local index, count, comparer = ...
      if count > 1 then
        local comp = getSortComp(t, comparer)
        if index == 0 and count == #t then
          tsort(t, comp)
        else
          checkIndexAndCount(t, index, count)
          local arr = {}
          tmove(t, index + 1, index + count, 1, arr)
          tsort(arr, comp)
          tmove(arr, 1, count, index + 1, t)
        end
        versions[t] = (versions[t] or 0) + 1
      end
    end
  end,
  toArray = function (t)
    local array = {}
    if t.GetEnumerator == arrayEnumerator then
      tmove(t, 1, #t, 1, array)
    else
      local count = 1
      for _, v in each(t) do
        array[count] = v == nil and null or v
        count = count + 1
      end
    end
    return arrayFromTable(array, t.__genericT__)
  end,
  TrueForAll = function (t, match)
    if t == nil then throw(ArgumentNullException("array")) end
    if match == nil then throw(ArgumentNullException("match")) end
    for i = 1, #t do
      local item = t[i]
      if item == null then item = nil end
      if not match(item) then
        return false
      end
    end
    return true
  end,
  Clone = function (this)
    local t = setmetatable({}, getmetatable(this))
    tmove(this, 1, #this, 1, t)
    return t
  end,
  CopyTo = function (t, ...)
    local n = select("#", ...)
    local sourceIndex, array, arrayIndex, len
    if n == 1 then
      sourceIndex, arrayIndex, len, array = 0, 0, #t, ...
      checkIndexAndCount(array, 0, len)
    elseif n == 2 then
      sourceIndex, len, array, arrayIndex = 0, #t, ...
      checkIndexAndCount(array, arrayIndex, len)
    elseif n == 3 then
      sourceIndex, array, arrayIndex, len = 0, ...
      checkIndexAndCount(t, sourceIndex, len)
      checkIndexAndCount(array, arrayIndex, len)
    else
      sourceIndex, array, arrayIndex, len = ...
      checkIndexAndCount(t, sourceIndex, len)
      checkIndexAndCount(array, arrayIndex, len)
    end
    local T = t.__genericT__
    if T.class == "S" then
      local default = T:default()
      if type(default) == "table" then
        for i = 1, len do
          array[arrayIndex + i] = t[sourceIndex + i]:__clone__()
        end
        return
      end
    end
    tmove(t, sourceIndex + 1, sourceIndex + len, arrayIndex + 1, array)
  end,
  GetEnumerator = arrayEnumerator,
  GetLength = function (this, dimension)
    if dimension ~= 0 then throw(IndexOutOfRangeException()) end
    return #this
  end,
  GetLowerBound = function (_, dimension)
    if dimension ~= 0 then throw(IndexOutOfRangeException()) end
    return 0
  end,
  GetUpperBound = function (this, dimension)
    if dimension ~= 0 then throw(IndexOutOfRangeException()) end
    return #this - 1
  end,
  GetValue = function (this, index1, index2)
    if index1 == nil then throw(ArgumentNullException("indices")) end
    return get(this, checkArrayIndex(index1, index2))
  end,
  SetValue = function (this, value, index1, index2)
    if index1 == nil then throw(ArgumentNullException("indices")) end
    set(this, checkArrayIndex(index1, index2), System.castWithNullable(this.__genericT__, value))
  end
}
function System.arrayFromList(t)
  return setmetatable(t, Array(t.__genericT__))
end
arrayFromTable = function (t, T, readOnly)
  assert(T)
  local array = setmetatable(t, Array(T))
  if readOnly then
    array.set = unset
  end
  return array
end
System.arrayFromTable = arrayFromTable
local function getIndex(t, ...)
  local rank = t.__rank__
  local id = 0
  local len = #rank
  for i = 1, len do
    id = id * rank[i] + select(i, ...)
  end
  return id, len
end
local function checkMultiArrayIndex(t, index1, ...)
  if index1 == nil then throw(ArgumentNullException("indices")) end
  local rank = t.__rank__
  local len = #rank
  if type(index1) == "table" then
    if #index1 ~= len then
      throw(ArgumentException("Indices length does not match the array rank."))
    end
    local id = 0
    for i = 1, len do
      id = id * rank[i] + index1[i]
    end
    return id
  elseif len ~= select("#", ...) + 1 then
    throw(ArgumentException("Indices length does not match the array rank."))
  end
  return getIndex(t, index1, ...)
end
local MultiArray = {
  set = function (this, ...)
    local index, len = getIndex(this, ...)
    set(this, index, select(len + 1, ...))
  end,
  get = function (this, ...)
    local index = getIndex(this, ...)
    return get(this, index)
  end,
  getRank = function (this)
    return #this.__rank__
  end,
  GetLength = function (this, dimension)
    local rank = this.__rank__
    if dimension < 0 or dimension >= #rank then throw(IndexOutOfRangeException()) end
    return rank[dimension + 1]
  end,
  GetLowerBound = function (this, dimension)
    local rank = this.__rank__
    if dimension < 0 or dimension >= #rank then throw(IndexOutOfRangeException()) end
    return 0
  end,
  GetUpperBound = function (this, dimension)
    local rank = this.__rank__
    if dimension < 0 or dimension >= #rank then throw(IndexOutOfRangeException()) end
    return rank[dimension + 1] - 1
  end,
  GetValue = function (this, ...)
    return get(this, checkMultiArrayIndex(this, ...))
  end,
  SetValue = function (this, value, ...)
    set(this, checkMultiArrayIndex(this, ...), System.castWithNullable(this.__genericT__, value))
  end,
  Clone = function (this)
    local array = { __rank__ = this.__rank__ }
    tmove(this, 1, #this, 1, array)
    return setmetatable(array, Array(this.__genericT__, #this.__rank__))
  end
}
function MultiArray.__call(T, rank, t)
  local len = 1
  for i = 1, #rank do
    len = len * rank[i]
  end
  t = buildArray(T, len, t)
  t.__rank__ = rank
  return t
end
System.defArray("System.Array", function(T)
  return {
    base = { System.ICloneable, System.IList_1(T), System.IReadOnlyList_1(T), System.IList },
    __genericT__ = T
  }
end, Array, MultiArray)
local cpool = {}
local function createCoroutine(f)
  local c = tremove(cpool)
  if c == nil then
    c = ccreate(function (...)
      f(...)
      while true do
        f = nil
        cpool[#cpool + 1] = c
        f = cyield(cpool)
        f(cyield())
      end
    end)
  else
    cresume(c, f)
  end
  return c
end
System.ccreate = createCoroutine
System.cpool = cpool
System.cresume = cresume
System.yield = cyield
local YieldEnumerable
YieldEnumerable = define("System.YieldEnumerable", function (T)
  return {
    base = { IEnumerable_1(T), IEnumerator_1(T), System.IDisposable },
    __genericT__ = T
  }
end, {
  getCurrent = System.getCurrent,
  Dispose = System.emptyFn,
  GetEnumerator = function (this)
    return setmetatable({ f = this.f, args = this.args }, YieldEnumerable(this.__genericT__))
  end,
  MoveNext = function (this)
    local c = this.c
    if c == false then
      return false
    end
    local ok, v
    if c == nil then
      c = createCoroutine(this.f)
      this.c = c
      local args = this.args
      ok, v = cresume(c, unpack(args, 1, args.n))
      this.args = nil
    else
      ok, v = cresume(c)
    end
    if ok then
      if v == cpool then
        this.c = false
        this.current = nil
        return false
      else
        this.current = v
        return true
      end
    else
      error(v)
    end
  end
}, 1)
local function yieldIEnumerable(f, T, ...)
  return setmetatable({ f = f, args = pack(...) }, YieldEnumerable(T))
end
System.yieldIEnumerable = yieldIEnumerable
System.yieldIEnumerator = yieldIEnumerable
local ReadOnlyCollection = {
  __ctor__ = function (this, list)
    if not list then throw(ArgumentNullException("list")) end
    this.list = list
  end,
  getCount = function (this)
    return #this.list
  end,
  get = function (this, index)
    return this.list:get(index)
  end,
  Contains = function (this, value)
    return this.list:Contains(value)
  end,
  GetEnumerator = function (this)
    return this.list:GetEnumerator()
  end,
  CopyTo = function (this, array, index)
    this.list:CopyTo(array, index)
  end,
  IndexOf = function (this, value)
    return this.list:IndexOf(value)
  end,
  getIsSynchronized = falseFn,
  getIsReadOnly = trueFn,
  getIsFixedSize = trueFn,
}
define("System.ReadOnlyCollection", function (T)
  return {
    base = { System.IList_1(T), System.IList, System.IReadOnlyList_1(T) },
    __genericT__ = T
  }
end, ReadOnlyCollection, 1)

end

-- CoreSystemLib: Span.lua
do
local System = System
local throw = System.throw
local ArgumentOutOfRangeException = System.ArgumentOutOfRangeException
local IndexOutOfRangeException = System.IndexOutOfRangeException
local Span = {
  __ctor__ = function (this, input, ...)
    local t = type(input)
    if t == "table" then
      local argsLen = select("#", ...)
      local maxLength = input:getLength()
      local start, length
      if argsLen == 2 then
        start, length = ...
        if start >= maxLength then
          throw(ArgumentOutOfRangeException("start"))
        end
        if start + length > maxLength then
          throw(ArgumentOutOfRangeException("length"))
        end
      else
        start, length = 0, maxLength
      end
      this._array = input
      this._min = start
      this._max = start + length - 1
  elseif t == "number" then
    this._array = System.Array(this.__genericT__)(input)
    this._min = 0
    this._max = input - 1
    else
      this._array = System.Array(this.__genericT__)(1)
      this._array:set(0, input)
      this._min = 0
      this._max = 0
    end
  end,
  get = function (this, index)
    local i = this._min + index
    if i > this._max then
      throw(IndexOutOfRangeException("index"))
    end
    return this._array:get(i)
  end,
  set = function (this, index, value)
    local i = this._min + index
    if i > this._max then
      throw(IndexOutOfRangeException("index"))
    end
    return this._array:set(i, value)
  end,
  getIsEmpty = function (this)
    return this:getLength() <= 0
  end,
  getLength = function (this)
    return this._max - this._min + 1
  end,
  Slice = function (this, start, ...)
    local newMin = this._min + start
    if newMin > this._max then
      throw(ArgumentOutOfRangeException("start"))
    end
    local newMax
    local argsLen = select("#", ...)
    if argsLen == 1 then
      length = ...
      newMax = newMin + length - 1
      if newMax > this._max then
        throw(ArgumentOutOfRangeException("length"))
      end
    else
      newMax = this._max
    end
    local ctor = System.Span(this.__genericT__)
    return ctor(this._array, newMin, newMax - newMin + 1)
  end,
  ctorArray = function (array)
    local ctor = System.Span(array.__genericT__)
    return ctor(array)
  end
}
local SpanFn = System.defStc("System.Span", function (T)
  return {
    __genericT__ = T
  }
end, Span, 1)
System.Span = SpanFn

end

-- CoreSystemLib: MemoryExtensions.lua
do
local System = System
local Span = System.Span
local Array = System.Array
System.MemoryExtensions = {
  AsSpan = function (array)
    local SpanT = Span(array.__genericT__)
    return SpanT(array)
  end,
  AsBoundedSpan = function (array, start, length)
    local SpanT = Span(array.__genericT__)
    return SpanT(array, start, length)
  end,
  Contains = function (span, value)
    return Array.Contains(span._array, value)
  end
}

end

-- CoreSystemLib: ReadOnlySpan.lua
do
local System = System
local throw = System.throw
local ArgumentOutOfRangeException = System.ArgumentOutOfRangeException
local IndexOutOfRangeException = System.IndexOutOfRangeException
local ReadOnlySpan = {
  __ctor__ = function (this, input, ...)
    if type(input) == "table" then
      local argsLen = select("#", ...)
      local maxLength = input:getLength()
      local start, length
      if argsLen == 2 then
        start, length = ...
        if start >= maxLength then
          throw(ArgumentOutOfRangeException("start"))
        end
        if start + length > maxLength then
          throw(ArgumentOutOfRangeException("length"))
        end
      else
        start, length = 0, maxLength
      end
      this._array = input
      this._min = start
      this._max = start + length - 1
    else
      this._array = System.Array(this.__genericT__)(1)
      this._array:set(0, input)
      this._min = 0
      this._max = 0
    end
  end,
  get = function (this, index)
    local i = this._min + index
    if i > this._max then
      throw(IndexOutOfRangeException("index"))
    end
    return this._array:get(i)
  end,
  getIsEmpty = function (this)
    return this:getLength() <= 0
  end,
  getLength = function (this)
    return this._max - this._min + 1
  end,
  Slice = function (this, start, ...)
    local newMin = this._min + start
    if newMin > this._max then
      throw(ArgumentOutOfRangeException("start"))
    end
    local newMax
    local argsLen = select("#", ...)
    if argsLen == 1 then
      length = ...
      newMax = newMin + length - 1
      if newMax > this._max then
        throw(ArgumentOutOfRangeException("length"))
      end
    else
      newMax = this._max
    end
    local ctor = System.ReadOnlySpan(this.__genericT__)
    return ctor(this._array, newMin, newMax - newMin + 1)
  end,
  ctorArray = function (array)
    local ctor = System.ReadOnlySpan(array.__genericT__)
    return ctor(array)
  end
}
local ReadOnlySpanFn = System.defStc("System.ReadOnlySpan", function (T)
  return {
    __genericT__ = T
  }
end, ReadOnlySpan, 1)
System.ReadOnlySpan = ReadOnlySpanFn

end

-- CoreSystemLib: Type.lua
do
local System = System
local throw = System.throw
local Object = System.Object
local Boolean = System.Boolean
local Delegate = System.Delegate
local getClass = System.getClass
local getGenericClass = System.getGenericClass
local arrayFromTable = System.arrayFromTable
local InvalidCastException = System.InvalidCastException
local ArgumentNullException = System.ArgumentNullException
local MissingMethodException = System.MissingMethodException
local TypeLoadException = System.TypeLoadException
local NullReferenceException = System.NullReferenceException
local Char = System.Char
local SByte = System.SByte
local Byte = System.Byte
local Int16 = System.Int16
local UInt16 = System.UInt16
local Int32 = System.Int32
local UInt32 = System.UInt32
local Int64 = System.Int64
local UInt64 = System.UInt64
local Single = System.Single
local Double = System.Double
local Int = System.Int
local Number = System.Number
local ValueType = System.ValueType
local assert = assert
local type = type
local setmetatable = setmetatable
local getmetatable = getmetatable
local select = select
local unpack = table.unpack
local floor = math.floor
local Type, typeof
local function isGenericName(name)
  return name:find('`') ~= nil
end
local function getBaseType(this)
  local baseType = this.baseType
  if baseType == nil then
    local baseCls = getmetatable(this[1])
    if baseCls ~= nil then
      baseType = typeof(baseCls)
      this.baseType = baseType
    end
  end
  return baseType
end
local function isSubclassOf(this, c)
  local p = this
  if p == c then
    return false
  end
  while p ~= nil do
    if p == c then
      return true
    end
    p = getmetatable(p)
  end
  return false
end
local function getIsInterface(this)
  return this[1].class == "I"
end
local function fillInterfaces(t, cls, set)
  local base = getmetatable(cls)
  if base then
    fillInterfaces(t, base, set)
  end
  local interface = cls.interface
  if interface then
    for i = 1, #interface do
      local it = interface[i]
      if not set[it] then
        t[#t + 1] = typeof(it)
        set[it] = true
      end
      fillInterfaces(t, it, set)
    end
  end
end
local function getInterfaces(this)
  local t = this.interfaces
  if t == nil then
    t = arrayFromTable({}, Type, true)
    fillInterfaces(t, this[1], {})
    this.interfaces = t
  end
  return t
end
local function implementInterface(this, ifaceType)
  local t = this
  while t ~= nil do
    local interfaces = getInterfaces(this)
    if interfaces ~= nil then
      for i = 1, #interfaces do
        local it = interfaces[i]
        if it == ifaceType or implementInterface(it, ifaceType) then
          return true
        end
      end
    end
    t = getBaseType(t)
  end
  return false
end
local function isAssignableFrom(this, c)
  if c == nil then
    return false
  end
  if this == c then
    return true
  end
  local left, right = this[1], c[1]
  if left == Object then
    return true
  end
  if isSubclassOf(right, left) then
    return true
  end
  if left.class == "I" then
    return implementInterface(c, this)
  end
  return false
end
local function isGenericTypeDefinition(this)
  local cls = this[1]
  return getGenericClass(cls) == cls
end
local function getIsArray(this)
  return this[1].__name__:byte(-2) == 91
end
Type = System.define("System.Type", {
  Equals = System.equals,
  getIsGenericType = function (this)
    return isGenericName(this[1].__name__)
  end,
  getContainsGenericParameters = function (this)
    return isGenericName(this[1].__name__)
  end,
  getIsGenericTypeDefinition = isGenericTypeDefinition,
  GetGenericTypeDefinition = function (this)
    local genericClass = getGenericClass(this[1])
    if genericClass then
      return typeof(genericClass)
    end
    throw(System.InvalidOperationException())
  end,
  MakeGenericType = function (this, ...)
    local T, n, args = this[1], select("#", ...)
    if n == 0 then
      return typeof(T())
    end
    if n == 1 then
      args = ...
      if System.isArrayLike(args) then
        n = #args
        if n == 0 then
          return typeof(T())
        end
        if n == 1 then
          return typeof(T(args[1][1]))
        end
      else
         return typeof(T(args[1]))
      end
    else
      args = { ... }
    end
    for i = 1, n do
      args[i] = args[i][1]
    end
    return typeof(T(unpack(args)))
  end,
  getIsEnum = function (this)
    return this[1].class == "E"
  end,
  getIsClass = function (this)
    return this[1].class == "C"
  end,
  getIsValueType = function (this)
    return this[1].class == "S"
  end,
  getName = function (this)
    local name = this.name
    if name == nil then
      local clsName = this[1].__name__
      local pattern = isGenericName(clsName) and "^.*()%.(.*)%[.+%]$" or "^.*()%.(.*)$"
      name = clsName:gsub(pattern, "%2")
      this.name = name
    end
    return name
  end,
  getFullName = function (this)
    return this[1].__name__
  end,
  getNamespace = function (this)
    local namespace = this.namespace
    if namespace == nil then
      local clsName = this[1].__name__
      local pattern = isGenericName(clsName) and "^(.*)()%..*%[.+%]$" or "^(.*)()%..*$"
      namespace = clsName:gsub(pattern, "%1")
      this.namespace = namespace
    end
    return namespace
  end,
  getBaseType = function (this)
    local cls = this[1]
    if cls.class ~= "I" and cls ~= Object then
      while true do
        local base = getmetatable(cls)
        if not base then
          break
        end
        if base.__index == base then
          return typeof(base)
        end
        cls = base
      end
    end
    return nil
  end,
  IsSubclassOf = function (this, c)
    return isSubclassOf(this[1], c[1])
  end,
  getIsInterface = getIsInterface,
  GetInterfaces = getInterfaces,
  IsAssignableFrom = isAssignableFrom,
  IsInstanceOfType = function (this, obj)
    if obj == nil then
      return false
    end
    return isAssignableFrom(this, obj:GetType())
  end,
  getIsArray = getIsArray,
  GetElementType = function (this)
    if getIsArray(this) then
      return typeof(this[1].__genericT__)
    end
    return nil
  end,
  ToString = function (this)
    return this[1].__name__
  end,
  GetTypeFrom = function (typeName, throwOnError, ignoreCase)
    if typeName == nil then
      throw(ArgumentNullException("typeName"))
    end
    if #typeName == 0 then
      if throwOnError then
        throw(TypeLoadException("Arg_TypeLoadNullStr"))
      end
      return nil
    end
    assert(not ignoreCase, "ignoreCase is not support")
    local cls = getClass(typeName)
    if cls ~= nil then
      return typeof(cls)
    end
    if throwOnError then
      throw(TypeLoadException(typeName .. ": failed to load."))
    end
    return nil
  end
})
Type.GetEnumValues = System.Enum.GetValues
local NumberType = {
  __index = Type,
  __eq = function (a, b)
    local c1, c2 = a[1], b[1]
    if c1 == c2 then
      return true
    end
    if c1 == Number or c2 == Number then
      return true
    end
    return false
  end
}
local function newNumberType(c)
  return setmetatable({ c }, NumberType)
end
local types = {
  [Char] = newNumberType(Char),
  [SByte] = newNumberType(SByte),
  [Byte] = newNumberType(Byte),
  [Int16] = newNumberType(Int16),
  [UInt16] = newNumberType(UInt16),
  [Int32] = newNumberType(Int32),
  [UInt32] = newNumberType(UInt32),
  [Int64] = newNumberType(Int64),
  [UInt64] = newNumberType(UInt64),
  [Single] = newNumberType(Single),
  [Double] = newNumberType(Double),
  [Int] = newNumberType(Int),
  [Number] = newNumberType(Number),
}
local customTypeof = System.config.customTypeof
function typeof(cls)
  assert(cls)
  local t = types[cls]
  if t == nil then
    if customTypeof then
      t = customTypeof(cls)
      if t then
        types[cls] = t
        return t
      end
    end
    t = setmetatable({ cls }, Type)
    types[cls] = t
  end
  return t
end
local function getType(obj)
  return typeof(getmetatable(obj))
end
System.typeof = typeof
System.Object.GetType = getType
local function addCheckInterface(set, cls)
  local interface = cls.interface
  if interface then
    for i = 1, #interface do
      local it = interface[i]
      set[it] = true
      addCheckInterface(set, it)
    end
  end
end
local function getCheckSet(cls)
  local set = {}
  local p = cls
  repeat
    set[p] = true
    addCheckInterface(set, p)
    p = getmetatable(p)
  until not p
  return set
end
local customTypeCheck = System.config.customTypeCheck
local checks = setmetatable({}, {
  __index = function (checks, cls)
    if customTypeCheck then
      local f, add = customTypeCheck(cls)
      if f then
        if add then
          checks[cls] = f
        end
        return f
      end
    end
    local set = getCheckSet(cls)
    local function check(obj, T)
      return set[T] == true
    end
    checks[cls] = check
    return check
  end
})
checks[Number] = function (obj, T)
  local set = getCheckSet(Number)
  local numbers = {
    [Char] = function (obj) return type(obj) == "number" and obj >= 0 and obj <= 65535 and floor(obj) == obj end,
    [SByte] = function (obj) return type(obj) == "number" and obj >= -128 and obj <= 127 and floor(obj) == obj end,
    [Byte] = function (obj) return type(obj) == "number" and obj >= 0 and obj <= 255 and floor(obj) == obj end,
    [Int16] = function (obj) return type(obj) == "number" and obj >= -32768 and obj <= 32767 and floor(obj) == obj end,
    [UInt16] = function (obj) return type(obj) == "number" and obj >= 0 and obj <= 32767 and floor(obj) == obj end,
    [Int32] = function (obj) return type(obj) == "number" and obj >= -2147483648 and obj <= 2147483647 and floor(obj) == obj end,
    [UInt32] = function (obj) return type(obj) == "number" and obj >= 0 and obj <= 4294967295 and floor(obj) == obj end,
    [Int64] = function (obj) return type(obj) == "number" and obj >= (-9223372036854775807 - 1) and obj <= 9223372036854775807 and floor(obj) == obj end,
    [UInt64] = function (obj) return type(obj) == "number" and obj >= 0 and obj <= 18446744073709551615 and floor(obj) == obj end,
    [Single] = function (obj) return type(obj) == "number" and obj >= -3.40282347E+38 and obj <= 3.40282347E+38 end,
    [Double] = function (obj) return type(obj) == "number" end
  }
  local function check(obj, T)
    local number = numbers[T]
    if number then
      return number(obj)
    end
    return set[T] == true
  end
  checks[Number] = check
  return check(obj, T)
end
local is, getName
if System.debugsetmetatable then
  is = function (obj, T)
    return checks[getmetatable(obj)](obj, T)
  end
  getName = function (obj)
    return obj.__name__
  end
  System.getClassFromObj = getmetatable
else
  local function getClassFromObj(obj)
    local t = type(obj)
    if t == "number" then
      return Number
    elseif t == "boolean" then
      return Boolean
    elseif t == "function" then
      return Delegate
    end
    return getmetatable(obj)
  end
  function System.ObjectGetType(this)
    if this == nil then throw(NullReferenceException()) end
    return typeof(getClassFromObj(this))
  end
  is = function (obj, T)
    local base = getClassFromObj(obj)
    if base then
      return checks[base](obj, T)
    end
    return false
  end
  getName = function (obj)
    return getClassFromObj(obj).__name__
  end
  System.getClassFromObj = getClassFromObj
end
System.is = is
function System.as(obj, cls)
  if obj ~= nil and is(obj, cls) then
    return obj
  end
  return nil
end
local function cast(cls, obj, nullable)
  if obj ~= nil then
    if is(obj, cls) then
      return obj
    end
    throw(InvalidCastException(("Unable to cast object of type '%s' to type '%s'."):format(getName(obj), cls.__name__)), 1)
  else
    if cls.class ~= "S" or nullable then
      return nil
    end
    throw(NullReferenceException(), 1)
  end
end
System.cast = cast
function System.castWithNullable(cls, obj)
  if System.isNullable(cls) then
    return cast(cls.__genericT__, obj, true)
  end
  return cast(cls, obj)
end

end

-- CoreSystemLib: List.lua
do
local System = System
local falseFn = System.falseFn
local lengthFn = System.lengthFn
local Array = System.Array
local List = {
  __ctor__ = Array.ctorList,
  getCapacity = lengthFn,
  setCapacity = Array.setCapacity,
  getCount = lengthFn,
  getIsFixedSize = falseFn,
  getIsReadOnly = falseFn,
  get = Array.get,
  set = Array.set,
  Add = Array.add,
  AddObj = Array.addObj,
  AddRange = Array.addRange,
  AsReadOnly = Array.AsReadOnly,
  BinarySearch = Array.BinarySearch,
  Clear = Array.clear,
  Contains = Array.Contains,
  CopyTo = Array.CopyTo,
  Exists = Array.Exists,
  Find = Array.Find,
  FindAll = Array.findAll,
  FindIndex = Array.FindIndex,
  FindLast = Array.FindLast,
  FindLastIndex = Array.FindLastIndex,
  ForEach = Array.ForEach,
  GetEnumerator = Array.GetEnumerator,
  GetRange = Array.getRange,
  IndexOf = Array.IndexOf,
  Insert = Array.insert,
  InsertRange = Array.insertRange,
  LastIndexOf = Array.LastIndexOf,
  Remove = Array.remove,
  RemoveAll = Array.removeAll,
  RemoveAt = Array.removeAt,
  RemoveRange = Array.removeRange,
  Reverse = Array.Reverse,
  Sort = Array.Sort,
  TrimExcess = System.emptyFn,
  ToArray = Array.toArray,
  TrueForAll = Array.TrueForAll
}
function System.listFromTable(t, T)
  return setmetatable(t, List(T))
end
local ListFn = System.define("System.Collections.Generic.List", function(T)
  return {
    base = { System.IList_1(T), System.IReadOnlyList_1(T), System.IList },
    __genericT__ = T,
  }
end, List, 1)
System.List = ListFn
System.ArrayList = ListFn(System.Object)

end

-- CoreSystemLib: Dictionary.lua
do
local System = _G.System
local define = System.define
local throw = System.throw
local null = System.null
local falseFn = System.falseFn
local each = System.each
local versions = System.versions
local Array = System.Array
local toString = System.toString
local hasHash = System.hasHash
local checkIndexAndCount = System.checkIndexAndCount
local throwFailedVersion = System.throwFailedVersion
local ArgumentNullException = System.ArgumentNullException
local ArgumentException = System.ArgumentException
local KeyNotFoundException = System.KeyNotFoundException
local EqualityComparer = System.EqualityComparer
local pairs = pairs
local next = next
local select = select
local getmetatable = getmetatable
local setmetatable = setmetatable
local tconcat = table.concat
local type = type
local counts = setmetatable({}, { __mode = "k" })
System.counts = counts
local function getCount(this)
  local t = counts[this]
  if t then
    return t[1]
  end
  return 0
end
local function pairsFn(t, i)
  local count =  counts[t]
  if count then
    if count[2] ~= count[3] then
      throwFailedVersion()
    end
  end
  local k, v = next(t, i)
  if v == null then
    return k
  end
  return k, v
end
function System.pairs(t)
  local count = counts[t]
  if count then
    count[3] = count[2]
  end
  return pairsFn, t
end
local KeyValuePairFn
local KeyValuePair = {
  __ctor__ = function (this, ...)
    if select("#", ...) == 0 then
      this[1], this[2] = this.__genericTKey__:default(), this.__genericTValue__:default()
    else
      this[1], this[2] = ...
    end
  end,
  Create = function (key, value, TKey, TValue)
    return setmetatable({ key, value }, KeyValuePairFn(TKey, TValue))
  end,
  Deconstruct = function (this)
    return this[1], this[2]
  end,
  ToString = function (this)
    local t = { "[" }
    local count = 2
    local k, v = this[1], this[2]
    if k ~= nil then
      t[count] = toString(k)
      count = count + 1
    end
    t[count] = ", "
    count = count + 1
    if v ~= nil then
      t[count] = toString(v)
      count = count + 1
    end
    t[count] = "]"
    return tconcat(t)
  end
}
KeyValuePairFn = System.defStc("System.Collections.Generic.KeyValuePair", function(TKey, TValue)
  local cls = {
    __genericTKey__ = TKey,
    __genericTValue__ = TValue,
  }
  return cls
end, KeyValuePair, 2)
System.KeyValuePair = KeyValuePairFn
local function isKeyValuePair(t)
  return getmetatable(getmetatable(t)) == KeyValuePair
end
local DictionaryEnumerator = define("System.Collections.Generic.DictionaryEnumerator", {
  getCurrent = System.getCurrent,
  Dispose = System.emptyFn,
  MoveNext = function (this)
    local t, kind = this.dict, this.kind
    local count = counts[t]
    if this.version ~= (count and count[2] or 0) then
      throwFailedVersion()
    end
    local k, v = next(t, this.index)
    if k ~= nil then
      if kind then
        kind[1] = k
        if v == null then v = nil end
        kind[2] = v
      elseif kind == false then
        if v == null then v = nil end
        this.current = v
      else
        this.current = k
      end
      this.index = k
      return true
    else
      if kind then
        kind[1], kind[2] = kind.__genericTKey__:default(), kind.__genericTValue__:default()
      elseif kind == false then
        this.current = t.__genericTValue__:default()
      else
        this.current = t.__genericTKey__:default()
      end
      return false
    end
  end
})
local function dictionaryEnumerator(t, kind)
  local current
  if not kind then
    local TKey, TValue = t.__genericTKey__, t.__genericTValue__
    kind = setmetatable({ TKey:default(), TValue:default() }, t.__genericT__)
    current = kind
  elseif kind == 1 then
    local TKey = t.__genericTKey__
    current = TKey:default()
    kind = nil
  else
    local TValue = t.__genericTValue__
    current = TValue:default()
    kind = false
  end
  local count = counts[t]
  local en = {
    dict = t,
    version = count and count[2] or 0,
    kind = kind,
    current = current
  }
  return setmetatable(en, DictionaryEnumerator)
end
local DictionaryCollection = define("System.Collections.Generic.DictionaryCollection", function (T)
    return {
      base = { System.ICollection_1(T), System.IReadOnlyCollection_1(T), System.ICollection },
      __genericT__ = T
    }
  end, {
  __ctor__ = function (this, dict, kind)
    this.dict = dict
    this.kind = kind
  end,
  getCount = function (this)
    return getCount(this.dict)
  end,
  Contains = function (this, v)
    if this.kind == 2 then
      return this.dict:ContainsValue(v)
    end
    return this.dict:ContainsKey(v)
  end,
  GetEnumerator = function (this)
    return dictionaryEnumerator(this.dict, this.kind)
  end
}, 1)
local ArrayDictionaryFn
local Dictionary = (function ()
  local function add(this, key, value)
    if key == nil then throw(ArgumentNullException("key")) end
    if this[key] ~= nil then throw(ArgumentException("key already exists")) end
    this[key] = value == nil and null or value
    local t = counts[this]
    if t then
      t[1] = t[1] + 1
      t[2] = t[2] + 1
    else
      counts[this] = { 1, 1 }
    end
  end
  local function remove(this, key)
    if key == nil then throw(ArgumentNullException("key")) end
    if this[key] ~= nil then
      this[key] = nil
      local t = counts[this]
      t[1] = t[1] - 1
      t[2] = t[2] + 1
      return true
    end
    return false
  end
  local function buildFromDictionary(this, dictionary)
    if dictionary == nil then throw(ArgumentNullException("dictionary")) end
    local count = 0
    for k, v in pairs(dictionary) do
      this[k] = v
      count = count + 1
    end
    counts[this] = { count, 0 }
  end
  local function buildHasComparer(this, ...)
    local Dictionary = ArrayDictionaryFn(this.__genericTKey__, this.__genericTValue__)
    setmetatable(this, Dictionary)
    Dictionary.__ctor__(this, ...)
 end
  return {
    getIsFixedSize = falseFn,
    getIsReadOnly = falseFn,
    __ctor__ = function (this, ...)
      local n = select("#", ...)
      if n == 1 then
        local comparer = ...
        if type(comparer) == "table" then
          local equals = comparer.EqualsOf
          if equals == nil then
            buildFromDictionary(this, comparer)
          else
            buildHasComparer(this, ...)
          end
        end
      elseif n == 2 then
        local dictionary, comparer = ...
        if comparer ~= nil then
          buildHasComparer(this, ...)
        end
        if type(dictionary) ~= "number" then
          buildFromDictionary(this, dictionary)
        end
      end
    end,
    AddKeyValue = add,
    Add = function (this, ...)
      local k, v
      if select("#", ...) == 1 then
        local pair = ...
        k, v = pair[1], pair[2]
      else
        k, v = ...
      end
      add(this, k ,v)
    end,
    Clear = function (this)
      for k, _ in pairs(this) do
        this[k] = nil
      end
      counts[this] = nil
    end,
    ContainsKey = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      return this[key] ~= nil
    end,
    ContainsValue = function (this, value)
      if value == nil then
        for _, v in pairs(this) do
          if v == null then
            return true
          end
        end
      else
        local comparer = EqualityComparer(this.__genericTValue__).getDefault()
        local equals = comparer.EqualsOf
          for _, v in pairs(this) do
            if v ~= null then
              if equals(comparer, value, v ) then
                return true
              end
            end
        end
      end
      return false
    end,
    Contains = function (this, pair)
      local key = pair[1]
      if key == nil then throw(ArgumentNullException("key")) end
      local value = this[key]
      if value ~= nil then
        if value == null then value = nil end
        local comparer = EqualityComparer(this.__genericTValue__).getDefault()
        if comparer:EqualsOf(value, pair[2]) then
          return true
        end
      end
      return false
    end,
    CopyTo = function (this, array, index)
      local count = getCount(this)
      checkIndexAndCount(array, index, count)
      if count > 0 then
        local T = this.__genericT__
        index = index + 1
        for k, v in pairs(this) do
          if v == null then v = nil end
          array[index] = setmetatable({ k, v }, T)
          index = index + 1
        end
      end
    end,
    RemoveKey = remove,
    Remove = function (this, key)
      if isKeyValuePair(key) then
        local k, v = key[1], key[2]
        if k == nil then throw(ArgumentNullException("key")) end
        local value = this[k]
        if value ~= nil then
          if value == null then value = nil end
          local comparer = EqualityComparer(this.__genericTValue__).getDefault()
          if comparer:EqualsOf(value, v) then
            remove(this, k)
            return true
          end
        end
        return false
      end
      return remove(this, key)
    end,
    removeWhere = function (this, match)
      local count = 0
      for k, v in pairs(this) do
       if match(k, v) then
          this[k] = nil
          count = count + 1
       end
      end
      if count > 0 then
        local t = counts[this]
        t[1] = t[1] - count
        t[2] = t[2] + 1
      end
      return count
    end,
    TryAdd = function (this, key, value)
      if key == nil then throw(ArgumentNullException("key")) end
      if this:ContainsKey(key) then
        return false
      end
      this:set(key, value)
      return true
    end,
    TryGetValue = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      local value = this[key]
      if value == nil then
        return false, this.__genericTValue__:default()
      end
      if value == null then return true end
      return true, value
    end,
    getComparer = function (this)
      return EqualityComparer(this.__genericTKey__).getDefault()
    end,
    getCount = getCount,
    get = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      local value = this[key]
      if value == nil then throw(KeyNotFoundException()) end
      if value ~= null then
        return value
      end
      return nil
    end,
    set = function (this, key, value)
      if key == nil then throw(ArgumentNullException("key")) end
      local t = counts[this]
      if t then
        if this[key] == nil then
          t[1] = t[1] + 1
        end
        t[2] = t[2] + 1
      else
        counts[this] = { 1, 1 }
      end
      this[key] = value == nil and null or value
    end,
    GetEnumerator = dictionaryEnumerator,
    getKeys = function (this)
      return DictionaryCollection(this.__genericTKey__)(this, 1)
    end,
    getValues = function (this)
      return DictionaryCollection(this.__genericTValue__)(this, 2)
    end
  }
end)()
local ArrayDictionaryEnumerator = define("System.Collections.Generic.ArrayDictionaryEnumerator", function (T)
  return {
    __genericT__ = T,
    base = { System.IEnumerator_1(T) }
  }
end, {
  getCurrent = System.getCurrent,
  Dispose = System.emptyFn,
  MoveNext = function (this)
    local t = this.list
    if this.version ~= versions[t] then
      throwFailedVersion()
    end
    local index = this.index
    while true do
      local pair = t[index]
      if pair == nil then
        break
      else
        local k = pair[1]
        if k ~= nil then
          if this.kind then
            this.current = pair[2]
          else
            this.current = k
          end
          this.index = index + 1
          return true
        else
          index = index + 1
        end
      end
    end
    this.current = this.__genericT__:default()
    return false
  end
}, 1)
local arrayDictionaryEnumerator = function (t, kind, T)
  return setmetatable({
    list = t, kind = kind, index = 1, version = versions[t], currnet = T:default()
  }, ArrayDictionaryEnumerator(T))
end
local ArrayDictionaryCollection = define("System.Collections.Generic.ArrayDictionaryCollection", function (T)
  return {
    base = { System.ICollection_1(T), System.IReadOnlyCollection_1(T), System.ICollection },
    __genericT__ = T
  }
  end, {
  __ctor__ = function (this, dict, kind)
    this.dict = dict
    this.kind = kind
  end,
  getCount = function (this)
    return this.dict.count
  end,
  get = function (this, index)
    local p = this.dict[index + 1]
    if p == nil then throw(System.ArgumentOutOfRangeException()) end
    if this.kind then
      return p[2]
    end
    return p[1]
  end,
  Contains = function (this, v)
    if this.kind then
      return this.dict:ContainsValue(v)
    end
    return this.dict:ContainsKey(v)
  end,
  GetEnumerator = function (this)
    return arrayDictionaryEnumerator(this.dict, this.kind, this.__genericT__)
  end
}, 1)
local ArrayDictionary = (function ()
  local function update(this, add, key, value, set)
    if key == nil then throw(ArgumentNullException("key")) end
    local comparer, indexs = this.comparer, this.indexs
    local code = comparer:GetHashCodeOf(key)
    while true do
      local index = indexs[code]
      local pair = this[index]
      if pair == nil then
        if add then
          local freeList, count = this.freeList, this.count
          if freeList then
            index = freeList
            pair = this[index]
            this.freeList = pair[3]
            pair[1], pair[2], pair[3] = key, value, nil
          else
            index = count + 1
            this[index] = setmetatable({ key, value }, this.__genericT__)
          end
          indexs[code] = index
          this.count = count + 1
          versions[this] = (versions[this] or 0) + 1
        else
          return false
        end
        return
      else
        if comparer:EqualsOf(pair[1], key) then
          if add then
            if set then
              pair[2] = value
              return
            else
              throw(ArgumentException("key already exists"))
            end
          else
            indexs[code] = nil
            local freeList, count = this.freeList, this.count
            pair[1], pair[2], pair[3] = nil, nil, freeList
            this.freeList = index
            this.count = count - 1
            versions[this] = (versions[this] or 0) + 1
            return true
          end
        else
          code = code + 1
        end
      end
    end
  end
  local function addRange(this, dictionary)
    if dictionary == nil then throw(ArgumentNullException("dictionary")) end
    for _, pair in each(dictionary) do
      local k, v = pair[1], pair[2]
      if type(k) == "table" and k.class == 'S' then
        k = k:__clone__()
      end
      update(this, true, k, v)
    end
  end
  local function find(this, key)
    local comparer, indexs = this.comparer, this.indexs
    local code = comparer:GetHashCodeOf(key)
    while true do
      local index = indexs[code]
      local pair = this[index]
      if pair == nil then
        return nil
      end
      if comparer:EqualsOf(pair[1], key) then
        return pair
      else
        code = code + 1
      end
    end
  end
  return {
    count = 0,
    getIsFixedSize = falseFn,
    getIsReadOnly = falseFn,
    __ctor__ = function (this, ...)
      local Comparer, dict
      local n = select("#", ...)
      if n == 1 then
        local comparer = ...
        if type(comparer) == "table" then
          local equals = comparer.EqualsOf
          if equals == nil then
            dict = comparer
          else
            Comparer = comparer
          end
        end
      elseif n == 2 then
        local dictionary, comparer = ...
        if type(dictionary) ~= "number" then
          dict = dictionary
        end
        Comparer = comparer
      end
      this.comparer = Comparer or EqualityComparer(this.__genericTKey__).getDefault()
      this.indexs = {}
      if dict then
        addRange(this, dict)
      end
    end,
    AddKeyValue = function (this, k, v)
      update(this, true, k, v)
    end,
    Add = function (this, ...)
      local k, v
      if select("#", ...) == 1 then
        local pair = ...
        k, v = pair[1], pair[2]
      else
        k, v = ...
      end
      update(this, true, k ,v)
    end,
    Clear = function (this)
      local count = this.count
      if count > 0 then
        this.indexs, this.count, this.freeList = {}, 0, nil
        Array.clear(this)
      end
    end,
    ContainsKey = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      local pair = find(this, key)
      return pair ~= nil
    end,
    ContainsValue = function (this, value)
      local comparer = EqualityComparer(this.__genericTValue__).getDefault()
      local equals = comparer.EqualsOf
      for i = 1, #this do
        local pair = this[i]
        if pair[1] ~= nil and equals(comparer, value, pair[2]) then
          return true
        end
      end
      return false
    end,
    Contains = function (this, pair)
      local key = pair[1]
      if key == nil then throw(ArgumentNullException("key")) end
      local p = find(this, key)
      if p then
        local comparer = EqualityComparer(this.__genericTValue__).getDefault()
        if comparer:EqualsOf(p[2], pair[2]) then
          return true
        end
      end
      return false
    end,
    CopyTo = function (this, array, index)
      local count = this.count
      checkIndexAndCount(array, index, count)
      if count > 0 then
        local T = this.__genericT__
        index = index + 1
        for i = 1, #this do
          local p = this[i]
          local k, v = p[1], p[2]
          if k ~= nil then
            if type(k) == "table" and k.class == 'S' then
              k = k:__clone__()
            end
            array[index] = setmetatable({ k, v }, T)
            index = index + 1
          end
        end
      end
    end,
    RemoveKey = function (this, key)
      return update(this, false, key)
    end,
    Remove = function (this, pair)
      if isKeyValuePair(pair) then
        if this:Contains(pair) then
          update(this, false, pair[1])
          return true
        end
      end
      return false
    end,
    removeWhere = function (this, match)
      local count = 0
      for i = 1, #this do
        local p = this[i]
        local k, v = p[1], p[2]
        if k ~= nil then
          if match(k, v) then
            update(this, false, k)
            count = count + 1
          end
        end
      end
      return count
    end,
    TryAdd = function (this, key, value)
      if key == nil then throw(ArgumentNullException("key")) end
      if this:ContainsKey(key) then
        return false
      end
      this:set(key, value)
      return true
    end,
    TryGetValue = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      local pair = find(this, key)
      if pair then
        return true, pair[2]
      end
      return false, this.__genericTValue__:default()
    end,
    getComparer = function (this)
      return this.comparer
    end,
    getCount = function (this)
      return this.count
    end,
    get = function (this, key)
      if key == nil then throw(ArgumentNullException("key")) end
      local pair = find(this, key)
      if pair then
        return pair[2]
      end
      throw(KeyNotFoundException())
    end,
    set = function (this, key, value)
      update(this, true, key, value, true)
    end,
    GetEnumerator = Array.GetEnumerator,
    getKeys = function (this)
      return ArrayDictionaryCollection(this.__genericTKey__)(this)
    end,
    getValues = function (this)
      return ArrayDictionaryCollection(this.__genericTValue__)(this, true)
    end
  }
end)()
ArrayDictionaryFn = define("System.Collections.Generic.ArrayDictionary", function(TKey, TValue)
  return {
    base = { System.IDictionary_2(TKey, TValue), System.IDictionary, System.IReadOnlyDictionary_2(TKey, TValue) },
    __genericT__ = KeyValuePairFn(TKey, TValue),
    __genericTKey__ = TKey,
    __genericTValue__ = TValue,
  }
end, ArrayDictionary, 2)
function System.dictionaryFromTable(t, TKey, TValue)
  return setmetatable(t, Dictionary(TKey, TValue))
end
function System.isDictLike(t)
  return type(t) == "table" and t.GetEnumerator == dictionaryEnumerator
end
local DictionaryFn = define("System.Collections.Generic.Dictionary", function(TKey, TValue)
  local array
  if hasHash(TKey) then
    array = ArrayDictionary
  end
  return {
    base = { System.IDictionary_2(TKey, TValue), System.IDictionary, System.IReadOnlyDictionary_2(TKey, TValue) },
    __genericT__ = KeyValuePairFn(TKey, TValue),
    __genericTKey__ = TKey,
    __genericTValue__ = TValue,
  }, array
end, Dictionary, 2)
System.Dictionary = DictionaryFn
System.ArrayDictionary = ArrayDictionaryFn
local Object = System.Object
System.Hashtable = DictionaryFn(Object, Object)

end

-- CoreSystemLib: Utilities.lua
do
local System = System
local throw = System.throw
local define = System.define
local trunc = System.trunc
local sl = System.sl
local bor = System.bor
local TimeSpan = System.TimeSpan
local ArgumentNullException = System.ArgumentNullException
local select = select
local type = type
local os = os
local clock = os.clock
local tostring = tostring
local collectgarbage = collectgarbage
define("System.Environment", {
  Exit = os.exit,
  getStackTrace = System.traceback,
  getTickCount = function ()
    return System.currentTimeMillis() % 2147483648
  end
})
define("System.GC", {
  Collect = function ()
    collectgarbage("collect")
  end,
  GetTotalMemory = function (forceFullCollection)
    if forceFullCollection then
      collectgarbage("collect")
    end
    return collectgarbage("count") * 1024
  end
})
local Lazy = {
  created = false,
  __ctor__ = function (this, ...)
    local n = select("#", ...)
    if n == 0 then
    elseif n == 1 then
      local valueFactory = ...
      if valueFactory == nil then
        throw(ArgumentNullException("valueFactory"))
      elseif type(valueFactory) ~= "boolean" then
        this.valueFactory = valueFactory
      end
    elseif n == 2 then
      local valueFactory = ...
      if valueFactory == nil then
        throw(ArgumentNullException("valueFactory"))
      end
      this.valueFactory = valueFactory
    end
  end,
  getIsValueCreated = function (this)
    return this.created
  end,
  getValue = function (this)
    if not this.created then
      local valueFactory = this.valueFactory
      if valueFactory then
        this.value = valueFactory()
        this.valueFactory = nil
      else
        this.value = this.__genericT__()
      end
      this.created = true
    end
    return this.value
  end,
  ToString = function (this)
    if this.created then
      return this.value:ToString()
    end
    return "Value is not created."
  end
}
define("System.Lazy", function (T)
  return {
    __genericT__ = T
  }
end, Lazy, 1)
local ticker, frequency
local time = System.config.time
if time then
  ticker = time
  frequency = 10000
else
  ticker = clock
  frequency = 1000
end
local function getRawElapsedSeconds(this)
  local timeElapsed = this.elapsed
  if this.running then
    local currentTimeStamp = ticker()
    local elapsedUntilNow  = currentTimeStamp - this.startTimeStamp
    timeElapsed = timeElapsed + elapsedUntilNow
  end
  return timeElapsed
end
local Stopwatch
Stopwatch = define("System.Diagnostics.Stopwatch", {
  elapsed = 0,
  running = false,
  IsHighResolution = false,
  Frequency = frequency,
  StartNew = function ()
    local t = Stopwatch()
    t:Start()
    return t
  end,
  GetTimestamp = function ()
    return trunc(ticker() * frequency)
  end,
  Start = function (this)
    if not this.running then
      this.startTimeStamp = ticker()
      this.running = true
    end
  end,
  Stop = function (this)
    if this.running then
      local endTimeStamp = ticker()
      local elapsedThisPeriod = endTimeStamp - this.startTimeStamp
      local elapsed = this.elapsed + elapsedThisPeriod
      this.running = false
      if elapsed < 0 then
        elapsed = 0
      end
      this.elapsed = elapsed
    end
  end,
  Reset = function (this)
    this.elapsed = 0
    this.running = false
    this.startTimeStamp = 0
  end,
  Restart = function (this)
    this.elapsed = 0
    this.startTimeStamp = ticker()
    this.running = true
  end,
  getIsRunning = function (this)
    return this.running
  end,
  getElapsed = function (this)
    return TimeSpan(trunc(getRawElapsedSeconds(this) * 1e7))
  end,
  getElapsedMilliseconds = function (this)
    return trunc(getRawElapsedSeconds(this) * 1000)
  end,
  getElapsedTicks = function (this)
    return trunc(getRawElapsedSeconds(this) * frequency)
  end
})
System.Stopwatch = Stopwatch
local weaks = setmetatable({}, { __mode = "kv" })
local function setWeakTarget(this, target)
  weaks[this] = target
end
define("System.WeakReference", {
  trackResurrection = false,
  SetTarget = setWeakTarget,
  setTarget = setWeakTarget,
  __ctor__ = function (this, target, trackResurrection)
    if trackResurrection then
      this.trackResurrection = trackResurrection
    end
    weaks[this] = target
  end,
  TryGetTarget = function (this)
    local target = weaks[this]
    return target ~= nil, target
  end,
  getIsAlive = function (this)
    return weaks[this] ~= nil
  end,
  getTrackResurrection = function (this)
    return this.trackResurrection
  end,
  getTarget = function (this)
    return weaks[this]
  end
})
define("System.Guid", {})
define("System.ArraySegment", {})

end

-- ===== Polyphase engine API module =====
do
-- Generated by CSharp.lua Compiler
local System = System
System.namespace("Polyphase", function (namespace)
  -- <summary>Engine log (maps to the Lua Log table).</summary>
  namespace.class("Log", function (namespace)
    return {}
  end)

  -- <summary>
  -- Escape hatch into raw Lua: engine APIs not yet wrapped in C#, and
  -- dynamic calls onto Lua scripts attached to other nodes.
  -- </summary>
  namespace.class("Lua", function (namespace)
    return {}
  end)
end)
System.namespace("Polyphase", function (namespace)
  -- <summary>
  -- Handle to an engine Node. The underlying Lua value IS the node userdata —
  -- there is no wrapper object at runtime; every member maps directly onto the
  -- engine's Lua binding for that node.
  -- </summary>
  namespace.class("Node", function (namespace)
    return {}
  end)

  -- <summary>
  -- Handle to an engine Node3D (transform-bearing node).
  -- </summary>
  namespace.class("Node3D", function (namespace)
    local __ctor__
    __ctor__ = function (this)
      System.base(this).__ctor__(this)
    end
    return {
      base = function (out)
        return {
          out.Polyphase.Node
        }
      end,
      __ctor__ = __ctor__
    }
  end)
end)
System.namespace("Polyphase", function (namespace)
  -- <summary>
  -- Exposes a field of a Script class to the Polyphase editor inspector,
  -- scene serialization, and hot-reload property restore.
  -- 
  -- The PolyphaseSharp transpiler rewrites a [Property] field into an accessor
  -- pair that reads/writes the owning node's uservalue field of the same name,
  -- and emits a matching entry in the generated GatherProperties() table.
  -- 
  -- v1 constraints (enforced by the transpiler):
  -- - Allowed types: int, short, byte, float, double, bool, string,
  -- Vector3, Color, Node and Node-derived handles.
  -- - Initializers must be literals or new Vector3/Color(literal, ...) calls.
  -- - Arrays are not yet supported.
  -- </summary>
  namespace.class("PropertyAttribute", function (namespace)
    local __ctor__
    __ctor__ = function (this)
      System.base(this).__ctor__(this)
    end
    return {
      base = function (out)
        return {
          System.Attribute
        }
      end,
      __ctor__ = __ctor__
    }
  end)
end)
System.namespace("Polyphase", function (namespace)
  -- <summary>
  -- Base class for a Polyphase script attached to any Node. Derive from this
  -- (or Script3D for transform-bearing nodes), override the lifecycle methods
  -- you need, and call the node API directly — bare calls like SetName(...)
  -- operate on the node the script is attached to, exactly like `self:...`
  -- in engine Lua scripts.
  -- 
  -- At runtime the C# instance is a companion object; the attached node is
  -- reachable as the Node property (this.__node in generated Lua).
  -- </summary>
  namespace.class("Script", function (namespace)
    local Create, Awake, Start, Tick, EditorTick, Stop, Destroy, BeginOverlap, 
    EndOverlap, OnCollision
    -- <summary>Called when the script instance is created, BEFORE serialized
    -- property values are applied. Field initializers run here.</summary>
    Create = function (this)
    end
    -- <summary>Called after properties are applied, before Start.</summary>
    Awake = function (this)
    end
    -- <summary>Called on the first tick after the node starts.</summary>
    Start = function (this)
    end
    -- <summary>Called every frame while the game is running.</summary>
    Tick = function (this, deltaTime)
    end
    -- <summary>Called every frame in the editor when the game is not running.</summary>
    EditorTick = function (this, deltaTime)
    end
    -- <summary>Called when the node stops.</summary>
    Stop = function (this)
    end
    -- <summary>Called when the script instance is destroyed.</summary>
    Destroy = function (this)
    end
    -- <summary>Called when another primitive begins overlapping this node.</summary>
    BeginOverlap = function (this, other)
    end
    -- <summary>Called when another primitive stops overlapping this node.</summary>
    EndOverlap = function (this, other)
    end
    -- <summary>Called on physics collision with contact point and normal.</summary>
    OnCollision = function (this, other, impactPoint, impactNormal)
    end
    return {
      Create = Create,
      Awake = Awake,
      Start = Start,
      Tick = Tick,
      EditorTick = EditorTick,
      Stop = Stop,
      Destroy = Destroy,
      BeginOverlap = BeginOverlap,
      EndOverlap = EndOverlap,
      OnCollision = OnCollision
    }
  end)

  -- <summary>
  -- Base class for scripts attached to Node3D (transform-bearing) nodes.
  -- Adds the transform API as bare calls / properties.
  -- </summary>
  namespace.class("Script3D", function (namespace)
    return {
      base = function (out)
        return {
          out.Polyphase.Script
        }
      end
    }
  end)
end)
System.namespace("Polyphase", function (namespace)
  -- <summary>
  -- The engine vector (Lua `Vec` userdata, glm::vec4 backed). Reference
  -- semantics — assignment aliases the same underlying vector, exactly as in
  -- engine Lua scripts. Use Clone() for an independent copy.
  -- </summary>
  namespace.class("Vector3", function (namespace)
    return {}
  end)

  -- <summary>
  -- Engine color (Lua `Vec` userdata with r/g/b/a in x/y/z/w). 0..1 floats.
  -- </summary>
  namespace.class("Color", function (namespace)
    return {}
  end)
end)
end

-- ===== Engine glue =====

-- Construct a companion instance with the node back-reference already in place,
-- so C# constructors and Create() can touch [Property] accessors (which route
-- through __node). Mirrors CoreSystem new(): setmetatable + __ctor__.
function CSharpCore.New(cls, node)
    local inst = setmetatable({ __node = node }, cls)
    local ctor = rawget(cls, "__ctor__")
    if ctor ~= nil then
        if type(ctor) == "table" then ctor = ctor[1] end
        ctor(inst)
    end
    return inst
end

-- Finalize every type the current generated file just registered. Clears stale
-- published globals first so the engine's per-file script hot reload can re-run
-- a generated chunk without tripping CoreSystem's duplicate-publish asserts.
function CSharpCore.Finalize()
    local names = System.getRegisteredModuleNames()
    if #names == 0 then return end
    table.sort(names)
    for i = 1, #names do
        local scope, key = _G, nil
        for part in string.gmatch(names[i], "[^%.]+") do
            if key ~= nil then
                scope = rawget(scope, key)
                if scope == nil then break end
            end
            key = part
        end
        if scope ~= nil and key ~= nil then
            rawset(scope, key, nil)
        end
    end
    System.init({ types = names })
end

-- Finalize the Polyphase API types registered above.
CSharpCore.Finalize()
