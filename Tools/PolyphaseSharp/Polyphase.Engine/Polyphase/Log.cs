#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>Engine log (maps to the Lua Log table).</summary>
    public static class Log
    {
        /// @CSharpLua.Template = "Log.Debug({0})"
        public static extern void Debug(string message);

        /// @CSharpLua.Template = "Log.Warning({0})"
        public static extern void Warning(string message);

        /// @CSharpLua.Template = "Log.Error({0})"
        public static extern void Error(string message);

        /// @CSharpLua.Template = "Log.Console({0})"
        public static extern void Console(string message);
    }

    /// <summary>
    /// Escape hatch into raw Lua: engine APIs not yet wrapped in C#, and
    /// dynamic calls onto Lua scripts attached to other nodes.
    /// </summary>
    public static class Lua
    {
        /// <summary>Read a Lua global.</summary>
        /// @CSharpLua.Template = "_G[{0}]"
        public static extern object GetGlobal(string name);

        /// <summary>Write a Lua global.</summary>
        /// @CSharpLua.Template = "_G[{0}] = {1}"
        public static extern void SetGlobal(string name, object value);

        /// <summary>Read a field on any Lua value — e.g. a Lua script's property
        /// on another node: Lua.Get(chestNode, "isOpen").</summary>
        /// @CSharpLua.Template = "{0}[{1}]"
        public static extern object Get(object target, string key);

        /// <summary>Write a field on any Lua value.</summary>
        /// @CSharpLua.Template = "{0}[{1}] = {2}"
        public static extern void Set(object target, string key, object value);

        /// <summary>Call `target:method(...)` on any Lua value (up to 6 args) —
        /// the engine's cross-script convention. Works on nodes running Lua
        /// scripts AND nodes running C# scripts (their public methods are
        /// exposed the same way).</summary>
        /// @CSharpLua.Template = "{0}[{1}]({0})"
        public static extern object Call(object target, string method);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2})"
        public static extern object Call(object target, string method, object a);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2}, {3})"
        public static extern object Call(object target, string method, object a, object b);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2}, {3}, {4})"
        public static extern object Call(object target, string method, object a, object b, object c);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2}, {3}, {4}, {5})"
        public static extern object Call(object target, string method, object a, object b, object c, object d);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2}, {3}, {4}, {5}, {6})"
        public static extern object Call(object target, string method, object a, object b, object c, object d, object e);

        /// @CSharpLua.Template = "{0}[{1}]({0}, {2}, {3}, {4}, {5}, {6}, {7})"
        public static extern object Call(object target, string method, object a, object b, object c, object d, object e, object f);

        /// <summary>Reinterpret a value as another handle type with NO runtime
        /// check — pure compile-time cast. Use to type a Node as a hand-written
        /// script façade (a Node3D subclass with extern template methods that
        /// mirror a Lua script's functions).</summary>
        /// @CSharpLua.Template = "{0}"
        public static extern T As<T>(object value);
    }
}
