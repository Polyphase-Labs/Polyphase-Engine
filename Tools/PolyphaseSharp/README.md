# PolyphaseSharp — C# scripting for Polyphase via C#→Lua transpilation

Compiles project C# scripts with the latest Roslyn, transpiles them to Lua via a
vendored [CSharp.lua](https://github.com/yanghuan/CSharp.lua) (Apache-2.0), and emits
engine-compatible script files that run on the Lua 5.3 VM the engine already ships on
**every** platform — desktop, Android, Web, PS2, PSP, 3DS, Wii/GameCube, Dreamcast.
Shipped games need no .NET; the .NET SDK (8+) is an authoring-time dependency only.

## How a script flows

```
Scripts/CSharp/Rotator.cs        (user writes this)
        │  polyphasesharp: Roslyn analyze → [Property] rewrite → CSharp.lua → wrapper
        ▼
Scripts/CSharp/Rotator.lua       (generated next to the source; committed)
Scripts/CSharp/CSharpCore.lua    (generated runtime bundle: CoreSystem + engine glue)
        │  nothing special from here on:
        ▼
normal engine Lua pipeline       (hot reload, packaging, embedding, script picker)
```

A C# script:

```csharp
using Polyphase;

public class Rotator : Script3D
{
    [Property] public Vector3 AngularVelocity = new Vector3(0, 90, 0);

    public override void Tick(float deltaTime)
        => AddRotation(AngularVelocity * deltaTime);   // bare calls, Godot-style
}
```

- Derive from `Script` (any node) or `Script3D` (transform nodes); override only the
  lifecycle methods you need (`Create/Awake/Start/Tick/Stop/Destroy/BeginOverlap/...`).
- The node API is inherited — `AddRotation(v)`, `Position`, `Name` operate on the
  attached node like `self:` in Lua. `Node` returns the handle (Unity's `gameObject`);
  other nodes are held as `Node`/`Node3D` values.
- `[Property]` fields appear in the editor inspector, serialize with the scene, and
  survive hot reload — they are rewritten to live on the node itself (the same place
  Lua script properties live).

## Architecture (the companion object)

The engine's script contract is a **global Lua table named after the file**, installed
onto the node userdata. CSharp.lua's class system needs plain-table instances. The
generated file therefore contains both:

1. the CSharp.lua **module** (real C# semantics: inheritance, virtual dispatch, ctors),
   finalized per-file by `CSharpCore.Finalize()` so engine per-file hot reload works;
2. an engine **wrapper** table (`Rotator = {}`) whose `Create()` constructs the C#
   companion instance with the node back-reference pre-seeded (`CSharpCore.New`), and
   which forwards exactly the lifecycle methods the C# class overrides.

`[Property]` accessors compile to `this.__node.FieldName` — the node uservalue is the
single source of truth, which is what makes inspector edits, scene serialization and
hot-reload restore work unchanged.

## Layout

- `PolyphaseSharp/` — the CLI (`polyphasesharp --scripts <Proj>/Scripts/CSharp [--check]`).
- `Polyphase.Engine/` — the C# reference assembly (IntelliSense + transpile surface);
  extern members carry `@CSharpLua.Template` doc comments mapping to engine Lua bindings.
- `External/CSharp.lua/` — vendored, pinned compiler + CoreSystem runtime; local
  patches documented in `External/CSharp.lua/VENDOR.md`.
- `Tests/` — end-to-end suite: transpile the sample, run it under a Lua 5.3 interpreter
  built from the engine's own `External/Lua` (same `LUA_32BITS` semantics).
  Run: `pwsh Tools/PolyphaseSharp/Tests/run_tests.ps1`

## v1 limits (enforced by the validator, PS-prefixed diagnostics)

- One script class per file; file name must equal class name; no generic script classes;
  public parameterless ctor only.
- `[Property]` types: int, short, byte, float, double, bool, string, Vector3, Color,
  Node, Node3D. Initializers: literals or `new Vector3/Color(<literals>)`. No arrays yet.
- Engine Lua is 32-bit (`LUA_32BITS`): `double` is really `float`; `long`/`ulong`/
  `decimal` are unrepresentable (warned). No threads; async has no scheduler (warned).
- Debugging is at the generated-Lua level (readable output, source file noted in the
  header comment); C# source maps are future work.
