#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Engine input (maps to the Lua Input table). Names follow the engine's
    /// canonical bindings; "Pressed"-style aliases exist on the Lua side too.
    /// </summary>
    public static class Input
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

    /// <summary>
    /// Keyboard key constants. Read from the engine's Lua Key table at runtime —
    /// keycodes are platform-defined, so they must never be baked into the
    /// transpiled output as numbers.
    /// </summary>
    public static class Key
    {
        /// @CSharpLua.Get = "Key.A"
        public static extern int A { get; }
        /// @CSharpLua.Get = "Key.B"
        public static extern int B { get; }
        /// @CSharpLua.Get = "Key.C"
        public static extern int C { get; }
        /// @CSharpLua.Get = "Key.D"
        public static extern int D { get; }
        /// @CSharpLua.Get = "Key.E"
        public static extern int E { get; }
        /// @CSharpLua.Get = "Key.F"
        public static extern int F { get; }
        /// @CSharpLua.Get = "Key.G"
        public static extern int G { get; }
        /// @CSharpLua.Get = "Key.H"
        public static extern int H { get; }
        /// @CSharpLua.Get = "Key.I"
        public static extern int I { get; }
        /// @CSharpLua.Get = "Key.J"
        public static extern int J { get; }
        /// @CSharpLua.Get = "Key.K"
        public static extern int K { get; }
        /// @CSharpLua.Get = "Key.L"
        public static extern int L { get; }
        /// @CSharpLua.Get = "Key.M"
        public static extern int M { get; }
        /// @CSharpLua.Get = "Key.N"
        public static extern int N { get; }
        /// @CSharpLua.Get = "Key.O"
        public static extern int O { get; }
        /// @CSharpLua.Get = "Key.P"
        public static extern int P { get; }
        /// @CSharpLua.Get = "Key.Q"
        public static extern int Q { get; }
        /// @CSharpLua.Get = "Key.R"
        public static extern int R { get; }
        /// @CSharpLua.Get = "Key.S"
        public static extern int S { get; }
        /// @CSharpLua.Get = "Key.T"
        public static extern int T { get; }
        /// @CSharpLua.Get = "Key.U"
        public static extern int U { get; }
        /// @CSharpLua.Get = "Key.V"
        public static extern int V { get; }
        /// @CSharpLua.Get = "Key.W"
        public static extern int W { get; }
        /// @CSharpLua.Get = "Key.X"
        public static extern int X { get; }
        /// @CSharpLua.Get = "Key.Y"
        public static extern int Y { get; }
        /// @CSharpLua.Get = "Key.Z"
        public static extern int Z { get; }
        /// @CSharpLua.Get = "Key.Space"
        public static extern int Space { get; }
        /// @CSharpLua.Get = "Key.Enter"
        public static extern int Enter { get; }
        /// @CSharpLua.Get = "Key.Escape"
        public static extern int Escape { get; }
        /// @CSharpLua.Get = "Key.ShiftL"
        public static extern int ShiftL { get; }
        /// @CSharpLua.Get = "Key.ControlL"
        public static extern int ControlL { get; }
        /// @CSharpLua.Get = "Key.Tab"
        public static extern int Tab { get; }
        /// @CSharpLua.Get = "Key.Up"
        public static extern int Up { get; }
        /// @CSharpLua.Get = "Key.Down"
        public static extern int Down { get; }
        /// @CSharpLua.Get = "Key.Left"
        public static extern int Left { get; }
        /// @CSharpLua.Get = "Key.Right"
        public static extern int Right { get; }
    }

    /// <summary>Gamepad button/axis constants (runtime reads of the Lua Gamepad table).</summary>
    public static class Gamepad
    {
        /// @CSharpLua.Get = "Gamepad.A"
        public static extern int A { get; }
        /// @CSharpLua.Get = "Gamepad.B"
        public static extern int B { get; }
        /// @CSharpLua.Get = "Gamepad.X"
        public static extern int X { get; }
        /// @CSharpLua.Get = "Gamepad.Y"
        public static extern int Y { get; }
        /// @CSharpLua.Get = "Gamepad.L1"
        public static extern int L1 { get; }
        /// @CSharpLua.Get = "Gamepad.R1"
        public static extern int R1 { get; }
        /// @CSharpLua.Get = "Gamepad.Start"
        public static extern int Start { get; }
        /// @CSharpLua.Get = "Gamepad.Select"
        public static extern int Select { get; }
        /// @CSharpLua.Get = "Gamepad.Up"
        public static extern int Up { get; }
        /// @CSharpLua.Get = "Gamepad.Down"
        public static extern int Down { get; }
        /// @CSharpLua.Get = "Gamepad.Left"
        public static extern int Left { get; }
        /// @CSharpLua.Get = "Gamepad.Right"
        public static extern int Right { get; }
        /// @CSharpLua.Get = "Gamepad.AxisLX"
        public static extern int AxisLX { get; }
        /// @CSharpLua.Get = "Gamepad.AxisLY"
        public static extern int AxisLY { get; }
        /// @CSharpLua.Get = "Gamepad.AxisRX"
        public static extern int AxisRX { get; }
        /// @CSharpLua.Get = "Gamepad.AxisRY"
        public static extern int AxisRY { get; }
        /// @CSharpLua.Get = "Gamepad.AxisLT"
        public static extern int AxisLT { get; }
        /// @CSharpLua.Get = "Gamepad.AxisRT"
        public static extern int AxisRT { get; }
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
