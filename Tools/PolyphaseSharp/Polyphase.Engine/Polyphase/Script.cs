#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Base class for a Polyphase script attached to any Node. Derive from this
    /// (or Script3D for transform-bearing nodes), override the lifecycle methods
    /// you need, and call the node API directly — bare calls like SetName(...)
    /// operate on the node the script is attached to, exactly like `self:...`
    /// in engine Lua scripts.
    ///
    /// At runtime the C# instance is a companion object; the attached node is
    /// reachable as the Node property (this.__node in generated Lua).
    /// </summary>
    public class Script
    {
        // ---- Lifecycle (override what you need; the transpiler forwards only
        //      the methods your class actually overrides) ----

        /// <summary>Called when the script instance is created, BEFORE serialized
        /// property values are applied. Field initializers run here.</summary>
        public virtual void Create() { }

        /// <summary>Called after properties are applied, before Start.</summary>
        public virtual void Awake() { }

        /// <summary>Called on the first tick after the node starts.</summary>
        public virtual void Start() { }

        /// <summary>Called every frame while the game is running.</summary>
        public virtual void Tick(float deltaTime) { }

        /// <summary>Called every frame in the editor when the game is not running.</summary>
        public virtual void EditorTick(float deltaTime) { }

        /// <summary>Called when the node stops.</summary>
        public virtual void Stop() { }

        /// <summary>Called when the script instance is destroyed.</summary>
        public virtual void Destroy() { }

        /// <summary>Called when another primitive begins overlapping this node.</summary>
        public virtual void BeginOverlap(Node other) { }

        /// <summary>Called when another primitive stops overlapping this node.</summary>
        public virtual void EndOverlap(Node other) { }

        /// <summary>Called on physics collision with contact point and normal.</summary>
        public virtual void OnCollision(Node other, Vector3 impactPoint, Vector3 impactNormal) { }

        // ---- The attached node ----

        /// <summary>The node this script is attached to (Unity's gameObject analogue).</summary>
        /// @CSharpLua.Get = "{this}.__node"
        public extern Node Node { get; }

        /// <summary>The world the node lives in (engine sets `world` on every scripted node).</summary>
        /// @CSharpLua.Get = "{this}.__node.world"
        public extern World World { get; }

        // ---- Node API, bare-call surface (mirrors Polyphase.Node) ----

        /// @CSharpLua.Template = "{this}.__node:GetName()"
        public extern string GetName();

        /// @CSharpLua.Template = "{this}.__node:SetName({0})"
        public extern void SetName(string name);

        /// @CSharpLua.Get = "{this}.__node:GetName()"
        /// @CSharpLua.Set = "{this}.__node:SetName({0})"
        public extern string Name { get; set; }

        /// @CSharpLua.Template = "{this}.__node:SetActive({0})"
        public extern void SetActive(bool active);

        /// @CSharpLua.Template = "{this}.__node:IsActive()"
        public extern bool IsActive();

        /// @CSharpLua.Template = "{this}.__node:SetVisible({0})"
        public extern void SetVisible(bool visible);

        /// @CSharpLua.Template = "{this}.__node:IsVisible()"
        public extern bool IsVisible();

        /// @CSharpLua.Template = "{this}.__node:GetParent()"
        public extern Node GetParent();

        /// @CSharpLua.Template = "{this}.__node:FindChild({0}, true)"
        public extern Node FindChild(string name);

        /// @CSharpLua.Template = "{this}.__node:FindChild({0}, {1})"
        public extern Node FindChild(string name, bool recursive);

        /// @CSharpLua.Template = "{this}.__node:AddTag({0})"
        public extern void AddTag(string tag);

        /// @CSharpLua.Template = "{this}.__node:RemoveTag({0})"
        public extern void RemoveTag(string tag);

        /// @CSharpLua.Template = "{this}.__node:HasTag({0})"
        public extern bool HasTag(string tag);

        /// @CSharpLua.Template = "{this}.__node:EnableTick({0})"
        public extern void EnableTick(bool enable);

        /// @CSharpLua.Template = "{this}.__node:DestroyDeferred()"
        public extern void DestroyNode();

        /// @CSharpLua.Template = "{this}.__node:EmitSignal({0})"
        public extern void EmitSignal(string signalName);
    }

    /// <summary>
    /// Base class for scripts attached to Node3D (transform-bearing) nodes.
    /// Adds the transform API as bare calls / properties.
    /// </summary>
    public class Script3D : Script
    {
        /// <summary>The attached node, typed as Node3D.</summary>
        /// @CSharpLua.Get = "{this}.__node"
        public extern new Node3D Node { get; }

        // ---- Local transform ----

        /// @CSharpLua.Get = "{this}.__node:GetPosition()"
        /// @CSharpLua.Set = "{this}.__node:SetPosition({0})"
        public extern Vector3 Position { get; set; }

        /// @CSharpLua.Get = "{this}.__node:GetRotation()"
        /// @CSharpLua.Set = "{this}.__node:SetRotation({0})"
        public extern Vector3 Rotation { get; set; }

        /// @CSharpLua.Get = "{this}.__node:GetScale()"
        /// @CSharpLua.Set = "{this}.__node:SetScale({0})"
        public extern Vector3 Scale { get; set; }

        // ---- World transform ----

        /// @CSharpLua.Get = "{this}.__node:GetWorldPosition()"
        /// @CSharpLua.Set = "{this}.__node:SetWorldPosition({0})"
        public extern Vector3 WorldPosition { get; set; }

        /// @CSharpLua.Get = "{this}.__node:GetWorldRotation()"
        /// @CSharpLua.Set = "{this}.__node:SetWorldRotation({0})"
        public extern Vector3 WorldRotation { get; set; }

        /// @CSharpLua.Get = "{this}.__node:GetWorldScale()"
        /// @CSharpLua.Set = "{this}.__node:SetWorldScale({0})"
        public extern Vector3 WorldScale { get; set; }

        /// @CSharpLua.Template = "{this}.__node:AddRotation({0})"
        public extern void AddRotation(Vector3 deltaDegrees);

        /// @CSharpLua.Template = "{this}.__node:AddWorldRotation({0})"
        public extern void AddWorldRotation(Vector3 deltaDegrees);

        /// @CSharpLua.Template = "{this}.__node:RotateAround({0}, {1}, {2})"
        public extern void RotateAround(Vector3 pivot, Vector3 axis, float degrees);

        /// @CSharpLua.Template = "{this}.__node:LookAt({0}, {1})"
        public extern void LookAt(Vector3 worldPosition, Vector3 up);

        /// @CSharpLua.Template = "{this}.__node:GetForwardVector()"
        public extern Vector3 GetForwardVector();

        /// @CSharpLua.Template = "{this}.__node:GetRightVector()"
        public extern Vector3 GetRightVector();

        /// @CSharpLua.Template = "{this}.__node:GetUpVector()"
        public extern Vector3 GetUpVector();
    }
}
