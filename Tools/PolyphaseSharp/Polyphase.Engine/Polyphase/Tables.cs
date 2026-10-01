#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// 2D vector / point (engine Vec userdata using x/y). Vector2 [Property]
    /// fields show a 2-component editor field (DatumType.Vector2D).
    /// </summary>
    public sealed class Vector2
    {
        /// @CSharpLua.Template = "Vec({0}, {1}, 0, 0)"
        public extern Vector2(float x, float y);

        /// @CSharpLua.Template = "Vec()"
        public extern Vector2();

        /// @CSharpLua.Get = "{this}.x"
        /// @CSharpLua.Set = "{this}.x = {0}"
        public extern float X { get; set; }

        /// @CSharpLua.Get = "{this}.y"
        /// @CSharpLua.Set = "{this}.y = {0}"
        public extern float Y { get; set; }

        /// @CSharpLua.Template = "({0} + {1})"
        public static extern Vector2 operator +(Vector2 a, Vector2 b);

        /// @CSharpLua.Template = "({0} - {1})"
        public static extern Vector2 operator -(Vector2 a, Vector2 b);

        /// @CSharpLua.Template = "({0} * {1})"
        public static extern Vector2 operator *(Vector2 a, float b);

        /// @CSharpLua.Template = "Vec({this}.x, {this}.y, 0, 0)"
        public extern Vector2 Clone();

        /// <summary>The same underlying Vec, viewed as a Vector3 (z = 0).</summary>
        /// @CSharpLua.Template = "{this}"
        public extern Vector3 AsVector3();

        /// @CSharpLua.Template = "{this}:Magnitude()"
        public extern float Magnitude();
    }

    /// <summary>Axis-aligned box table ({min, max, center, extents}) as returned by GetAABB().</summary>
    public sealed class AABB
    {
        private AABB() { }

        /// @CSharpLua.Get = "{this}.min"
        public extern Vector3 Min { get; }

        /// @CSharpLua.Get = "{this}.max"
        public extern Vector3 Max { get; }

        /// @CSharpLua.Get = "{this}.center"
        public extern Vector3 Center { get; }

        /// @CSharpLua.Get = "{this}.extents"
        public extern Vector3 Extents { get; }
    }

    /// <summary>Bounding sphere + box table ({center, radius, min, max}) as returned by GetBounds().</summary>
    public sealed class Bounds
    {
        private Bounds() { }

        /// @CSharpLua.Get = "{this}.center"
        public extern Vector3 Center { get; }

        /// @CSharpLua.Get = "{this}.radius"
        public extern float Radius { get; }

        /// @CSharpLua.Get = "{this}.min"
        public extern Vector3 Min { get; }

        /// @CSharpLua.Get = "{this}.max"
        public extern Vector3 Max { get; }
    }

    /// <summary>World fog table. Construct one to pass to World.SetFogSettings; read from GetFogSettings.</summary>
    public sealed class FogSettings
    {
        /// @CSharpLua.Template = "{ enable = {0}, color = {1}, exponential = {2}, near = {3}, far = {4} }"
        public extern FogSettings(bool enable, Color color, bool exponential, float near, float far);

        /// @CSharpLua.Get = "{this}.enable"
        /// @CSharpLua.Set = "{this}.enable = {0}"
        public extern bool Enable { get; set; }

        /// @CSharpLua.Get = "{this}.color"
        /// @CSharpLua.Set = "{this}.color = {0}"
        public extern Color Color { get; set; }

        /// @CSharpLua.Get = "{this}.exponential"
        /// @CSharpLua.Set = "{this}.exponential = {0}"
        public extern bool Exponential { get; set; }

        /// @CSharpLua.Get = "{this}.near"
        /// @CSharpLua.Set = "{this}.near = {0}"
        public extern float Near { get; set; }

        /// @CSharpLua.Get = "{this}.far"
        /// @CSharpLua.Set = "{this}.far = {0}"
        public extern float Far { get; set; }
    }

    /// <summary>Result of World.FindNavPath ({success, points}).</summary>
    public sealed class NavPath
    {
        private NavPath() { }

        /// @CSharpLua.Get = "{this}.success"
        public extern bool Success { get; }

        /// @CSharpLua.Get = "CSharpCore.Array({this}.points, Polyphase.Vector3)"
        public extern Vector3[] Points { get; }
    }

    /// <summary>One entry of MultiHitResult.Hits ({node, normal, position, fraction}).</summary>
    public sealed class HitEntry
    {
        private HitEntry() { }

        /// @CSharpLua.Get = "{this}.node"
        public extern Node HitNode { get; }

        /// @CSharpLua.Get = "{this}.normal"
        public extern Vector3 HitNormal { get; }

        /// @CSharpLua.Get = "{this}.position"
        public extern Vector3 HitPosition { get; }

        /// @CSharpLua.Get = "{this}.fraction"
        public extern float HitFraction { get; }
    }

    /// <summary>Result of World.RayTestMulti ({start, end, numHits, hits}).</summary>
    public sealed class MultiHitResult
    {
        private MultiHitResult() { }

        /// @CSharpLua.Get = "{this}.numHits"
        public extern int NumHits { get; }

        /// @CSharpLua.Get = "CSharpCore.Array({this}.hits, Polyphase.HitEntry)"
        public extern HitEntry[] Hits { get; }
    }

    /// <summary>Result of Camera3D.TraceScreenToWorld (world position + hit node).</summary>
    public sealed class ScreenTrace
    {
        private ScreenTrace() { }

        /// @CSharpLua.Get = "{this}[1]"
        public extern Vector3 Position { get; }

        /// @CSharpLua.Get = "{this}[2]"
        public extern Node HitNode { get; }
    }

    /// <summary>A discovered network session (Network.GetSessions / GetSession).</summary>
    public sealed class NetSessionInfo
    {
        private NetSessionInfo() { }

        /// @CSharpLua.Get = "{this}.name"
        public extern string Name { get; }

        /// @CSharpLua.Get = "{this}.ipAddress"
        public extern string IpAddress { get; }

        /// @CSharpLua.Get = "{this}.port"
        public extern int Port { get; }

        /// @CSharpLua.Get = "{this}.lobbyId"
        public extern string LobbyId { get; }

        /// @CSharpLua.Get = "{this}.maxPlayers"
        public extern int MaxPlayers { get; }

        /// @CSharpLua.Get = "{this}.numPlayers"
        public extern int NumPlayers { get; }
    }

    /// <summary>A connected client (Network.GetClients / connect + disconnect callbacks).</summary>
    public sealed class NetClientInfo
    {
        private NetClientInfo() { }

        /// @CSharpLua.Get = "{this}.id"
        public extern int Id { get; }

        /// @CSharpLua.Get = "{this}.ipAddress"
        public extern string IpAddress { get; }

        /// @CSharpLua.Get = "{this}.port"
        public extern int Port { get; }

        /// @CSharpLua.Get = "{this}.onlineId"
        public extern string OnlineId { get; }

        /// @CSharpLua.Get = "{this}.ping"
        public extern float Ping { get; }

        /// @CSharpLua.Get = "{this}.ready"
        public extern bool Ready { get; }
    }

    /// <summary>Options table for Network.OpenSession.</summary>
    public sealed class SessionOptions
    {
        /// @CSharpLua.Template = "{ name = {0}, maxPlayers = {1}, port = {2}, lan = {3}, private = {4} }"
        public extern SessionOptions(string name, int maxPlayers, int port, bool lan, bool isPrivate);

        /// @CSharpLua.Template = "{ name = {0}, maxPlayers = {1} }"
        public extern SessionOptions(string name, int maxPlayers);
    }

    /// <summary>Engine Rect userdata (x, y, w, h). Rect.Create(x, y, w, h) makes one.</summary>
    public partial class Rect
    {
        /// @CSharpLua.Get = "{this}.x"
        /// @CSharpLua.Set = "{this}.x = {0}"
        public extern float X { get; set; }

        /// @CSharpLua.Get = "{this}.y"
        /// @CSharpLua.Set = "{this}.y = {0}"
        public extern float Y { get; set; }

        /// @CSharpLua.Get = "{this}.w"
        /// @CSharpLua.Set = "{this}.w = {0}"
        public extern float W { get; set; }

        /// @CSharpLua.Get = "{this}.h"
        /// @CSharpLua.Set = "{this}.h = {0}"
        public extern float H { get; set; }
    }

    /// <summary>DataAsset: typed accessors over the Lua Get/Set (values are the property's datum type).</summary>
    public partial class DataAsset
    {
        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern float GetFloat(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern int GetInt(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern bool GetBool(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern string GetString(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern Vector3 GetVector(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern Color GetColor(string property);

        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern Asset GetAsset(string property);

        /// <summary>Typed Get for any handle (unchecked cast).</summary>
        /// @CSharpLua.Template = "{this}:Get({0})"
        public extern T Get<T>(string property);
    }
}
