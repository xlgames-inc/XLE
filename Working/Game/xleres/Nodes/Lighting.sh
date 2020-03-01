
#include "../TechniqueLibrary/SceneEngine/Lighting/LightingAlgorithm.hlsl"
#include "../TechniqueLibrary/Framework/Transform.hlsl"
#include "../TechniqueLibrary/Framework/Surface.hlsl"

float SkyReflectionFresnelFactor(VSOutput geo)
{
    float3 worldSpaceNormal = GetNormal(geo);
    float3 viewVector = WorldSpaceView - GetWorldPosition(geo);
    float viewVectorMag = rsqrt(dot(viewVector, viewVector));
    return SchlickFresnelCore(dot(worldSpaceNormal, viewVector) * viewVectorMag);
}
