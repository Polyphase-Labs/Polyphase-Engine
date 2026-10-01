using System;

namespace Polyphase
{
    /// <summary>
    /// Exposes a field of a Script class to the Polyphase editor inspector,
    /// scene serialization, and hot-reload property restore.
    ///
    /// The PolyphaseSharp transpiler rewrites a [Property] field into an accessor
    /// pair that reads/writes the owning node's uservalue field of the same name,
    /// and emits a matching entry in the generated GatherProperties() table.
    ///
    /// Constraints (enforced by the transpiler):
    /// - Allowed types: int, short, byte, float, double, bool, string,
    ///   Vector3, Color, Node handles (Node, Node3D, StaticMesh3D, Spline3D, ...),
    ///   Asset handles (Asset, Texture, StaticMesh, Material, MaterialLite, ...),
    ///   and single-dimension arrays of any of those (inspector: +/- to size).
    /// - Initializers must be literals or new Vector3/Color(literal, ...) calls;
    ///   arrays: `new T[] { literals }`, `{ literals }`, or `new T[N]` for value
    ///   types. Handle arrays start empty.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class PropertyAttribute : Attribute
    {
        public PropertyAttribute() { }

        /// <summary>Optional display name shown in the editor inspector.</summary>
        public string Display { get; set; }
    }

    /// <summary>
    /// Marks a field as server-to-client replicated state (Lua's
    /// GatherReplicatedData). Same storage rules as [Property] (the value lives
    /// on the node), same type set. The server writes it; clients receive it.
    /// OnRep names a public method called on clients when the value changes:
    ///
    ///   [Replicated(OnRep = nameof(OnHealthChanged))] public int Health = 100;
    ///   public void OnHealthChanged() { ... }
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class ReplicatedAttribute : Attribute
    {
        public ReplicatedAttribute() { }

        /// <summary>Public method invoked on clients after the value is replicated.</summary>
        public string OnRep { get; set; }
    }

    /// <summary>
    /// Declares a public method as a network function (Lua's GatherNetFuncs).
    /// Call it remotely with InvokeNetFunc(nameof(Method), args...):
    /// Server funcs run on the server when invoked by the owning client,
    /// Client funcs run on the owning client when invoked by the server,
    /// Multicast funcs run on every host.
    ///
    ///   [NetFunc(NetFuncType.Server, Reliable = true)]
    ///   public void S_Fire(Vector3 dir) { ... }
    /// </summary>
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class NetFuncAttribute : Attribute
    {
        public NetFuncAttribute(NetFuncType type)
        {
            Type = type;
        }

        public NetFuncType Type { get; }

        /// <summary>Reliable delivery (default false).</summary>
        public bool Reliable { get; set; }
    }

    /// <summary>
    /// Renders a clickable button in the editor's Properties inspector that
    /// invokes this method. The method must be public, non-static, and take no
    /// parameters (a button click carries no arguments).
    ///
    ///   [Button("Reset Score", "Sets the score back to zero")]
    ///   public void ResetScore() { ... }
    /// </summary>
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class ButtonAttribute : Attribute
    {
        public ButtonAttribute() { }

        public ButtonAttribute(string title)
        {
            Title = title;
        }

        public ButtonAttribute(string title, string tooltip)
        {
            Title = title;
            Tooltip = tooltip;
        }

        /// <summary>Button label. Defaults to the method name.</summary>
        public string Title { get; set; }

        /// <summary>Shown when hovering the button.</summary>
        public string Tooltip { get; set; }
    }
}
