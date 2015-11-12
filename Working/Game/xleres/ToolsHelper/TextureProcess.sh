// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

Texture2D	Input : register(t0);

cbuffer Material
{
    float4 Scale;
}

float4 copy(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    return Scale * Input.Load(int3(position.xy, 0));
}
