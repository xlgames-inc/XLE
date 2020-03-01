// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Ocean.hlsl"
#include "OceanShallow.hlsl"

#define WRITING_VELOCITIES
#include "ShallowFlux.hlsl"

[numthreads(SHALLOW_WATER_TILE_DIMENSION, 1, 1)]
    void		InitPipeModel2(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Velocities0	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities1	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities2	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities3	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities4	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities5	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities6	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
    Velocities7	[uint3(dispatchThreadId.xy, ArrayIndex)] = 0.f;		// (always begin with zero velocity)
}
