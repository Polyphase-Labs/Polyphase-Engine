# Setting Up a Multiplayer Game

This is an end-to-end guide to building a networked multiplayer game in Lua with
Polyphase. It covers the networking model, hosting/joining sessions, the
connection lifecycle, state replication, and remote procedure calls.

For the exhaustive per-function reference, see
[Systems → Network](../Systems/Network.md). For the script callbacks
(`GatherReplicatedData`, `GatherNetFuncs`, `OwnerChanged`) see
[Scripting](../../Info/Scripting.md). The HTTP client is documented
separately in [Networking → Http](Http.md).

---

## The model

Polyphase uses an **authoritative-server** model:

- One host is the **server** (also called the *authority*). It owns the
  canonical game state and is the only host allowed to spawn replicated nodes
  and run authoritative gameplay logic.
- Every other host is a **client**. Clients receive replicated state from the
  server and ask the server to do things on their behalf via RPCs.
- A game with no active session is **local** — a single machine that is treated
  as its own authority (so single-player and host code share the same paths).

Each connected host has a **NetHostId**:

- `NetHost.Invalid` (`0`) — no host / not assigned.
- `NetHost.Server` (`1`) — the server host.
- Clients get IDs assigned by the server on connect.

> **Key principle:** write gameplay logic so the *server* decides outcomes and
> clients only render/predict. Guard authoritative code with
> `Network.IsAuthority()` (or `node:HasAuthority()`), which is true on the
> server **and** when playing locally.

---

## Checking your role

```lua
Network.IsServer()      -- this host is the server
Network.IsClient()      -- this host is a client
Network.IsLocal()       -- no session active (single machine)
Network.IsAuthority()   -- IsServer() or IsLocal() -- the gameplay authority
Network.GetHostId()     -- this host's NetHostId
Network.GetNetStatus()  -- "Server" | "Client" | "Connecting" | "Local"
```

`Network.IsAuthority()` is the check you will reach for most often:

```lua
function Bomb:Tick(dt)
    if Network.IsAuthority() then
        -- Only the server counts down the fuse and decides when it explodes.
        self.time = self.time - dt
        if self.time <= 0 then
            self:Explode()
        end
    end
end
```

---

## Hosting a session

Call `Network.OpenSession()` to become the server. The options table is
optional — omit it to use all defaults.

```lua
Network.OpenSession({
    name       = "My Game",  -- shown to clients searching for sessions
    lan        = true,       -- LAN-only (no online platform lobby)
    private     = false,     -- if true, only friends can join (online sessions)
    port       = 7777,       -- listen port (may be ignored for online sessions)
    maxPlayers = 4,          -- includes the server host itself
})
```

A LAN session is broadcast automatically so clients can discover it — session
broadcast is **on by default**. You only need this call if you previously turned
it off (e.g. for a private/direct-connect game):

```lua
Network.EnableSessionBroadcast(true)   -- default is already true
```

Close the session (and disconnect everyone) with `Network.CloseSession()`.

---

## Discovering and joining a session

There are two ways to join: discover a session on the LAN, or connect directly.

### Discover on the LAN

Start a search, poll for results, then join one. Sessions only appear after
`Network.BeginSessionSearch()` has been called.

```lua
function Browser:StartSearch()
    Network.BeginSessionSearch()

    -- Poll once a second until we find a session.
    self.timer = TimerManager.SetTimer(function()
        if Network.GetNumSessions() > 0 then
            local session = Network.GetSession(1)   -- 1-based index
            Network.JoinSession(session)            -- or Connect(ip, port)
            self:StopSearch()
        end
    end, 1.0, true)
end

function Browser:StopSearch()
    Network.EndSessionSearch()
    TimerManager.ClearTimer(self.timer)
    self.timer = nil
end
```

Each session returned by `Network.GetSession(i)` / `Network.GetSessions()` is a
table with: `ipAddress`, `port`, `lobbyId`, `name`, `maxPlayers`, `numPlayers`.

### Connect directly

If you already know the address, skip the search:

```lua
Network.JoinSession({ ipAddress = "192.168.1.42", port = 7777 })
-- For an online platform (e.g. Steam) lobby, pass lobbyId instead:
Network.JoinSession({ lobbyId = "109775240..." })
```

`Network.Connect(ipAddress, port)` is the lower-level equivalent; prefer
`JoinSession`.

---

## The connection lifecycle (callbacks)

Register callbacks once (e.g. in your game-state `Init`). The arguments each
callback receives are part of the contract:

