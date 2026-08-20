using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Text;

namespace PolyphaseSharp
{
    /// <summary>
    /// PolyphaseSharp — C# → Lua transpiler for Polyphase engine scripts.
    ///
    ///   polyphasesharp --scripts &lt;Project&gt;/Scripts/CSharp [--check] [--verbose]
    ///
    /// Reads *.cs under the scripts dir, compiles with Roslyn + CSharp.lua, and
    /// writes one engine-compatible .lua next to each .cs (plus CSharpCore.lua,
    /// the runtime bundle). Exit codes: 0 ok, 1 compile/validation error, 2 usage.
    /// </summary>
    public static class Program
    {
        private static bool sVerbose;

        public static int Main(string[] args)
        {
            string scriptsDir = null;
            string scriptsRoot = null;
            var extraLuaRoots = new List<string>();
            bool checkOnly = false;
            bool noTrim = false;

            for (int i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--scripts": scriptsDir = Next(args, ref i); break;
                    case "--scripts-root": scriptsRoot = Next(args, ref i); break;
                    // Additional .lua script roots (';'-separated) whose class names
                    // must not collide with C# script classes — the engine's script
                    // namespace is global across Engine/Scripts, Scripts/, and every
                    // Packages/*/Scripts (packaging silently drops duplicates).
                    case "--lua-roots":
                        foreach (string root in Next(args, ref i).Split(';', StringSplitOptions.RemoveEmptyEntries))
                            extraLuaRoots.Add(root);
                        break;
                    case "--check": checkOnly = true; break;
                    case "--no-trim": noTrim = true; break;
                    case "--verbose": sVerbose = true; break;
                    case "--help" or "-h":
                        PrintUsage();
                        return 0;
                    default:
                        Console.Error.WriteLine($"polyphasesharp: unknown argument '{args[i]}'");
                        PrintUsage();
                        return 2;
                }
            }

            if (scriptsDir == null)
            {
                PrintUsage();
                return 2;
            }

            scriptsDir = Path.GetFullPath(scriptsDir);
            if (!Directory.Exists(scriptsDir))
            {
                Console.Error.WriteLine($"polyphasesharp: scripts directory not found: {scriptsDir}");
                return 2;
            }
            scriptsRoot = Path.GetFullPath(scriptsRoot ?? Path.GetDirectoryName(scriptsDir));
            string requirePrefix = Path.GetRelativePath(scriptsRoot, scriptsDir).Replace('\\', '/');

            try
            {
                return Run(scriptsDir, scriptsRoot, extraLuaRoots, requirePrefix, checkOnly, noTrim);
            }
            catch (CSharpLua.CompilationErrorException e)
            {
                // CSharp.lua's own diagnostics (post-rewrite); rare because our
                // Roslyn pass runs first, but e.g. unsupported-construct errors land here.
                Console.WriteLine(e.Message);
                return 1;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine($"polyphasesharp: internal error: {e}");
                return 1;
            }
        }

