// CompoundDocument:1

float2 Graph_TexCoordFn(float4 i);

float2 TexCoordFn(float4 i) { return position.xy; } 

// float4 Signal_SomeSignal(float4 position, float2 texCoord, float3 normal);

float4 s0(float4 position, float2 texCoord, float3 normal);

/* <<Chunk:GraphSyntax:SlotPrototype>>--(

import SP = "xleres/System/SlotPrototype2.sh";

auto s0(float4 position, float2 texCoord, float3 normal, graph<Graph_TexCoordFn> tcGenerator)
{
	node n0 = SP::MakeParam1(o : SP::MakeParam().result, t : "<two>");
	node t0 = tcGenerator(i: position);
	return SP::SomeFunction(
		position : position, 
		texCoord : t0.result, 
		normal : n0.result
		).result;
}

auto Signal_main(float4 position, float2 texCoord, float3 normal) implements SP::Signal_SomeSignal
{
	return s0(position:position, texCoord:texCoord, normal:normal, txGenerator:TexCoordFn).result;
	
	// node n = [position:position, texCoord:texCoord, normal:normal, txGenerator:TexCoordFn] => s0;
	// return n.result;
}

)-- */
