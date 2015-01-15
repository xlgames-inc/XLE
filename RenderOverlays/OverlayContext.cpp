// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OverlayContext.h"
#include "Font.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/RenderUtils.h"
#include "../SceneEngine/CommonResources.h"
#include "../Utility/StringFormat.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace RenderOverlays
{
    class Vertex_PC     { public: Float3 _position; unsigned _colour;                                           Vertex_PC(Float3 position, unsigned colour) : _position(position), _colour(colour) {};                                          static RenderCore::Metal::InputElementDesc inputElements[]; };
    class Vertex_PCR    { public: Float3 _position; unsigned _colour; float _radius;                            Vertex_PCR(Float3 position, unsigned colour, float radius) : _position(position), _colour(colour), _radius(radius) {};          static RenderCore::Metal::InputElementDesc inputElements[]; };
    class Vertex_PCT    { public: Float3 _position; unsigned _colour; Float2 _texCoord;                         Vertex_PCT(Float3 position, unsigned colour, Float2 texCoord) : _position(position), _colour(colour), _texCoord(texCoord) {};   static RenderCore::Metal::InputElementDesc inputElements[]; };
    class Vertex_PCTT   { public: Float3 _position; unsigned _colour; Float2 _texCoord0; Float2 _texCoord1;    Vertex_PCTT(Float3 position, unsigned colour, Float2 texCoord0, Float2 texCoord1) : _position(position), _colour(colour), _texCoord0(texCoord0), _texCoord1(texCoord1) {};   static RenderCore::Metal::InputElementDesc inputElements[]; };

    static inline unsigned  HardwareColor(ColorB input)
    {
        return (uint32(input.a) << 24) | (uint32(input.b) << 16) | (uint32(input.g) << 8) | uint32(input.r);
    }

    RenderCore::Metal::InputElementDesc Vertex_PC::inputElements[] = 
    {
        RenderCore::Metal::InputElementDesc( "POSITION",   0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
        RenderCore::Metal::InputElementDesc( "COLOR",      0, RenderCore::Metal::NativeFormat::R8G8B8A8_UNORM  )
    };

    RenderCore::Metal::InputElementDesc Vertex_PCR::inputElements[] = 
    {
        RenderCore::Metal::InputElementDesc( "POSITION",   0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
        RenderCore::Metal::InputElementDesc( "COLOR",      0, RenderCore::Metal::NativeFormat::R8G8B8A8_UNORM  ),
        RenderCore::Metal::InputElementDesc( "RADIUS",     0, RenderCore::Metal::NativeFormat::R32_FLOAT )
    };

    RenderCore::Metal::InputElementDesc Vertex_PCT::inputElements[] = 
    {
        RenderCore::Metal::InputElementDesc( "POSITION",   0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
        RenderCore::Metal::InputElementDesc( "COLOR",      0, RenderCore::Metal::NativeFormat::R8G8B8A8_UNORM  ),
        RenderCore::Metal::InputElementDesc( "TEXCOORD",   0, RenderCore::Metal::NativeFormat::R32G32_FLOAT    )
    };

    RenderCore::Metal::InputElementDesc Vertex_PCTT::inputElements[] = 
    {
        RenderCore::Metal::InputElementDesc( "POSITION",   0, RenderCore::Metal::NativeFormat::R32G32B32_FLOAT ),
        RenderCore::Metal::InputElementDesc( "COLOR",      0, RenderCore::Metal::NativeFormat::R8G8B8A8_UNORM  ),
        RenderCore::Metal::InputElementDesc( "TEXCOORD",   0, RenderCore::Metal::NativeFormat::R32G32_FLOAT    ),
        RenderCore::Metal::InputElementDesc( "TEXCOORD",   1, RenderCore::Metal::NativeFormat::R32G32_FLOAT    )
    };

    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PC>() const -> VertexFormat      { return PC ; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCR>() const -> VertexFormat     { return PCR; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCT>() const -> VertexFormat     { return PCT; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCTT>() const -> VertexFormat    { return PCTT; }
    
    void ImmediateOverlayContext::DrawPoint      (ProjectionMode::Enum proj, const Float3& v,     const ColorB& col,      uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::PointList), _writePointer, 1, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v, HardwareColor(col), float(size));
        _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawPoints     (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::PointList), _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col), float(size));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawPoints     (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::PointList), _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col[c]), float(size));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawLine       (ProjectionMode::Enum proj, const Float3& v0,    const ColorB& colV0,    const Float3& v1,     const ColorB& colV1, float thickness)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + 2 * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::LineList), _writePointer, 2, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v0, HardwareColor(colV0)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v1, HardwareColor(colV1)); _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawLines      (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    float thickness)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::LineList), _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawLines      (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   float thickness)
    {
        typedef Vertex_PC Vertex;
        unsigned maxPointsPerBatch = (sizeof(_workingBuffer) / sizeof(Vertex)) & ~0x1;
        while (numPoints) {
            unsigned pointsThisBatch = std::min(numPoints, maxPointsPerBatch);

            if ((_writePointer + pointsThisBatch * sizeof(Vertex)) > sizeof(_workingBuffer)) {
                Flush();
            }

            PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::LineList), _writePointer, pointsThisBatch, AsVertexFormat<Vertex>(), proj));
            for (unsigned c=0; c<pointsThisBatch; ++c) {
                *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col[c]));
                _writePointer += sizeof(Vertex);
            }

            v += pointsThisBatch;
            col += pointsThisBatch;
            numPoints -= pointsThisBatch;
        }
    }

    void ImmediateOverlayContext::DrawTriangles  (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB& col)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::TriangleList), _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawTriangles  (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[])
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::TriangleList), _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v[c], HardwareColor(col[c]));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawTriangle   (  ProjectionMode::Enum proj,
                                                    const Float3& v0,    const ColorB& colV0,    const Float3& v1,     
                                                    const ColorB& colV1, const Float3& v2,       const ColorB& colV2)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + 3 * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::TriangleList), _writePointer, 3, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v0, HardwareColor(colV0)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v1, HardwareColor(colV1)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(v2, HardwareColor(colV2)); _writePointer += sizeof(Vertex);
    }

    void    ImmediateOverlayContext::DrawQuad(
        ProjectionMode::Enum proj,
        const Float3& mins, const Float3& maxs, 
        ColorB color,
        const Float2& minTex0, const Float2& maxTex0, 
        const Float2& minTex1, const Float2& maxTex1,
        const std::string& pixelShader)
    {
        typedef Vertex_PCTT Vertex;
        if ((_writePointer + 6 * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::TriangleList), _writePointer, 6, AsVertexFormat<Vertex>(), proj, pixelShader));
        auto col = color.AsUInt32();
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], mins[1], mins[2]), col, Float2(minTex0[0], minTex0[1]), Float2(minTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, Float2(minTex0[0], maxTex0[1]), Float2(minTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, Float2(maxTex0[0], minTex0[1]), Float2(maxTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, Float2(maxTex0[0], minTex0[1]), Float2(maxTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, Float2(minTex0[0], maxTex0[1]), Float2(minTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], maxs[1], mins[2]), col, Float2(maxTex0[0], maxTex0[1]), Float2(maxTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawTexturedQuad(
        ProjectionMode::Enum proj, 
        const Float3& mins, const Float3& maxs, 
        const std::string& texture,
        ColorB color, const Float2& minTex0, const Float2& maxTex0)
    {
        typedef Vertex_PCTT Vertex;
        if ((_writePointer + 6 * sizeof(Vertex)) > sizeof(_workingBuffer)) {
            Flush();
        }

        PushDrawCall(DrawCall(unsigned(RenderCore::Metal::Topology::TriangleList), _writePointer, 6, AsVertexFormat<Vertex>(), proj, std::string(), texture));
        auto col = color.AsUInt32();
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], mins[1], mins[2]), col, Float2(minTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, Float2(minTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, Float2(maxTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, Float2(maxTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, Float2(minTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer[_writePointer] = Vertex(Float3(maxs[0], maxs[1], mins[2]), col, Float2(maxTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
    }

    static UiAlign AsUiAlign(TextAlignment::Enum alignment)
    {
        switch (alignment) {
        case TextAlignment::TopLeft:       return UIALIGN_TOP_LEFT;
        case TextAlignment::Top:           return UIALIGN_TOP;
        case TextAlignment::TopRight:      return UIALIGN_TOP_RIGHT;
        case TextAlignment::Left:          return UIALIGN_LEFT;
        default:
        case TextAlignment::Center:        return UIALIGN_CENTER;
        case TextAlignment::Right:         return UIALIGN_RIGHT;
        case TextAlignment::BottomLeft:    return UIALIGN_BOTTOM_LEFT;
        case TextAlignment::Bottom:        return UIALIGN_BOTTOM;
        case TextAlignment::BottomRight:   return UIALIGN_BOTTOM_RIGHT;
        }
    }

    float ImmediateOverlayContext::DrawText      (  const std::tuple<Float3, Float3>& quad, float scale, TextStyle* textStyle, ColorB col, 
                                                    TextAlignment::Enum alignment, const char text[], va_list args)
    {
            //
            //      Because _textStyle.Draw() will draw immediately, we need to flush out
            //      any pending draws
            //
        Flush();

        ucs4 unicharBuffer[4096];

        utf8 buffer[dimof(unicharBuffer)];
        if (args) {
            xl_vsnprintf((char*)buffer, dimof(buffer), text, args);

                //  this conversion doesn't really make sense. Either we should
                //  do the whole thing in ucs2 or ucs4 or just utf8
        
            XlMultiToWide(unicharBuffer, dimof(unicharBuffer), buffer);
        } else {
            XlMultiToWide(unicharBuffer, dimof(unicharBuffer), (const utf8*)text);
        }

        if (!textStyle)
            textStyle = &_defaultTextStyle;

        Quad q;
        q.min = Float2(std::get<0>(quad)[0], std::get<0>(quad)[1]);
        q.max = Float2(std::get<1>(quad)[0], std::get<1>(quad)[1]);
        Float2 alignedPosition = textStyle->AlignText(q, AsUiAlign(alignment), unicharBuffer);
        return textStyle->Draw(
            _deviceContext.get(), 
            alignedPosition[0], alignedPosition[1],
            unicharBuffer, dimof(unicharBuffer),
            0.f, 1.f, 0.f, 
            LinearInterpolate(std::get<0>(quad)[2], std::get<1>(quad)[2], 0.5f),
            col.AsUInt32(), UI_TEXT_STATE_NORMAL, true, nullptr); // &q);
    }

    float ImmediateOverlayContext::StringWidth    (float scale, TextStyle* textStyle, const char text[], va_list args)
    {
        ucs4 unicharBuffer[4096];

        utf8 buffer[dimof(unicharBuffer)];
        if (args) {
            xl_vsnprintf((char*)buffer, dimof(buffer), text, args);

                //  this conversion doesn't really make sense. Either we should
                //  do the whole thing in ucs2 or ucs4 or just utf8
        
            XlMultiToWide(unicharBuffer, dimof(unicharBuffer), buffer);
        } else {
            XlMultiToWide(unicharBuffer, dimof(unicharBuffer), (const utf8*)text);
        }

        if (!textStyle)
            textStyle = &_defaultTextStyle;

        return scale * textStyle->StringWidth(unicharBuffer);
    }

    float ImmediateOverlayContext::TextHeight(TextStyle* textStyle) 
    {
        if (!textStyle)
            textStyle = &_defaultTextStyle;
        return textStyle->_font->LineHeight();
    }

    void ImmediateOverlayContext::CaptureState() 
    {
        SetState(OverlayState());
    }

    void ImmediateOverlayContext::ReleaseState() 
    {
    }

    struct ReciprocalViewportDimensions
    {
    public:
        float _reciprocalWidth, _reciprocalHeight;
        float _pad[2];
    };

    RenderCore::Metal::ConstantBufferLayoutElement ReciprocalViewportDimensions_Elements[] = {
        { "ReciprocalViewportDimensions", RenderCore::Metal::NativeFormat::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth), 0 }
    };
    
    void ImmediateOverlayContext::SetState(const OverlayState& state) 
    {
        using namespace RenderCore::Metal;
        _deviceContext->Bind(SceneEngine::CommonResources()._dssReadWrite);
        _deviceContext->Bind(SceneEngine::CommonResources()._blendStraightAlpha);
        _deviceContext->Bind(SceneEngine::CommonResources()._defaultRasterizer);
        _deviceContext->BindPS(RenderCore::ResourceList<SamplerState, 1>(std::make_tuple(SceneEngine::CommonResources()._defaultSampler)));

        ViewportDesc viewportDesc(*_deviceContext);
        ReciprocalViewportDimensions reciprocalViewportDimensions = { 1.f / float(viewportDesc.Width), 1.f / float(viewportDesc.Height), 0.f, 0.f };
        _viewportConstantBuffer = RenderCore::MakeSharedPkt(
            (const uint8*)&reciprocalViewportDimensions, 
            (const uint8*)PtrAdd(&reciprocalViewportDimensions, sizeof(reciprocalViewportDimensions)));
    }

    void ImmediateOverlayContext::Flush()
    {
        using namespace RenderCore::Metal;
        if (_writePointer != 0) {
            VertexBuffer temporaryBuffer(_workingBuffer, _writePointer);
            for (auto i=_drawCalls.cbegin(); i!=_drawCalls.cend(); ++i) {
                _deviceContext->Bind((Topology::Enum)i->_topology);

                    //
                    //      Rebind the vertex buffer for each draw call
                    //          (because we need to reset offset and vertex stride
                    //          information)
                    //
                const VertexBuffer* vbs[1];
                unsigned strides[1], offsets[1];
                vbs[0] = &temporaryBuffer;
                strides[0] = VertexSize(i->_vertexFormat);
                offsets[0] = i->_vertexOffset;
                _deviceContext->Bind(0, 1, vbs, strides, offsets);

                    //
                    //      The shaders we need to use (and the vertex input
                    //      layout) are determined by the vertex format and 
                    //      topology selected.
                    //
                SetShader(i->_topology, i->_vertexFormat, i->_projMode, i->_pixelShaderName);
                if (!i->_textureName.empty()) {
                    _deviceContext->BindPS(RenderCore::MakeResourceList(
                        Assets::GetAssetDep<DeferredShaderResource>(i->_textureName.c_str()).GetShaderResource()));
                }
                _deviceContext->Draw(i->_vertexCount);
            }
        }

        _drawCalls.clear();
        _writePointer = 0;
    }

    unsigned        ImmediateOverlayContext::VertexSize(VertexFormat format)
    {
        switch (format) {
        case PC:    return sizeof(Vertex_PC);
        case PCT:   return sizeof(Vertex_PCT);
        case PCR:   return sizeof(Vertex_PCR);
        case PCTT:  return sizeof(Vertex_PCTT);
        }
        return 0;
    }

    void            ImmediateOverlayContext::PushDrawCall(const DrawCall& drawCall)
    {
            //  Append this draw call to the previous one (if the formats match)
        if (!_drawCalls.empty()) {
            DrawCall& prevCall = _drawCalls[_drawCalls.size()-1];
            if (    prevCall._topology == drawCall._topology
                &&  prevCall._vertexFormat == drawCall._vertexFormat
                &&  prevCall._pixelShaderName == drawCall._pixelShaderName
                &&  prevCall._textureName == drawCall._textureName
                &&  (prevCall._vertexOffset + prevCall._vertexCount * VertexSize(prevCall._vertexFormat)) == drawCall._vertexOffset) {
                prevCall._vertexCount += drawCall._vertexCount;
                return;
            }
        }
        _drawCalls.push_back(drawCall);
    }

    void            ImmediateOverlayContext::SetShader(unsigned topology, VertexFormat format, ProjectionMode::Enum projMode, const std::string& pixelShaderName)
    {
                // \todo --     we should cache the input layout result
                //              (since it's just the same every time)
        using namespace RenderCore::Metal;

        InputLayout inputLayout;
        ShaderProgram* shaderProgram = nullptr;
        if (topology == Topology::PointList) {

            if (format == PCR) {
                const char* vertexShaderSource      = (projMode==ProjectionMode::P2D)?"game/xleres/basic2D.vsh:P2CR:vs_*":"game/xleres/basic3D.vsh:PCR:vs_*";
                const char geometryShaderSource[]   = "game/xleres/basic.gsh:PCR:gs_*";
                if (pixelShaderName.empty()) {
                    const char pixelShaderSource[]  = "game/xleres/basic.psh:PC:ps_*";
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, geometryShaderSource, pixelShaderSource, "");
                } else {
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, geometryShaderSource, 
                        (std::string("game/xleres/") + pixelShaderName + ":ps_*").c_str(), "");
                }
                inputLayout = std::make_pair(Vertex_PCR::inputElements, dimof(Vertex_PCR::inputElements));
            }

        } else {

            if (format == PC) {
                const char* vertexShaderSource     = (projMode==ProjectionMode::P2D)?"game/xleres/basic2D.vsh:P2C:vs_*":"game/xleres/basic3D.vsh:PC:vs_*";
                if (pixelShaderName.empty()) {
                    const char pixelShaderSource[]  = "game/xleres/basic.psh:PC:ps_*";
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, pixelShaderSource);
                } else {
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, 
                        (std::string("game/xleres/") + pixelShaderName + ":ps_*").c_str());
                }
                inputLayout = std::make_pair(Vertex_PC::inputElements, dimof(Vertex_PC::inputElements));
            } else if (format == PCT) {
                const char* vertexShaderSource     = (projMode==ProjectionMode::P2D)?"game/xleres/basic2D.vsh:P2CT:vs_*":"game/xleres/basic3D.vsh:PCT:vs_*";
                if (pixelShaderName.empty()) {
                    const char pixelShaderSource[]  = "game/xleres/basic.psh:PCT:ps_*";
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, pixelShaderSource);
                } else {
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, 
                        (std::string("game/xleres/") + pixelShaderName + ":ps_*").c_str());
                }
                inputLayout = std::make_pair(Vertex_PCT::inputElements, dimof(Vertex_PCT::inputElements));
            } else if (format == PCTT) {
                const char* vertexShaderSource     = (projMode==ProjectionMode::P2D)?"game/xleres/basic2D.vsh:P2CTT:vs_*":"game/xleres/basic3D.vsh:PCTT:vs_*";
                if (pixelShaderName.empty()) {
                    const char pixelShaderSource[]  = "game/xleres/basic.psh:PCT:ps_*";
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, pixelShaderSource);
                } else {
                    shaderProgram = &Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, 
                        (std::string("game/xleres/") + pixelShaderName + ":ps_*").c_str());
                }
                inputLayout = std::make_pair(Vertex_PCTT::inputElements, dimof(Vertex_PCTT::inputElements));
            }

        }

        if (shaderProgram) {
            BoundInputLayout boundInputLayout(inputLayout, *shaderProgram);
            _deviceContext->Bind(*shaderProgram);
            _deviceContext->Bind(boundInputLayout);

            BoundUniforms boundLayout(*shaderProgram);
            boundLayout.BindConstantBuffer(
                Hash64("ReciprocalViewportDimensions"), 0, 1,
                ReciprocalViewportDimensions_Elements, dimof(ReciprocalViewportDimensions_Elements));
            boundLayout.BindConstantBuffer(Hash64("GlobalTransform"), 1, 1);

            ConstantBufferPacket constants[] = { _viewportConstantBuffer, _globalTransformConstantBuffer };
            boundLayout.Apply(*_deviceContext.get(), UniformsStream(), UniformsStream(constants, nullptr, dimof(constants)));
        } else {
            assert(0);
        }
    }

    RenderCore::Metal::DeviceContext*   ImmediateOverlayContext::GetDeviceContext()
    {
        return _deviceContext.get();
    }

    ImmediateOverlayContext::ImmediateOverlayContext(RenderCore::Metal::DeviceContext* deviceContext, const Float4x4& viewProjectionTransform)
    : _deviceContext(deviceContext)
    , _font(GetX2Font("ui/font/yoon_snail_b.ttf", 16))
    , _defaultTextStyle(*_font.get())
    {
        _writePointer = 0;
        _drawCalls.reserve(64);

        RenderCore::GlobalTransform trans;
        trans._worldToClip = viewProjectionTransform;
        trans._frustumCorners[0] = trans._frustumCorners[1] = trans._frustumCorners[2] = trans._frustumCorners[3] = Float4(0.f, 0.f, 0.f, 0.f);
        trans._worldSpaceView = Float3(0.f, 0.f, 0.f);
        trans._nearClip = trans._farClip = trans._projRatio0 = trans._projRatio1 = 0.f;
        trans._dummy[0] = 0.f;
        trans._viewToWorld = Identity<Float4x4>();
        _globalTransformConstantBuffer = RenderCore::MakeSharedPkt(
            (const uint8*)&trans, (const uint8*)PtrAdd(&trans, sizeof(trans)));
    }

    ImmediateOverlayContext::~ImmediateOverlayContext()
    {
        TRY {
            Flush();
        } CATCH(...) {
            // suppressed exception during ~ImmediateOverlayContext
        } CATCH_END
    }


    IOverlayContext::~IOverlayContext() {}

}