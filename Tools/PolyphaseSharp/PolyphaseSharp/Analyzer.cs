using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace PolyphaseSharp
{
    /// <summary>
    /// Semantic analysis over the ORIGINAL user sources (+ engine API sources):
    /// discovers script classes, collects [Property] fields, detects overridden
    /// lifecycle methods, and validates the v1 subset.
    /// </summary>
    public sealed class Analyzer
    {
        public const string DefaultNamespace = "Game";

        // Lifecycle methods the engine dispatches. Create is handled specially
        // (the wrapper always emits Create to construct the companion object).
        private static readonly (string Name, int Arity)[] kLifecycle =
        {
            ("Create", 0), ("Awake", 0), ("Start", 0), ("Tick", 1), ("EditorTick", 1),
            ("Stop", 0), ("Destroy", 0), ("BeginOverlap", 1), ("EndOverlap", 1), ("OnCollision", 3),
        };

        public readonly List<Diagnostic> Diagnostics = new();
        public bool HasErrors => Diagnostics.Any(d => d.IsError);

        private CSharpCompilation mCompilation;
        private INamedTypeSymbol mScriptBase;
        private INamedTypeSymbol mPropertyAttr;

        public CSharpCompilation Compile(IReadOnlyList<SyntaxTree> userTrees, IReadOnlyList<SyntaxTree> apiTrees)
        {
            var refs = new List<MetadataReference>();
            string tpa = (string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES");
            var wanted = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
            {
                "System.Private.CoreLib.dll", "System.Runtime.dll", "netstandard.dll",
                "System.Core.dll", "System.dll", "mscorlib.dll", "System.Linq.dll",
                "System.Collections.dll",
            };
            foreach (string path in tpa.Split(Path.PathSeparator))
            {
                if (wanted.Contains(Path.GetFileName(path)))
                    refs.Add(MetadataReference.CreateFromFile(path));
            }

            mCompilation = CSharpCompilation.Create(
                "PolyphaseScripts",
                userTrees.Concat(apiTrees),
                refs,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

            foreach (var diag in mCompilation.GetDiagnostics())
            {
                if (diag.Severity != DiagnosticSeverity.Error)
                    continue;
                var pos = diag.Location.GetLineSpan();
                Diagnostics.Add(new Diagnostic
                {
                    File = pos.Path ?? "<unknown>",
                    Line = pos.StartLinePosition.Line + 1,
                    Column = pos.StartLinePosition.Character + 1,
                    IsError = true,
                    Code = diag.Id,
                    Message = diag.GetMessage(CultureInfo.InvariantCulture),
                });
            }

            mScriptBase = mCompilation.GetTypeByMetadataName("Polyphase.Script");
            mPropertyAttr = mCompilation.GetTypeByMetadataName("Polyphase.PropertyAttribute");
            if (mScriptBase == null || mPropertyAttr == null)
            {
                Error(null, 0, 0, "PS0001", "internal: Polyphase engine API sources not found in compilation");
            }
            return mCompilation;
        }

        /// <summary>Analyze one user source tree into a SourceUnit (RewrittenText filled later).</summary>
        public SourceUnit Analyze(SyntaxTree tree, string scriptsRoot, HashSet<string> existingLuaClassNames)
        {
            var unit = new SourceUnit
            {
                SourceFile = tree.FilePath,
                RelativePath = Path.GetRelativePath(scriptsRoot, tree.FilePath).Replace('\\', '/'),
            };

            if (HasErrors)
                return unit; // compilation is broken; skip deeper analysis

            var model = mCompilation.GetSemanticModel(tree);
            var root = tree.GetCompilationUnitRoot();
            var scriptClasses = new List<(ClassDeclarationSyntax Syntax, INamedTypeSymbol Symbol)>();

            foreach (var cls in root.DescendantNodes().OfType<ClassDeclarationSyntax>())
            {
                var symbol = model.GetDeclaredSymbol(cls);
                if (symbol == null || symbol.IsAbstract || !DerivesFromScript(symbol))
                    continue;
                scriptClasses.Add((cls, symbol));
            }

            if (scriptClasses.Count == 0)
                return unit;

            string baseName = Path.GetFileNameWithoutExtension(tree.FilePath);

            if (scriptClasses.Count > 1)
            {
                var extra = scriptClasses[1];
                Error(tree.FilePath, Line(extra.Syntax), Col(extra.Syntax), "PS1002",
                    $"only one script class is allowed per file; move '{extra.Symbol.Name}' to its own file");
                return unit;
            }

            var (syntax, cls2) = scriptClasses[0];
            if (!string.Equals(cls2.Name, baseName, StringComparison.Ordinal))
            {
                Error(tree.FilePath, Line(syntax), Col(syntax), "PS1001",
                    $"script class '{cls2.Name}' must live in a file named '{cls2.Name}.cs' " +
                    $"(the engine derives the script class name from the file name)");
                return unit;
            }
            if (cls2.IsGenericType)
            {
                Error(tree.FilePath, Line(syntax), Col(syntax), "PS1003",
                    $"script class '{cls2.Name}' cannot be generic");
                return unit;
            }
            if (!HasUsableConstructor(cls2))
            {
                Error(tree.FilePath, Line(syntax), Col(syntax), "PS1004",
                    $"script class '{cls2.Name}' needs a public parameterless constructor (or none)");
                return unit;
            }
            if (existingLuaClassNames.Contains(cls2.Name))
            {
                Error(tree.FilePath, Line(syntax), Col(syntax), "PS1007",
                    $"script class '{cls2.Name}' collides with an existing hand-written Lua script " +
                    $"of the same name under Scripts/ (script class names are global)");
                return unit;
            }

            var script = new ScriptClass
            {
                ClassName = cls2.Name,
                FullLuaPath = LuaPathOf(cls2),
                SourceFile = tree.FilePath,
            };

            // Chain from Polyphase.Script (exclusive) down to the class itself, base-first.
            var chain = new List<INamedTypeSymbol>();
            for (var t = cls2; t != null && !IsEngineApiType(t); t = t.BaseType)
                chain.Add(t);
            chain.Reverse();

            foreach (var type in chain)
            {
                CollectProperties(type, script);

                // Cross-file Require dependencies (user base classes in other files).
                if (!SymbolEqualityComparer.Default.Equals(type, cls2))
                {
                    string declFile = type.DeclaringSyntaxReferences.FirstOrDefault()?.SyntaxTree.FilePath;
                    if (declFile != null && !PathsEqual(declFile, tree.FilePath))
                    {
                        string rel = Path.GetRelativePath(scriptsRoot, declFile).Replace('\\', '/');
                        rel = rel.EndsWith(".cs", StringComparison.OrdinalIgnoreCase) ? rel[..^3] : rel;
                        if (!unit.RequiredFiles.Contains(rel))
                            unit.RequiredFiles.Add(rel);
                    }
                }
            }

            CollectPublicMethods(chain, script, tree.FilePath);

            foreach (var (name, arity) in kLifecycle)
            {
                bool overridden = chain.Any(t => t.GetMembers(name)
                    .OfType<IMethodSymbol>()
                    .Any(m => m.Parameters.Length == arity && !m.IsStatic));
                if (!overridden)
                    continue;
                if (name == "Create")
                    script.OverridesCreate = true;
                else
                    script.OverriddenMethods.Add(name);
            }

            LintSubset(tree, model);

            unit.Script = script;
            return unit;
        }

        private static readonly HashSet<string> kLuaKeywords = new(StringComparer.Ordinal)
        {
            "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto",
            "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true",
            "until", "while", "self",
        };

        /// <summary>
        /// Public instance methods get forwarders on the engine wrapper table, which
        /// is what makes a C# script callable from Lua scripts (and from other C#
        /// scripts) through the engine's `node:Method(args)` convention.
        /// </summary>
        private void CollectPublicMethods(List<INamedTypeSymbol> chain, ScriptClass script, string file)
        {
            var lifecycleNames = new HashSet<string>(kLifecycle.Select(l => l.Name), StringComparer.Ordinal);
            var byName = new Dictionary<string, List<IMethodSymbol>>(StringComparer.Ordinal);

            foreach (var type in chain)
            {
                foreach (var method in type.GetMembers().OfType<IMethodSymbol>())
                {
                    if (method.MethodKind != MethodKind.Ordinary ||
                        method.IsStatic ||
                        method.IsExtern ||
                        method.DeclaredAccessibility != Accessibility.Public ||
                        lifecycleNames.Contains(method.Name))
                    {
                        continue;
                    }
                    if (!byName.TryGetValue(method.Name, out var list))
                        byName[method.Name] = list = new List<IMethodSymbol>();
                    list.Add(method);
                }
            }

            foreach (var (name, methods) in byName.OrderBy(kv => kv.Key, StringComparer.Ordinal))
            {
                // Overrides of a user base method share a name — one forwarder is
                // right. Genuine overloads can't be dispatched from Lua (one slot
                // per name on the wrapper table): warn and skip the name.
                var arities = methods.Select(m => m.Parameters.Length).Distinct().ToList();
                if (arities.Count > 1)
                {
                    Warn(file, 0, 0, "PS2003",
                        $"public method '{name}' is overloaded; overloads cannot be called " +
                        "from Lua (one method slot per name) — no cross-script forwarder emitted");
                    continue;
                }

                var canonical = methods[0];
                var paramNames = new string[canonical.Parameters.Length];
                for (int i = 0; i < paramNames.Length; ++i)
                {
                    string p = canonical.Parameters[i].Name;
                    bool valid = Regex.IsMatch(p, "^[A-Za-z_][A-Za-z0-9_]*$") && !kLuaKeywords.Contains(p);
                    paramNames[i] = valid ? p : ("a" + (i + 1));
                }

                script.PublicMethods.Add(new ScriptMethod { Name = name, ParamNames = paramNames });
            }
        }

        // ---- helpers ----

        private bool DerivesFromScript(INamedTypeSymbol symbol)
        {
            for (var t = symbol.BaseType; t != null; t = t.BaseType)
            {
                if (SymbolEqualityComparer.Default.Equals(t, mScriptBase))
                    return true;
            }
            return false;
        }

        private static bool IsEngineApiType(INamedTypeSymbol t)
            => t.ContainingNamespace?.ToDisplayString() == "Polyphase";

        private static bool HasUsableConstructor(INamedTypeSymbol cls)
        {
            var ctors = cls.InstanceConstructors.Where(c => !c.IsImplicitlyDeclared).ToList();
            return ctors.Count == 0 || ctors.Any(c => c.Parameters.Length == 0 &&
                c.DeclaredAccessibility == Accessibility.Public);
        }

        private string LuaPathOf(INamedTypeSymbol cls)
        {
            string ns = cls.ContainingNamespace != null && !cls.ContainingNamespace.IsGlobalNamespace
                ? cls.ContainingNamespace.ToDisplayString()
                : DefaultNamespace; // the rewriter wraps namespace-less files in DefaultNamespace
            return ns + "." + cls.Name;
        }

        private void CollectProperties(INamedTypeSymbol type, ScriptClass script)
        {
            foreach (var field in type.GetMembers().OfType<IFieldSymbol>())
            {
                var attr = field.GetAttributes().FirstOrDefault(a =>
                    SymbolEqualityComparer.Default.Equals(a.AttributeClass, mPropertyAttr));
                if (attr == null)
                    continue;

                var declSyntax = field.DeclaringSyntaxReferences.FirstOrDefault()?.GetSyntax() as VariableDeclaratorSyntax;
                string file = script.SourceFile;
                int line = declSyntax != null ? Line(declSyntax) : 0;
                int col = declSyntax != null ? Col(declSyntax) : 0;

                if (field.IsStatic || field.IsReadOnly || field.IsConst)
                {
                    Error(file, line, col, "PS1008",
                        $"[Property] field '{field.Name}' must be a non-static, writable instance field");
                    continue;
                }

                if (declSyntax?.Parent is VariableDeclarationSyntax varDecl && varDecl.Variables.Count > 1)
                {
                    Error(file, line, col, "PS1009",
                        $"[Property] fields must declare one variable each; split '{field.Name}' into its own declaration");
                    continue;
                }

                string datumType = MapDatumType(field.Type);
                if (datumType == null)
                {
                    Error(file, line, col, "PS1005",
                        $"[Property] field '{field.Name}' has unsupported type '{field.Type.ToDisplayString()}'. " +
                        "Supported: int, short, byte, float, double, bool, string, Vector3, Color, Node, Node3D.");
                    continue;
                }

                string defaultLiteral = null;
                if (declSyntax?.Initializer != null)
                {
                    defaultLiteral = ToLuaLiteral(declSyntax.Initializer.Value);
                    if (defaultLiteral == null)
                    {
                        Error(file, line, col, "PS1006",
                            $"[Property] field '{field.Name}' initializer must be a literal or " +
                            "new Vector3/Color(<literals>) (evaluated before serialized values are applied)");
                        continue;
                    }
                }

                string display = attr.NamedArguments
                    .Where(kv => kv.Key == "Display")
                    .Select(kv => kv.Value.Value as string)
                    .FirstOrDefault();

                script.Properties.Add(new ScriptProperty
                {
                    Name = field.Name,
                    DatumType = datumType,
                    DisplayName = display,
                    DefaultLuaLiteral = defaultLiteral,
                    CSharpType = field.Type.ToDisplayString(),
                });
            }
        }

        private string MapDatumType(ITypeSymbol type)
        {
            switch (type.SpecialType)
            {
                case SpecialType.System_Int32: return "Integer";
                case SpecialType.System_Int16: return "Short";
                case SpecialType.System_Byte: return "Byte";
                case SpecialType.System_Single:
                case SpecialType.System_Double: return "Float";
                case SpecialType.System_Boolean: return "Bool";
                case SpecialType.System_String: return "String";
            }
            string full = type.ToDisplayString();
            switch (full)
            {
                case "Polyphase.Vector3": return "Vector";
                case "Polyphase.Color": return "Color";
                case "Polyphase.Node3D": return "Node3D";
                case "Polyphase.Node": return "Node";
            }
            // Node-derived handles map to their nearest engine datum.
            for (var t = type as INamedTypeSymbol; t != null; t = t.BaseType)
            {
                if (t.ToDisplayString() == "Polyphase.Node3D") return "Node3D";
                if (t.ToDisplayString() == "Polyphase.Node") return "Node";
            }
            return null;
        }

        /// <summary>Literal / new Vector3(...) / new Color(...) → Lua source, else null.</summary>
        public static string ToLuaLiteral(ExpressionSyntax expr)
        {
            switch (expr)
            {
                case LiteralExpressionSyntax lit:
                    switch (lit.Kind())
                    {
                        case SyntaxKind.NumericLiteralExpression:
                            return FormatNumber(lit.Token.Value);
                        case SyntaxKind.TrueLiteralExpression: return "true";
                        case SyntaxKind.FalseLiteralExpression: return "false";
                        case SyntaxKind.StringLiteralExpression:
                            return QuoteLua(lit.Token.ValueText);
                        case SyntaxKind.NullLiteralExpression: return "nil";
                    }
                    return null;

                case PrefixUnaryExpressionSyntax unary when unary.IsKind(SyntaxKind.UnaryMinusExpression):
                {
                    string inner = ToLuaLiteral(unary.Operand);
                    return inner != null ? "-" + inner : null;
                }

                case ObjectCreationExpressionSyntax creation:
                {
                    string typeName = creation.Type.ToString();
                    bool isVec = typeName is "Vector3" or "Polyphase.Vector3";
                    bool isColor = typeName is "Color" or "Polyphase.Color";
                    if (!isVec && !isColor)
                        return null;
                    var args = creation.ArgumentList?.Arguments ?? default;
                    var parts = new List<string>();
                    foreach (var arg in args)
                    {
                        string part = ToLuaLiteral(arg.Expression);
                        if (part == null)
                            return null;
                        parts.Add(part);
                    }
                    if (isVec && parts.Count is not (0 or 3)) return null;
                    if (isColor && parts.Count != 4) return null;
                    return "Vec(" + string.Join(", ", parts) + ")";
                }

                case ImplicitObjectCreationExpressionSyntax implicitNew:
                {
                    // new(x, y, z) — target-typed; caller validated the field type already.
                    var parts = new List<string>();
                    foreach (var arg in implicitNew.ArgumentList.Arguments)
                    {
                        string part = ToLuaLiteral(arg.Expression);
                        if (part == null)
                            return null;
                        parts.Add(part);
                    }
                    if (parts.Count is not (0 or 3 or 4)) return null;
                    return "Vec(" + string.Join(", ", parts) + ")";
                }
            }
            return null;
        }

        private static string FormatNumber(object value)
        {
            return value switch
            {
                float f => f.ToString("R", CultureInfo.InvariantCulture),
                double d => d.ToString("R", CultureInfo.InvariantCulture),
                decimal m => m.ToString(CultureInfo.InvariantCulture),
                IFormattable n => n.ToString(null, CultureInfo.InvariantCulture),
                _ => null,
            };
        }

        private static string QuoteLua(string s)
        {
            return "\"" + s
                .Replace("\\", "\\\\")
                .Replace("\"", "\\\"")
                .Replace("\n", "\\n")
                .Replace("\r", "\\r") + "\"";
        }

        private void LintSubset(SyntaxTree tree, SemanticModel model)
        {
            var root = tree.GetRoot();
            foreach (var pred in root.DescendantNodes().OfType<PredefinedTypeSyntax>())
            {
                var kind = pred.Keyword.Kind();
                if (kind is SyntaxKind.LongKeyword or SyntaxKind.ULongKeyword or SyntaxKind.DecimalKeyword)
                {
                    Warn(tree.FilePath, Line(pred), Col(pred), "PS2001",
                        $"'{pred.Keyword.Text}' is not representable at runtime — the engine Lua VM uses " +
                        "32-bit integers and 32-bit floats on every platform");
                }
            }
            foreach (var method in root.DescendantNodes().OfType<MethodDeclarationSyntax>())
            {
                if (method.Modifiers.Any(SyntaxKind.AsyncKeyword))
                {
                    Warn(tree.FilePath, Line(method), Col(method), "PS2002",
                        "async methods have no scheduler in the engine runtime; continuations only run " +
                        "if you pump them manually — prefer plain methods");
                }
            }
        }

        private static bool PathsEqual(string a, string b)
            => string.Equals(Path.GetFullPath(a), Path.GetFullPath(b), StringComparison.OrdinalIgnoreCase);

        private static int Line(SyntaxNode n) => n.GetLocation().GetLineSpan().StartLinePosition.Line + 1;
        private static int Col(SyntaxNode n) => n.GetLocation().GetLineSpan().StartLinePosition.Character + 1;

        private void Error(string file, int line, int col, string code, string msg)
            => Diagnostics.Add(new Diagnostic { File = file ?? "", Line = line, Column = col, IsError = true, Code = code, Message = msg });

        private void Warn(string file, int line, int col, string code, string msg)
            => Diagnostics.Add(new Diagnostic { File = file ?? "", Line = line, Column = col, IsError = false, Code = code, Message = msg });
    }
}
