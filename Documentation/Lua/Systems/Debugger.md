# Debugger

Script-side hooks for the **in-engine Lua debugger**. All functions in this
module are silent no-ops in shipping (non-editor) builds, so the same script
ships unmodified to packaged games. See also the
[In-editor Lua Debugger](../README.md#in-editor-lua-debugger) section in the
overview.

---
### Break
Pause execution at the call site and open the Lua Debugger panel for
inspection. In shipping builds this is a no-op and execution continues.

The optional `message` is shown in the debugger panel header so you know
*why* this breakpoint fired (helpful for assert-style breaks).

Sig: `Debugger.Break([message])`
 - Arg: `string message` (optional) Reason for breaking; shown in the panel
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