```lua
function GameState:Init()
    Network.SetConnectCallback(GameState.OnConnect)       -- server: a client joined
    Network.SetAcceptCallback(GameState.OnAccept)         -- client: server accepted us
    Network.SetRejectCallback(GameState.OnReject)         -- client: server refused us
    Network.SetDisconnectCallback(GameState.OnDisconnect) -- server: a client left
    Network.SetKickCallback(GameState.OnKick)             -- client: we were kicked
end

-- Fires on the SERVER. `client` is a host-profile table:
-- { ipAddress, port, hostId, onlineId, ping, ready }
GameState.OnConnect = function(client)
    Log.Debug("Client connected: hostId " .. client.hostId)
end

-- Fires on the CLIENT. No arguments.
GameState.OnAccept = function()
    Log.Debug("Server accepted our connection")
end

-- Fires on the CLIENT. `reason` is an integer:
-- 0 = InvalidGameCode, 1 = VersionMismatch, 2 = SessionFull
GameState.OnReject = function(reason)
    Log.Debug("Connection rejected, reason " .. reason)
end

-- Fires on the SERVER. `client` is the same host-profile table as OnConnect.
GameState.OnDisconnect = function(client)
    Log.Debug("Client left: hostId " .. client.hostId)
end

-- Fires on the CLIENT. `reason` is an integer:
-- 0 = SessionClose, 1 = Timeout, 2 = Forced
GameState.OnKick = function(reason)
    Log.Debug("We were kicked, reason " .. reason)
end
```

On the server you can inspect/manage connected clients at any time:

```lua
Network.GetNumClients()         -- server only
Network.GetClients()            -- array of host-profile tables (server only)
Network.FindNetClient(hostId)   -- one profile, or nil
Network.Kick(hostId)            -- server only
```

---

## Ownership and pawns

A replicated node tracks an **owning host**. Ownership decides who is allowed to
drive a node and is independent of authority.

```lua
node:GetOwningHost()        -- NetHostId that owns this node (NetHost.Invalid if none)
node:SetOwningHost(hostId)  -- server: assign ownership
node:IsOwned()              -- true if THIS host owns the node
node:IsLocallyControlled()  -- alias for IsOwned()
node:HasAuthority()         -- node-scoped Network.IsAuthority()
```

A common pattern: the server spawns a player node per client and assigns
ownership, then each host runs input only for the node it owns.

```lua
function Bomber:UpdateInput()
    -- Only read input on the machine that owns this Bomber.
    if not self:IsLocallyControlled() then return end
    -- ... read Input.* and drive movement ...
end
```

Register a host's pawn so the engine can use it for relevancy checks, and react
to ownership changes with the `OwnerChanged` script callback:

```lua
Network.SetPawn(hostId, node)   -- server
Network.GetPawn(hostId)

function Bomber:OwnerChanged()
    -- Called whenever this node's owning host changes.
    if (not Network.IsLocal()) and self:HasStarted() and self:IsOwned() then
        self:SetWorldPosition(self.netPosition)
    end
end
```

A node only participates in networking once it is marked replicated. Most nodes
authored in a scene are replicated by default; you can toggle it explicitly:

```lua
node:SetReplicate(true)         -- include this node in replication
node:SetReplicateTransform(true)-- also auto-replicate its transform
node:IsReplicated()
node:ForceReplication()         -- push an immediate replication update
```

---

## Replicating state

