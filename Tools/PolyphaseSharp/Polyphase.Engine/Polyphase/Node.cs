#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Handle to an engine Node. The underlying Lua value IS the node userdata —
    /// there is no wrapper object at runtime; every member maps directly onto the
    /// engine's Lua binding for that node.
    /// </summary>
    public partial class Node
    {
        // Protected (not internal): user code may subclass Node/Node3D to build a
        // typed façade over a Lua script's functions — extern members carrying
        // @CSharpLua.Template doc comments, typed via Lua.As<T>(node). Handle
        // classes are never constructed from C#; instances only arrive from the
        // engine.
        protected Node() { }

        /// @CSharpLua.Template = "{this}:GetName()"
        public extern string GetName();

        /// @CSharpLua.Template = "{this}:SetName({0})"
        public extern void SetName(string name);

        /// @CSharpLua.Get = "{this}:GetName()"
        /// @CSharpLua.Set = "{this}:SetName({0})"
        public extern string Name { get; set; }

        /// @CSharpLua.Template = "{this}:IsValid()"
        public extern bool IsValid();

        /// @CSharpLua.Template = "{this}:SetActive({0})"
        public extern void SetActive(bool active);

        /// @CSharpLua.Template = "{this}:IsActive()"
        public extern bool IsActive();

        /// @CSharpLua.Template = "{this}:SetVisible({0})"
        public extern void SetVisible(bool visible);

        /// @CSharpLua.Template = "{this}:IsVisible()"
        public extern bool IsVisible();

        /// @CSharpLua.Template = "{this}:GetParent()"
        public extern Node GetParent();

        /// @CSharpLua.Template = "{this}:FindChild({0}, true)"
        public extern Node FindChild(string name);

        /// @CSharpLua.Template = "{this}:FindChild({0}, {1})"
        public extern Node FindChild(string name, bool recursive);

        /// @CSharpLua.Template = "{this}:GetNumChildren()"
        public extern int GetNumChildren();

        /// @CSharpLua.Template = "{this}:AddChild({0})"
        public extern void AddChild(Node child);

        /// @CSharpLua.Template = "{this}:Attach({0})"
        public extern void Attach(Node newParent);

        /// @CSharpLua.Template = "{this}:Detach()"
        public extern void Detach();

        /// @CSharpLua.Template = "{this}:AddTag({0})"
        public extern void AddTag(string tag);

        /// @CSharpLua.Template = "{this}:RemoveTag({0})"
        public extern void RemoveTag(string tag);

        /// @CSharpLua.Template = "{this}:HasTag({0})"
        public extern bool HasTag(string tag);

        /// @CSharpLua.Template = "{this}:EnableTick({0})"
        public extern void EnableTick(bool enable);

        /// @CSharpLua.Template = "{this}:IsTickEnabled()"
        public extern bool IsTickEnabled();

        /// <summary>Deferred destroy (engine DestroyDeferred / Doom).</summary>
        /// @CSharpLua.Template = "{this}:DestroyDeferred()"
        public extern void Destroy();

        /// @CSharpLua.Template = "{this}:IsDestroyed()"
        public extern bool IsDestroyed();

        /// @CSharpLua.Template = "{this}:CreateChild({0})"
        public extern Node CreateChild(string nodeClass);


        /// <summary>Runtime class check by engine class name ("StaticMesh3D", "Camera3D", ...),
        /// including base classes. C# `is` / `as` against the handle classes below
        /// do the same thing with static typing.</summary>
        /// @CSharpLua.Template = "{this}:CheckType({0})"
        public extern bool Is(string className);

        /// @CSharpLua.Template = "{this}:GetWorld()"
        public extern World GetWorld();
    }

    /// <summary>
    /// Handle to an engine Node3D (transform-bearing node).
    /// </summary>
    public partial class Node3D : Node
    {
        protected Node3D() { }

        // ---- Local transform ----

        /// @CSharpLua.Get = "{this}:GetPosition()"
        /// @CSharpLua.Set = "{this}:SetPosition({0})"
        public extern Vector3 Position { get; set; }

        /// @CSharpLua.Get = "{this}:GetRotation()"
        /// @CSharpLua.Set = "{this}:SetRotation({0})"
        public extern Vector3 Rotation { get; set; }

        /// @CSharpLua.Get = "{this}:GetScale()"
        /// @CSharpLua.Set = "{this}:SetScale({0})"
        public extern Vector3 Scale { get; set; }

        // ---- World transform ----

        /// @CSharpLua.Get = "{this}:GetWorldPosition()"
        /// @CSharpLua.Set = "{this}:SetWorldPosition({0})"
        public extern Vector3 WorldPosition { get; set; }

        /// @CSharpLua.Get = "{this}:GetWorldRotation()"
        /// @CSharpLua.Set = "{this}:SetWorldRotation({0})"
        public extern Vector3 WorldRotation { get; set; }

        /// @CSharpLua.Get = "{this}:GetWorldScale()"
        /// @CSharpLua.Set = "{this}:SetWorldScale({0})"
        public extern Vector3 WorldScale { get; set; }

        /// @CSharpLua.Template = "{this}:AddRotation({0})"
        public extern void AddRotation(Vector3 deltaDegrees);

        /// @CSharpLua.Template = "{this}:AddWorldRotation({0})"
        public extern void AddWorldRotation(Vector3 deltaDegrees);

        /// @CSharpLua.Template = "{this}:RotateAround({0}, {1}, {2})"
        public extern void RotateAround(Vector3 pivot, Vector3 axis, float degrees);

        /// @CSharpLua.Template = "{this}:LookAt({0})"
        public extern void LookAt(Vector3 worldPosition);

        /// @CSharpLua.Template = "{this}:LookAt({0}, {1})"
        public extern void LookAt(Vector3 worldPosition, Vector3 up);

        /// @CSharpLua.Template = "{this}:GetForwardVector()"
        public extern Vector3 GetForwardVector();

        /// @CSharpLua.Template = "{this}:GetRightVector()"
        public extern Vector3 GetRightVector();

        /// @CSharpLua.Template = "{this}:GetUpVector()"
        public extern Vector3 GetUpVector();
    }

    /// <summary>
    /// Result of a physics sweep or ray test. Phantom over the Lua result table
    /// ({hitNode, hitNormal, hitPosition, hitFraction}).
    /// </summary>
    public sealed class HitResult
    {
        private HitResult() { }

        /// <summary>The node hit, or null.</summary>
        /// @CSharpLua.Get = "{this}.hitNode"
        public extern Node HitNode { get; }

        /// @CSharpLua.Get = "{this}.hitNormal"
        public extern Vector3 HitNormal { get; }

        /// @CSharpLua.Get = "{this}.hitPosition"
        public extern Vector3 HitPosition { get; }

        /// <summary>0..1 along the sweep/ray at the hit point.</summary>
        /// @CSharpLua.Get = "{this}.hitFraction"
        public extern float HitFraction { get; }
    }

    /// <summary>Handle to a Primitive3D (collision-capable node).</summary>
    public partial class Primitive3D : Node3D
    {
        protected Primitive3D() { }

        /// <summary>Sweep the collider to a world position; moves unless blocked.</summary>
        /// @CSharpLua.Template = "{this}:SweepToWorldPosition({0})"
        public extern HitResult SweepToWorldPosition(Vector3 worldPosition);

        /// <summary>mask 0 = default; testOnly true = query without moving.</summary>
        /// @CSharpLua.Template = "{this}:SweepToWorldPosition({0}, {1}, {2})"
        public extern HitResult SweepToWorldPosition(Vector3 worldPosition, int collisionMask, bool testOnly);

        /// @CSharpLua.Template = "{this}:EnableOverlaps({0})"
        public extern void EnableOverlaps(bool enable);

        /// @CSharpLua.Template = "{this}:EnableCollision({0})"
        public extern void EnableCollision(bool enable);

        /// @CSharpLua.Template = "{this}:EnablePhysics({0})"
        public extern void EnablePhysics(bool enable);
    }

    /// <summary>Handle to a Camera3D.</summary>
    public partial class Camera3D : Node3D
    {
        protected Camera3D() { }

        /// @CSharpLua.Get = "{this}:GetFieldOfView()"
        /// @CSharpLua.Set = "{this}:SetFieldOfView({0})"
        public extern float FieldOfView { get; set; }
    }

    /// <summary>Handle to a Mesh3D — base of StaticMesh3D / SkeletalMesh3D /
    /// TextMesh3D; owns the material slot.</summary>
    public partial class Mesh3D : Primitive3D
    {
        protected Mesh3D() { }

        /// <summary>The material actually used for rendering: the override if one
        /// is set, else the mesh asset's material. Cast with `as MaterialLite` /
        /// `as MaterialInstance` to reach the concrete API.</summary>
        /// @CSharpLua.Template = "{this}:GetMaterial()"
        public extern Material GetMaterial();

        /// @CSharpLua.Template = "{this}:GetMaterialOverride()"
        public extern Material GetMaterialOverride();

        /// <summary>Per-node material override (null clears it).</summary>
        /// @CSharpLua.Template = "{this}:SetMaterialOverride({0})"
        public extern void SetMaterialOverride(Material material);

        /// <summary>Create a MaterialInstance of the current material, assign it as
        /// this node's override, and return it — the way to recolor one mesh
        /// without touching the shared asset.</summary>
        /// @CSharpLua.Template = "{this}:InstantiateMaterial()"
        public extern Material InstantiateMaterial();

        /// @CSharpLua.Get = "{this}:IsBillboard()"
        /// @CSharpLua.Set = "{this}:SetBillboard({0})"
        public extern bool Billboard { get; set; }
    }

    /// <summary>Handle to a StaticMesh3D node (renders a StaticMesh asset).</summary>
    public partial class StaticMesh3D : Mesh3D
    {
        protected StaticMesh3D() { }

        /// @CSharpLua.Get = "{this}:GetStaticMesh()"
        /// @CSharpLua.Set = "{this}:SetStaticMesh({0})"
        public extern StaticMesh Mesh { get; set; }

        /// @CSharpLua.Template = "{this}:GetStaticMesh()"
        public extern StaticMesh GetStaticMesh();

        /// @CSharpLua.Template = "{this}:SetStaticMesh({0})"
        public extern void SetStaticMesh(StaticMesh mesh);

        /// @CSharpLua.Get = "{this}:GetUseTriangleCollision()"
        /// @CSharpLua.Set = "{this}:SetUseTriangleCollision({0})"
        public extern bool UseTriangleCollision { get; set; }

        /// @CSharpLua.Template = "{this}:GetBakeLighting()"
        public extern bool GetBakeLighting();
    }

    /// <summary>Handle to a SkeletalMesh3D (animated mesh).</summary>
    public partial class SkeletalMesh3D : Mesh3D
    {
        protected SkeletalMesh3D() { }

        /// @CSharpLua.Template = "{this}:PlayAnimation({0})"
        public extern void PlayAnimation(string animName);

        /// @CSharpLua.Template = "{this}:PlayAnimation({0}, {1}, {2})"
        public extern void PlayAnimation(string animName, int priority, bool loop);

        /// @CSharpLua.Template = "{this}:PlayAnimation({0}, {1}, {2}, {3}, {4})"
        public extern void PlayAnimation(string animName, int priority, bool loop, float speed, float weight);

        /// @CSharpLua.Template = "{this}:StopAnimation({0})"
        public extern void StopAnimation(string animName);

        /// @CSharpLua.Template = "{this}:StopAllAnimations()"
        public extern void StopAllAnimations();

        /// @CSharpLua.Template = "{this}:IsAnimationPlaying({0})"
        public extern bool IsAnimationPlaying(string animName);

        /// <summary>Queue an animation to play when a dependent one finishes.</summary>
        /// @CSharpLua.Template = "{this}:QueueAnimation({0}, {1}, {2}, {3}, {4}, {5})"
        public extern void QueueAnimation(string animName, string dependentAnim, int priority, bool loop, float speed, float weight);
    }

    /// <summary>
    /// Handle to a Spline3D. A spline's points are its "pointN" child nodes
    /// (placed in the editor); the *SplinePoint* / length / distance queries read
    /// those. Indices are 0-based here (the Lua binding is 1-based).
    /// </summary>
    public partial class Spline3D : Node3D
    {
        protected Spline3D() { }

        /// @CSharpLua.Template = "{this}:GetNumSplinePoints()"
        public extern int GetNumSplinePoints();

        /// <summary>Point position relative to the spline node.</summary>
        /// @CSharpLua.Template = "{this}:GetSplinePointPosition(({0}) + 1)"
        public extern Vector3 GetSplinePointPosition(int index);

        /// @CSharpLua.Template = "{this}:GetSplinePointWorldPosition(({0}) + 1)"
        public extern Vector3 GetSplinePointWorldPosition(int index);

        /// <summary>Arc length in world units (re-sampled every call — cache it).</summary>
        /// @CSharpLua.Template = "{this}:GetSplineLength()"
        public extern float GetSplineLength();

        /// <summary>Distance along the spline (world units) of the point closest to worldPos.</summary>
        /// @CSharpLua.Template = "{this}:GetClosestDistanceAlong({0})"
        public extern float GetClosestDistanceAlong(Vector3 worldPos);

        /// <summary>Catmull-Rom sample over the legacy AddPoint() point list, t in [0,1].</summary>
        /// @CSharpLua.Template = "{this}:GetPositionAt({0})"
        public extern Vector3 GetPositionAt(float t);

        /// <summary>Normalized tangent over the legacy AddPoint() point list, t in [0,1].</summary>
        /// @CSharpLua.Template = "{this}:GetTangentAt({0})"
        public extern Vector3 GetTangentAt(float t);

        // ---- Legacy point list (feeds GetPositionAt / GetTangentAt) ----

        /// @CSharpLua.Template = "{this}:AddPoint({0})"
        public extern void AddPoint(Vector3 point);

        /// @CSharpLua.Template = "{this}:ClearPoints()"
        public extern void ClearPoints();

        /// @CSharpLua.Template = "{this}:GetPointCount()"
        public extern int GetPointCount();

        /// @CSharpLua.Template = "{this}:GetPoint(({0}) + 1)"
        public extern Vector3 GetPoint(int index);

        /// @CSharpLua.Template = "{this}:SetPoint(({0}) + 1, {1})"
        public extern void SetPoint(int index, Vector3 point);

        // ---- Playback / follow links ----

        /// @CSharpLua.Template = "{this}:Play()"
        public extern void Play();

        /// @CSharpLua.Template = "{this}:Stop()"
        public extern void Stop();

        /// @CSharpLua.Get = "{this}:IsPaused()"
        /// @CSharpLua.Set = "{this}:SetPaused({0})"
        public extern bool Paused { get; set; }

        /// <summary>Follow-link slots are 1..64, matching the editor and Lua.</summary>
        /// @CSharpLua.Template = "{this}:SetFollowLinkEnabled({0}, {1})"
        public extern void SetFollowLinkEnabled(int link, bool enabled);

        /// @CSharpLua.Template = "{this}:IsFollowLinkEnabled({0})"
        public extern bool IsFollowLinkEnabled(int link);

        /// @CSharpLua.Template = "{this}:IsNearLinkFrom({0}, {1})"
        public extern bool IsNearLinkFrom(int link, float epsilon);

        /// @CSharpLua.Template = "{this}:IsNearLinkTo({0}, {1})"
        public extern bool IsNearLinkTo(int link, float epsilon);

        /// @CSharpLua.Template = "{this}:IsLinkDirectionForward({0}, {1})"
        public extern bool IsLinkDirectionForward(int link, float threshold);

        /// @CSharpLua.Template = "{this}:TriggerLink({0})"
        public extern bool TriggerLink(int link);

        /// @CSharpLua.Template = "{this}:CancelActiveLink()"
        public extern void CancelActiveLink();
    }

    /// <summary>Handle to the world a node lives in (Script.World).</summary>
    public sealed partial class World
    {
        private World() { }

        /// @CSharpLua.Template = "{this}:GetRootNode()"
        public extern Node GetRootNode();

        /// <summary>Ray test against physics. mask 0x02 = default environment group.</summary>
        /// @CSharpLua.Template = "{this}:RayTest({0}, {1}, {2})"
        public extern HitResult RayTest(Vector3 start, Vector3 end, int collisionMask);

        /// @CSharpLua.Template = "{this}:FindNode({0})"
        public extern Node FindNode(string name);

        /// <summary>Every node carrying the tag (empty array if none).</summary>
        /// @CSharpLua.Template = "CSharpCore.Array({this}:FindNodesWithTag({0}), Polyphase.Node)"
        public extern Node[] FindNodesWithTag(string tag);

        /// @CSharpLua.Template = "CSharpCore.Array({this}:FindNodesWithName({0}), Polyphase.Node)"
        public extern Node[] FindNodesWithName(string name);

        /// <summary>Spawn a node of an engine class ("StaticMesh3D", "PointLight3D", ...)
        /// at the world origin, parented to the root.</summary>
        /// @CSharpLua.Template = "{this}:SpawnNode({0})"
        public extern Node SpawnNode(string nodeClass);

        /// @CSharpLua.Template = "{this}:SpawnNode({0}, {1})"
        public extern Node SpawnNode(string nodeClass, Vector3 position);

        /// @CSharpLua.Template = "{this}:GetActiveCamera()"
        public extern Camera3D GetActiveCamera();

        /// @CSharpLua.Template = "{this}:SetActiveCamera({0})"
        public extern void SetActiveCamera(Camera3D camera);
    }
}
