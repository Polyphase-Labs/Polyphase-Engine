#pragma warning disable CS0626 // extern without DllImport — transpile surface only

namespace Polyphase
{
    /// <summary>
    /// Handle to an engine Asset. Like Node, the Lua value IS the engine asset
    /// userdata — never constructed from C#; instances arrive from the engine
    /// (AssetManager.LoadAsset, node getters, [Property] fields).
    /// </summary>
    public partial class Asset
    {
        protected Asset() { }

        /// @CSharpLua.Template = "{this}:GetName()"
        public extern string GetName();

        /// @CSharpLua.Get = "{this}:GetName()"
        public extern string Name { get; }

        /// <summary>Engine class name: "Texture", "MaterialLite", "StaticMesh", ...</summary>
        /// @CSharpLua.Template = "{this}:GetTypeName()"
        public extern string GetTypeName();

        /// @CSharpLua.Template = "{this}:IsLoaded()"
        public extern bool IsLoaded();

        /// @CSharpLua.Template = "{this}:IsTransient()"
        public extern bool IsTransient();

        /// @CSharpLua.Template = "{this}:IsRefCounted()"
        public extern bool IsRefCounted();

        /// @CSharpLua.Template = "{this}:GetRefCount()"
        public extern int GetRefCount();
    }

    /// <summary>Handle to a Texture asset.</summary>
    public partial class Texture : Asset
    {
        protected Texture() { }

        /// @CSharpLua.Template = "{this}:GetWidth()"
        public extern int GetWidth();

        /// @CSharpLua.Template = "{this}:GetHeight()"
        public extern int GetHeight();

        /// @CSharpLua.Template = "{this}:GetMipLevels()"
        public extern int GetMipLevels();

        /// @CSharpLua.Template = "{this}:GetLayers()"
        public extern int GetLayers();

        /// @CSharpLua.Template = "{this}:IsMipmapped()"
        public extern bool IsMipmapped();

        /// @CSharpLua.Template = "{this}:IsRenderTarget()"
        public extern bool IsRenderTarget();
    }

    /// <summary>Handle to a StaticMesh asset (the geometry a StaticMesh3D node renders).</summary>
    public partial class StaticMesh : Asset
    {
        protected StaticMesh() { }

        /// <summary>The asset's default material (shared by every node using the mesh).</summary>
        /// @CSharpLua.Get = "{this}:GetMaterial()"
        /// @CSharpLua.Set = "{this}:SetMaterial({0})"
        public extern Material Material { get; set; }

        /// @CSharpLua.Template = "{this}:GetMaterial()"
        public extern Material GetMaterial();

        /// @CSharpLua.Template = "{this}:SetMaterial({0})"
        public extern void SetMaterial(Material material);

        /// @CSharpLua.Template = "{this}:GetNumVertices()"
        public extern int GetNumVertices();

        /// @CSharpLua.Template = "{this}:GetNumIndices()"
        public extern int GetNumIndices();

        /// @CSharpLua.Template = "{this}:GetNumFaces()"
        public extern int GetNumFaces();

        /// @CSharpLua.Template = "{this}:HasVertexColor()"
        public extern bool HasVertexColor();

        /// @CSharpLua.Template = "{this}:HasTriangleMeshCollision()"
        public extern bool HasTriangleMeshCollision();

        /// @CSharpLua.Template = "{this}:EnableTriangleMeshCollision({0})"
        public extern void EnableTriangleMeshCollision(bool enable);
    }

    /// <summary>
    /// Handle to any material (Material / MaterialInstance / MaterialLite).
    /// Mesh3D.GetMaterial() returns this base type; use `as MaterialLite` /
    /// `as MaterialInstance` for the concrete API, or IsLite()/IsInstance().
    /// </summary>
    public partial class Material : Asset
    {
        protected Material() { }

        /// @CSharpLua.Template = "{this}:IsBase()"
        public extern bool IsBase();

        /// @CSharpLua.Template = "{this}:IsInstance()"
        public extern bool IsInstance();

        /// @CSharpLua.Template = "{this}:IsLite()"
        public extern bool IsLite();

        /// <summary>Named shader parameters (graph materials and their instances).</summary>
        /// @CSharpLua.Template = "{this}:SetScalarParameter({0}, {1})"
        public extern void SetScalarParameter(string name, float value);

