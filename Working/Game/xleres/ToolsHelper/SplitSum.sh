// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
        const bool clusterAroundHighlight = false;
        if (clusterAroundHighlight) {
            L = BuildSampleHalfVectorGGX(s, sampleCount, reflect(-V, normal), alphag);
            H = normalize(L + V);
        } else {
            H = BuildSampleHalfVectorGGX(s, sampleCount, normal, alphag);
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
