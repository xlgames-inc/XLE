// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define DUPLEX_VEL

#if !defined(WRITING_VELOCITIES) /////////////////////////////

    Texture2DArray<float>	Velocities0	 : register(t5);
    Texture2DArray<float>	Velocities1	 : register(t6);
    Texture2DArray<float>	Velocities2	 : register(t7);
    Texture2DArray<float>	Velocities3	 : register(t8);

    #if defined(DUPLEX_VEL)
        Texture2DArray<float>	Velocities4	 : register(t9);
        Texture2DArray<float>	Velocities5	 : register(t10);
        Texture2DArray<float>	Velocities6	 : register(t11);
        Texture2DArray<float>	Velocities7  : register(t12);
    #endif

#else /////////////////////////////////////////////////////

    RWTexture2DArray<float>	Velocities0	 : register(u0);
    RWTexture2DArray<float>	Velocities1	 : register(u1);
    RWTexture2DArray<float>	Velocities2	 : register(u2);
    RWTexture2DArray<float>	Velocities3	 : register(u3);

    #if defined(DUPLEX_VEL)
        RWTexture2DArray<float>	Velocities4	 : register(u4);
        RWTexture2DArray<float>	Velocities5	 : register(u5);
        RWTexture2DArray<float>	Velocities6	 : register(u6);
        RWTexture2DArray<float>	Velocities7  : register(u7);
    #endif

#endif ////////////////////////////////////////////////////

#if defined(DUPLEX_VEL)
    //
    //	Cells:
    //		0     1     2
    //		3   center  4
    //		5     6     7
    //
    static const uint AdjCellCount = 8;

    static const int2 AdjCellDir[] =
    {
        int2(-1, -1), int2( 0, -1), int2(+1, -1),
        int2(-1,  0), 				int2(+1,  0),
        int2(-1, +1), int2( 0, +1), int2(+1, +1)
    };

    static const uint AdjCellComplement[] =
    {
        7, 6, 5,
        4,    3,
        2, 1, 0
    };
#endif

void LoadVelocities(out float velocities[AdjCellCount], uint3 coord)
{
    velocities[0] = Velocities0[coord];
    velocities[1] = Velocities1[coord];
    velocities[2] = Velocities2[coord];
    velocities[3] = Velocities3[coord];

    #if defined(DUPLEX_VEL)
        velocities[4] = Velocities4[coord];
        velocities[5] = Velocities5[coord];
        velocities[6] = Velocities6[coord];
        velocities[7] = Velocities7[coord];
    #endif
}