        /// @CSharpLua.Template = "{this}:SetVectorParameter({0}, {1})"
        public extern void SetVectorParameter(string name, Color value);

        /// @CSharpLua.Template = "{this}:SetVectorParameter({0}, {1})"
        public extern void SetVectorParameter(string name, Vector3 value);

        /// @CSharpLua.Template = "{this}:SetTextureParameter({0}, {1})"
        public extern void SetTextureParameter(string name, Texture value);

        /// @CSharpLua.Template = "{this}:GetScalarParameter({0})"
        public extern float GetScalarParameter(string name);

        /// @CSharpLua.Template = "{this}:GetVectorParameter({0})"
        public extern Color GetVectorParameter(string name);

        /// @CSharpLua.Template = "{this}:GetTextureParameter({0})"
        public extern Texture GetTextureParameter(string name);

        /// @CSharpLua.Template = "{this}:GetBlendMode()"
        public extern BlendMode GetBlendMode();

        /// @CSharpLua.Template = "{this}:GetMaskCutoff()"
        public extern float GetMaskCutoff();

        /// @CSharpLua.Template = "{this}:GetSortPriority()"
        public extern int GetSortPriority();

        /// @CSharpLua.Template = "{this}:IsDepthTestDisabled()"
        public extern bool IsDepthTestDisabled();

        /// @CSharpLua.Template = "{this}:ShouldApplyFog()"
        public extern bool ShouldApplyFog();

        /// @CSharpLua.Template = "{this}:GetCullMode()"
        public extern CullMode GetCullMode();
    }

    /// <summary>Handle to a MaterialInstance (parameter overrides on a base material).</summary>
    public partial class MaterialInstance : Material
    {
        protected MaterialInstance() { }

        /// <summary>New transient instance of a base material (null = engine default).</summary>
        /// @CSharpLua.Template = "MaterialInstance.Create({0})"
        public static extern MaterialInstance Create(Material baseMaterial);

        /// @CSharpLua.Template = "{this}:GetBaseMaterial()"
        public extern Material GetBaseMaterial();

        /// @CSharpLua.Template = "{this}:SetBaseMaterial({0})"
        public extern void SetBaseMaterial(Material baseMaterial);
    }

    /// <summary>
    /// Handle to a MaterialLite — the fixed-function material (color, textures,
    /// shading model, fresnel, emission...) that renders on every platform.
    /// </summary>
    public partial class MaterialLite : Material
    {
        protected MaterialLite() { }

        /// <summary>New transient MaterialLite, copying `source` when given (null = defaults).</summary>
        /// @CSharpLua.Template = "MaterialLite.Create({0})"
        public static extern MaterialLite Create(Material source);

        /// @CSharpLua.Template = "MaterialLite.Create(nil)"
        public static extern MaterialLite Create();

        /// @CSharpLua.Get = "{this}:GetColor()"
        /// @CSharpLua.Set = "{this}:SetColor({0})"
        public extern Color Color { get; set; }

        /// @CSharpLua.Template = "{this}:GetColor()"
        public extern Color GetColor();

        /// @CSharpLua.Template = "{this}:SetColor({0})"
        public extern void SetColor(Color color);

        /// <summary>Texture slots are 0-based (0..3).</summary>
        /// @CSharpLua.Template = "{this}:GetTexture({0})"
        public extern Texture GetTexture(int slot);

        /// @CSharpLua.Template = "{this}:SetTexture({0}, {1})"
        public extern void SetTexture(int slot, Texture texture);

        /// @CSharpLua.Get = "{this}:GetShadingModel()"
        /// @CSharpLua.Set = "{this}:SetShadingModel({0})"
        public extern ShadingModel ShadingModel { get; set; }

        /// @CSharpLua.Template = "{this}:SetBlendMode({0})"
        public extern void SetBlendMode(BlendMode mode);

        /// @CSharpLua.Template = "{this}:SetCullMode({0})"
        public extern void SetCullMode(CullMode mode);

        /// @CSharpLua.Get = "{this}:GetOpacity()"
        /// @CSharpLua.Set = "{this}:SetOpacity({0})"
        public extern float Opacity { get; set; }