        private static int Run(string scriptsDir, string scriptsRoot, List<string> extraLuaRoots,
                               string requirePrefix, bool checkOnly, bool noTrim)
        {
            string exeDir = AppContext.BaseDirectory;
            string apiDir = Path.Combine(exeDir, "EngineApi");
            string coreSystemDir = Path.Combine(exeDir, "CoreSystem.Lua");

            // ---- gather sources ----
            var userFiles = Directory.EnumerateFiles(scriptsDir, "*.cs", SearchOption.AllDirectories)
                .Where(f => !IsInBuildDir(scriptsDir, f))
                .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
                .ToList();

            if (userFiles.Count == 0)
            {
                int removed = CleanOrphans(scriptsDir, new HashSet<string>());
                Console.WriteLine($"polyphasesharp: no C# sources under {scriptsDir}" +
                                  (removed > 0 ? $" ({removed} stale generated file(s) removed)" : ""));
                return 0;
            }

            // Duplicate basenames break the engine's class-name-from-filename model
            // (and embedded script keying) project-wide.
            var byBase = userFiles.GroupBy(f => Path.GetFileNameWithoutExtension(f), StringComparer.OrdinalIgnoreCase);
            bool namesOk = true;
            foreach (var group in byBase.Where(g => g.Count() > 1))
            {
                Console.WriteLine($"{group.First()}(1,1): error PS1010: multiple C# files share the base name " +
                                  $"'{group.Key}' — script/helper file names must be unique: {string.Join(", ", group)}");
                namesOk = false;
            }
            foreach (string f in userFiles)
            {
                string baseName = Path.GetFileNameWithoutExtension(f);
                if (!Regex.IsMatch(baseName, "^[A-Za-z_][A-Za-z0-9_]*$"))
                {
                    Console.WriteLine($"{f}(1,1): error PS1011: file base name '{baseName}' must be a valid " +
                                      "identifier (it becomes the Lua script class name)");
                    namesOk = false;
                }
            }
            if (!namesOk)
                return 1;

            var apiFiles = Directory.EnumerateFiles(apiDir, "*.cs", SearchOption.AllDirectories)
                .OrderBy(f => f, StringComparer.OrdinalIgnoreCase).ToList();
            if (apiFiles.Count == 0)
            {
                Console.Error.WriteLine($"polyphasesharp: engine API sources missing at {apiDir}");
                return 1;
            }

            // Hand-written .lua class names for collision validation: the whole
            // project Scripts tree plus any extra roots the editor passes in
            // (Engine/Scripts, addon Packages) — excluding our own generated files.
            var existingLua = new HashSet<string>(StringComparer.Ordinal);
            var luaRoots = new List<string> { scriptsRoot };
            luaRoots.AddRange(extraLuaRoots);
            foreach (string root in luaRoots)
            {
                if (!Directory.Exists(root))
                    continue;
                foreach (string lua in Directory.EnumerateFiles(root, "*.lua", SearchOption.AllDirectories))
                {
                    if (!IsGenerated(lua))
                        existingLua.Add(Path.GetFileNameWithoutExtension(lua));
                }
            }

            // ---- parse + analyze (original sources) ----
            var parseOptions = new CSharpParseOptions(LanguageVersion.Latest, DocumentationMode.Parse);
            var userTrees = userFiles.Select(f => CSharpSyntaxTree.ParseText(
                SourceText.From(File.ReadAllText(f), Encoding.UTF8), parseOptions, path: f)).ToList();
            var apiTrees = apiFiles.Select(f => CSharpSyntaxTree.ParseText(
                SourceText.From(File.ReadAllText(f), Encoding.UTF8), parseOptions, path: f)).ToList();

            var analyzer = new Analyzer();
            analyzer.Compile(userTrees, apiTrees);

            var units = new List<SourceUnit>();
            if (!analyzer.HasErrors)
            {
                foreach (var tree in userTrees)
                    units.Add(analyzer.Analyze(tree, scriptsDir, existingLua));
            }

            foreach (var diag in analyzer.Diagnostics)
                Console.WriteLine(diag.ToString());
            if (analyzer.HasErrors)
                return 1;
            if (checkOnly)
            {
                Console.WriteLine($"polyphasesharp: check ok — {units.Count(u => u.Script != null)} script class(es), " +
                                  $"{analyzer.Diagnostics.Count} warning(s)");
                return 0;
            }

            // ---- rewrite + transpile ----
            string tempRoot = Path.Combine(Path.GetTempPath(), "PolyphaseSharp", Guid.NewGuid().ToString("N"));
            string tempSrc = Path.Combine(tempRoot, "src");
            string tempOut = Path.Combine(tempRoot, "out");
            Directory.CreateDirectory(tempSrc);
            Directory.CreateDirectory(tempOut);

            try
            {
                for (int i = 0; i < userTrees.Count; ++i)
                {
                    units[i].RewrittenText = SourceRewriter.Rewrite(userTrees[i], Analyzer.DefaultNamespace);
                    string dst = Path.Combine(tempSrc, units[i].RelativePath);
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    File.WriteAllText(dst, units[i].RewrittenText, Utf8NoBom);
                }

                // API sources concatenate into one uniquely-named unit so its output
                // is easy to find and can never collide with a user file's output.
                // Usings are hoisted (a using mid-file is a syntax error).
                var apiConcat = new StringBuilder();
                apiConcat.AppendLine("#pragma warning disable CS0626");
                var seenUsings = new HashSet<string>(StringComparer.Ordinal);
                var apiBodies = new StringBuilder();
                foreach (var tree in apiTrees)
                {
                    var root = tree.GetCompilationUnitRoot();
                    foreach (var use in root.Usings)
                    {
                        if (seenUsings.Add(use.ToString()))
                            apiConcat.AppendLine(use.ToString());
                    }
                    foreach (var member in root.Members)
                        apiBodies.AppendLine(member.ToFullString());
                }
                apiConcat.Append(apiBodies);
                File.WriteAllText(Path.Combine(tempSrc, "__PolyphaseEngineApi__.cs"), apiConcat.ToString(), Utf8NoBom);

                Verbose($"transpiling {userFiles.Count} file(s) via CSharp.lua...");
                var compiler = new CSharpLua.Compiler(tempSrc, tempOut, "", null, "", false, null, null);
                compiler.Compile();

                // ---- emit final files ----
                string apiModule = ReadOutLua(tempOut, "__PolyphaseEngineApi__");
                if (apiModule == null)
                {
                    Console.Error.WriteLine("polyphasesharp: internal error: engine API module output missing");
                    return 1;
                }

                var produced = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                int written = 0, unchanged = 0;

                // Cross-file references: a file's module may reach another user
                // class at runtime (statics/singletons like `GameMgr.Instance`,
                // helper calls). Each generated file must Require the defining
                // file so packaged games — where scripts load on demand — never
                // see an unloaded class. Map every user class's Lua path to its
                // defining file, then scan each module's emitted text.
                var classPathToFile = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var unit in units)
                {
                    string relNoExt = unit.RelativePath.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)
                        ? unit.RelativePath[..^3] : unit.RelativePath;
                    var root = CSharpSyntaxTree.ParseText(unit.RewrittenText).GetCompilationUnitRoot();
                    foreach (var cls in root.DescendantNodes().OfType<Microsoft.CodeAnalysis.CSharp.Syntax.BaseTypeDeclarationSyntax>())
                    {
                        string ns = cls.Ancestors().OfType<Microsoft.CodeAnalysis.CSharp.Syntax.BaseNamespaceDeclarationSyntax>()
                            .Select(n => n.Name.ToString()).FirstOrDefault() ?? Analyzer.DefaultNamespace;
                        classPathToFile[ns + "." + cls.Identifier.Text] = relNoExt;
                    }
                }

