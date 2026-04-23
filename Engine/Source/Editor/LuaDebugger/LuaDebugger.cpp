#if EDITOR

#include "LuaDebugger.h"

#include "Log.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

LuaDebugger* LuaDebugger::sInstance = nullptr;

static const char* kPauseSentinel = "__POLYPHASE_DEBUGGER_PAUSE__";

const char* LuaDebugger::GetPauseSentinel()
{
    return kPauseSentinel;
}

void LuaDebugger::Create()
{
    if (sInstance == nullptr)
    {
        sInstance = new LuaDebugger();
    }
}

void LuaDebugger::Destroy()
{
    delete sInstance;
    sInstance = nullptr;
}

LuaDebugger* LuaDebugger::Get()
{
    return sInstance;
}

bool LuaDebugger::IsLuaPandaActive(lua_State* L)
{
    if (L == nullptr)
        return false;

    lua_getglobal(L, "LuaPanda");
    bool active = lua_istable(L, -1);
    lua_pop(L, 1);
    return active;
}

void LuaDebugger::Install(lua_State* L)
{
    if (mInstalled || L == nullptr)
        return;

    if (IsLuaPandaActive(L))
    {
        // LuaPanda has installed its own line hook. Replacing it disables
        // LuaPanda's VS Code remote debugging for this run, but we can't
        // chain hooks (Lua only allows one). Log so the user knows and can
        // disable OCT_LUA_DEBUGGING if they want LuaPanda exclusively.
        LogWarning("LuaDebugger: LuaPanda hook detected -- replacing it with in-engine debugger. Disable OCT_LUA_DEBUGGING in Engine.cpp if you prefer LuaPanda.");
    }

    mL = L;
    lua_sethook(L, &LuaDebugger::OnHookTrampoline, LUA_MASKLINE, 0);
    mInstalled = true;
    LogDebug("LuaDebugger: Installed line hook on lua_State %p", (void*)L);
}

// ---------------------------------------------------------------------------
// Source path normalization
// ---------------------------------------------------------------------------

