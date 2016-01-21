// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)



struct Rectangle
{
	float2	topLeft;
	float2	bottomRight;
};

struct VSOutput
{
	float2		topLeft		: TOPLEFT;
	float2		bottomRight : BOTTOMRIGHT;
	float		value		: VALUE;
};

struct GSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
	float4 color : COLOR0;
};

static const float2 CharDim = float2(36.f, 64.f);

const uint2 ScreenDimensions;

float4 ToNDCCoords(float2 pixelCoordsPosition)
{
	return float4(float2(-1,1) + float2(2,-2) * pixelCoordsPosition.xy / float2(ScreenDimensions.xy), 0.f, 1.f);
}

	// not clear why we're getting this warning...
#pragma warning(disable:4715) // emitting a system-interpreted value which may not be written in every execution path of the shader

void OutputCharacter(	int character, inout Rectangle rect,
						inout TriangleStream<GSOutput> outputStream)
{
	float2 charPixelSize = CharDim.xy / 2.f;
	float4 p[4]; float2 t[4];
	p[0] = ToNDCCoords(rect.topLeft.xy);
	p[1] = ToNDCCoords(rect.topLeft.xy + float2(0, charPixelSize.y));
	p[2] = ToNDCCoords(rect.topLeft.xy + float2(charPixelSize.x, 0));
	p[3] = ToNDCCoords(rect.topLeft.xy + float2(charPixelSize.x, charPixelSize.y));

	const float characterCoordWidth = CharDim.x / 512.f;
	t[0] = float2(character*characterCoordWidth, 0.f);
	t[1] = float2(character*characterCoordWidth, 1.f);
	t[2] = float2((character+1)*characterCoordWidth, 0.f);
	t[3] = float2((character+1)*characterCoordWidth, 1.f);

	for (uint c=0; c<4; ++c) {
		GSOutput vert;
		vert.position = p[c];
		vert.texCoord = t[c];
		vert.color = 1.0.xxxx;
		outputStream.Append(vert);
	}
	outputStream.RestartStrip();

	rect.topLeft.x += charPixelSize.x;
}

uint GetDigit(float value, int digitPlace)
{
	value *= pow(10.f, -digitPlace);
	return uint(fmod(value, 10.f));
}

bool RequiresComma(int digitPlace)
{
	return (((uint)(digitPlace+27))%3)==0 && (digitPlace!=0);
}

[maxvertexcount(80)]
	void main(point VSOutput input[1], inout TriangleStream<GSOutput> outputStream)
{
	Rectangle rect;
	rect.topLeft = input[0].topLeft;
	rect.bottomRight = input[0].bottomRight;
	float value = input[0].value;

		//	write '-' for negative numbers
	if (value<0) {
		OutputCharacter(12, rect, outputStream);
		value = -value;
	}

	float absValue = value;

	bool addK = false;
	if (absValue > 32*1024) {
		absValue = floor(absValue/1024.f);
		addK = true;
	}

	bool wroteFirstDigit = false;
	for (int digitPlace = 6; digitPlace>=0; --digitPlace) {
		int digit = GetDigit(absValue, digitPlace);
		if (!digit && !wroteFirstDigit && digitPlace != 0) {
			continue;
		}

		OutputCharacter(digit, rect, outputStream);
		if (RequiresComma(digitPlace)) {
			OutputCharacter(11, rect, outputStream);
		}
		wroteFirstDigit = true;
	}

	if (frac(absValue)>=0.001f) {
		OutputCharacter(10, rect, outputStream);

		for (int digitPlace = 1; digitPlace<3; ++digitPlace) {
			int digit = GetDigit(absValue, -digitPlace);
			OutputCharacter(digit, rect, outputStream);
		}
	}

	if (addK) {
		OutputCharacter(13, rect, outputStream);
	}
}
