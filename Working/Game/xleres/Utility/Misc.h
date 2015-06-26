// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(UTILITY_MISC_H)
#define UTILITY_MISC_H

float DitherPatternValue(uint2 pixelCoords)
{
	int ditherArray[16] = 
	{
		 4, 12,  0,  8,
		10,  2, 14,  6,
		15,  7, 11,  3,
		 1,  9,  5, 13
	};
	uint2 t = pixelCoords.xy & 0x3;
	return float(ditherArray[t.x+t.y*4]) / 16.f;
}

#endif