std::string LuaDebugger::NormalizeSource(const char* luaSource)
{
    if (luaSource == nullptr || luaSource[0] == '\0')
        return std::string();

    const char* p = luaSource;
    if (*p == '@' || *p == '=')
    {
        ++p;
    }

    std::string out(p);

    for (char& c : out)
    {
        if (c == '\\') c = '/';
#if PLATFORM_WINDOWS
        c = (char)std::tolower((unsigned char)c);
#endif
    }

    if (out.size() >= 4)
    {
        std::string tail = out.substr(out.size() - 4);
        if (tail == ".lua")
        {
            out.resize(out.size() - 4);
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Breakpoints
// ---------------------------------------------------------------------------

void LuaDebugger::ToggleBreakpoint(const std::string& sourceFile, int line)
{
    if (line <= 0) return;
    std::string key = NormalizeSource(sourceFile.c_str());
    if (key.empty()) return;

    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    auto& set = mBreakpoints[key];
    auto it = set.find(line);
    if (it == set.end())
    {
        set.insert(line);
        LogDebug("LuaDebugger: Set breakpoint at %s:%d", key.c_str(), line);
    }
    else
    {
        set.erase(it);
        LogDebug("LuaDebugger: Cleared breakpoint at %s:%d", key.c_str(), line);
        if (set.empty())
        {
            mBreakpoints.erase(key);
        }
    }
}

void LuaDebugger::SetBreakpoint(const std::string& sourceFile, int line)
{
    if (line <= 0) return;
    std::string key = NormalizeSource(sourceFile.c_str());
    if (key.empty()) return;

    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    mBreakpoints[key].insert(line);
}

void LuaDebugger::ClearBreakpoint(const std::string& sourceFile, int line)
{
    std::string key = NormalizeSource(sourceFile.c_str());
    if (key.empty()) return;

    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    auto it = mBreakpoints.find(key);
    if (it != mBreakpoints.end())
    {
        it->second.erase(line);
        if (it->second.empty())
        {
            mBreakpoints.erase(it);
        }
    }
}

void LuaDebugger::ClearAllBreakpoints()
{
    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    mBreakpoints.clear();
}

bool LuaDebugger::HasBreakpoint(const std::string& sourceFile, int line) const
{
    std::string key = NormalizeSource(sourceFile.c_str());
    if (key.empty()) return false;

    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    auto it = mBreakpoints.find(key);
    if (it == mBreakpoints.end()) return false;
    return it->second.find(line) != it->second.end();
}

std::set<int> LuaDebugger::GetBreakpointsForFile(const std::string& sourceFile) const
{
    std::string key = NormalizeSource(sourceFile.c_str());
    if (key.empty()) return {};

    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    auto it = mBreakpoints.find(key);
    if (it == mBreakpoints.end()) return {};
    return it->second;
}

std::vector<LuaDebugger::BreakpointEntry> LuaDebugger::GetAllBreakpoints() const
{
    std::vector<BreakpointEntry> out;
    std::lock_guard<std::mutex> lock(mBreakpointMutex);
    for (const auto& kv : mBreakpoints)
    {
        for (int line : kv.second)
        {
            out.push_back({kv.first, line});
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Pause / Continue
// ---------------------------------------------------------------------------

void LuaDebugger::RequestContinue()
{
    if (!mPaused.load()) return;

    // Arm a one-shot skip at the current break location so the next firing
    // of the hook there is suppressed -- otherwise the very next Tick would
    // immediately re-trap on the same line.
    mSkipOnceArmed = true;
    mSkipOnceFile = mPauseFile;
    mSkipOnceLine = mPauseLine;

    mPaused.store(false);
    mPauseMessage.clear();
    mPauseFile.clear();
    mPauseLine = -1;
    mCallStack.clear();
    mFrameLocals.clear();
    mFrameUpvalues.clear();
}

// ---------------------------------------------------------------------------
// Snapshot capture
// ---------------------------------------------------------------------------

std::string LuaDebugger::FormatLuaValue(lua_State* L, int idx)
{
    int t = lua_type(L, idx);
    switch (t)
    {
    case LUA_TNIL:     return "nil";
    case LUA_TBOOLEAN: return lua_toboolean(L, idx) ? "true" : "false";
    case LUA_TNUMBER:
    {
        char buf[64];
        if (lua_isinteger(L, idx))
        {
            snprintf(buf, sizeof(buf), "%lld", (long long)lua_tointeger(L, idx));
        }
        else
        {
            snprintf(buf, sizeof(buf), "%g", (double)lua_tonumber(L, idx));
        }
        return buf;
    }
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        std::string out = "\"";
        out.append(s, len);
        out += "\"";
        return out;
    }
    case LUA_TTABLE:
    {
        // 1-level shallow summary: {k=v, k=v, ...} (cap entries).
        std::string out = "{";
        int absIdx = lua_absindex(L, idx);
        lua_pushnil(L);
        int count = 0;
        const int kMaxEntries = 6;
        while (lua_next(L, absIdx) != 0)
        {
            if (count > 0) out += ", ";
            // Key at -2, value at -1
            // Format key (avoid recursing into nested tables)
            int kt = lua_type(L, -2);
            if (kt == LUA_TSTRING)
            {
                out += lua_tostring(L, -2);
            }
            else if (kt == LUA_TNUMBER)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%g", (double)lua_tonumber(L, -2));
                out += buf;
            }
            else
            {
                out += "?";
            }
            out += "=";
            int vt = lua_type(L, -1);
            if (vt == LUA_TTABLE)
                out += "{...}";
            else if (vt == LUA_TFUNCTION)
                out += "<func>";
            else if (vt == LUA_TUSERDATA || vt == LUA_TLIGHTUSERDATA)
                out += "<userdata>";
            else
                out += FormatLuaValue(L, -1);
            lua_pop(L, 1); // pop value, keep key for next
            ++count;
            if (count >= kMaxEntries)
            {
                lua_pop(L, 1);
                out += ", ...";
                break;
            }
        }
        out += "}";
        return out;
    }
    case LUA_TFUNCTION:      return "<function>";
    case LUA_TUSERDATA:      return "<userdata>";
    case LUA_TLIGHTUSERDATA: return "<lightuserdata>";
    case LUA_TTHREAD:        return "<thread>";
    default:                 return "<unknown>";
    }
}

void LuaDebugger::CaptureSnapshot(lua_State* L, int startLevel)
{
    mCallStack.clear();
    mFrameLocals.clear();
    mFrameUpvalues.clear();

    lua_Debug ar;
    int level = startLevel;
    while (lua_getstack(L, level, &ar))
    {
        lua_getinfo(L, "nSl", &ar);

        StackFrame frame;
        frame.mSource = NormalizeSource(ar.source);
        frame.mFuncName = (ar.name != nullptr) ? ar.name : "";
        frame.mWhat = (ar.what != nullptr) ? ar.what : "";
        frame.mCurrentLine = ar.currentline;
        mCallStack.push_back(std::move(frame));

        // Locals
        std::vector<LocalVar> locals;
        for (int n = 1;; ++n)
        {
            const char* name = lua_getlocal(L, &ar, n);
            if (name == nullptr) break;
            // Skip internal Lua names beginning with '('
            if (name[0] != '(')
            {
                LocalVar v;
                v.mName = name;
                v.mTypeStr = lua_typename(L, lua_type(L, -1));
                v.mValueStr = FormatLuaValue(L, -1);
                locals.push_back(std::move(v));
            }
            lua_pop(L, 1);
        }
        mFrameLocals.push_back(std::move(locals));

        // Upvalues (only meaningful for Lua functions on the stack)
        std::vector<LocalVar> upvalues;
        if (ar.what != nullptr && std::strcmp(ar.what, "Lua") == 0)
        {
            // ar.i_ci references the function -- push it via lua_getinfo "f".
            lua_Debug arf = ar;
            if (lua_getinfo(L, "f", &arf))
            {
                int funcIdx = lua_gettop(L);
                for (int n = 1;; ++n)
                {
                    const char* name = lua_getupvalue(L, funcIdx, n);
                    if (name == nullptr) break;
                    LocalVar v;
                    v.mName = name;
                    v.mTypeStr = lua_typename(L, lua_type(L, -1));
                    v.mValueStr = FormatLuaValue(L, -1);
                    upvalues.push_back(std::move(v));
                    lua_pop(L, 1);
                }
                lua_pop(L, 1); // pop the function
            }
        }
        mFrameUpvalues.push_back(std::move(upvalues));

        ++level;
        // Safety cap
        if (level > 64) break;
    }

    // Diagnostic: log capture summary so we can sanity-check from the log
    // that frames + var counts look right after a pause.
    int totalLocals = 0, totalUpvalues = 0;
    for (const auto& v : mFrameLocals)   totalLocals   += (int)v.size();
    for (const auto& v : mFrameUpvalues) totalUpvalues += (int)v.size();
    LogDebug("LuaDebugger: Snapshot captured -- %d frame(s), %d locals, %d upvalues (start level %d)",
             (int)mCallStack.size(), totalLocals, totalUpvalues, startLevel);
}

std::vector<LuaDebugger::LocalVar> LuaDebugger::GetSnapshotVars(int frameIndex, VarKind kind) const
{
    const auto& src = (kind == VarKind::Local) ? mFrameLocals : mFrameUpvalues;
    if (frameIndex < 0 || frameIndex >= (int)src.size())
        return {};
    return src[frameIndex];
}

// ---------------------------------------------------------------------------
// Pause entry
// ---------------------------------------------------------------------------

void LuaDebugger::EnterPaused(lua_State* L, lua_Debug* ar, const char* optionalMessage, int snapshotStartLevel)
{
    // Capture pause location. NOTE: lua_getstack only sets up the activation
    // record for use with lua_getinfo -- fields like 'source' / 'currentline'
    // are uninitialized garbage until lua_getinfo populates them. Always call
    // lua_getinfo here regardless of caller, otherwise we'd pass garbage to
    // NormalizeSource and crash on the bogus pointer.
    mPauseFile.clear();
    mPauseLine = -1;
    if (ar != nullptr)
    {
        lua_getinfo(L, "Sl", ar);
        if (ar->source != nullptr)
        {
            mPauseFile = NormalizeSource(ar->source);
        }
        mPauseLine = ar->currentline;
    }
    else
    {
        // Fall back to top-of-stack frame
        lua_Debug top;
        if (lua_getstack(L, 0, &top))
        {
            lua_getinfo(L, "Sl", &top);
            if (top.source != nullptr)
            {
                mPauseFile = NormalizeSource(top.source);
            }
            mPauseLine = top.currentline;
        }
    }

    mPauseMessage = (optionalMessage != nullptr) ? optionalMessage : "";

    CaptureSnapshot(L, snapshotStartLevel);

    mPaused.store(true);

    LogDebug("LuaDebugger: Paused at %s:%d%s%s",
             mPauseFile.c_str(),
             mPauseLine,
             mPauseMessage.empty() ? "" : " - ",
             mPauseMessage.c_str());

    // Longjmp out via lua_error. This aborts the current pcall; the sentinel
    // is matched by ScriptUtils::CallLuaFunc which swallows it silently.
    char errbuf[256];
    if (mPauseMessage.empty())
    {
        snprintf(errbuf, sizeof(errbuf), "%s", kPauseSentinel);
    }
    else
    {
        snprintf(errbuf, sizeof(errbuf), "%s:%s", kPauseSentinel, mPauseMessage.c_str());
    }
    lua_pushstring(L, errbuf);
    lua_error(L); // never returns
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

void LuaDebugger::OnHookTrampoline(lua_State* L, lua_Debug* ar)
{
    if (sInstance != nullptr)
    {
        sInstance->OnHook(L, ar);
    }
}

void LuaDebugger::OnHook(lua_State* L, lua_Debug* ar)
{
    if (ar->event != LUA_HOOKLINE)
        return;

    // One-shot diagnostic: confirm the hook is actually firing.
    if (!mFirstHookLogged)
    {
        mFirstHookLogged = true;
        LogDebug("LuaDebugger: First line-hook fire (hook is live).");
    }

    // Fast path: nothing armed -> bail without touching getinfo.
    {
        std::lock_guard<std::mutex> lock(mBreakpointMutex);
        if (mBreakpoints.empty() && !mSkipOnceArmed)
            return;
    }

    // Pull source + currentline.
    if (lua_getinfo(L, "Sl", ar) == 0)
        return;
    if (ar->source == nullptr || ar->currentline <= 0)
        return;

    std::string normalized = NormalizeSource(ar->source);
    int line = ar->currentline;

    // Skip-once: if we just continued past a breakpoint, suppress the very
    // next hit at the same location and clear the flag.
    if (mSkipOnceArmed && mSkipOnceFile == normalized && mSkipOnceLine == line)
    {
        mSkipOnceArmed = false;
        mSkipOnceFile.clear();
        mSkipOnceLine = -1;
        return;
    }

    bool hit = false;
    {
        std::lock_guard<std::mutex> lock(mBreakpointMutex);
        auto it = mBreakpoints.find(normalized);
        if (it != mBreakpoints.end() && it->second.find(line) != it->second.end())
        {
            hit = true;
        }
    }

    if (hit)
    {
        LogDebug("LuaDebugger: Breakpoint hit at %s:%d", normalized.c_str(), line);
        EnterPaused(L, ar, nullptr);
        // EnterPaused does not return.
    }
}

// ---------------------------------------------------------------------------
// Debugger.Break(msg) binding entry
// ---------------------------------------------------------------------------

int LuaDebugger::LuaBreakBinding(lua_State* L)
{
    LuaDebugger* dbg = LuaDebugger::Get();
    if (dbg == nullptr)
    {
        return 0; // shouldn't happen in editor builds, but be safe
    }

    const char* msg = nullptr;
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1))
    {
        msg = lua_tostring(L, 1);
    }

    // The C function itself sits at level 0. Level 1 is the Lua call site.
    // Pass startLevel = 1 so the snapshot skips this C frame and the user
    // sees the Lua caller's locals immediately on the default selected frame.
    lua_Debug ar;
    if (lua_getstack(L, 1, &ar))
    {
        dbg->EnterPaused(L, &ar, msg, /*snapshotStartLevel=*/1);
    }
    else
    {
        dbg->EnterPaused(L, nullptr, msg, /*snapshotStartLevel=*/1);
    }

    // EnterPaused does not return (it lua_errors), but keep the compiler happy.
    return 0;
}

#endif // EDITOR
