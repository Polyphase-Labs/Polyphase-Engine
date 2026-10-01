# C# Scripting

Polyphase supports writing scripts in C#. Sources compile with the latest Roslyn and
transpile to Lua (via a vendored [CSharp.lua](https://github.com/yanghuan/CSharp.lua)),
so C# scripts run on **every** platform the engine ships — desktop, Android, Web, and
all retro console targets — with no .NET runtime in the shipped game. The .NET SDK
(8 or newer) is needed only while authoring; team members who don't touch C# never
need it, because the generated `.lua` files are committed alongside the sources.

## Getting started

- **New project**: pick **C#** next to Lua in the Create New Project panel, or
- **Existing project**: `Tools > CSharp > Enable C# for This Project` (or the
  "C# Scripting" checkbox in Project Settings).

Either way the editor scaffolds `Scripts/CSharp/` with a `Game.csproj` + `Game.sln`
and a sample script. If the .NET SDK is missing you get a guided install
(`winget install Microsoft.DotNet.SDK.8` on Windows).

**IntelliSense**: `Tools > CSharp > Open C# Solution` launches the IDE chosen in
`Preferences > External > Editors > C# IDE`:

- **Visual Studio** (default) — opens `Game.sln`.
- **VS Code** — opens the `Scripts/CSharp` *folder* via the `code` CLI (that's
  what makes IntelliSense work there; opening the `.csproj` as a file just shows
  XML). Needs the *C# Dev Kit* (or *C#*) extension — VS Code offers to install
  it when it sees the project.
- **Custom** — your own executable + args with `{editor}`, `{solution}`,
  `{project}`, `{folder}` placeholders (e.g. Rider).

The engine API is source-referenced by the csproj, so completion covers the whole
Polyphase surface with the doc comments; the path is refreshed automatically when
the project opens on a different machine.

## Writing a script

```csharp
using Polyphase;

public class SpinnerCS : Script3D
{
    [Property] public Vector3 AngularVelocity = new Vector3(0, 90, 0);
    [Property(Display = "Spin Enabled")] public bool Enabled = true;

    public override void Start()
    {
        Log.Debug("started on " + Name);
    }

    public override void Tick(float deltaTime)
    {
        if (!Enabled) { return; }
        AddRotation(AngularVelocity * deltaTime);   // bare call = the attached node
    }
}
```

- Derive from `Script` (any node), `Script3D` (transform nodes), `ScriptWidget`
  (UI widgets — the Widget API becomes bare calls) or `Script<TNode>` when you
  want `Node` typed as a concrete class (`Script<StaticMesh3D>` → `Node.GetMaterial()`).
- Override only the lifecycle methods you need: `Create` (before serialized
  properties apply), `Awake`, `Start`, `Tick(dt)`, `EditorTick(dt)`, `Stop`,
  `Destroy`, `BeginOverlap(other)`, `EndOverlap(other)`,
  `OnCollision(other, point, normal)`.
- The node API is inherited — `AddRotation(...)`, `Position`, `Name`, `AddTag(...)`
  work like `self:` in Lua. `Node` is the attached node's handle (like Unity's
  `gameObject`); other nodes are held/passed as `Node` / `Node3D` values or a
  concrete handle such as `StaticMesh3D` / `Spline3D` (see *Engine types* below).
- `[Property]` fields appear in the inspector, serialize with the scene, and
  survive hot reload. `[Property(Display = "...")]` sets the inspector label.
- `[Button("Title", "Tooltip")]` on a **public, parameterless** method renders a
  clickable button in the Properties inspector that invokes the method (title
  and tooltip optional — the method name is the default label):

  ```csharp
  [Button("Spin Faster", "Doubles the angular velocity")]
  public void SpinFaster() { AngularVelocity = AngularVelocity * 2.0f; }
  ```

  Lua scripts can do the same by returning
  `{ name = "MethodName", type = DatumType.Function, display_name = "...", tooltip = "..." }`
  from `GatherProperties()`.
- `Lua.GetGlobal / SetGlobal / Call` reach engine Lua APIs not yet wrapped in C#.

Save the file — the editor transpiles automatically (about a second) and
hot-reloads the result. **Ctrl+R** rebuilds C# and reloads all scripts.
The file name must equal the class name (script class names are global across
Engine/Scripts, your Scripts/, and every addon — the transpiler validates this).

## Engine types: nodes, assets, widgets, systems

Engine objects are not C# objects — a `Node3D`, `Button` or `MaterialLite` value
**is** the engine's Lua userdata, and the `Polyphase.*` classes are typed views over
it (every member is a template onto the matching Lua binding, zero runtime cost).
Handles are never constructed with `new`; they come from `[Property]` fields, node
getters, `CreateNode(...)`, the `World`, or `AssetManager`.

**The whole Lua API is available.** Every class, module and enum table the engine
binds to Lua (`Documentation/Lua/`) exists in C# with the same names, generated
straight from the bindings:

| Area | C# |
|---|---|
| Nodes | `Node` → `Node3D` → `Primitive3D` (physics: velocity, forces, collision groups, sweeps) → `Mesh3D` → `StaticMesh3D` / `SkeletalMesh3D` / `TextMesh3D` / `Terrain3D` / `Voxel3D` / `TileMap2D` …; `Camera3D`, `Spline3D`, lights, `Audio3D`, `Particle3D`, `TimelinePlayer`, `NodeGraphPlayer`, … |
| Widgets | `Widget` → `Quad`, `Text`, `Button`, `Canvas`, `Slider`, `InputField`, `CheckBox`, `ProgressBar`, `ComboBox`, `ScrollContainer`, `Window`, `DialogWindow`, `ListViewWidget`, … |
| Assets | `Asset` → `Texture`, `StaticMesh`, `SkeletalMesh`, `Material` → `MaterialLite` / `MaterialInstance`, `SoundWave`, `Font`, `Scene`, `ParticleSystem`, `DataAsset`, `UIDocument`, … |
| Statics | `Engine`, `Input`, `PlayerInput`, `Audio`, `Network`, `Renderer`, `TimerManager`, `Tween`, `SignalBus`, `SaveData`, `AssetManager`, `Http`, `WebSocket`, `Serial`, `Gizmos`, `LoadingMenu`, `WindowManager`, `ToolTip`, `Maths`, `Sys` (Lua `System`), `LuaScript` (Lua `Script`), `Log`, `Debugger` |
| Enums | `AnchorMode`, `Justification`, `BlendMode`, `ShadingModel`, `Easing`, `NetFuncType`, `ButtonState`, `AttenuationFunc`, `ParticleOrientation`, … with the engine's values. Key codes are engine constants: `Key.W`, `Mouse.Left`, `Gamepad.A` (ints). |
| Tables | Lua result tables get phantom classes: `HitResult`, `MultiHitResult`, `AABB`, `Bounds`, `NavPath`, `FogSettings`, `ScreenTrace`, `NetSessionInfo`, `NetClientInfo`, `Rect`. Free-form tables (TileMap cells, `DataAsset.Get`, `HttpResponse.GetJson`) are `object`; read them with `Lua.Get(table, "key")`. |

Conventions that differ from Lua: indices are 0-based where the binding is 1-based
(`GetChild(i)`, spline points, `Network.GetSession(i)`); functions that return
several numbers return a `Vector2` / `Vector3` / `Rect`; Lua node arrays come back
as `Node[]` (`World.FindNodesWithTag`); optional trailing arguments are overloads.
Inside a script, `Node` is the attached node, so allocate new nodes with
`CreateNode("Text")` (the Lua `Node.Construct`) and attach them.

`is`, `as` and casts work on engine values and honour the engine's class hierarchy
(the runtime asks the node/asset itself, the way Lua's `node:Is("StaticMesh3D")`
does). `Node.Is("ClassName")` is the string form.

### Callbacks: signals, timers, tweens

Lambdas and method groups are handed to the engine as plain Lua functions:

```csharp
Node.ConnectSignal("Opened", Node, (listener, who) => Log.Debug("opened by " + who));
int id = TimerManager.SetTimer(() => Respawn(), 3.0f, loop: false);
Tween.Position(Node, Easing.OutQuad, target, 0.5f, () => Log.Debug("arrived"));
Http.Get("https://example.com/api", r => Log.Debug(r.GetBody()));
```

Signal handlers always receive the listener node first (that is how the engine
calls them), then the arguments passed to `EmitSignal(name, a, b, ...)`. Widget events
(`OnActivated`, `OnToggled`, `OnTextChanged`, …), `OnFinished`, `OwnerChanged`,
`OnDrawGizmos` and timeline function tracks are dispatched by **name** to any public
method on the script — just declare `public void OnActivated(Widget button)`.

### Networking

```csharp
public class Player : Script3D
{
    [Replicated(OnRep = nameof(OnHealthChanged))] public int Health = 100;
    [Property] [Replicated] public string DisplayName = "";   // inspector + replicated

    [NetFunc(NetFuncType.Server, Reliable = true)]
    public void S_TakeDamage(int amount) { Health -= amount; }   // runs on the server

    [NetFunc(NetFuncType.Multicast)]
    public void M_PlayHitFx() { /* runs everywhere */ }

    public void OnHealthChanged() { /* client-side, after Health arrived */ }

    public void Hit(int amount) { InvokeNetFunc("S_TakeDamage", amount); }
}
```

`[Replicated]` fields live on the node like `[Property]` fields (same types, no
arrays); `[NetFunc]` methods take up to 8 arguments. `Network.IsAuthority()`,
`IsOwned()` and `HasAuthority()` gate what runs where, exactly as in Lua.

### Materials and arrays

An LED strip driven along a spline:

```csharp
using Polyphase;

public class LEDStrip : Script3D
{
    [Property] public int count = 160;
    [Property] public Spline3D spline;
    [Property] public StaticMesh3D[] leds;

    public void SetLEDColor(int index, Color color)
    {
        if (index < 0 || index >= leds.Length) return;

        var led = leds[index];                       // null if the slot is empty
        var material = led?.GetMaterial() as MaterialLite;
        if (material == null) return;                // graph material, not a lite one

        material.SetColor(color);
    }

    public void LayOut()
    {
        for (int i = 0; i < leds.Length; ++i)
        {
            if (leds[i] == null) continue;
            leds[i].WorldPosition = spline.GetSplinePointWorldPosition(i % spline.GetNumSplinePoints());
        }
    }
}
```

Two things to know about arrays: the C# array **is** the node's Lua table (Lua
scripts and the inspector read the same one, so an assignment like `leds = other`
is visible everywhere), and an unassigned handle slot reads as `null` rather than
throwing. `GetMaterial()` returns the shared asset unless the node has an override —
call `InstantiateMaterial()` once per node before recoloring, or every mesh using
that material changes.

