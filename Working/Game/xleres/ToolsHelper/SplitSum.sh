// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define FORCE_GGX_REF

#include "../Lighting/SpecularMethods.h"
#include "../Lighting/ImageBased.h"

float2 GenerateSplitTerm(float NdotV, float roughness)
{
    // This generates the lookup table used by the glossy specular reflections
    // split sum approximation.
    // Based on the method presented by Unreal course notes:
    //  http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
    // While referencing the implementation in "IBL Baker":
    //  https://github.com/derkreature/IBLBaker/
    // We should maintain our own version, because we need to take into account the
    // same remapping on "roughness" that we do for run-time specular.
    //
    // For the most part, we will just be integrating the specular equation over the
    // hemisphere with white incoming light. We use importance sampling with a fixed
    // number of samples to try to make the work load manageable...

    float3 normal = float3(0.0f, 0.0f, 1.0f);
    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

    const uint sampleCount = 64 * 1024;

        // Karis suggests that using our typical Disney remapping
        // for the alpha value for the G term will make IBL much too dark.
        // Indeed, it changes the output texture a lot.
        // However, it seems like a good improvement to me. It does make
        // low roughness materials slightly darker. However, it seems to
        // have the biggest effect on high roughness materials. These materials
        // get much lower reflections around the edges. This seems to be
        // good, however... Because without it, low roughness get a clear
        // halo around their edges. This doesn't happen so much with the
        // runtime specular. So it actually seems better with the this remapping.
    float alphag = RoughnessToGAlpha(roughness);
    float G2 = SmithG(NdotV, alphag);

    float A = 0.f, B = 0.f;
    [loop] for (uint s=0; s<sampleCount; ++s) {
        float3 H, L;

            // We could consider clustering the samples around the highlight.
            // However, this changes the probability density function in confusing
            // ways. The result is similiar -- but it is as if a shadow has moved
            // across the output image.
            //
            // note -- I'm not sure it's really correct to pass the "alpha" value for
            //          "G" to BuildSampleHalfVectorGGX
            //          But the results are very similar to reference calculations. So
            //          it seems to get a result that's close, anyway.
        const bool clusterAroundHighlight = false;
        if (clusterAroundHighlight) {
            L = BuildSampleHalfVectorGGX(s, sampleCount, reflect(-V, normal), roughness*roughness);
            H = normalize(L + V);
        } else {
            H = BuildSampleHalfVectorGGX(s, sampleCount, normal, roughness*roughness);
            L = 2.f * dot(V, H) * H - V;
        }

        float NdotL = L.z;
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));

        if (NdotL > 0) {
                // using "precise" here has a massive effect...
                // Without it, there are clear errors in the result.
            precise float G = SmithG(NdotL, alphag) * G2;

            // F0 gets factored out of the equation here
            // the result we will generate is actually a scale and offset to
            // the runtime F0 value.
            precise float F = pow(1.f - VdotH, 5.f);

            // As per SampleSpecularIBL_Ref, we need to apply the inverse of the
            // propability density function to get a normalized result.
            // This factors out the D term, and introduces some other terms.
            //      pdf inverse = 4.f * VdotH / (D * NdotH)
            //      specular eq = D*G*F / (4*NdotL*NdotV)
            precise float normalizedSpecular = G * VdotH / (NdotH * NdotV);  // (excluding F term)

            A += (1.f - F) * normalizedSpecular;
            B += F * normalizedSpecular;
        }
    }

    return float2(A, B) / float(sampleCount).xx;
}