Define which of a script's variables are synced from server → clients with the
`GatherReplicatedData()` script callback. Each entry has a `name`, a
[DatumType](../Misc/Enums.md#datumtype), and an optional `onRep` handler.

```lua
function Bomber:GatherReplicatedData()
    return {
        { name = 'netYaw',      type = DatumType.Float },
        { name = 'netPosition', type = DatumType.Vector, onRep = 'OnRep_netPosition' },
        { name = 'curMoveSpeed',type = DatumType.Float },
        { name = 'bombCount',   type = DatumType.Byte },
        { name = 'bombRange',   type = DatumType.Byte },
        { name = 'moveSpeed',   type = DatumType.Float },
    }
end

-- Called on the CLIENT whenever `netPosition` arrives from the server.
function Bomber:OnRep_netPosition()
    if self:IsOwned() then
        self:SetWorldPosition(self.netPosition)
    end
end
```

- The server writes to these variables; clients receive the values
  automatically.
- An `onRep` function (named by string) runs on the client every time that
  variable is replicated — use it to react to changes (play an effect, snap a
  position, etc.).

Replication behaviour can be tuned globally:

```lua
Network.EnableIncrementalReplication(true) -- only send changed fields
Network.EnableReliableReplication(false)   -- reliable+ordered (costlier) vs. best-effort
```

---

## Remote procedure calls (NetFuncs)

RPCs let one host run a function on another. Declare them with
`GatherNetFuncs()`, then call them with `node:InvokeNetFunc(name, ...)` (up to 8
arguments). Each entry has a `name`, a
[NetFuncType](../Misc/Enums.md#netfunctype), and a `reliable` flag.

```lua
function Bomber:GatherNetFuncs()
    return {
        { name = 'S_PlantBomb',         type = NetFuncType.Server,    reliable = true  },
        { name = 'S_SyncTransform',     type = NetFuncType.Server,    reliable = false },
        { name = 'C_ForceWorldPosition',type = NetFuncType.Client,    reliable = true  },
        { name = 'M_SwingCane',         type = NetFuncType.Multicast, reliable = false },
    }
end
```

`NetFuncType` controls where the function executes:

| Type        | Runs on        | Called from | Typical use |
|-------------|----------------|-------------|-------------|
| `Server`    | the server     | owning client | client asks the server to do something |
| `Client`    | owning client  | the server  | server corrects/notifies one client |
| `Multicast` | all hosts      | the server  | broadcast a cosmetic event to everyone |

`reliable = true` guarantees delivery and ordering (use for one-shot, important
events like *plant bomb*). `reliable = false` is best-effort and cheaper (use
for high-frequency updates like *sync transform*).

By convention these games prefix the function name with `S_` / `C_` / `M_` to
make the call site obvious, but the prefix is cosmetic — `type` is what matters.

```lua
-- Client-side: ask the server to plant a bomb.
function Bomber:TryPlantBomb()
    if self:IsLocallyControlled() then
        self:InvokeNetFunc('S_PlantBomb')   -- runs S_PlantBomb on the server
    end
end

-- Server-side body. Only the server spawns the (replicated) bomb node.
function Bomber:S_PlantBomb()
    local bomb = self.bombScene:Instantiate()
    -- ... position it, add to the field ...
end

-- Server tells everyone to play the swing animation.
function Bomber:S_SwingCane()
    self:InvokeNetFunc('M_SwingCane')       -- multicast to all hosts
end

function Bomber:M_SwingCane()
    self.mesh:PlayAnimation('Swing')
end
```

---

## Tuning and diagnostics

```lua
-- Net relevancy: stop replicating nodes that are far from a client's pawn.
Network.EnableNetRelevancy(true)
Network.SetRelevancyDistance(50.0)
node:SetAlwaysRelevant(true)   -- exempt a specific node from relevancy culling

-- Bandwidth counters (per frame / running average).
Network.GetBytesSent()
Network.GetBytesReceived()
Network.GetUploadRate()
Network.GetDownloadRate()
```

---

## Putting it together

A minimal flow for a host and a joining client:

```lua
-- 1. Register lifecycle callbacks once at startup.
function GameState:Init()
    Network.SetConnectCallback(GameState.OnConnect)
    Network.SetDisconnectCallback(GameState.OnDisconnect)
end

-- 2a. Host a match.
function GameState:Host()
    Network.OpenSession({ name = "My Game", lan = true, maxPlayers = 4 })
    Engine.GetWorld():LoadScene('SC_Match')
end

-- 2b. Or find and join one.
function GameState:Join()
    Network.BeginSessionSearch()
    self.timer = TimerManager.SetTimer(function()
        if Network.GetNumSessions() > 0 then
            Network.JoinSession(Network.GetSession(1))
            Network.EndSessionSearch()
            TimerManager.ClearTimer(self.timer)
        end
    end, 1.0, true)
end

-- 3. In gameplay scripts: guard authority, replicate state, use RPCs.
function Player:Tick(dt)
    if self:IsLocallyControlled() then
        self:ReadInput(dt)
        self:InvokeNetFunc('S_SyncTransform', self:GetWorldPosition())
    end
    if Network.IsAuthority() then
        self:RunAuthoritativeLogic(dt)
    end
end
```

---

## See also

- [Systems → Network](../Systems/Network.md) — full function reference.
- [Scripting](../../Info/Scripting.md) — `GatherReplicatedData`,
  `GatherNetFuncs`, `OwnerChanged` script callbacks.
- [Enums → NetFuncType / NetHost](../Misc/Enums.md#netfunctype) — RPC types and
  host IDs.
- [Node](../Nodes/Node.md) — `InvokeNetFunc`, `IsOwned`, `GetOwningHost`,
  `SetReplicate`, and other per-node networking methods.