### Widgets from code

```csharp
public class Hud : ScriptWidget            // Node is a Widget; Widget API is bare calls
{
    public override void Start()
    {
        var label = CreateNode("Text") as Text;
        label.SetAnchorMode(AnchorMode.TopLeft);
        label.SetPosition(10, 20);
        label.SetText("Ready");
        label.Attach(Node);
        SetOpacityFloat(1.0f);                 // this widget
    }
}
```

**When a binding is missing or mistyped:** the C# surface is generated from
`Engine/Source/LuaBindings/*_Lua.cpp` by `python Tools/generate_csharp_api.py`
(run it after adding a Lua binding; the test suite fails when it is stale).
Anything can also be reached untyped right away with `Lua.Call(node, "Name", args)`.
Hand-written refinements go in `Tools/PolyphaseSharp/Polyphase.Engine/Polyphase/*.cs`
as `extern` members with a `@CSharpLua.Template` doc comment:

```csharp
/// @CSharpLua.Template = "{this}:SetBakeLighting({0})"
public extern void SetBakeLighting(bool bake);
```

`{this}` is the handle, `{0}`… the arguments; the template is pasted verbatim into
the generated Lua. Rebuild the tool (Ctrl+R in the editor does it) and the member
is available with IntelliSense.

## NuGet and third-party libraries

