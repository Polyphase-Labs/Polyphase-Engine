# Debugger

Script-side hooks for the **in-engine Lua debugger**. All functions in this
module are silent no-ops in shipping (non-editor) builds, so the same script
ships unmodified to packaged games. See also the
[In-editor Lua Debugger](../README.md#in-editor-lua-debugger) section in the
overview.

---
### Break
**Hard stop** at the call site. Aborts the surrounding pcall via a Lua
error, captures a snapshot, freezes the world. Matches the behaviour
programmers expect from `pdb.set_trace()` or `debugger;`.

The current Lua call stops at this line — anything after `Debugger.Break`
in the same function does **not** execute for that call. Click *Continue*
to resume the world ticking, but the aborted function does NOT re-run on
its own (the engine flips `mHasStarted` / `mHasAwoken` *before* calling
those, so they're considered "done").

If you put `Debugger.Break` in a one-shot init function (`Awake`, `Start`,
or anything that spawns scenes / connects signals), be aware: hitting it
will halt that init partway. Use `Debugger.Snapshot` (below) instead if
you need the rest of the init to complete.

In shipping builds `Debugger.Break` is a no-op and execution continues.

Sig: `Debugger.Break([message])`
 - Arg: `string message` (optional) Reason for breaking; shown in the panel
---
### Snapshot
**Soft pause**: capture the call stack / locals / upvalues at the call
site and freeze the world from the **next** frame, but let the current
Lua call run to its natural end. Use this when you need to inspect state
inside a one-shot init function (`Awake` / `Start`) that spawns scenes
or registers signals — otherwise the rest of the init is lost and
dependent scripts crash with `nil` errors.

The snapshot in the debugger panel reflects the moment of the Snapshot
call, even though the surrounding function continues past it. Click
*Continue* to resume world ticking.

In shipping builds `Debugger.Snapshot` is a no-op and execution continues.

Sig: `Debugger.Snapshot([message])`
 - Arg: `string message` (optional) Reason for snapshotting; shown in the panel
---
### IsAttached
Returns `true` when the in-engine debugger is installed (editor build), and
`false` everywhere else (shipping / packaged game). Use this to gate
debug-only diagnostics so they cost nothing in shipped builds.

Sig: `attached = Debugger.IsAttached()`
 - Ret: `boolean attached` Whether the debugger is active
---

## Example

```lua
function Goblin:OnDamage(amount)
    self.hp = self.hp - amount
    if self.hp < 0 then
        Debugger.Break("hp went negative")  -- pauses here in editor
    end
end

if Debugger.IsAttached() then
    print("Running under the in-engine Lua debugger")
end
```

When `Debugger.Break("hp went negative")` fires under the editor, the
engine freezes, the Lua Debugger panel pops up showing the call stack,
locals (`self`, `amount`), upvalues, and the message *"hp went negative"*
in its header. Click **Continue (F5)** to resume; the engine arms a
one-shot skip so the script can step past its own `Debugger.Break` call
without immediately re-trapping.

In a packaged build, `Debugger.Break` returns immediately without logging
or side effects, and `Debugger.IsAttached()` returns `false`.