                // Emit user files first so the CoreSystem bundle can be trimmed
                // against everything the project's generated Lua actually uses.
                var finals = new List<(string Dst, string Content)>();
                var usage = new StringBuilder();
                foreach (var unit in units)
                {
                    string relNoExt = unit.RelativePath.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)
                        ? unit.RelativePath[..^3] : unit.RelativePath;
                    string moduleLua = ReadOutLua(tempOut, relNoExt);
                    if (moduleLua == null)
                    {
                        Console.Error.WriteLine($"polyphasesharp: internal error: no transpiler output for {unit.RelativePath}");
                        return 1;
                    }

                    foreach (var (classPath, definingFile) in classPathToFile)
                    {
                        if (definingFile == relNoExt || unit.RequiredFiles.Contains(definingFile))
                            continue;
                        if (Regex.IsMatch(moduleLua, $@"\b{Regex.Escape(classPath)}\b"))
                            unit.RequiredFiles.Add(definingFile);
                    }

                    string final = WrapperEmitter.Emit(unit, moduleLua, requirePrefix);
                    finals.Add((Path.Combine(scriptsDir, relNoExt + ".lua"), final));
                    usage.AppendLine(final);
                }

                var droppedModules = new List<string>();
                var apiTypeOrder = analyzer.GetFinalizeOrder(apiTrees);
                string core = CoreSystemBundler.Build(coreSystemDir, new[] { apiModule },
                    noTrim ? null : usage.ToString(), droppedModules, apiTypeOrder);
                string corePath = Path.Combine(scriptsDir, "CSharpCore.lua");
                produced.Add(Path.GetFullPath(corePath));
                if (WriteIfChanged(corePath, core)) ++written; else ++unchanged;

                foreach (var (dst, content) in finals)
                {
                    produced.Add(Path.GetFullPath(dst));
                    if (WriteIfChanged(dst, content)) ++written; else ++unchanged;
                }

                int removed = CleanOrphans(scriptsDir, produced);
                if (droppedModules.Count > 0)
                {
                    // Never trim silently — a wrongly-dropped module should be
                    // findable from this line (and overridable with --no-trim).
                    Console.WriteLine($"polyphasesharp: CoreSystem trimmed, {droppedModules.Count} unused module(s) " +
                                      $"left out: {string.Join(", ", droppedModules)} (use --no-trim to keep all)");
                }
                Console.WriteLine($"polyphasesharp: {units.Count} file(s) transpiled — {written} written, " +
                                  $"{unchanged} unchanged, {removed} orphan(s) removed, " +
                                  $"{analyzer.Diagnostics.Count} warning(s)");
                return 0;
            }
            finally
            {
                try { Directory.Delete(tempRoot, true); } catch { /* best effort */ }
            }
        }

        // ---- helpers ----

        private static readonly UTF8Encoding Utf8NoBom = new(false);

        private static string Next(string[] args, ref int i)
        {
            if (i + 1 >= args.Length)
            {
                Console.Error.WriteLine($"polyphasesharp: missing value for {args[i]}");
                Environment.Exit(2);
            }
            return args[++i];
        }

        private static bool IsInBuildDir(string root, string file)
        {
            string rel = Path.GetRelativePath(root, file).Replace('\\', '/');
            return rel.Split('/').Any(part =>
                part.Equals("obj", StringComparison.OrdinalIgnoreCase) ||
                part.Equals("bin", StringComparison.OrdinalIgnoreCase) ||
                part.StartsWith('.'));
        }

        private static bool IsGenerated(string luaFile)
        {
            try
            {
                using var reader = new StreamReader(luaFile);
                string first = reader.ReadLine();
                return first != null && first.StartsWith(WrapperEmitter.Marker, StringComparison.Ordinal);
            }
            catch
            {
                return false;
            }
        }

        /// <summary>Find CSharp.lua's output for a source file — mirrored path first,
        /// flat basename as fallback (output layout differs across versions).</summary>
        private static string ReadOutLua(string outDir, string relNoExt)
        {
            string mirrored = Path.Combine(outDir, relNoExt + ".lua");
            if (File.Exists(mirrored))
                return File.ReadAllText(mirrored);
            string flat = Path.Combine(outDir, Path.GetFileName(relNoExt) + ".lua");
            return File.Exists(flat) ? File.ReadAllText(flat) : null;
        }

        private static bool WriteIfChanged(string path, string content)
        {
            if (File.Exists(path) && File.ReadAllText(path) == content)
            {
                Verbose($"unchanged {path}");
                return false;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, content, Utf8NoBom);
            Verbose($"wrote {path}");
            return true;
        }

        private static int CleanOrphans(string scriptsDir, HashSet<string> produced)
        {
            int removed = 0;
            foreach (string lua in Directory.EnumerateFiles(scriptsDir, "*.lua", SearchOption.AllDirectories))
            {
                string full = Path.GetFullPath(lua);
                if (!produced.Contains(full) && IsGenerated(full))
                {
                    File.Delete(full);
                    Verbose($"removed orphan {full}");
                    ++removed;
                }
            }
            return removed;
        }

        private static void Verbose(string msg)
        {
            if (sVerbose)
                Console.WriteLine("polyphasesharp: " + msg);
        }

        private static void PrintUsage()
        {
            Console.WriteLine(
@"PolyphaseSharp - C# -> Lua transpiler for Polyphase engine scripts.

Usage:
  polyphasesharp --scripts <Project>/Scripts/CSharp [options]

Options:
  --scripts <dir>       Directory containing the project's C# script sources.
                        Generated .lua files are written next to each .cs.
  --scripts-root <dir>  The project's Scripts/ root (default: parent of --scripts).
  --lua-roots <dirs>    Extra ';'-separated .lua roots checked for class-name
                        collisions (e.g. the engine's Scripts dir).
  --check               Compile + validate only; write nothing.
  --no-trim             Bundle the full CoreSystem runtime instead of leaving
                        out modules nothing references (use if a dynamically
                        reached module was trimmed away).
  --verbose             Per-file logging.");
        }
    }
}
