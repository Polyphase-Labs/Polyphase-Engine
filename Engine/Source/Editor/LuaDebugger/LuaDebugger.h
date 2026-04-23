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

    // Removes our line hook from the lua_State and, if LuaPanda is loaded,
    // tries to restore its hook so it can resume polling for a VS Code
    // connection. Called by the panel's "Active" toggle when the user wants
    // to hand off to LuaPanda without restarting the editor.
    void Uninstall();

    bool IsInstalled() const { return mInstalled; }

    // Persists the user's "Active" preference to a JSON file in the editor
    // preferences directory so it carries across editor restarts. The
    // preference defaults to true (in-engine debugger active) on first run.
    static bool LoadActivePreference(); // returns last-saved value, or true
    static void SaveActivePreference(bool active);

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

    // Clears transient pause/skip state. Called when Play-In-Editor restarts
    // so a "skip-once" left over from the previous run doesn't suppress the
    // first Debugger.Break / Debugger.Snapshot of the next run.
    void ResetTransientState();

    // Called from Debugger.Break() in Lua to pause at the call site.
    // Captures the snapshot for the caller's frame, sets paused, then
    // throws a Lua error to abort the surrounding pcall. Does not return.
    static int LuaBreakBinding(lua_State* L);

    // Called from Debugger.Snapshot() in Lua. Soft variant: captures the
    // snapshot + sets paused, then RETURNS so the surrounding Lua call can
    // finish naturally before the world freezes next frame.
    static int LuaSnapshotBinding(lua_State* L);

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
    void CaptureSnapshot(lua_State* L, int startLevel = 0);

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

    // Snapshot + pause-flag, but DOES call lua_error to abort the running
    // pcall. Used by line breakpoints, where stopping mid-line is the only
    // way to actually halt execution.
    void EnterPaused(lua_State* L, lua_Debug* ar, const char* optionalMessage, int snapshotStartLevel = 0);

    // Snapshot + pause-flag without lua_error. The current Lua function
    // continues to its natural end; world freezes from the next frame.
    // Used by Debugger.Break() so init code (Start, etc.) completes before
    // the world freezes -- otherwise Continue can't recover the state.
    void EnterPausedSoft(lua_State* L, lua_Debug* ar, const char* optionalMessage, int snapshotStartLevel = 0);


    static std::string FormatLuaValue(lua_State* L, int idx);

    static LuaDebugger* sInstance;

    bool mInstalled = false;
    bool mFirstHookLogged = false;
    lua_State* mL = nullptr;

    // Saved cursor state captured when we entered the pause, restored on
    // Continue. Lets the user actually click the panel during PIE pause
    // (where the game would otherwise have hidden / locked / trapped the
    // cursor for mouselook). Also captures EditorState::mGamePreviewCaptured
    // because GamePreview::DrawPanel re-traps the cursor every frame while
    // it's true.
    bool mSavedCursorShown        = true;
    bool mSavedCursorLocked       = false;
    bool mSavedCursorTrapped      = false;
    bool mSavedGamePreviewCapture = false;
    bool mSavedCursorValid        = false;

    void FreeCursorForInspection();
    void RestoreCursor();

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

    // Same idea but for Debugger.Break() calls (which don't go through the
    // line hook). After Continue, suppress the next Debugger.Break call from
    // this exact (file, line) so a Break in a per-frame Tick doesn't re-trap
    // immediately.
    bool mSkipBreakOnceArmed = false;
    std::string mSkipBreakFile;
    int mSkipBreakLine = -1;
};

#endif // EDITOR
