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

        /// @CSharpLua.Template = "{this}:LookAt({0}, {1})"
        public extern void LookAt(Vector3 worldPosition, Vector3 up);

        /// @CSharpLua.Template = "{this}:GetForwardVector()"
        public extern Vector3 GetForwardVector();

        /// @CSharpLua.Template = "{this}:GetRightVector()"
        public extern Vector3 GetRightVector();

        /// @CSharpLua.Template = "{this}:GetUpVector()"
        public extern Vector3 GetUpVector();
    }
}
