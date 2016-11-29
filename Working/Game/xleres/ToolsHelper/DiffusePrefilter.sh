// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/MathConstants.h"

Texture2D<float3> Input;
SamplerState DefaultSampler;

float3 SphericalToCartesian(float3 spherical)
{
    float s0, c0, s1, c1;
    sincos(spherical.x, s0, c0);
    sincos(spherical.y, s1, c1);
    return float3(
        spherical.z * s0 * c1,
        spherical.z * s0 * s1,
        spherical.z * c0);
}

float3 EquirectangularCoordToDirection(uint2 input, uint2 dims)
{
    // Given the x, y pixel coord within an equirectangular texture, what
    // is the corresponding direction vector?
    float phi = 2.0f * pi * (input.x / float(dims.x) - 0.5f);
    float theta = pi * input.y / float(dims.y);
    return SphericalToCartesian(float3(theta, phi, 1.0f));
}

float2 EquirectangularMappingCoord(float3 direction)
{
		// note -- 	the trigonometry here is a little inaccurate. It causes shaking
		//			when the camera moves. We might need to replace it with more
		//			accurate math.
	float theta = atan2(direction.y, direction.x);
	float inc = asin(direction.z); // atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = .5f-(inc / pi);
	return float2(x, y);
}

float4 ReferenceDiffuseFilter(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    uint2 dims = uint2(position.xy / texCoord);
    float3 direction = EquirectangularCoordToDirection(uint2(position.xy), dims);

    float3 result = float3(0,0,0);

    uint2 inputDims;
    Input.GetDimensions(inputDims.x, inputDims.y);
    for (uint y=0; y<inputDims.y; ++y) {
        for (uint x=0; x<inputDims.x; ++x) {
            float3 sampleDirection = EquirectangularCoordToDirection(uint2(x, y), inputDims);
            float cosFilter = max(0.0, dot(sampleDirection, direction));

            float texelAreaWeight = (2.0f * pi / inputDims.x) * (pi / inputDims.y);
            texelAreaWeight *= sin(pi * y / float(inputDims.y));
            result += texelAreaWeight * cosFilter * Input.Load(uint3(x, y, 0));
        }
    }

    return float4(result, 1.0f);

    // float2 back = EquirectangularMappingCoord(direction);
    // float2 diff = back - float2(position.xy) / float2(dims);
    // return float4(abs(diff.xy), 0, 1);
}


////////////////////////////////////////////////////////////////////////////////

float Sq(float v) { return v*v; }

// Take an input equirectangular input texture and generate the spherical
// harmonic coefficients that best represent it.
float4 ProjectToSphericalHarmonic(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    // We're only going to support the first 3 orders; which means we generate 9 coefficients
    // l= 0, m= 0 -- .5/sqrt(1/pi)
    // l= 1, m=-1 -- -sqrt(3/(4pi)) * y
    // l= 1, m= 0 --  sqrt(3/(4pi)) * z
    // l= 1, m= 1 -- -sqrt(3/(4pi)) * x
    // l= 2, m=-2 --  0.5  * sqrt(15/pi) * x * y
    // l= 2, m=-1 -- -0.5  * sqrt(15/pi) * y * z
    // l= 2, m= 0 --  0.25 * sqrt( 5/pi) * (-x^2-y^2+2z^2)
    // l= 2, m= 1 -- -0.5  * sqrt(15/pi) * x * z
    // l= 2, m= 2 --  0.25 * sqrt(15/pi) * (x^2 - y^2)

    uint index = uint(position.x)%9;

    float3 result = float3(0, 0, 0);
    uint2 inputDims;
    Input.GetDimensions(inputDims.x, inputDims.y);
    for (uint y=0; y<inputDims.y; ++y) {
        for (uint x=0; x<inputDims.x; ++x) {
            float3 sampleDirection = EquirectangularCoordToDirection(uint2(x, y), inputDims);
            float texelAreaWeight = (2.0f * pi / inputDims.x) * (pi / inputDims.y);
            texelAreaWeight *= sin(pi * y / float(inputDims.y));

            float value;
            switch (index) {
            case 0: value = .5/sqrt(1/pi); break;
            case 1: value = -sqrt(3/(4*pi)) * sampleDirection.y; break;
            case 2: value =  sqrt(3/(4*pi)) * sampleDirection.z; break;
            case 3: value = -sqrt(3/(4*pi)) * sampleDirection.x; break;
            case 4: value =  0.5  * sqrt(15/pi) * sampleDirection.x * sampleDirection.y; break;
            case 5: value = -0.5  * sqrt(15/pi) * sampleDirection.y * sampleDirection.z; break;
            case 6: value =  0.25 * sqrt( 5/pi) * (-Sq(sampleDirection.x) - Sq(sampleDirection.y) + 2*Sq(sampleDirection.z)); break;
            case 7: value = -0.5  * sqrt(15/pi) * sampleDirection.x * sampleDirection.z; break;
            case 8: value =  0.25 * sqrt(15/pi) * (Sq(sampleDirection.x) - Sq(sampleDirection.y)); break;
            }

            result += texelAreaWeight * value * Input.Load(uint3(x, y, 0));
        }
    }

    return float4(result, 1.0f);
}

float4 ResolveSphericalHarmonic(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float3 L00  = Input.Load(uint3(0,0,0)).rgb;
    float3 L1n1 = Input.Load(uint3(1,0,0)).rgb;
    float3 L10  = Input.Load(uint3(2,0,0)).rgb;
    float3 L1p1 = Input.Load(uint3(3,0,0)).rgb;
    float3 L2n2 = Input.Load(uint3(4,0,0)).rgb;
    float3 L2n1 = Input.Load(uint3(5,0,0)).rgb;
    float3 L20  = Input.Load(uint3(6,0,0)).rgb;
    float3 L2p1 = Input.Load(uint3(7,0,0)).rgb;
    float3 L2p2 = Input.Load(uint3(8,0,0)).rgb;

    float c1 = 0.429043, c2 = 0.511664,
        c3 = 0.743125, c4 = 0.886227, c5 = 0.247708;

    uint2 dims = uint2(position.xy / texCoord);
    float3 D = EquirectangularCoordToDirection(uint2(position.xy), dims);
    float3 result =
          L00
        + L1n1 * D.y
        + L10  * D.z
        + L1p1 * D.x
        + L2n2 * D.x * D.y
        + L2n1 * D.y * D.z
        + L20  * (D.z*D.z*D.z - 1)
        + L2p1 * (D.x * D.z)
        + L2p2 * (D.x*D.x - D.y*D.y)
        ;
    return float4(result/5.0f, 1.0f);
}
