using System.Collections.Generic;

namespace PolyphaseSharp
{
    /// <summary>One [Property] field exposed to the editor.</summary>
    public sealed class ScriptProperty
    {
        public string Name;              // exact C# field name (kept verbatim in Lua)
        public string DatumType;         // Lua DatumType enum member name, e.g. "Vector"
        public string DisplayName;       // optional [Property(Display = "...")]
        public string DefaultLuaLiteral; // Lua expression for the initializer, or null
        public string CSharpType;        // for diagnostics
    }

    /// <summary>A public instance method exposed on the engine wrapper so Lua
    /// (and other scripts via node calls) can invoke it: `node:Name(args)`.</summary>
    public sealed class ScriptMethod
    {
        public string Name;
        public string[] ParamNames;  // sanitized to valid, non-keyword Lua identifiers
    }

    /// <summary>An inspector button ([Button] method) — rendered via a
    /// DatumType.Function property whose click calls the same-named method.</summary>
    public sealed class ScriptButton
    {
        public string MethodName;
        public string Title;    // null → method name
        public string Tooltip;  // null → none
    }

    /// <summary>A concrete class deriving from Polyphase.Script found in user sources.</summary>
    public sealed class ScriptClass
    {
        public string ClassName;                 // simple name == required file basename
        public string FullLuaPath;               // dotted path after namespace enforcement, e.g. "Game.Rotator"
        public string SourceFile;                // absolute path of the defining .cs
        public bool OverridesCreate;
        public readonly List<string> OverriddenMethods = new();  // lifecycle names except Create
        public readonly List<ScriptProperty> Properties = new(); // whole chain, base-first
        public readonly List<ScriptMethod> PublicMethods = new(); // forwarded for cross-script calls
        public readonly List<ScriptButton> Buttons = new();       // [Button] methods, chain order
    }

    /// <summary>Per-.cs-file transpile unit.</summary>
    public sealed class SourceUnit
    {
        public string SourceFile;                 // absolute .cs path
        public string RelativePath;               // relative to the scripts root, fwd slashes
        public string RewrittenText;              // source after [Property]/namespace rewrite
        public ScriptClass Script;                // at most one per file (validated); may be null (helper file)
        public readonly List<string> RequiredFiles = new(); // other generated files (no ext) this one must Require first
        // Lua paths of every type this file registers, base classes before
        // derived — System.init resolves bases eagerly within a batch, so
        // Finalize must init in inheritance order, not alphabetical order.
        public readonly List<string> FinalizeOrder = new();
    }

    public sealed class Diagnostic
    {
        public string File;
        public int Line;      // 1-based
        public int Column;    // 1-based
        public bool IsError;
        public string Code;   // e.g. "PS0001"
        public string Message;

        public override string ToString()
            => $"{File}({Line},{Column}): {(IsError ? "error" : "warning")} {Code}: {Message}";
    }
}