Binary NuGet packages are **not supported** and cannot be: they ship compiled IL,
the pipeline transpiles C# *source* to Lua, and shipped games have no .NET
runtime to execute IL. Adding a `PackageReference` will fail with a clear
`CS0246` (type not found) at transpile time. Instead:

- **Vendor the source**: pure-C# libraries whose source you have (GitHub,
  source-only packages) can be dropped into `Scripts/CSharp/` as helper classes
  and transpile like your own code — great for pathfinding, noise, easing, FSMs,
  parsers, math. Not for anything using reflection, `unsafe`, native interop,
  I/O, threads, or 64-bit numerics (the subset lints flag most of it).
- **Use the built-in BCL subset**: collections, LINQ, string, Math,
  StringBuilder are already there and bundle only when used.
- **Need native functionality?** That's what native C++ addons are for: expose
  Lua bindings from the addon, then write a thin typed C# façade over them
  (extern members with `@CSharpLua.Template`) — native speed on every platform.

## Calling other scripts — including Lua ones

Cross-script calls use the engine's node-call convention in both languages, in
both directions. A C# `Node`/`Node3D` value IS the node, so a script attached to
it (Lua or C#) is reachable through it.

**C# → Lua script** (chest has a hand-written `Chest.lua` with `Chest:Open(...)`):

```csharp
[Property] public Node3D TargetChest;

// 1. Dynamic — one-liner, no declarations:
object loot = Lua.Call(TargetChest, "Open", Name);
object count = Lua.Get(TargetChest, "lootCount");   // read a script field
Lua.Set(TargetChest, "isOpen", false);              // write one

// 2. Typed façade — declare the Lua script's surface once, get IntelliSense
//    and zero-overhead direct calls (the cast erases at transpile time):
public class ChestHandle : Node3D
{
    /// @CSharpLua.Template = "{this}:Open({0})"
    public extern int Open(string instigatorName);

    /// @CSharpLua.Get = "{this}.isOpen"
    /// @CSharpLua.Set = "{this}.isOpen = {0}"
    public extern bool IsOpen { get; set; }
}

ChestHandle chest = Lua.As<ChestHandle>(TargetChest);
int n = chest.Open(Name);
```

**Lua → C# script**: every public instance method of a C# script is exposed on
its wrapper table, so plain Lua just calls it like any script method:

```lua
local loot = raiderNode:GetLoot()
raiderNode:Taunt("gg")
```

The same is true C#-to-C# across nodes (`Lua.Call(node, "GetLoot")` or a typed
façade). Caveats: overloaded public methods get no Lua-callable slot (Lua has one
method per name — warned as PS2003); values crossing the boundary are Lua values
(numbers are 32-bit floats, tables arrive as `object`).

