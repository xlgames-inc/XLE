// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CommonShapes.h"
#include "CommonBrushes.h"

IShape2D Shape;
IBrush Fill;
IBrush Outline;

float4 Paint(
    float4 position	    : SV_Position,
    float4 color		: COLOR0,
    float2 texCoord0	: TEXCOORD0,
    float2 texCoord1	: TEXCOORD1,
    nointerpolation float2 outputDimensions : OUTPUTDIMENSIONS) : SV_Target0
{
    DebuggingShapesCoords coords =
        DebuggingShapesCoords_Make(position, texCoord0, outputDimensions);

    ShapeDesc shapeDesc = MakeShapeDesc(0.0.xx, 1.0.xx, texCoord1.x, texCoord1.y);
    ShapeResult shape = Shape.Calculate(coords, shapeDesc);

    float4 fill = Fill.Calculate(coords, color); fill.a *= shape._fill;
    float4 outline = Outline.Calculate(coords, color); outline.a *= shape._border;

    float3 A = fill.rgb * fill.a;
    float a = 1.f - fill.a;
    A = A * (1.f - outline.a) + outline.rgb * outline.a;
    a = a * (1.f - outline.a);
    return float4(A, 1.f - a);
}
