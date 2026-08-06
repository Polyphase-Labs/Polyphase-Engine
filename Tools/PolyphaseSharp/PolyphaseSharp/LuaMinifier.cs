using System.Text;

namespace PolyphaseSharp
{
    /// <summary>
    /// Comment/blank-line stripper for the CoreSystem bundle. Lexes just enough
    /// Lua to never touch string contents: short strings with escapes, long
    /// strings/comments with [=*[ ... ]=*] level matching. Keeps code exactly
    /// as written otherwise (no identifier renaming — output stays debuggable).
    /// </summary>
    public static class LuaMinifier
    {
        public static string Strip(string src)
        {
            var sb = new StringBuilder(src.Length);
            int i = 0;
            int n = src.Length;

            while (i < n)
            {
                char c = src[i];

                // comments
                if (c == '-' && i + 1 < n && src[i + 1] == '-')
                {
                    int level = LongBracketLevel(src, i + 2);
                    if (level >= 0)
                    {
                        i = SkipLongBracket(src, i + 2 + level + 2, level);
                    }
                    else
                    {
                        while (i < n && src[i] != '\n') ++i;
                    }
                    continue;
                }

                // short strings
                if (c == '"' || c == '\'')
                {
                    int start = i;
                    ++i;
                    while (i < n)
                    {
                        if (src[i] == '\\') { i += 2; continue; }
                        if (src[i] == c) { ++i; break; }
                        ++i;
                    }
                    sb.Append(src, start, i - start);
                    continue;
                }

                // long strings
                if (c == '[')
                {
                    int level = LongBracketLevel(src, i);
                    if (level >= 0)
                    {
                        int start = i;
                        i = SkipLongBracket(src, i + level + 2, level);
                        sb.Append(src, start, i - start);
                        continue;
                    }
                }

                sb.Append(c);
                ++i;
            }

            // Collapse whitespace-only lines and trailing spaces.
            var outSb = new StringBuilder(sb.Length);
            foreach (string rawLine in sb.ToString().Split('\n'))
            {
                string line = rawLine.TrimEnd();
                if (line.Length > 0)
                    outSb.Append(line).Append('\n');
            }
            return outSb.ToString();
        }

        /// <summary>At src[i]=='[': returns the '=' count if a long bracket opens here, else -1.</summary>
        private static int LongBracketLevel(string src, int i)
        {
            if (i >= src.Length || src[i] != '[')
                return -1;
            int j = i + 1;
            int level = 0;
            while (j < src.Length && src[j] == '=') { ++level; ++j; }
            return (j < src.Length && src[j] == '[') ? level : -1;
        }

        /// <summary>Skips to just past the matching ]=*] closer.</summary>
        private static int SkipLongBracket(string src, int i, int level)
        {
            while (i < src.Length)
            {
                if (src[i] == ']')
                {
                    int j = i + 1;
                    int eq = 0;
                    while (j < src.Length && src[j] == '=') { ++eq; ++j; }
                    if (eq == level && j < src.Length && src[j] == ']')
                        return j + 1;
                }
                ++i;
            }
            return i;
        }
    }
}