## Statics and singletons

The Unity-style singleton pattern works as-is:

```csharp
public class GameMgr : Script3D
{
    public static GameMgr Instance;

    public override void Create() { Instance = this; }
    public void AddScore(int points) { /* ... */ }
}

// from any other script:
GameMgr.Instance.AddScore(10);
```

Static fields live on the class's Lua table, shared across the whole VM. Details:

- The transpiler emits a `Script.Require` for every cross-file class reference,
  so the defining script is always loaded before use — even in packaged games,
  where scripts otherwise load on demand. Load *order* between files doesn't
  matter beyond that (references are late-bound).
- `Instance` is the C# companion object, so `Instance.Position`, `Instance.Name`
  etc. work. To pass the underlying engine node somewhere, use `Instance.Node`.
- Attach the manager script to a node that exists before its consumers run, and
  prefer touching `Instance` from `Start`/`Tick` rather than `Create` — node
  `Create` order within a scene is not guaranteed.
- Hot-reloading the manager's file resets its statics; the engine restarts the
  affected script instance, so `Create` re-registers `Instance` automatically.

## How it works

`Scripts/CSharp/Foo.cs` → generated `Scripts/CSharp/Foo.lua` (right next to it)
plus one `Scripts/CSharp/CSharpCore.lua` runtime bundle. From there the normal Lua
pipeline handles everything: hot reload, the script picker, packaging, embedding.
Packaged builds never contain `.cs` sources. The runtime bundle is trimmed to the
BCL modules your code actually uses (about 200 KB / ~500 KB Lua heap for a typical
project; `--no-trim` on the tool keeps everything). Generated files carry a
`~PolyphaseSharp~` marker — never edit them, and never name your own Lua that way.

## Limits (v1)

| Area | Limit |
|---|---|
| Numbers | The engine Lua VM is 32-bit on every platform: `double` **is** `float` at runtime; `long`, `ulong`, `decimal` are unrepresentable (warned as PS2001). |
| Threads / async | No threads. `async`/`await` has no scheduler (warned as PS2002). |
| Reflection | Not supported (no metadata is exported). |
| `[Property]` types | int, short, byte, float, double, bool, string, Vector3, Color, any node handle (Node, Node3D, StaticMesh3D, Spline3D, Camera3D, ...), any asset handle (Asset, Texture, StaticMesh, Material, MaterialLite, ...), and single-dimension arrays of those. Initializers: literals, `new Vector3/Color(<literals>)`, `new T[] { <literals> }`, `new T[N]` (value types only). |
| Engine API coverage | Every Lua binding is generated into the `Polyphase` namespace (see *Engine types*). Functions returning free-form Lua tables (TileMap cells, DataAsset.Get, Http JSON) come back as `object` — read fields with `Lua.Get(table, "key")`. |
| Callbacks | Single-cast delegates and lambdas only. `delegate += other` builds a multicast table the engine rejects. |
| Networking | `[Replicated]` fields cannot be arrays; `[NetFunc]` methods take at most 8 parameters. |
| Script classes | One per file, file name == class name, not generic, public parameterless constructor only. |
| LINQ / collections | Available (List, Dictionary, LINQ, etc. bundle on use) — but they allocate; on console targets prefer plain loops in `Tick`. |
| Debugging | Breakpoints land in the generated `.lua` (readable, source file noted in its header). C# source maps are future work. |
| Performance | C# runs at Lua interpreter speed — same class as hand-written Lua scripts. It is a scripting layer, not a systems language. |

Diagnostics use standard `file(line,col): error CODE` format in the build log —
`CS*` codes are C# compile errors, `PS1xxx` are Polyphase validation errors,
`PS2xxx` are subset warnings.

## Tooling internals

The transpiler lives at `Tools/PolyphaseSharp/` (see its README for architecture).
The editor builds it on first use with `dotnet build` and reruns it when its
sources change. Packaging aborts if C# compilation fails.
