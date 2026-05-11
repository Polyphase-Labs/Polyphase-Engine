# Addon-Packaged Material Shaders

Ship `MaterialBase` shader source files directly inside your addon package so users do not have to copy `.glsl` files into the project `Shaders/` folder manually.

## Outcome

- Your addon can include material shader sources under `Packages/<addon-id>/Shaders/...`.
- `MaterialBase` resolves shader names from engine, project, and addon package paths.
- Packaged builds copy addon `Shaders/` folders into the packaged project output.
- Users can disambiguate shader name collisions with a package-qualified shader name.

## Directory layout

```
Packages/com.example.retro-pbr/
    package.json
    Source/
        RetroPbrAddon.cpp
    Shaders/
        mat/
            RetroPBR.glsl
        ToonOutline.glsl
```

Both of these are valid shader locations:

- `Shaders/mat/<name>.glsl`
- `Shaders/<name>.glsl`

## Shader lookup order

When a `MaterialBase` has `Shader = "MyShader"`, the engine searches in this order:

1. `Engine/Shaders/GLSL/mat/MyShader.glsl`
2. `<Project>/Shaders/MyShader.glsl`
3. `<Project>/Packages/*/Shaders/mat/MyShader.glsl`
4. `<Project>/Packages/*/Shaders/MyShader.glsl`

If multiple addon shaders match the same name, the engine logs a warning and picks a deterministic first match.

To avoid ambiguity, use package-qualified shader names:

- `Shader = "com.example.retro-pbr/RetroPBR"`

That explicitly resolves inside `Packages/com.example.retro-pbr/` first.

## Minimal shader example

`Packages/com.example.retro-pbr/Shaders/mat/RetroPBR.glsl`

```glsl
MAT_VECTOR(BaseColor)
MAT_SCALAR(Roughness)
MAT_SCALAR(Metalness)
MAT_TEXTURE(AlbedoTex)

vec4 SampleBaseColor(vec2 uv)
{
    vec4 tex = texture(AlbedoTex, uv);
    return tex * BaseColor;
}

void UserMaterial(in MaterialVertexInput vtx, inout MaterialSurface surf)
{
    vec4 c = SampleBaseColor(vtx.uv0);
    surf.baseColor = c.rgb;
    surf.alpha = c.a;
    surf.roughness = clamp(Roughness, 0.02, 1.0);
    surf.metallic = clamp(Metalness, 0.0, 1.0);
}
```

## Using it in a material

1. Create a `MaterialBase` asset.
2. Set **Shader** to either:
   - `RetroPBR` (works if unique), or
   - `com.example.retro-pbr/RetroPBR` (recommended).
3. Click **Compile** in the inspector.
4. Assign the material to meshes as usual.

## Packaging behavior

During build packaging, the engine now copies:

- `Packages/<addon>/Scripts/`
- `Packages/<addon>/Shaders/`

into the packaged project directory, so addon shader source is available in packaged data exactly like addon scripts.

## Notes

- Engine-level shaders still take precedence over project/addon names.
- Project `Shaders/` still takes precedence over addon package shaders.
- If a shader cannot be found, `MaterialBase::Compile` logs an error.

## See also

- [Native Addon Overview](../NativeAddon.md)
- [Custom Material Type (Node Graph)](Editor/NodeGraph/CustomMaterialType.md)
- [External Library Integration](ExternalLibrary.md)
