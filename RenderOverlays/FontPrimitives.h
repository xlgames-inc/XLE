// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"

struct  FT_FaceRec_;
typedef struct FT_FaceRec_* FT_Face;

struct  FT_GlyphSlotRec_;
typedef struct FT_GlyphSlotRec_* FT_GlyphSlot;

namespace RenderOverlays
{

enum FontTexKind 
{
    FTK_GENERAL = 0,
    FTK_DAMAGEDISPLAY,
    FTK_IMAGETEXT,
    FTK_MAX
};

typedef unsigned        FontCharID;
static const FontCharID FontCharID_Invalid = ~FontCharID(0x0);

class Color4 
{
public:
    float r, g, b, a;

    static Color4 Create(float r, float g, float b, float a);

    operator const float* () const { return (float*)this; }
    float operator[] (int i) const { return ((float*)this)[i]; }

    Color4 operator - () const;
    Color4& operator += (const Color4& c);
    Color4& operator -= (const Color4& c);
    Color4& operator *= (const Color4& c);
    bool operator == (const Color4& c) const;
    bool operator != (const Color4& c) const;

    Color4 operator + (const Color4& c) const;
    Color4 operator - (const Color4& c) const;
    Color4 operator * (const Color4& c) const;
    friend Color4 operator * (float s, const Color4& c);
    bool operator < (const Color4& c) const;

    void ExtractHSV(float& h, float& s, float& v) const;
    static Color4 HSV(float h, float s, float v, float a);
};

class Quad 
{
public:
    Float2 min, max;

    Quad() : min(0.f, 0.f), max(0.f, 0.f) {}
    static Quad Empty();
    static Quad MinMax(float minX, float minY, float maxX, float maxY);
    static Quad MinMax(const Float2& min, const Float2& max);
    static Quad CenterExtent(const Float2& center, const Float2& extent);

    bool operator == (const Quad& v) const;
    bool operator != (const Quad& v) const;

    Float2 Center() const;
    Float2 Extent() const;
    float Length() const { return max[0] - min[0]; }
    float Height() const { return max[1] - min[1]; }
};


}
