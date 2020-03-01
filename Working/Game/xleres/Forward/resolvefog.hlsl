// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if OUTPUT_FOG_COLOR == 1

    #include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
    #include "../TechniqueLibrary/Framework/Transform.hlsl"
    #include "../TechniqueLibrary/SceneEngine/Lighting/RangeFogResolve.hlsl"
    #include "../TechniqueLibrary/SceneEngine/Lighting/BasicLightingEnvironment.hlsl"
    #include "../TechniqueLibrary/SceneEngine/VolumetricEffect/resolvefog.hlsl"

    float4 ResolveOutputFogColor(float3 localPosition, float3 localSpaceView)
    {
        // There are two differ distances we can use here
        // 	-- 	either straight-line distance to the view point, or distance to the view plane
        //		distance to the view plane is a little more efficient, and should better match
        //		the calculations we make for deferred geometry.
        // We can calculate this at a per-vertex level or a per-pixel level. For some objects, there
        // may actually be more vertices than pixels -- in which case, maybe per-pixel is better...?
        //
        // Note that for order independent transparency objects, we may get a better result by doing
        // this only once per pixel, after an approximate depth has been calculated.
        float3 negCameraForward = float3(CameraBasis[0].z, CameraBasis[1].z, CameraBasis[2].z);
        float3 localViewVector = localSpaceView - localPosition;
        float distanceToView = dot(localViewVector, negCameraForward);
        float4 fogColor = float4(0.0.xxx, 1.0f);
        LightResolve_RangeFog(BasicRangeFog, distanceToView, fogColor.a, fogColor.rgb);

        // Also apply fogging from volumetric fog volumes, if they exists
        [branch] if (BasicVolumeFog.EnableFlag != false) {
            float transmission, inscatter;
            CalculateTransmissionAndInscatter(
                BasicVolumeFog,
                localSpaceView, localPosition, transmission, inscatter);

            float cosTheta = -dot(localViewVector, BasicLight[0].Position) * rsqrt(dot(localViewVector, localViewVector));
            float4 volFog = float4(inscatter * GetInscatterColor(BasicVolumeFog, cosTheta), transmission);
            fogColor.rgb = volFog.rgb + fogColor.rgb * volFog.a;
            fogColor.a *= volFog.a;
        }
        return fogColor;
    }

#endif
