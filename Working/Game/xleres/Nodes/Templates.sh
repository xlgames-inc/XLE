
#include "../MainGeometry.h"
#include "../gbuffer.h"

float3 CoordinatesToColor(float3 coords);
float4 AmendColor(VSOutput geo, float4 inputColor);

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

