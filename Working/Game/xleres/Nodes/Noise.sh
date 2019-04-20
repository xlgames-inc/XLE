
#include "../Utility/perlinnoise.h"

float3 FBMNormalMap(float2 position, float hgrid, float gain, float lacunarity, int octaves, float slopeScale, float tightness)
{
    // note -- this should be a pretty silly way of calculating a the derivatives of FBM noise. It would
    //      probably make more sense to just use the algebratic derivatives of the noise equation. But
    //      I just want to generate some simple input data --

	const float weight = 1.f/68.f;
	float filter[5][5] = {
		{ -2.f, -1.f,  0.f,  1.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -4.f,  0.f,  4.f,  2.f },
		{ -2.f, -1.f,  0.f,  1.f,  2.f }
	};

	float filteredX = 0.0, filteredY = 0.0;
	for (int y=0; y<5; ++y) {
		for (int x=0; x<5; ++x) {

            float sample = fbmNoise2D(
                position + tightness * float2(x - 2, y - 2),
                hgrid, gain, lacunarity, octaves);

			filteredX += filter[y][x] * sample;
			filteredY += filter[x][y] * sample;
		}
	}

    filteredX *= weight * slopeScale;
    filteredY *= weight * slopeScale;

    // primitive math --
    float3 tangentX = float3(tightness, 0.0, filteredX);
    float3 tangentY = float3(0.0, tightness, filteredY);
    float3 finalNormal = normalize(cross(tangentX, tangentY));
    return 0.5.xxx + 0.5 * finalNormal;
}
