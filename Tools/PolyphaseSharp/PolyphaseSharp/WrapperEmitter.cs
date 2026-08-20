using System.Collections.Generic;
using System.Text;

namespace PolyphaseSharp
{
    /// <summary>
    /// Emits the final per-file .lua: generated-file marker, Require lines, the
    /// CSharp.lua module body, CSharpCore.Finalize(), and — for script classes —
    /// the engine wrapper (companion-object glue).
    /// </summary>
    public static class WrapperEmitter
    {
        /// <summary>First-line marker identifying files owned by the transpiler
        /// (orphan cleanup keys on this; never touches hand-written Lua).</summary>
        public const string Marker = "-- ~PolyphaseSharp~";

        // Engine dispatch signatures: wrapper params → args forwarded to the companion.
        // Overlap/collision handlers drop thisNode (it equals the script's own node).
        private static readonly Dictionary<string, (string Params, string Args)> kForward = new()
        {
            ["Awake"] = ("", ""),
            ["Start"] = ("", ""),
            ["Stop"] = ("", ""),
            ["Destroy"] = ("", ""),
            ["Tick"] = ("deltaTime", "deltaTime"),
            ["EditorTick"] = ("deltaTime", "deltaTime"),
            ["BeginOverlap"] = ("thisNode, otherNode", "otherNode"),
            ["EndOverlap"] = ("thisNode, otherNode", "otherNode"),
            ["OnCollision"] = ("thisNode, otherNode, impactPoint, impactNormal", "otherNode, impactPoint, impactNormal"),
        };

        public static string Emit(SourceUnit unit, string moduleLua, string requirePrefix)
        {
            var sb = new StringBuilder(moduleLua.Length + 1024);
            sb.Append(Marker).Append(" generated from ").Append(unit.RelativePath)
              .AppendLine(" - DO NOT EDIT, changes will be overwritten.");
            sb.AppendLine($"Script.Require(\"{requirePrefix}/CSharpCore\")");
            foreach (string dep in unit.RequiredFiles)
            {
                sb.AppendLine($"Script.Require(\"{requirePrefix}/{dep}\")");
            }
            sb.AppendLine();
            sb.AppendLine(moduleLua.TrimEnd());
            sb.AppendLine();
            if (unit.FinalizeOrder.Count > 0)
            {
                // Explicit inheritance-depth order — System.init resolves bases
                // eagerly within a batch (see Analyzer.GetFinalizeOrder).
                sb.Append("CSharpCore.Finalize({ ");
                sb.Append(string.Join(", ", unit.FinalizeOrder.ConvertAll(n => "\"" + n + "\"")));
                sb.AppendLine(" })");
            }
            else
            {
                sb.AppendLine("CSharpCore.Finalize()");
            }

            var script = unit.Script;
            if (script == null)
            {
                // Helper file (no script class): the engine's script loader still
                // requires a global table named after the file basename.
                string stub = System.IO.Path.GetFileNameWithoutExtension(unit.RelativePath);
                sb.AppendLine();
                sb.Append(stub).Append(" = ").Append(stub).AppendLine(" or {}");
                return sb.ToString();
            }

            string cls = script.ClassName;
            sb.AppendLine();
            sb.Append(cls).AppendLine(" = {}");
            sb.AppendLine();

            // Create: property defaults (pre-serialization), then companion construction.
            sb.Append("function ").Append(cls).AppendLine(":Create()");
            foreach (var prop in script.Properties)
            {
                if (prop.DefaultLuaLiteral != null)
                    sb.Append("    self.").Append(prop.Name).Append(" = ").AppendLine(prop.DefaultLuaLiteral);
            }
            sb.Append("    self.__cs = CSharpCore.New(").Append(script.FullLuaPath).AppendLine(", self)");
            if (script.OverridesCreate)
                sb.AppendLine("    self.__cs:Create()");
            sb.AppendLine("end");

            if (script.Properties.Count > 0 || script.Buttons.Count > 0)
            {
                sb.AppendLine();
                sb.Append("function ").Append(cls).AppendLine(":GatherProperties()");
                sb.AppendLine("    return {");
                foreach (var prop in script.Properties)
                {
                    sb.Append("        { name = \"").Append(prop.Name)
                      .Append("\", type = DatumType.").Append(prop.DatumType);
                    if (!string.IsNullOrEmpty(prop.DisplayName))
                        sb.Append(", display_name = ").Append(QuoteLua(prop.DisplayName));
                    sb.AppendLine(" },");
                }
                foreach (var button in script.Buttons)
                {
                    // Function props are editor-only buttons; the click handler
                    // calls the same-named class-table method (forwarded below).
                    sb.Append("        { name = \"").Append(button.MethodName)
                      .Append("\", type = DatumType.Function");
                    if (!string.IsNullOrEmpty(button.Title))
                        sb.Append(", display_name = ").Append(QuoteLua(button.Title));
                    if (!string.IsNullOrEmpty(button.Tooltip))
                        sb.Append(", tooltip = ").Append(QuoteLua(button.Tooltip));
                    sb.AppendLine(" },");
                }
                sb.AppendLine("    }");
                sb.AppendLine("end");
            }

            foreach (string method in script.OverriddenMethods)
            {
                var (params_, args) = kForward[method];
                sb.AppendLine();
                sb.Append("function ").Append(cls).Append(':').Append(method)
                  .Append('(').Append(params_).AppendLine(")");
                sb.Append("    self.__cs:").Append(method).Append('(').Append(args).AppendLine(")");
                sb.AppendLine("end");
            }

            // Cross-script surface: public C# methods become wrapper methods, so
            // Lua scripts (and other C# scripts holding a node handle) can call
            // `node:Method(args)` exactly like they would on a Lua script.
            foreach (var method in script.PublicMethods)
            {
                string paramList = string.Join(", ", method.ParamNames);
                sb.AppendLine();
                sb.Append("function ").Append(cls).Append(':').Append(method.Name)
                  .Append('(').Append(paramList).AppendLine(")");
                sb.Append("    return self.__cs:").Append(method.Name)
                  .Append('(').Append(paramList).AppendLine(")");
                sb.AppendLine("end");
            }

            return sb.ToString();
        }

        private static string QuoteLua(string s)
        {
            return "\"" + s
                .Replace("\\", "\\\\")
                .Replace("\"", "\\\"")
                .Replace("\n", "\\n")
                .Replace("\r", "\\r") + "\"";
        }
    }
}
