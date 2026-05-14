#version 450
#extension GL_ARB_separate_shader_objects : enable

// Depth-only shadow pass. We intentionally do not bind set=2 (material) from
// the per-mesh draw site, because doing so requires either:
//  - matching the receiving material's descriptor layout (different per
//    MaterialBase variant, so it doesn't generalize), OR
//  - overriding the fragment shader via BindMaterialResource (which would
//    swap Shadow.frag for the material's PBR shader, but the shadow render
//    pass has no color attachment for that shader to write to).
//
// Skipping the material set means alpha-masked materials cast a SOLID shadow
// of their bounding geometry instead of a punched-out one. Worth revisiting if
// foliage / fences / windows become important; for now opaque shadows work.
layout(location = 0) in vec2 inTexcoord;

void main()
{
}