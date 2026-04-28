#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsl"

struct FxaaUniforms
{
    float mEdgeThresholdMin;
    float mEdgeThresholdMax;
    float mSubpixelQuality;
    int mPad0;
};

layout (set = 0, binding = 0) uniform GlobalUniformBuffer
{
    GlobalUniforms global;
};

layout (set = 1, binding = 0) uniform FxaaUniformBuffer
{
    FxaaUniforms fxaa;
};

layout (set = 1, binding = 1) uniform sampler2D srcTexture;

layout (location = 0) in vec2 inTexcoord;
layout (location = 0) out vec4 outColor;

float Luminance(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 texelSize = 1.0 / global.mScreenDimensions;

    // Sample center and 4 cardinal neighbors
    vec3 colorCenter = texture(srcTexture, inTexcoord).rgb;
    float lumaCenter = Luminance(colorCenter);

    float lumaDown  = Luminance(texture(srcTexture, inTexcoord + vec2( 0, -1) * texelSize).rgb);
    float lumaUp    = Luminance(texture(srcTexture, inTexcoord + vec2( 0,  1) * texelSize).rgb);
    float lumaLeft  = Luminance(texture(srcTexture, inTexcoord + vec2(-1,  0) * texelSize).rgb);
    float lumaRight = Luminance(texture(srcTexture, inTexcoord + vec2( 1,  0) * texelSize).rgb);

    // Find min/max luma around current pixel
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    // Early exit if contrast too low
    if (lumaRange < max(fxaa.mEdgeThresholdMin, lumaMax * fxaa.mEdgeThresholdMax))
    {
        outColor = vec4(colorCenter, 1.0);
        return;
    }

    // Sample 4 diagonal neighbors
    float lumaDownLeft  = Luminance(texture(srcTexture, inTexcoord + vec2(-1, -1) * texelSize).rgb);
    float lumaUpRight   = Luminance(texture(srcTexture, inTexcoord + vec2( 1,  1) * texelSize).rgb);
    float lumaUpLeft    = Luminance(texture(srcTexture, inTexcoord + vec2(-1,  1) * texelSize).rgb);
    float lumaDownRight = Luminance(texture(srcTexture, inTexcoord + vec2( 1, -1) * texelSize).rgb);

    float lumaDownUp    = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;

    // Compute subpixel offset
    float lumaLeftCorners  = lumaDownLeft + lumaUpLeft;
    float lumaDownCorners  = lumaDownLeft + lumaDownRight;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners    = lumaUpRight + lumaUpLeft;

    // Determine edge direction (horizontal vs vertical)
    float edgeHorizontal = abs(-2.0 * lumaLeft   + lumaLeftCorners)  +
                           abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 +
                           abs(-2.0 * lumaRight  + lumaRightCorners);
    float edgeVertical   = abs(-2.0 * lumaUp     + lumaUpCorners)    +
                           abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 +
                           abs(-2.0 * lumaDown   + lumaDownCorners);
    bool isHorizontal = (edgeHorizontal >= edgeVertical);

    // Choose edge direction and step
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;

    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    float stepLength = isHorizontal ? texelSize.y : texelSize.x;
    float lumaLocalAverage = 0.0;

    if (is1Steepest)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    }
    else
    {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }

    // Shift UV in perpendicular direction by half step
    vec2 currentUv = inTexcoord;
    if (isHorizontal)
        currentUv.y += stepLength * 0.5;
    else
        currentUv.x += stepLength * 0.5;

    // Edge search along the edge direction
    vec2 offset = isHorizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    vec2 uv1 = currentUv - offset;
    vec2 uv2 = currentUv + offset;

    float lumaEnd1 = Luminance(texture(srcTexture, uv1).rgb) - lumaLocalAverage;
    float lumaEnd2 = Luminance(texture(srcTexture, uv2).rgb) - lumaLocalAverage;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    // Continue searching with progressive step sizes (FXAA 3.11 Quality 39)
    if (!reachedBoth)
    {
        for (int i = 0; i < 12; ++i)
        {
            float quality;
            if      (i < 5)  quality = 1.0;
            else if (i == 5)  quality = 1.5;
            else if (i < 10) quality = 2.0;
            else if (i == 10) quality = 4.0;
            else              quality = 8.0;

            if (!reached1) lumaEnd1 = Luminance(texture(srcTexture, uv1).rgb) - lumaLocalAverage;
            if (!reached2) lumaEnd2 = Luminance(texture(srcTexture, uv2).rgb) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;

            if (!reached1) uv1 -= offset * quality;
            if (!reached2) uv2 += offset * quality;

            if (reached1 && reached2) break;
        }
    }

    // Compute final offset
    float distance1 = isHorizontal ? (inTexcoord.x - uv1.x) : (inTexcoord.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - inTexcoord.x) : (uv2.y - inTexcoord.y);

    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;

    float pixelOffset = -distanceFinal / edgeThickness + 0.5;

    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    // Subpixel antialiasing
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * fxaa.mSubpixelQuality;

    finalOffset = max(finalOffset, subPixelOffsetFinal);

    // Compute final UV and sample
    vec2 finalUv = inTexcoord;
    if (isHorizontal)
        finalUv.y += finalOffset * stepLength;
    else
        finalUv.x += finalOffset * stepLength;

    outColor = vec4(texture(srcTexture, finalUv).rgb, 1.0);
}
