# SignalBus

`SignalBus` is a global event bus for communication across scenes/world transitions.

Unlike `Signal` objects (which are instance-owned) or node signals (`Node:EmitSignal` / `Node:ConnectSignal`, scoped to one node), this bus is process-global and identified by string channel names.

> ℹ️ `SignalBus` is one of **three signal systems** in Polyphase, and they don't interoperate — `SignalBus.Emit("X", ...)` is only seen by `SignalBus.Subscribe("X", ...)` listeners. A `Node:ConnectSignal("X", ...)` or `Signal:Create()` on a node table is a separate channel. See [Signal — choosing the right signal type](../Misc/Signal.md#the-three-signal-flavors--pick-one-and-stick-with-it) for the decision tree before picking this one.

---
### Subscribe
Register a listener node + function for a channel.

Sig: `SignalBus.Subscribe(name, listener, func)`
- Arg: `string name` Channel/event name.
- Arg: `Node listener` Node used as `self` when invoking `func`.
- Arg: `function func` Listener callback (`function MyNode:OnEvent(...)`).

Example:
```lua
function HUD:OnScoreChanged(score)
    self.scoreText:SetText("Score: " .. tostring(score))
    return score
end

function HUD:Start()
    SignalBus.Subscribe("ScoreChanged", self, HUD.OnScoreChanged)
end
```

---
### Unsubscribe
Remove a listener from a channel.

Sig: `SignalBus.Unsubscribe(name, listener)`
- Arg: `string name` Channel/event name.
- Arg: `Node listener` Listener node to remove.

---
### Emit
Emit a global signal channel with any number of args.

Sig: `ret0, ret1, ... = SignalBus.Emit(name, args...)`
- Arg: `string name` Channel/event name.
- Arg: `args...` Payload values forwarded to listeners.
- Ret: `...` Return values from listeners (one return value per listener that returned a value).

Example:
```lua
local a, b = SignalBus.Emit("RequestDifficulty", "campaign")
```

---
### Clear
Remove all channels and listeners from the global bus.

Sig: `SignalBus.Clear()`

---
## C++
C++ can access the same global bus:

```cpp
static Datum OnGlobalEvent(Node* listener, const std::vector<Datum>& args)
{
    // listener is your subscribed node
    return Datum(true);
}

SignalBus* bus = GetSignalBus();
bus->Subscribe("MyEvent", this, &OnGlobalEvent);
std::vector<Datum> ret = bus->Emit("MyEvent", { Datum(123) });
```

Supported C++ listener callback forms:
- `Datum (*)(Node*, const std::vector<Datum>&)`
- `void (*)(Node*, const std::vector<Datum>&)`
- `ScriptFunc`