float4 main(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
        // Note that we could scale the range for roughness or NdotV here if
        // We find it gives us more accuracy...
        // Offset by a little bit to hit the middle of our range (assume 256x256 output)
    float NdotV = texCoord.x + (.5f / 256.f);
    float roughness = 1.f-(texCoord.y + (.5f / 256.f));

    return float4(GenerateSplitTerm(NdotV, roughness), 0, 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer SubResourceId
{
    uint ArrayIndex, MipIndex;
    uint PassIndex, PassCount;
}

Texture2D Input;

float3 SampleInputTexture(float3 direction)
{
    // return Input.SampleLevel(DefaultSampler, AdjSkyCubeMapCoords(L), 0).rgb;
    direction = float3(direction.y, direction.x, direction.z);
    float2 coord = EquirectangularMappingCoord(direction);
    coord.x = -coord.x; // hack to match IBL Baker
    return Input.SampleLevel(DefaultSampler, coord, 0).rgb;
}

// See DirectX documentation:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204881(v=vs.85).aspx
static const float3 CubeMapPanels_DX[6][3] =
{
        // +X, -X
    { float3(0,0,-1), float3(0,-1,0), float3(1,0,0) },
    { float3(0,0,1), float3(0,-1,0), float3(-1,0,0) },

        // +Y, -Y
    { float3(1,0,0), float3(0,0,1), float3(0,1,0) },
    { float3(1,0,0), float3(0,0,-1), float3(0,-1,0) },

        // +Z, -Z
    { float3(1,0,0), float3(0,-1,0), float3(0,0,1) },
    { float3(-1,0,0), float3(0,-1,0), float3(0,0,-1) },
};

float3 CalculateCubeMapDirection(uint panelIndex, float2 texCoord)
{
    float3 plusX  = CubeMapPanels_DX[panelIndex][0];
    float3 plusY  = CubeMapPanels_DX[panelIndex][1];
    float3 center = CubeMapPanels_DX[panelIndex][2];
    return normalize(
          center
        + plusX * (2.f * texCoord.x - 1.f)
        + plusY * (2.f * texCoord.y - 1.f));
}

float4 EquiRectFilterGlossySpecular(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    // This is the second term of the "split-term" solution for IBL glossy specular
    // Here, we prefilter the reflection texture in such a way that the blur matches
    // the GGX equation.
    //
    // This is very similar to calculating the full IBL reflections. However, we're
    // making some simplifications to make it practical to precalculate it.
    // We can choose to use an importance-sampling approach. This will limit the number
    // of samples to some fixed amount. Alternatively, we can try to sample the texture
    // it some regular way (ie, by sampling every texel instead of just the ones suggested
    // by importance sampling).
    //
    // If we sample every pixel we need to weight by the solid angle of the texel we're
    // reading from. But if we're just using the importance sampling approach, we can skip
    // this step (it's just taken care of by the probability density function weighting)

    float3 cubeMapDirection = CalculateCubeMapDirection(ArrayIndex, texCoord);

        // undoing AdjSkyCubeMapCoords
    cubeMapDirection = float3(cubeMapDirection.x, -cubeMapDirection.z, cubeMapDirection.y);

        // have to add another flip to get the same result as the rest of the pipeline
        // it feels like there's ome confusion coming in from going through CubeMapGen (which
        // natively uses an OpenGL cubemap format). Plus all these other conversion and adjustments!!
    cubeMapDirection = float3(-cubeMapDirection.x, -cubeMapDirection.y, cubeMapDirection.z);

    // return float4(cubeMapDirection, 1.f);
    // return float4(SampleInputTexture(cubeMapDirection), 1);

    // Here is the key simplification -- we assume that the normal and view direction are
    // the same. This means that some distortion on extreme angles is lost. This might be
    // incorrect, but it will probably only be noticeable when compared to a ray traced result.
    float3 normal = cubeMapDirection;
    float3 viewDirection = cubeMapDirection;

    // We can adjust the mapping between roughness and the mipmaps as needed...
    // Each successive mipmap is smaller, so we loose resolution linearly against
    // roughness (even though the blurring amount is not actually linear against roughness)
    // We could use the inverse of the GGX function to calculate something that is
    // more linear against the sample cone size, perhaps...?
    // Does it make sense to offset by .5 to get a value in the middle of the range? We
    // will be using trilinear filtering to get a value between 2 mipmaps.
    const float specularIBLMipMapCount = 9.f;
    const float offset = 0.0f; // 0.5f
    float roughness = saturate((MipIndex + offset) / specularIBLMipMapCount);
    roughness = max(roughness, 0.025f);
    SpecularParameters specParam = SpecularParameters_RoughF0(roughness, 1.0.xxx);

    float alphag = RoughnessToGAlpha(specParam.roughness);
    float alphad = RoughnessToDAlpha(specParam.roughness);

    float3 result = 0.0.xxx;
        // We need a huge number of samples for a smooth result
        // Perhaps we should use double precision math?
        // Anyway, we need to split it up into multiple passes, otherwise
        // the GPU gets locked up in the single draw call for too long.
    const uint passSampleCount = 1024;
    const uint totalSampleCount = passSampleCount * PassCount;
    [loop] for (uint s=0; s<passSampleCount; ++s) {
            // We could build a distribution of "H" vectors here,
            // or "L" vectors. It makes sense to use H vectors
        precise float3 H = BuildSampleHalfVectorGGX(s+PassIndex*passSampleCount, totalSampleCount, normal, alphad);
        precise float3 L = 2.f * dot(viewDirection, H) * H - viewDirection;

            // Now we can light as if the point on the reflection map
            // is a directonal light.
            // Note that we could estimate the average distance between
            // samples in this step, and compare that to the input texture
            // size. In this way, we could select a mipmap from the input
            // texture. This would help avoid sampling artifacts, and we would
            // end up with slightly more blurry result.

        float3 lightColor = SampleInputTexture(L);

            // Unreal course notes say the probability distribution function is
            //      D * NdotH / (4 * VdotH)
            // We need to apply the inverse of this to weight the sample correctly.
            // For the most part, it just factors out of the equation...
        float NdotH = saturate(dot(normal, H));
        float VdotH = saturate(dot(viewDirection, H));

        // precise float3 brdf = CalculateSpecular(normal, viewDirection, L, H, specParam); // (also contains NdotL term)
        // precise float D = TrowReitzD(NdotH, specParam.roughness * specParam.roughness);
        // float pdfWeight = (4.f * VdotH) / (D * NdotH);
        // result += lightColor * brdf * pdfWeight;

        float NdotV = 1.f; // saturate(dot(normal, viewDirection));
        float NdotL = saturate(dot(normal, L));
        precise float G = SmithG(NdotL, alphag); // * SmithG(NdotV, alphag);
        float scale = G * VdotH / (NdotH * NdotV);

            // We're getting nan values here sometimes...?
            // This occurs in the high roughness values when we have a very large
            // number of samples. This might be a result of the bit hacks we're
            // doing in the hammersly calculation?
        if (!isnan(scale))
            result += lightColor * scale;
    }

    // Might be more accuracy to divide by "passSampleCount" here, and then later on divide
    // by PassCount...?
    return float4(result / float(totalSampleCount), 1.f);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
