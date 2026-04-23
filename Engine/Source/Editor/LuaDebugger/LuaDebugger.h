#pragma once

#if EDITOR

#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

extern "C" {
    struct lua_State;
    struct lua_Debug;
}

// In-engine Lua breakpoint / pause facility. Editor-only.
//
// Architecture (v1):
//   - lua_sethook(LUA_MASKLINE) installed once on the main lua_State.
//   - On a LINE event, we look up (file, line) in mBreakpoints. On a hit (or
//     when Debugger.Break() is called from script), we capture a snapshot of
//     the call stack / locals / upvalues, set mPaused = true, and then call
//     lua_error with a sentinel string to longjmp out of the script.
//   - ScriptUtils::CallLuaFunc detects the sentinel and swallows it silently.
//   - While paused, Script::CallTick / CallFunction early-return so the world
//     is effectively frozen. The editor keeps rendering normally and the
//     LuaDebuggerPanel shows the snapshot.
//   - Continue clears mPaused and arms a "skip-once" record for the last
//     break location so the immediately-next firing of the hook at that
//     same (file, line) is suppressed. This lets the next Tick run past the
//     breakpoint instead of trapping on it again.
//
// Live in-frame stepping (over/into/out) is intentionally NOT in v1: it
// requires a reusable editor-frame-pump extraction that is too invasive to
// land without supervision. Tracked for v2.
class LuaDebugger
{
public:
    static void Create();
    static void Destroy();
    static LuaDebugger* Get();

    void Install(lua_State* L);
    bool IsInstalled() const { return mInstalled; }

    // ----- Breakpoints --------------------------------------------------

    void ToggleBreakpoint(const std::string& sourceFile, int line);
    void SetBreakpoint(const std::string& sourceFile, int line);
    void ClearBreakpoint(const std::string& sourceFile, int line);
    void ClearAllBreakpoints();
    bool HasBreakpoint(const std::string& sourceFile, int line) const;
    std::set<int> GetBreakpointsForFile(const std::string& sourceFile) const;

    // Returns a flat list of (normalized-file, line) pairs for the panel.
    struct BreakpointEntry { std::string mFile; int mLine; };
    std::vector<BreakpointEntry> GetAllBreakpoints() const;

    // ----- Pause state --------------------------------------------------

    bool IsPaused() const { return mPaused.load(); }
    const std::string& GetPauseMessage() const { return mPauseMessage; }
    const std::string& GetPauseFile() const { return mPauseFile; }
    int GetPauseLine() const { return mPauseLine; }

    void RequestContinue();

    // Called from Debugger.Break() in Lua to pause at the call site.
    // Captures the snapshot for the caller's frame, sets paused, then
    // throws a Lua error to abort the surrounding pcall. Does not return.
    static int LuaBreakBinding(lua_State* L);

    // ----- Snapshot (valid while paused) --------------------------------

    struct StackFrame
    {
        std::string mSource;     // normalized
        std::string mFuncName;   // may be empty for anonymous frames
        std::string mWhat;       // "Lua", "C", "main", "tail"
        int mCurrentLine = -1;
    };

    struct LocalVar
    {
        std::string mName;
        std::string mTypeStr;
        std::string mValueStr;
    };

    const std::vector<StackFrame>& GetCallStack() const { return mCallStack; }

    // Returns locals (kind = 0) or upvalues (kind = 1) for a given frame
    // index in the captured snapshot. Empty if frame index is out of range.
    enum class VarKind { Local, Upvalue };
    std::vector<LocalVar> GetSnapshotVars(int frameIndex, VarKind kind) const;

    // ----- Hook trampoline ---------------------------------------------

    static void OnHookTrampoline(lua_State* L, lua_Debug* ar);

    // ----- Helpers ------------------------------------------------------

    // Sentinel used both for the lua_error message and for matching it back
    // out in ScriptUtils::CallLuaFunc so we don't log it as a real error.
    static const char* GetPauseSentinel();

    // Strip leading '@', drop ".lua", lowercase on Windows, replace '\' -> '/'.
    static std::string NormalizeSource(const char* luaSource);

    // True if LuaPanda has installed itself on this state. Used during
    // Install() to avoid fighting LuaPanda over lua_sethook.
    static bool IsLuaPandaActive(lua_State* L);

private:
    LuaDebugger() = default;

    void OnHook(lua_State* L, lua_Debug* ar);
    void EnterPaused(lua_State* L, lua_Debug* ar, const char* optionalMessage, int snapshotStartLevel = 0);
    void CaptureSnapshot(lua_State* L, int startLevel = 0);

    static std::string FormatLuaValue(lua_State* L, int idx);

    static LuaDebugger* sInstance;

    bool mInstalled = false;
    bool mFirstHookLogged = false;
    lua_State* mL = nullptr;

    mutable std::mutex mBreakpointMutex;
    std::map<std::string, std::set<int>> mBreakpoints; // key: normalized file

    std::atomic<bool> mPaused{false};
    std::string mPauseMessage;
    std::string mPauseFile;
    int mPauseLine = -1;

    // Captured snapshot data
    std::vector<StackFrame> mCallStack;
    // Per-frame local/upvalue lists, indexed [frame][kind].
    std::vector<std::vector<LocalVar>> mFrameLocals;
    std::vector<std::vector<LocalVar>> mFrameUpvalues;

    // After Continue, suppress the next single hook event at this exact
    // (file, line) so the script can step past its own breakpoint.
    bool mSkipOnceArmed = false;
    std::string mSkipOnceFile;
    int mSkipOnceLine = -1;
};

#endif // EDITOR
