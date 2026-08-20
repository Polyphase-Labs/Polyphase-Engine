#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Handle to an engine Node. The underlying Lua value IS the node userdata —
    /// there is no wrapper object at runtime; every member maps directly onto the
    /// engine's Lua binding for that node.
    /// </summary>
    public class Node
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

        /// @CSharpLua.Template = "{this}:GetChild({0})"
        public extern Node GetChild(int index);

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

        /// @CSharpLua.Template = "{this}:EmitSignal({0})"
        public extern void EmitSignal(string signalName);
    }

    /// <summary>
    /// Handle to an engine Node3D (transform-bearing node).
    /// </summary>
    public class Node3D : Node
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
    public class Primitive3D : Node3D
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
    public class Camera3D : Node3D
    {
        protected Camera3D() { }

        /// @CSharpLua.Get = "{this}:GetFieldOfView()"
        /// @CSharpLua.Set = "{this}:SetFieldOfView({0})"
        public extern float FieldOfView { get; set; }
    }

    /// <summary>Handle to a SkeletalMesh3D (animated mesh).</summary>
    public class SkeletalMesh3D : Node3D
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

    /// <summary>Handle to the world a node lives in (Script.World).</summary>
    public sealed class World
    {
        private World() { }

        /// @CSharpLua.Template = "{this}:GetRootNode()"
        public extern Node GetRootNode();

        /// <summary>Ray test against physics. mask 0x02 = default environment group.</summary>
        /// @CSharpLua.Template = "{this}:RayTest({0}, {1}, {2})"
        public extern HitResult RayTest(Vector3 start, Vector3 end, int collisionMask);

        /// @CSharpLua.Template = "{this}:FindNode({0})"
        public extern Node FindNode(string name);
    }
}