        /// @CSharpLua.Get = "{this}:GetEmission()"
        /// @CSharpLua.Set = "{this}:SetEmission({0})"
        public extern float Emission { get; set; }

        /// @CSharpLua.Get = "{this}:GetSpecular()"
        /// @CSharpLua.Set = "{this}:SetSpecular({0})"
        public extern float Specular { get; set; }

        /// @CSharpLua.Get = "{this}:GetWrapLighting()"
        /// @CSharpLua.Set = "{this}:SetWrapLighting({0})"
        public extern float WrapLighting { get; set; }

        /// @CSharpLua.Template = "{this}:EnableFresnel({0})"
        public extern void EnableFresnel(bool enable);

        /// @CSharpLua.Template = "{this}:IsFresnelEnabled()"
        public extern bool IsFresnelEnabled();

        /// @CSharpLua.Get = "{this}:GetFresnelColor()"
        /// @CSharpLua.Set = "{this}:SetFresnelColor({0})"
        public extern Color FresnelColor { get; set; }

        /// @CSharpLua.Get = "{this}:GetFresnelPower()"
        /// @CSharpLua.Set = "{this}:SetFresnelPower({0})"
        public extern float FresnelPower { get; set; }

        /// <summary>UV offset of UV set 0; only X/Y of the vector are used.</summary>
        /// @CSharpLua.Get = "{this}:GetUvOffset()"
        /// @CSharpLua.Set = "{this}:SetUvOffset({0})"
        public extern Vector3 UvOffset { get; set; }

        /// @CSharpLua.Get = "{this}:GetUvScale()"
        /// @CSharpLua.Set = "{this}:SetUvScale({0})"
        public extern Vector3 UvScale { get; set; }

        /// @CSharpLua.Template = "{this}:SetMaskCutoff({0})"
        public extern void SetMaskCutoff(float cutoff);

        /// @CSharpLua.Template = "{this}:SetSortPriority({0})"
        public extern void SetSortPriority(int priority);

        /// @CSharpLua.Template = "{this}:SetDepthTestDisabled({0})"
        public extern void SetDepthTestDisabled(bool disabled);

        /// @CSharpLua.Template = "{this}:GetTevMode({0})"
        public extern TevMode GetTevMode(int slot);

        /// @CSharpLua.Template = "{this}:SetTevMode({0}, {1})"
        public extern void SetTevMode(int slot, TevMode mode);

        /// @CSharpLua.Template = "{this}:GetUvMap({0})"
        public extern int GetUvMap(int slot);

        /// @CSharpLua.Template = "{this}:SetUvMap({0}, {1})"
        public extern void SetUvMap(int slot, int uvMap);
    }

    /// <summary>Engine asset registry (Lua global `AssetManager`). Names are asset
    /// names without extension, e.g. "SM_Cube", "M_Default".</summary>
    public static partial class AssetManager
    {
        /// <summary>Load (or fetch, if already loaded) an asset by name; null if unknown.</summary>
        /// @CSharpLua.Template = "AssetManager.LoadAsset({0})"
        public static extern Asset LoadAsset(string name);

        /// <summary>Typed LoadAsset: `AssetManager.Load&lt;StaticMesh&gt;("SM_Cube")`.
        /// The cast is unchecked (transpile-time only) — check IsLoaded()/GetTypeName()
        /// if the asset could be of another type.</summary>
        /// @CSharpLua.Template = "AssetManager.LoadAsset({0})"
        public static extern T Load<T>(string name) where T : Asset;

        /// <summary>Already-loaded asset by name, or null (never loads from disk).</summary>
        /// @CSharpLua.Template = "AssetManager.GetAsset({0})"
        public static extern Asset GetAsset(string name);

        /// <summary>Returns a handle immediately whose target fills in once the
        /// background load finishes (IsLoaded() flips to true).</summary>
        /// @CSharpLua.Template = "AssetManager.AsyncLoadAsset({0})"
        public static extern Asset AsyncLoadAsset(string name);

        /// @CSharpLua.Template = "AssetManager.UnloadAsset({0})"
        public static extern void UnloadAsset(string name);

        /// <summary>Unload every asset nothing references any more.</summary>
        /// @CSharpLua.Template = "AssetManager.RefSweep()"
        public static extern void RefSweep();
    }
}
