// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COLOUR_H)
#define COLOUR_H

float3 SRGBToLinear_Fast(float3 input)		{ return input*input; }
float3 LinearToSRGB_Fast(float3 input)		{ return sqrt(input); }

float3 LinearToSRGB(float3 input)		    { return pow(max(0.0.xxx, input), 1.f/2.2f); }
float3 SRGBToLinear(float3 input)		    { return pow(max(0.0.xxx, input), 2.2f); }

static const float LightingScale = 1.f;     // note -- LightingScale is currently not working with high res screenshots (it is applied twice, so only 1 is safe)

float4 ByteColor(uint r, uint g, uint b, uint a) { return float4(r/float(0xff), g/float(0xff), b/float(0xff), a/float(0xff)); }

float SRGBLuminance(float3 rgb)
{
    const float3 constants = float3(0.2126f, 0.7152f, 0.0722f);
    return dot(constants, rgb);
}

float3 HSV2RGB( float3 hsv )
{
	float Hprime = hsv.x / 60.;
	float C = hsv.y * hsv.z;
	float X = C * (1 - abs( fmod(Hprime,2.)-1. ));
	float3 rgb1;
	if (Hprime < 1.)		rgb1 = float3( C, X, 0 );
	else if (Hprime < 2.)	rgb1 = float3( X, C, 0 );
	else if (Hprime < 3.)	rgb1 = float3( 0, C, X );
	else if (Hprime < 4.)	rgb1 = float3( 0, X, C );
	else if (Hprime < 5.)	rgb1 = float3( X, 0, C );
	else					rgb1 = float3( C, 0, X );
	float m = hsv.z - C;
	return float3( rgb1.x + m, rgb1.y + m, rgb1.z + m );
}

float3 HSL2RGB( float3 hsl )
{
	float C;
	if (hsl.z <= .5)	C = 2 * hsl.z * hsl.y;
	else				C = (2-2*hsl.z)*hsl.y;
	float Hprime = hsl.x / 60.;
	float X = C * (1 - abs( fmod(Hprime,2.)-1. ));
	float3 rgb1;
	if (Hprime < 1.)		rgb1 = float3( C, X, 0 );
	else if (Hprime < 2.)	rgb1 = float3( X, C, 0 );
	else if (Hprime < 3.)	rgb1 = float3( 0, C, X );
	else if (Hprime < 4.)	rgb1 = float3( 0, X, C );
	else if (Hprime < 5.)	rgb1 = float3( X, 0, C );
	else					rgb1 = float3( C, 0, X );
	float m = hsl.z - .5 * C;
	return float3( rgb1.x + m, rgb1.y + m, rgb1.z + m );
}

float3 RGB2HSV( float3 rgb )
{
	float M = max( rgb.r, max( rgb.g, rgb.b ) );
	float m = min( rgb.r, min( rgb.g, rgb.b ) );
	float C = M - m;
	float Hprime;
	if (C == 0) 			Hprime = 0;
	else if (M==rgb.r)		Hprime = fmod( (rgb.g-rgb.b)/C, 6 );
	else if (M==rgb.g)		Hprime = (rgb.b-rgb.r)/C + 2;
	else					Hprime = (rgb.r-rgb.g)/C + 4;
	return float3( Hprime * 60, C/M, M );
}

float3 RGB2HSL( float3 rgb )
{
	float M = max( rgb.r, max( rgb.g, rgb.b ) );
	float m = min( rgb.r, min( rgb.g, rgb.b ) );
	float C = M - m;
	float Hprime;
	if (C == 0) 			Hprime = 0;
	else if (M==rgb.r)		Hprime = fmod( (rgb.g-rgb.b)/C, 6 );
	else if (M==rgb.g)		Hprime = (rgb.b-rgb.r)/C + 2;
	else					Hprime = (rgb.r-rgb.g)/C + 4;
	float L = .5*(M+m);
	float S;
	if (L <= .5)		S = C/(2.*L);
	else				S = C/(2.-2.*L);
	return float3( Hprime * 60, S, L );
}

#endif
