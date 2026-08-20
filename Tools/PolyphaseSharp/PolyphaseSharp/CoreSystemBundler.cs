using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace PolyphaseSharp
{
    /// <summary>
    /// Builds Scripts/CSharp/CSharpCore.lua: the CSharp.lua CoreSystem runtime
    /// (single-file bundle, same technique as upstream GenerateSingleFile), the
    /// compiled Polyphase API module, and the engine glue (CSharpCore.New /
    /// CSharpCore.Finalize).
    /// </summary>
    public static class CoreSystemBundler
    {
        private static readonly Regex kLoadLine = new("load\\(\"([^\"]+)\"\\)", RegexOptions.Compiled);

        /// <summary>
        /// A CoreSystem module we may leave out of the bundle when nothing in the
        /// project's generated Lua references it. Consoles are the audience: the
        /// full bundle costs ~1.2 MB of Lua heap, painful on a 32 MB PS2.
        /// Tokens are matched word-bounded against ALL generated user code;
        /// over-keeping is always safe, dropping is only done on zero hits.
        /// KeptBy names other droppable modules that lazily reach into this one
        /// (verified by grepping CoreSystem cross-references at vendoring time).
        /// </summary>
        private sealed class DroppableModule
        {
            public string Name;             // All.lua load() name
            public string[] Tokens;         // usage tokens that force keeping it
            public string[] KeptBy = new string[0]; // kept if any of these modules are kept
        }

        private static readonly DroppableModule[] kDroppable =
        {
            new DroppableModule { Name = "TimeSpan", Tokens = new[]{ "TimeSpan", "DateTime" }, KeptBy = new[]{ "Convert" } },
            new DroppableModule { Name = "DateTime", Tokens = new[]{ "DateTime" }, KeptBy = new[]{ "Convert" } },
            new DroppableModule { Name = "Collections.Queue", Tokens = new[]{ "Queue" } },
            new DroppableModule { Name = "Collections.Stack", Tokens = new[]{ "Stack" } },
            new DroppableModule { Name = "Collections.HashSet", Tokens = new[]{ "HashSet", "ISet" } },
            new DroppableModule { Name = "Collections.LinkedList", Tokens = new[]{ "LinkedList" } },
            new DroppableModule { Name = "Collections.SortedSet", Tokens = new[]{ "SortedSet" } },
            new DroppableModule { Name = "Collections.SortedList", Tokens = new[]{ "SortedList" } },
            new DroppableModule { Name = "Collections.SortedDictionary", Tokens = new[]{ "SortedDictionary" } },
            new DroppableModule { Name = "Collections.PriorityQueue", Tokens = new[]{ "PriorityQueue" } },
            new DroppableModule { Name = "Collections.Linq", Tokens = new[]{ "Linq" } },
            new DroppableModule { Name = "Convert", Tokens = new[]{ "Convert" } },
            new DroppableModule { Name = "Random", Tokens = new[]{ "Random" } },
            new DroppableModule { Name = "Text.StringBuilder", Tokens = new[]{ "StringBuilder" },
                                  KeptBy = new[]{ "Numerics.Matrix3x2", "Numerics.Matrix4x4", "Numerics.Plane",
                                                  "Numerics.Quaternion", "Numerics.Vector2", "Numerics.Vector3",
                                                  "Numerics.Vector4" } },
            new DroppableModule { Name = "Console", Tokens = new[]{ "Console" } },
            new DroppableModule { Name = "IO.File", Tokens = new[]{ "File", "IO" } },
            new DroppableModule { Name = "Reflection.Assembly", Tokens = new[]{ "Assembly", "Reflection", "getType", "GetType", "typeof" } },
            new DroppableModule { Name = "Threading.Timer", Tokens = new[]{ "Timer", "Thread", "Task", "async", "await" } },
            new DroppableModule { Name = "Threading.Thread", Tokens = new[]{ "Thread", "Task", "async", "await", "Monitor" } },
            new DroppableModule { Name = "Threading.Task", Tokens = new[]{ "Task", "async", "await" } },
            new DroppableModule { Name = "Globalization.Globalization", Tokens = new[]{ "Globalization", "Culture" } },
            new DroppableModule { Name = "Numerics.HashCodeHelper", Tokens = new[]{ "Numerics", "Complex", "Quaternion", "Matrix3x2", "Matrix4x4", "Plane" } },
            new DroppableModule { Name = "Numerics.Complex", Tokens = new[]{ "Complex" } },
            new DroppableModule { Name = "Numerics.Matrix3x2", Tokens = new[]{ "Matrix3x2" } },
            new DroppableModule { Name = "Numerics.Matrix4x4", Tokens = new[]{ "Matrix4x4" } },
            new DroppableModule { Name = "Numerics.Plane", Tokens = new[]{ "Plane" } },
            new DroppableModule { Name = "Numerics.Quaternion", Tokens = new[]{ "Quaternion" } },
            new DroppableModule { Name = "Numerics.Vector", Tokens = new[]{ "Numerics" } },
            new DroppableModule { Name = "Numerics.Vector2", Tokens = new[]{ "Numerics" } },
            new DroppableModule { Name = "Numerics.Vector3", Tokens = new[]{ "Numerics" } },
            new DroppableModule { Name = "Numerics.Vector4", Tokens = new[]{ "Numerics" } },
        };

        /// <param name="coreSystemDir">Directory containing All.lua and CoreSystem/.</param>
        /// <param name="apiModuleLua">CSharp.lua output for the Polyphase API sources.</param>
        /// <param name="usageText">All generated user Lua concatenated — scanned for
        /// module-usage tokens. Null/empty disables trimming (full bundle).</param>
        /// <param name="droppedModules">Receives the names of modules left out.</param>
        /// <param name="apiTypeOrder">API type Lua paths, base-before-derived.</param>
        public static string Build(string coreSystemDir, IEnumerable<string> apiModuleLua,
                                   string usageText, List<string> droppedModules,
                                   IReadOnlyList<string> apiTypeOrder)
        {
            string allFile = Path.Combine(coreSystemDir, "All.lua");
            if (!File.Exists(allFile))
                throw new FileNotFoundException("CoreSystem All.lua not found", allFile);

            var moduleNames = new List<string>();
            foreach (string line in File.ReadAllLines(allFile))
            {
                var m = kLoadLine.Match(line);
                if (m.Success)
                    moduleNames.Add(m.Groups[1].Value);
            }

            // Trim pass: figure out which droppable modules nothing references.
            var dropped = new HashSet<string>(StringComparer.Ordinal);
            if (!string.IsNullOrEmpty(usageText))
            {
                foreach (var mod in kDroppable)
                {
                    bool used = mod.Tokens.Any(t =>
                        Regex.IsMatch(usageText, $@"\b{Regex.Escape(t)}\b"));
                    if (!used)
                        dropped.Add(mod.Name);
                }
                // Keep modules a kept module lazily depends on (iterate to fixpoint —
                // the chains here are one deep, but cheap to be thorough).
                bool changed = true;
                while (changed)
                {
                    changed = false;
                    foreach (var mod in kDroppable)
                    {
                        if (dropped.Contains(mod.Name) &&
                            mod.KeptBy.Any(k => !dropped.Contains(k) && moduleNames.Contains(k)))
                        {
                            dropped.Remove(mod.Name);
                            changed = true;
                        }
                    }
                }
            }
            droppedModules?.AddRange(dropped.OrderBy(s => s, StringComparer.Ordinal));

            var libFiles = new List<string>();
            foreach (string name in moduleNames)
            {
                if (dropped.Contains(name))
                    continue;
                libFiles.Add(Path.Combine(coreSystemDir, "CoreSystem", name.Replace('.', '/') + ".lua"));
            }

            var sb = new StringBuilder(1 << 20);
            sb.Append(WrapperEmitter.Marker)
              .AppendLine(" CSharpCore runtime bundle - DO NOT EDIT, regenerated on every C# build.");
            sb.AppendLine("-- CSharp.lua CoreSystem (Apache-2.0, https://github.com/yanghuan/CSharp.lua)");
            sb.AppendLine();
            // Engine script contract: this file's global class table.
            sb.AppendLine("CSharpCore = CSharpCore or {}");
            sb.AppendLine();
            // Hot reload guard: re-running this chunk must not rebuild the runtime —
            // live class metatables would be orphaned. Restart the game to swap cores.
            sb.AppendLine("if rawget(_G, \"__POLYPHASE_CSHARP_CORE__\") then return end");
            sb.AppendLine("__POLYPHASE_CSHARP_CORE__ = true");
            sb.AppendLine();
            sb.AppendLine("CSharpLuaSingleFile = true");

            bool first = true;
            foreach (string lib in libFiles)
            {
                string code = File.ReadAllText(lib);
                if (!first)
                    code = StripLicenseComment(code);
                first = false;
                // Comments/blank lines are pure embed-size cost on consoles.
                code = LuaMinifier.Strip(code);
                sb.AppendLine();
                sb.Append("-- CoreSystemLib: ").AppendLine(Path.GetFileName(lib));
                sb.AppendLine("do");
                sb.AppendLine(code);
                sb.AppendLine("end");
            }

            sb.AppendLine();
            sb.AppendLine("-- ===== Polyphase engine API module =====");
            foreach (string module in apiModuleLua)
            {
                sb.AppendLine("do");
                sb.AppendLine(module.TrimEnd());
                sb.AppendLine("end");
            }

            sb.AppendLine();
            sb.Append("local POLYPHASE_API_TYPE_ORDER = { ");
            if (apiTypeOrder != null)
            {
                sb.Append(string.Join(", ", apiTypeOrder.Select(n => "\"" + n + "\"")));
            }
            sb.AppendLine(" }");
            sb.AppendLine(@"-- ===== Engine glue =====

-- Construct a companion instance with the node back-reference already in place,
-- so C# constructors and Create() can touch [Property] accessors (which route
-- through __node). Mirrors CoreSystem new(): setmetatable + __ctor__.
function CSharpCore.New(cls, node)
    local inst = setmetatable({ __node = node }, cls)
    local ctor = rawget(cls, ""__ctor__"")
    if ctor ~= nil then
        if type(ctor) == ""table"" then ctor = ctor[1] end
        ctor(inst)
    end
    return inst
end

-- Finalize every type the current generated file just registered. Clears stale
-- published globals first so the engine's per-file script hot reload can re-run
-- a generated chunk without tripping CoreSystem's duplicate-publish asserts.
-- `ordered` (optional array of dotted type names) is emitted by the transpiler
-- in inheritance-depth order: System.init resolves a class's base eagerly, so
-- initing a derived type before its same-batch base fails with 'base is nil'.
-- Pending types missing from the list are appended (sorted) as a safety net;
-- listed-but-unregistered names are skipped.
function CSharpCore.Finalize(ordered)
    local pending = {}
    local pendingCount = 0
    for _, n in ipairs(System.getRegisteredModuleNames()) do
        pending[n] = true
        pendingCount = pendingCount + 1
    end
    if pendingCount == 0 then return end

    local names = {}
    if ordered ~= nil then
        for i = 1, #ordered do
            local n = ordered[i]
            if pending[n] then
                names[#names + 1] = n
                pending[n] = nil
            end
        end
    end
    local rest = {}
    for n in pairs(pending) do rest[#rest + 1] = n end
    table.sort(rest)
    for i = 1, #rest do names[#names + 1] = rest[i] end

    for i = 1, #names do
        local scope, key = _G, nil
        for part in string.gmatch(names[i], ""[^%.]+"") do
            if key ~= nil then
                scope = rawget(scope, key)
                if scope == nil then break end
            end
            key = part
        end
        if scope ~= nil and key ~= nil then
            rawset(scope, key, nil)
        end
    end
    System.init({ types = names })
end

-- Finalize the Polyphase API types registered above (inheritance-depth order).
CSharpCore.Finalize(POLYPHASE_API_TYPE_ORDER)");

            return sb.ToString();
        }

        private static string StripLicenseComment(string code)
        {
            const string begin = "--[[";
            const string end = "--]]";
            int i = code.IndexOf(begin, StringComparison.Ordinal);
            if (i != -1 && code[..i].All(char.IsWhiteSpace))
            {
                int j = code.IndexOf(end, i + begin.Length, StringComparison.Ordinal);
                if (j != -1)
                    return code[(j + end.Length)..].Trim();
            }
            return code;
        }
    }
}
