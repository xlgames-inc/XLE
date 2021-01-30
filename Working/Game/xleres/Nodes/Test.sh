
float LightTemplate(float3 normal, float3 lightDirection);

float BasicLambert(float3 normal, float3 lightDirection)
{
    return max(0, dot(normal, -lightDirection));
}

/*

import basic = "xleres/Nodes/Basic.sh"
import test = "xleres/Nodes/Test.sh"

float3 DefaultLight(float3 diffuse, float3 normal, float3 lightDirection, graph<test::LightTemplate> lighter)
{
	if "DoLight"
		return [[visualNode1]]basic::Multiply3Scalar(rhs:[[visualNode0]]lighter(normal:normal, lightDirection:lightDirection).result, lhs:diffuse).result;
}
float3 main(float3 normal, float3 lightDirection)
{
	return [[visualNode2]]DefaultLight(normal:normal, lightDirection:lightDirection, lighter:test::BasicLambert()).result;
}

*/
