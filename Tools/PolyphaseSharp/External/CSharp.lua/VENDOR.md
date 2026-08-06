# Vendored: CSharp.lua

- **Upstream:** https://github.com/yanghuan/CSharp.lua
- **Pinned commit:** `6dcc5b155a4521305e70ec96d936501d42484075` (2026-07-29)
- **License:** Apache-2.0 (see `LICENSE`)
- **What is vendored:** the `CSharp.lua` compiler library (netstandard2.1) including
  `CoreSystem.Lua` (the Lua BCL runtime). The upstream Launcher, tests, and docker
  files are NOT vendored — PolyphaseSharp has its own CLI driver.

## Local patches (re-apply if updating the vendor)

1. **Implicit-this property templates** — `CSharp.lua/LuaSyntaxNodeTransform.cs`,
   in `VisitPropertyOrEventIdentifierName` (search for `POLYPHASE PATCH`):
   upstream only populates a `LuaPropertyTemplateExpressionSyntax`'s receiver in
   `BuildFieldOrPropertyMemberAccessExpression` (explicit `x.Prop` access). A bare
   `Prop` access inside the class (implicit `this`) left `GetExpression` null and
   crashed `LuaRenderer.Render(LuaCodeTemplateExpressionSyntax)` with an NRE.
   The patch eagerly binds the `this` receiver at creation; an explicit member
   access later calls `Update()` again and harmlessly overwrites.
   The whole Polyphase `Script`/`Script3D` bare-call API relies on this.

2. **Pending-module enumeration** — `CSharp.lua/CoreSystem.Lua/CoreSystem/Core.lua`,
   right after `local modules, imports = {}, {}` (search for `POLYPHASE PATCH`):
   adds `System.getRegisteredModuleNames()` so the generated per-file
   `CSharpCore.Finalize()` glue can `System.init` exactly the types that one
   generated script file registered (required for the engine's per-file script
   hot reload; upstream assumes a single whole-program `System.init`).

## Update procedure

1. Clone upstream at the new commit into a scratch dir.
2. Diff `CSharp.lua/` (excluding `bin`/`obj`) against this tree; port the
   `POLYPHASE PATCH` blocks forward.
3. Rebuild `Tools/PolyphaseSharp` and run its golden-file tests before committing.
