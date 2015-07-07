// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(EDGE_DETECTION_H)
#define EDGE_DETECTION_H

static const float SharrConstant = 1.f/60.f;

static const float SharrHoriz5x5[5][5] =
{
    { -1.f * SharrConstant, -1.f * SharrConstant,  0.f,  1.f * SharrConstant,  1.f * SharrConstant },
    { -2.f * SharrConstant, -2.f * SharrConstant,  0.f,  2.f * SharrConstant,  2.f * SharrConstant },
    { -3.f * SharrConstant, -6.f * SharrConstant,  0.f,  6.f * SharrConstant,  3.f * SharrConstant },
    { -2.f * SharrConstant, -2.f * SharrConstant,  0.f,  2.f * SharrConstant,  2.f * SharrConstant },
    { -1.f * SharrConstant, -1.f * SharrConstant,  0.f,  1.f * SharrConstant,  1.f * SharrConstant }
};

static const float SharrVert5x5[5][5] =
{
    { -1.f * SharrConstant, -2.f * SharrConstant, -3.f * SharrConstant, -2.f * SharrConstant, -1.f * SharrConstant },
    { -1.f * SharrConstant, -2.f * SharrConstant, -6.f * SharrConstant, -2.f * SharrConstant, -1.f * SharrConstant },
    {  0.f * SharrConstant,  0.f * SharrConstant,  0.f * SharrConstant,  0.f * SharrConstant,  0.f * SharrConstant },
    {  1.f * SharrConstant,  2.f * SharrConstant,  6.f * SharrConstant,  2.f * SharrConstant,  1.f * SharrConstant },
    {  1.f * SharrConstant,  2.f * SharrConstant,  3.f * SharrConstant,  2.f * SharrConstant,  1.f * SharrConstant }
};

static const float SharrConstant3x3 = 1.f/32.f;

static const float SharrHoriz3x3[3][3] =
{
    {  -3.f * SharrConstant3x3, 0.f,  3.f * SharrConstant3x3 },
    { -10.f * SharrConstant3x3, 0.f, 10.f * SharrConstant3x3 },
    {  -3.f * SharrConstant3x3, 0.f,  3.f * SharrConstant3x3 },
};

static const float SharrVert3x3[3][3] =
{
    {  -3.f * SharrConstant3x3, -10.f * SharrConstant3x3,  -3.f * SharrConstant3x3 },
    { 0.f, 0.f, 0.f },
    {   3.f * SharrConstant3x3,  10.f * SharrConstant3x3,   3.f * SharrConstant3x3 },
};


#endif
