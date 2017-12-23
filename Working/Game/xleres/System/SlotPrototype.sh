import SP = "xleres/System/SlotPrototype2.sh"

auto s0(float4 position, float2 texCoord, float3 normal, graph<SP::Prototype_TexCoordFn> tcGenerator)
{
	node n0 = SP::MakeParam1(o : SP::MakeParam().result, t : "<two>");
	node t0 = tcGenerator(i: position);
	return SP::SomeFunction(
		position : position, 
		texCoord : t0.result, 
		normal : n0.result
		).result;
}

auto Signal_main(float4 position, float2 texCoord, float3 normal) implements SP::Prototype_SomeSignal
{
	return s0(position:position, texCoord:texCoord, normal:normal, tcGenerator:SP::TexCoordFn).result;
	
	// node n = [position:position, texCoord:texCoord, normal:normal, tcGenerator:TexCoordFn] => s0;
	// return n.result;
}
