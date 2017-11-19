
float4 MakeParam() { return float4(1,0,1,0); }
float4 MakeParam1(float4 o, float4 t) { return t; }

float4 Signal_SomeSignal(float4 position, float2 texCoord, float3 normal);

float4 SomeFunction(float4 position, float2 texCoord, float3 normal)
{
    return position;
}

/*

slot s0 implements <SlotPrototype2.sh:Signal_SomeSignal>;
node n0 = <SlotPrototype2.sh:MakeParam1>(o : <SlotPrototype2.sh:MakeParam>().result, t : "<two>");
s0.result : <SlotPrototype2.sh:SomeFunction>(
	position : s0.position, texCoord : n0.result, normal : s0.normal
	).result;

export s0;

*/
