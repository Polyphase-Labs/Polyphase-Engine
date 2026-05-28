# Signal

A `Signal` is a standalone Lua-side event object — call `:Emit(...)` to fire it, `:Connect(node, func)` to subscribe. Use it when you want an event channel that **isn't tied to a specific Node's built-in signal table**, e.g. a script-defined event you store as a field on a controller table.

> ⚠️ **Polyphase has three different signal systems.** They look similar, share the word "signal", and **do not interoperate**. Picking the wrong one is the most common cause of "I connected, I emitted, no error, but my callback never fires." Read the next section before writing new code.

---

## The three signal flavors — pick one and stick with it

| System | API on the emitter | API on the listener | Lives on | Use it when |
|---|---|---|---|---|
| **Node signals** (built-in) | `node:EmitSignal("name", { ... })` | `node:ConnectSignal("name", self, fn)` | A specific `Node` (auto-cleaned up when the node dies) | One node owns the event — `Button` "Activated", `Audio3D` "OnFinished", `Slider` "ValueChanged", etc. **Default choice.** |
| **`Signal` objects** *(this page)* | `mySignal:Emit(...)` | `mySignal:Connect(self, fn)` | A Lua table you store anywhere — usually `self.mySignal` on a script | You want an event channel that isn't a node, or your emitter isn't a node (e.g. a pure-Lua controller table) |
| **`SignalBus`** | `SignalBus.Emit("name", ...)` | `SignalBus.Subscribe("name", self, fn)` | Process-global, identified by string channel name | Cross-scene / cross-world communication, or the emitter doesn't know who's listening. See [SignalBus.md](../Systems/SignalBus.md). |

### They do NOT cross-talk

Each system is its own channel — the name `"OnFoo"` in one system has zero connection to `"OnFoo"` in another. The most common mistake looks like this:

```lua
-- AudioPlaylist.lua — emitter side
function AudioPlaylist:Create()
    self.OnNextSong = Signal:Create()        -- ← creates a Lua Signal object
end

function AudioPlaylist:Next()
    -- ... advance index ...
    self.OnNextSong:Emit(self.currentIndex)  -- ← emits on the Lua Signal object
end


-- AudioButtonBar.lua — listener side (BROKEN)
self.audioPlaylist:ConnectSignal("OnNextSong", self, function(self, idx)
    self.songTitle:SetTextString(...)
end)
-- ConnectSignal subscribes to the NODE's built-in "OnNextSong" signal.
-- Nothing ever calls audioPlaylist:EmitSignal("OnNextSong", ...), so this
-- callback never fires. No error, no warning — silent.
```

### How to fix it — match the systems

Pick one system on both ends.

**Option A — use node signals everywhere (recommended; matches `Activated`, `OnFinished`, etc.):**

```lua
-- emitter
function AudioPlaylist:Next()
    -- ... advance index ...
    self:EmitSignal("OnNextSong", { self:GetSongName() })
end

-- listener
self.audioPlaylist:ConnectSignal("OnNextSong", self, function(self, songName)
    self.songTitle:SetTextString(songName)
end)
```

No `Signal:Create()` call needed — node signals are named on the fly and live on the node itself.

**Option B — keep the Lua `Signal` and listen with `:Connect`:**

```lua
-- emitter (same as before)
self.OnNextSong = Signal:Create()
-- ...
self.OnNextSong:Emit(self.currentIndex)

-- listener — go through the Signal object, NOT ConnectSignal
self.audioPlaylist.OnNextSong:Connect(self, function(self, idx)
    self.songTitle:SetTextString(self.audioPlaylist:GetSongName())
end)
```

### Quick decision tree

- I'm publishing an event from a **Node** that other nodes will listen to → **Node signal** (`node:EmitSignal` / `node:ConnectSignal`).
- I'm publishing from a non-node, or I want a signal that isn't named via a node's signal table → **`Signal:Create()`** (this page).
- I'm publishing something that crosses scene boundaries, or any subscriber may want it → **[`SignalBus`](../Systems/SignalBus.md)**.

When in doubt, prefer **node signals** — every example in the Polyphase docs (`Activated` on `Button`, `OnFinished` on `Audio3D`, `ValueChanged` on `Slider`) uses them, so call sites stay consistent.

---

## API reference

A signal is an object that provides event-driven behavior. When a signal is emitted, any connected listeners react.

---
### Create
Create a new signal.

Sig: `signal = Signal.Create()`
 - Ret: `Signal signal` Newly created Signal
 ---
### Emit
Emit the signal so that connected listeners can react. The listener function is called as `func(listener, args...)`.

Sig: `Signal:Emit(args...)`
 - Arg: `args...` Any number of arguments that will be passed to connected handler functions.
---
### Connect
Connect a node to the signal. The function passed in should be a member function of the node's table — it will be invoked as `func(node, args...)` where `args...` are the values passed to `Emit`.

Example:

```lua
function HealthBar:OnHealthChanged(newHp)
    self.quad:SetXRatio(newHp / Player.MaxHp)
end

function HealthBar:Start()
    GetPlayer().healthSignal:Connect(self, HealthBar.OnHealthChanged)
end
```

Sig: `Signal:Connect(listener, func)`
 - Arg: `Node listener` The node that will be notified when the signal is emit
 - Arg: `function func` The function on the node that will be invoked when the signal is emit
 ---
 ### Disconnect
 Sig: `Signal:Disconnect(listener)`
  - Arg: `Node listener` The node that will be disconnected from the signal
---
