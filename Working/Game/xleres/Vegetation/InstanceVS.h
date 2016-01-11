// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"
#include "../MainGeometry.h"

#if GEO_HAS_INSTANCE_ID==1
    struct InstanceDef
    {
        float4 posAndShadowing;
        float2 sinCosTheta;
    };

    StructuredBuffer<InstanceDef> InstanceOffsets : register(t15);

    float3 InstanceWorldPosition(VSInput input, out float3 worldNormal, out float3 objectCentreWorld)
    {
        float3 localPosition = VSIn_GetLocalPosition(input);

            // Ideally the object we're instantiating should have an
            // identity local-to-world. But if it doesn't we need to transform
            // the position here.
        localPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;

        objectCentreWorld = InstanceOffsets[input.instanceId].posAndShadowing.xyz;
        float2 sc = InstanceOffsets[input.instanceId].sinCosTheta;
        float2x2 rotMat = float2x2(float2(sc.y, -sc.x), float2(sc.x, sc.y));
        float scale = .75f + .25f * sc.x;       // add cheap scale component (directly related to rotation)

        float3 localNormal = VSIn_GetLocalNormal(input);
        worldNormal = float3(mul(rotMat, localNormal.xy), localNormal.z);

        return float3(
            objectCentreWorld.xy + scale * mul(rotMat, localPosition.xy),
            objectCentreWorld.z +  scale * localPosition.z);
    }

    float GetInstanceShadowing(VSInput input)
    {
        return InstanceOffsets[input.instanceId].posAndShadowing.w;
    }

#endif
