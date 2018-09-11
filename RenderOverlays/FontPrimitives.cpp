// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FontPrimitives.h"
#include "../Core/Types.h"
#include <assert.h>
#include <algorithm>

namespace RenderOverlays
{

inline float FloatBits(uint32 i) { return *(float*)&i; }
static const float FP_INFINITY = FloatBits(0x7F800000);
static const float FP_NEG_INFINITY = FloatBits(0xFF800000);

#if 0
static float Min(float x, float y, float z)
{
    if (x <= y && x <= z) {
        return x;
    }
    if (y <= x && y <= z) {
        return y;
    }
    return z;
}

static float Max(float x, float y, float z)
{
    if (x >= y && x >= z) {
        return x;
    }
    if (y >= x && y >= z) {
        return y;
    }
    return z;
}
#endif

// --------------------------------------------------------------------------
// Color4
// --------------------------------------------------------------------------

Color4 Color4::Create(float r, float g, float b, float a)
{
    Color4 c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

Color4 Color4::operator - () const
{
    return Color4::Create(-r, -g, -b, -a);
}

Color4& Color4::operator += (const Color4& c)
{
    r += c.r;
    g += c.g;
    b += c.b;
    a += c.a;
    return *this;
}

Color4& Color4::operator -= (const Color4& c)
{
    r -= c.r;
    g -= c.g;
    b -= c.b;
    a -= c.a;
    return *this;
}

Color4& Color4::operator *= (const Color4& c)
{
    r *= c.r;
    g *= c.g;
    b *= c.b;
    a *= c.a;
    return *this;
}

bool Color4::operator == (const Color4& c) const
{
    return (r == c.r && g == c.g && b == c.b && a == c.a);
}

bool Color4::operator != (const Color4& c) const
{
    return (r != c.r || g != c.g || b != c.b || a != c.a);
}

Color4 Color4::operator + (const Color4& c) const
{
    return Color4::Create(r + c.r, g + c.g, b + c.b, a + c.a);
}

Color4 Color4::operator - (const Color4& c) const
{
    return Color4::Create(r - c.r, g - c.g, b - c.b, a - c.a);
}

Color4 Color4::operator * (const Color4& c) const
{
    return Color4::Create(r * c.r, g * c.g, b * c.b, a * c.a);
}

Color4 operator * (float s, const Color4& c)
{
    return Color4::Create(s * c.r, s * c.g, s * c.b, s * c.a);
}

bool Color4::operator < (const Color4& c) const
{
    if (r < c.r) return true;
    if (r > c.r) return false;
    if (g < c.g) return true;
    if (g > c.g) return false;
    if (b < c.b) return true;
    if (b > c.b) return false;
    return a < c.a;
}

#if 0
void Color4::ExtractHSV(float& h, float& s, float& v) const
{
    float minValue = Min(r, g, b);
    
    v = Max(r, g, b);
    
    float delta = v - minValue;
  
    if (delta == 0.0f) {
        s = 0.0f;
        h = 0.0f;
    } else {
        s = delta / v;

        float del_R = (((v - r) / 6.0f) + (delta / 2.0f)) / delta;
        float del_G = (((v - g) / 6.0f) + (delta / 2.0f)) / delta;
        float del_B = (((v - b) / 6.0f) + (delta / 2.0f)) / delta;

        if (r == v) {
            h = del_B - del_G;
        } else if (g == v) {
            h = (1.0f / 3.0f) + del_R - del_B;
        } else if (b == v) {
            h = (2.0f / 3.0f) + del_G - del_R;
        }

        if (h < 0.0f) {
            h += 1.0f;
        }

        if (h > 1.0f) {
            h -= 1.0f;
        }
    }
}

Color4 Color4::HSV(float h, float s, float v, float a)
{
    float r = v, g = v, b = v;
    if (s == 0.0f) {    // color is on black-and-white center line
        r = v;          // achromatic: shades of gray
        g = v;          // supposedly invalid for h=0 when s=0 but who cares
        b = v;
    } else {
        float var_h = h * 6;
        int i = int(var_h);             //Or ... i = floor( var_h )
        float f = var_h - i;            // fractional part of h
        float p = v * (1.0f - s);
        float q = v * (1.0f - (s * f));
        float t = v * (1.0f - (s * (1.0f - f)));
        
        switch (i) {
        case 0: 
            r = v;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = v;
            b = p;
            break;
        case 2:
            r = p;
            g = v;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = v;
            break;
        case 4:
            r = t;
            g = p;
            b = v;
            break;
        case 5:
            r = v;
            g = p;
            b = q;
            break;
        }
    }
    return Color4::Create(r,g,b,a);
}
#endif

// --------------------------------------------------------------------------
// Quad
// --------------------------------------------------------------------------

Quad Quad::Empty()
{
    Quad q;
    q.min[0] = FP_INFINITY;
    q.min[1] = FP_INFINITY;
    q.max[0] = FP_NEG_INFINITY;
    q.max[1] = FP_NEG_INFINITY;
    return q;
}

Quad Quad::MinMax(float minX, float minY, float maxX, float maxY)
{
    Quad q;
    q.min[0] = minX;
    q.min[1] = minY;
    q.max[0] = maxX;
    q.max[1] = maxY;
    return q;
}


Quad Quad::MinMax(const Float2& min, const Float2& max)
{
    Quad q;
    q.min = min;
    q.max = max;
    return q;
}

Quad Quad::CenterExtent(const Float2& center, const Float2& extent)
{
    Quad q;
    q.min = center - extent;
    q.max = center + extent;
    return q;
}

bool Quad::operator == (const Quad& v) const
{
    return min == v.min && max == v.max;
}

bool Quad::operator != (const Quad& v) const
{
    return min != v.min || max != v.max;
}

Float2 Quad::Center() const
{
    return Float2(0.5f * (min[0] + max[0]), 0.5f * (min[1] + max[1]));
}

Float2 Quad::Extent() const
{
    return max - Center();
}

}


