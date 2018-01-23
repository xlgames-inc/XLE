
float4 MakeParam() { return float4(1,0,1,0); }
float4 MakeParam1(float4 o, float4 t) { return t; }

float4 Prototype_SomeSignal(float4 position, float2 texCoord, float3 normal);

float4 SomeFunction(float4 position, float2 texCoord, float3 normal)
{
    return position;
}

float2 Prototype_TexCoordFn(float4 i);

float2 TexCoordFn(float4 i) { return position.xy; } 

