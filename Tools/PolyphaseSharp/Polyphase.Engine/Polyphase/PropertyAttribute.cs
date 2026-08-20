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
    /// v1 constraints (enforced by the transpiler):
    /// - Allowed types: int, short, byte, float, double, bool, string,
    ///   Vector3, Color, Node and Node-derived handles.
    /// - Initializers must be literals or new Vector3/Color(literal, ...) calls.
    /// - Arrays are not yet supported.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class PropertyAttribute : Attribute
    {
        public PropertyAttribute() { }

        /// <summary>Optional display name shown in the editor inspector.</summary>
        public string Display { get; set; }
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
