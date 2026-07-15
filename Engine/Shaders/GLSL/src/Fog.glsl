
void ApplyFog(inout vec4 ioColor,
             vec3 fragmentPos,
             GlobalUniforms global)
{
    if (global.mFogEnabled != 0)
    {

        float fragDepth = distance(fragmentPos, global.mViewPosition.xyz);
        float fogLength = (global.mFogFar - global.mFogNear);
        float fogAlpha = clamp((fragDepth - global.mFogNear) / fogLength, 0.0, 1.0);
        fogAlpha *= global.mFogColor.a;
        float fogFactor = 0.0;

        if (global.mFogDensityFunc == FOG_FUNC_LINEAR)
        {
            fogFactor = fogAlpha;
        }
        else if (global.mFogDensityFunc == FOG_FUNC_EXPONENTIAL)
        {
            // Just hack this for now to get close to dolphin exponential.
            fogFactor = 1.0 - clamp(pow(20,-fogAlpha), 0.0 , 1.0);
        }

        ioColor.rgb = mix(ioColor.rgb, global.mFogColor.rgb, fogFactor);
    }
}

// Horizon-based fog for the skybox. Distance fog is meaningless on the sky (it sits far
// beyond the fog Far plane and would flood the whole dome), so instead fade by the view
// ray's elevation: full fog at/below the horizon, clearing toward the zenith. This
// seamlessly continues fogged distant terrain into the sky.
void ApplyFogSky(inout vec4 ioColor,
                 vec3 fragmentPos,
                 GlobalUniforms global)
{
    if (global.mFogEnabled != 0)
    {
        // View-ray elevation: ~0 at the horizon, ->1 looking straight up.
        vec3 dir = normalize(fragmentPos - global.mViewPosition.xyz);
        const float kHorizonFalloff = 0.25;             // ~14 deg fade band above the horizon
        float t = clamp(dir.y / kHorizonFalloff, 0.0, 1.0);
        float fogFactor = (1.0 - smoothstep(0.0, 1.0, t)) * global.mFogColor.a;

        ioColor.rgb = mix(ioColor.rgb, global.mFogColor.rgb, fogFactor);
    }
}
