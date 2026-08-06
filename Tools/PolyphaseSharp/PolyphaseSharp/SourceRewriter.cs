using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace PolyphaseSharp
{
    /// <summary>
    /// Syntax-level pre-pass before CSharp.lua sees the code:
    /// 1. [Property] fields become extern accessor properties whose Get/Set doc
    ///    templates route through the node uservalue ({this}.__node.Name) — making
    ///    the node the single source of truth so editor round-trip, serialization,
    ///    and hot-reload restore all keep working.
    /// 2. Files with no namespace get wrapped in the default namespace so
    ///    CSharp.lua's global publishing can never collide with the engine's
    ///    global script class tables.
    /// </summary>
    public static class SourceRewriter
    {
        public static string Rewrite(SyntaxTree tree, string defaultNamespace)
        {
            var root = tree.GetCompilationUnitRoot();
            var rewritten = (CompilationUnitSyntax)new PropertyFieldRewriter().Visit(root);

            bool hasNamespace = rewritten.Members.Any(m =>
                m is NamespaceDeclarationSyntax or FileScopedNamespaceDeclarationSyntax);

            // No NormalizeWhitespace anywhere: keep user code close to its original
            // lines so any late CSharp.lua diagnostics stay meaningful. The namespace
            // wrap is done textually for the same reason.
            if (hasNamespace || rewritten.Members.Count == 0)
                return rewritten.ToFullString();

            var sb = new StringBuilder();
            foreach (var ext in rewritten.Externs) sb.Append(ext.ToFullString());
            foreach (var use in rewritten.Usings) sb.Append(use.ToFullString());
            sb.AppendLine();
            sb.Append("namespace ").AppendLine(defaultNamespace);
            sb.AppendLine("{");
            foreach (var member in rewritten.Members) sb.Append(member.ToFullString());
            sb.AppendLine();
            sb.AppendLine("}");
            return sb.ToString();
        }

        private sealed class PropertyFieldRewriter : CSharpSyntaxRewriter
        {
            public override SyntaxNode VisitFieldDeclaration(FieldDeclarationSyntax node)
            {
                if (!HasPropertyAttribute(node))
                    return base.VisitFieldDeclaration(node);

                // Replace the field (possibly declaring several variables) with one
                // extern property per variable, each carrying uservalue templates.
                // Validation (types, initializers) already happened in the Analyzer;
                // initializers are dropped here — the wrapper emits them into Create().
                var sb = new StringBuilder();
                string typeText = node.Declaration.Type.ToString();
                foreach (var variable in node.Declaration.Variables)
                {
                    string name = variable.Identifier.Text;
                    sb.AppendLine($"/// @CSharpLua.Get = \"{{this}}.__node.{name}\"");
                    sb.AppendLine($"/// @CSharpLua.Set = \"{{this}}.__node.{name} = {{0}}\"");
                    sb.AppendLine($"public extern {typeText} {name} {{ get; set; }}");
                }

                // Multi-variable [Property] fields are rejected by the Analyzer
                // (PS1009), so exactly one member comes back here.
                var members = SyntaxFactory.ParseCompilationUnit(sb.ToString()).Members;
                return members[0]
                    .WithLeadingTrivia(node.GetLeadingTrivia().AddRange(members[0].GetLeadingTrivia()))
                    .WithTrailingTrivia(node.GetTrailingTrivia());
            }

            private static bool HasPropertyAttribute(FieldDeclarationSyntax node)
            {
                foreach (var list in node.AttributeLists)
                {
                    foreach (var attr in list.Attributes)
                    {
                        string name = attr.Name.ToString();
                        if (name is "Property" or "PropertyAttribute"
                            or "Polyphase.Property" or "Polyphase.PropertyAttribute")
                        {
                            return true;
                        }
                    }
                }
                return false;
            }
        }
    }
}
