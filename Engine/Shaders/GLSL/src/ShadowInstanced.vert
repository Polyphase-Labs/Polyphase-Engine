#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsl"

layout (set = 0, binding = 0) uniform GlobalUniformBuffer
{
    GlobalUniforms global;
};

layout (set = 1, binding = 0) uniform GeometryUniformBuffer
{
    GeometryUniforms geometry;
};

layout (std140, set = 1, binding = 1) buffer GeometryInstanceBuffer
{
    MeshInstanceData instanceData[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;

layout(location = 0) out vec2 outTexcoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    mat4 worldMatrix = geometry.mWorldMatrix * instanceData[gl_InstanceIndex].mTransform;
    gl_Position = global.mShadowViewProj * worldMatrix * vec4(inPosition, 1.0);
    outTexcoord = inTexcoord;
}
