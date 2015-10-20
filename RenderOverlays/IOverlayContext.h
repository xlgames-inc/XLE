// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IOverlayContext_Forward.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Metal/Forward.h"        // for RenderCore::Metal::UniformsStream
#include "../Core/Types.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"

namespace RenderCore { namespace Techniques { class ProjectionDesc; } }

namespace RenderOverlays
{
    class TextStyle;

    class ColorB
    {
    public:
        uint8           a, r, g, b;

                        ColorB() {}
                        ColorB(uint8 r_, uint8 g_, uint8 b_, uint8 a_ = 0xff) : r(r_), g(g_), b(b_), a(a_) {}
        explicit        ColorB(uint32 rawColor)    { a = rawColor >> 24; r = (rawColor >> 16) & 0xff; g = (rawColor >> 8) & 0xff; b = (rawColor >> 0) & 0xff; }
        unsigned        AsUInt32() const           { return (uint32(a) << 24) | (uint32(r) << 16) | (uint32(g) << 8) | uint32(b); }

        static ColorB   FromNormalized(float r_, float g_, float b_, float a_ = 1.f)
        {
            return ColorB(  uint8(Clamp(r_, 0.f, 1.f) * 255.f + 0.5f), uint8(Clamp(g_, 0.f, 1.f) * 255.f + 0.5f), 
                            uint8(Clamp(b_, 0.f, 1.f) * 255.f + 0.5f), uint8(Clamp(a_, 0.f, 1.f) * 255.f + 0.5f));
        }

        static const ColorB White;
        static const ColorB Black;
        static const ColorB Red;
        static const ColorB Green;
        static const ColorB Blue;
        static const ColorB Zero;
    };

////////////////////////////////////////////////////////////////////////////////

    class OverlayState
    {
    public:

            //
            //      Simplified render state settings for
            //      rendering basic debugging things
            //

        struct DepthMode    { enum Enum { Ignore, Read, ReadAndWrite }; };
        DepthMode::Enum     _depthMode;
        OverlayState(DepthMode::Enum depthMode) : _depthMode(depthMode) {}
        OverlayState() : _depthMode(DepthMode::Ignore) {}

    };

////////////////////////////////////////////////////////////////////////////////

    namespace TextAlignment
    {
        enum Enum {
            TopLeft, Top, TopRight,
            Left, Center, Right,
            BottomLeft, Bottom, BottomRight
        };
    }

    namespace ProjectionMode   
    {
        enum Enum { P2D, P3D }; 
    };

////////////////////////////////////////////////////////////////////////////////

        //
        //      IOverlayContext
        //
        //          Common utilities for rendering overlay graphics.
        //          This is mostly required for debugging tools. It should
        //          generally not be used in the shipping product.
        //
        //          
        //
    
    class IOverlayContext
    {
    public:
        virtual void    DrawPoint       (ProjectionMode::Enum proj, const Float3& v,       const ColorB& col,      uint8 size = 1) = 0;
        virtual void    DrawPoints      (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB& col,    uint8 size = 1) = 0;
        virtual void    DrawPoints      (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB col[],   uint8 size = 1) = 0;

        virtual void    DrawLine        (ProjectionMode::Enum proj, const Float3& v0,      const ColorB& colV0,    const Float3& v1,     const ColorB& colV1,        float thickness = 1.0f) = 0;
        virtual void    DrawLines       (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB& col,    float thickness = 1.0f) = 0;
        virtual void    DrawLines       (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB col[],   float thickness = 1.0f) = 0;

        virtual void    DrawTriangles   (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB& col)    = 0;
        virtual void    DrawTriangles   (ProjectionMode::Enum proj, const Float3 v[],      uint32 numPoints,       const ColorB col[])   = 0;
        virtual void    DrawTriangle(
            ProjectionMode::Enum proj, 
            const Float3& v0,      const ColorB& colV0,    const Float3& v1,     
            const ColorB& colV1,   const Float3& v2,       const ColorB& colV2) = 0;

        virtual void    DrawQuad(
            ProjectionMode::Enum proj, 
            const Float3& mins, const Float3& maxs, 
            ColorB color0, ColorB color1,
            const Float2& minTex0, const Float2& maxTex0, 
            const Float2& minTex1, const Float2& maxTex1,
            const std::string& pixelShader = std::string()) = 0;

        virtual void    DrawQuad(
            ProjectionMode::Enum proj, 
            const Float3& mins, const Float3& maxs, 
            ColorB color,
            const std::string& pixelShader = std::string()) = 0;

        virtual void    DrawTexturedQuad(
            ProjectionMode::Enum proj, 
            const Float3& mins, const Float3& maxs, 
            const std::string& texture,
            ColorB color = ColorB(0xffffffff),
            const Float2& minTex0 = Float2(0.f, 0.f), const Float2& maxTex0 = Float2(1.0f, 1.f)) = 0;

        virtual float   DrawText(
            const std::tuple<Float3, Float3>& quad,
            TextStyle* textStyle, ColorB col, TextAlignment::Enum alignment, const char text[], va_list args) = 0;

        virtual float   StringWidth     (float scale, TextStyle* textStyle, const char text[], va_list args) = 0;
        virtual float   TextHeight      (TextStyle* textStyle = nullptr) = 0;

        virtual void    CaptureState    () = 0;
        virtual void    ReleaseState    () = 0;
        virtual void    SetState        (const OverlayState& state) = 0;

        virtual RenderCore::Techniques::ProjectionDesc      GetProjectionDesc() const = 0;
        virtual const RenderCore::Metal::UniformsStream&    GetGlobalUniformsStream() const = 0;
        virtual RenderCore::IThreadContext*                 GetDeviceContext() = 0;

        ~IOverlayContext();
    };



}

