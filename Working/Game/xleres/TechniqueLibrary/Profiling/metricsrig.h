// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

interface IGetValue
{
    uint GetValue();
};

#if defined(VALUE_SOURCE_COUNT)
    IGetValue ValueSource[VALUE_SOURCE_COUNT];
#endif

struct VSOUT
{
    float2	topLeft : TOPLEFT;
    float2	bottomRight : BOTTOMRIGHT;
    float	value : VALUE;
};

const uint2 ScreenDimensions;

uint TileCount()
{
    return ((ScreenDimensions.x + 15) / 16) * ((ScreenDimensions.y + 15) / 16);
}

VSOUT metricsrig_main(uint vertexId : SV_VertexId)
{

        //		Get the value we want to write from the metrics
        //		buffer, and

    const float lineHeight = 64.f/2.f;
    VSOUT output;
    output.topLeft = float2(64.f, lineHeight*float(4+vertexId));
    output.bottomRight = float2(256.f+64.f, lineHeight*float(5+vertexId));

    #if defined(VALUE_SOURCE_COUNT)
        if (vertexId < VALUE_SOURCE_COUNT) {
            if (vertexId == 0) {
                output.value = ValueSource[0].GetValue();
            } else if (vertexId == 1) {
                output.value = ValueSource[1].GetValue();
            } else if (vertexId == 2) {
                output.value = ValueSource[2].GetValue();
            } else if (vertexId == 3) {
                output.value = ValueSource[3].GetValue();
            } else if (vertexId == 4) {
                output.value = ValueSource[4].GetValue();
            } else if (vertexId == 5) {
                output.value = ValueSource[5].GetValue();
            }
        } else
    #endif
    {
        output.value = 0.f;
    }

    return output;
}
