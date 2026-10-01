#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Engine input (maps to the Lua Input table). Names follow the engine's
    /// canonical bindings; "Pressed"-style aliases exist on the Lua side too.
    /// </summary>
    public static partial class Input
    {
        /// @CSharpLua.Template = "Input.IsKeyDown({0})"
        public static extern bool IsKeyDown(int key);

        /// <summary>True only on the frame the key went down.</summary>
        /// @CSharpLua.Template = "Input.IsKeyJustDown({0})"
        public static extern bool IsKeyJustDown(int key);

        /// @CSharpLua.Template = "Input.IsKeyJustUp({0})"
        public static extern bool IsKeyJustUp(int key);

        /// @CSharpLua.Template = "Input.IsGamepadButtonDown({0})"
        public static extern bool IsGamepadButtonDown(int button);

        /// @CSharpLua.Template = "Input.IsGamepadButtonJustDown({0})"
        public static extern bool IsGamepadButtonJustDown(int button);

        /// @CSharpLua.Template = "Input.GetGamepadAxisValue({0})"
        public static extern float GetGamepadAxis(int axis);

        /// @CSharpLua.Template = "Input.IsGamepadConnected({0})"
        public static extern bool IsGamepadConnected(int index);

        /// @CSharpLua.Template = "Input.IsMouseButtonDown({0})"
        public static extern bool IsMouseButtonDown(int button);

        /// @CSharpLua.Template = "Input.IsMouseButtonJustDown({0})"
        public static extern bool IsMouseButtonJustDown(int button);

        /// <summary>Mouse movement since last frame, in X/Y of the returned vector.</summary>
        /// @CSharpLua.Template = "Vec(Input.GetMouseDelta())"
        public static extern Vector3 GetMouseDelta();

        /// @CSharpLua.Template = "Vec(Input.GetMousePosition())"
        public static extern Vector3 GetMousePosition();

        /// @CSharpLua.Template = "Input.GetScrollWheelDelta()"
        public static extern int GetScrollWheelDelta();

        /// @CSharpLua.Template = "Input.LockCursor({0})"
        public static extern void LockCursor(bool lockCursor);

        /// @CSharpLua.Template = "Input.TrapCursor({0})"
        public static extern void TrapCursor(bool trap);

        /// @CSharpLua.Template = "Input.ShowCursor({0})"
        public static extern void ShowCursor(bool show);
    }

    /// <summary>Engine math helpers (Lua Math table + lua math library).</summary>
    public static class Mathf
    {
        /// @CSharpLua.Template = "Math.Clamp({0}, {1}, {2})"
        public static extern float Clamp(float value, float min, float max);

        /// @CSharpLua.Template = "Math.Lerp({0}, {1}, {2})"
        public static extern float Lerp(float a, float b, float t);

        /// @CSharpLua.Template = "Math.Approach({0}, {1}, {2}, {3})"
        public static extern float Approach(float current, float target, float speed, float deltaTime);

        /// <summary>Angle-aware approach (degrees, wraps at 180).</summary>
        /// @CSharpLua.Template = "Math.ApproachAngle({0}, {1}, {2}, {3})"
        public static extern float ApproachAngle(float current, float target, float speed, float deltaTime);

        /// @CSharpLua.Template = "Math.RandRange({0}, {1})"
        public static extern float RandRange(float min, float max);

        /// @CSharpLua.Template = "math.abs({0})"
        public static extern float Abs(float value);

        /// @CSharpLua.Template = "math.min({0}, {1})"
        public static extern float Min(float a, float b);

        /// @CSharpLua.Template = "math.max({0}, {1})"
        public static extern float Max(float a, float b);

        /// @CSharpLua.Template = "math.floor({0})"
        public static extern float Floor(float value);

        /// @CSharpLua.Template = "math.sqrt({0})"
        public static extern float Sqrt(float value);

        /// @CSharpLua.Template = "math.sin({0})"
        public static extern float Sin(float radians);

        /// @CSharpLua.Template = "math.cos({0})"
        public static extern float Cos(float radians);

        /// <summary>atan2 — angle of (y, x) in radians.</summary>
        /// @CSharpLua.Template = "math.atan({0}, {1})"
        public static extern float Atan2(float y, float x);

        /// @CSharpLua.Template = "math.deg({0})"
        public static extern float Deg(float radians);

        /// @CSharpLua.Template = "math.rad({0})"
        public static extern float Rad(float degrees);
    }
}
