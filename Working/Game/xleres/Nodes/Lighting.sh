
#include "../Lighting/LightingAlgorithm.h"
#include "../Transform.h"
#include "../Surface.h"

float SkyReflectionFresnelFactor(VSOutput geo)
{
    float3 worldSpaceNormal = GetNormal(geo);
    float3 viewVector = WorldSpaceView - GetWorldPosition(geo);
    float viewVectorMag = rsqrt(dot(viewVector, viewVector));
    return SchlickFresnelCore(dot(worldSpaceNormal, viewVector) * viewVectorMag);
}
