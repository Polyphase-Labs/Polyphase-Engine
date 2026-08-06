#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// The engine vector (Lua `Vec` userdata, glm::vec4 backed). Reference
    /// semantics — assignment aliases the same underlying vector, exactly as in
    /// engine Lua scripts. Use Clone() for an independent copy.
    /// </summary>
    public sealed class Vector3
    {
        /// @CSharpLua.Template = "Vec({0}, {1}, {2})"
        public extern Vector3(float x, float y, float z);

        /// @CSharpLua.Template = "Vec()"
        public extern Vector3();

        /// @CSharpLua.Get = "{this}.x"
        /// @CSharpLua.Set = "{this}.x = {0}"
        public extern float X { get; set; }

        /// @CSharpLua.Get = "{this}.y"
        /// @CSharpLua.Set = "{this}.y = {0}"
        public extern float Y { get; set; }

        /// @CSharpLua.Get = "{this}.z"
        /// @CSharpLua.Set = "{this}.z = {0}"
        public extern float Z { get; set; }

        /// @CSharpLua.Template = "({0} + {1})"
        public static extern Vector3 operator +(Vector3 a, Vector3 b);

        /// @CSharpLua.Template = "({0} - {1})"
        public static extern Vector3 operator -(Vector3 a, Vector3 b);

        /// @CSharpLua.Template = "({0} * {1})"
        public static extern Vector3 operator *(Vector3 a, float b);

        /// @CSharpLua.Template = "({0} * {1})"
        public static extern Vector3 operator *(Vector3 a, Vector3 b);

        /// @CSharpLua.Template = "({0} / {1})"
        public static extern Vector3 operator /(Vector3 a, float b);

        /// @CSharpLua.Template = "(-{0})"
        public static extern Vector3 operator -(Vector3 a);

        /// @CSharpLua.Template = "Vec({this}.x, {this}.y, {this}.z)"
        public extern Vector3 Clone();
    }

    /// <summary>
    /// Engine color (Lua `Vec` userdata with r/g/b/a in x/y/z/w). 0..1 floats.
    /// </summary>
    public sealed class Color
    {
        /// @CSharpLua.Template = "Vec({0}, {1}, {2}, {3})"
        public extern Color(float r, float g, float b, float a);

        /// @CSharpLua.Get = "{this}.x"
        /// @CSharpLua.Set = "{this}.x = {0}"
        public extern float R { get; set; }

        /// @CSharpLua.Get = "{this}.y"
        /// @CSharpLua.Set = "{this}.y = {0}"
        public extern float G { get; set; }

        /// @CSharpLua.Get = "{this}.z"
        /// @CSharpLua.Set = "{this}.z = {0}"
        public extern float B { get; set; }

        /// @CSharpLua.Get = "{this}.w"
        /// @CSharpLua.Set = "{this}.w = {0}"
        public extern float A { get; set; }
    }
}
