
#if !defined(PASS_STANDARD_SH)
#define PASS_STANDARD_SH

#include "../../MainGeometry.h"
#include "../../gbuffer.h"

bool EarlyRejectionTest(VSOutput geo);
GBufferValues PerPixel(VSOutput geo);
    
void PerPixel_Separate(
    VSOutput geo, 
    out float3 diffuseAlbedo,
    out float3 worldSpaceNormal,

    out CommonMaterialParam material,

    out float blendingAlpha,
    out float normalMapAccuracy,
    out float cookedAmbientOcclusion,
    out float cookedLightOcclusion,

    out float3 transmission);

bool EarlyRejectionTest_Default(VSOutput geo)
{
    return DoAlphaTest(geo, GetAlphaThreshold());
}

#endif
