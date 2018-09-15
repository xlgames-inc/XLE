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
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/BufferView.h"
#include "../RenderCore/UniformsStream.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../Utility/StringFormat.h"
#include "../Utility/StringUtils.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace RenderOverlays
{
    using namespace RenderCore;

    class Vertex_PC     { public: Float3 _position; unsigned _colour;                                                               Vertex_PC(Float3 position, unsigned colour) : _position(position), _colour(colour) {};                                          static InputElementDesc inputElements[]; };
    class Vertex_PCR    { public: Float3 _position; unsigned _colour; float _radius;                                                Vertex_PCR(Float3 position, unsigned colour, float radius) : _position(position), _colour(colour), _radius(radius) {};          static InputElementDesc inputElements[]; };
    class Vertex_PCT    { public: Float3 _position; unsigned _colour; Float2 _texCoord;                                             Vertex_PCT(Float3 position, unsigned colour, Float2 texCoord) : _position(position), _colour(colour), _texCoord(texCoord) {};   static InputElementDesc inputElements[]; };
    class Vertex_PCCTT  { public: Float3 _position; unsigned _colour0; unsigned _colour1; Float2 _texCoord0; Float2 _texCoord1;     Vertex_PCCTT(Float3 position, unsigned colour0, unsigned colour1, Float2 texCoord0, Float2 texCoord1) : _position(position), _colour0(colour0), _colour1(colour1), _texCoord0(texCoord0), _texCoord1(texCoord1) {};   static InputElementDesc inputElements[]; };

    static inline unsigned  HardwareColor(ColorB input)
    {
        return (uint32(input.a) << 24) | (uint32(input.b) << 16) | (uint32(input.g) << 8) | uint32(input.r);
    }

    InputElementDesc Vertex_PC::inputElements[] = 
    {
        InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT ),
        InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM  )
    };

    InputElementDesc Vertex_PCR::inputElements[] = 
    {
        InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT ),
        InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM  ),
        InputElementDesc( "RADIUS",     0, Format::R32_FLOAT )
    };

    InputElementDesc Vertex_PCT::inputElements[] = 
    {
        InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT ),
        InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM  ),
        InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT    )
    };

    InputElementDesc Vertex_PCCTT::inputElements[] = 
    {
        InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT ),
        InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM  ),
        InputElementDesc( "COLOR",      1, Format::R8G8B8A8_UNORM  ),
        InputElementDesc( "TEXCOORD",   0, Format::R32G32_FLOAT    ),
        InputElementDesc( "TEXCOORD",   1, Format::R32G32_FLOAT    )
    };

    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PC>() const -> VertexFormat      { return PC ; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCR>() const -> VertexFormat     { return PCR; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCT>() const -> VertexFormat     { return PCT; }
    template<> auto ImmediateOverlayContext::AsVertexFormat<Vertex_PCCTT>() const -> VertexFormat   { return PCCTT; }
    
    void ImmediateOverlayContext::DrawPoint      (ProjectionMode::Enum proj, const Float3& v,     const ColorB& col,      uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::PointList, _writePointer, 1, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v, HardwareColor(col), float(size));
        _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawPoints     (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::PointList, _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col), float(size));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawPoints     (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   uint8 size)
    {
        typedef Vertex_PCR Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::PointList, _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
            *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col[c]), float(size));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawLine       (ProjectionMode::Enum proj, const Float3& v0,    const ColorB& colV0,    const Float3& v1,     const ColorB& colV1, float thickness)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + 2 * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::LineList, _writePointer, 2, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v0, HardwareColor(colV0)); _writePointer += sizeof(Vertex);
		*(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v1, HardwareColor(colV1)); _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawLines      (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB& col,    float thickness)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::LineList, _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
			*(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawLines      (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[],   float thickness)
    {
        typedef Vertex_PC Vertex;
        unsigned maxPointsPerBatch = (_workingBufferSize / sizeof(Vertex)) & ~0x1;
        while (numPoints) {
            unsigned pointsThisBatch = std::min(numPoints, maxPointsPerBatch);

            if ((_writePointer + pointsThisBatch * sizeof(Vertex)) > _workingBufferSize) {
                Flush();
            }

            PushDrawCall(DrawCall(Topology::LineList, _writePointer, pointsThisBatch, AsVertexFormat<Vertex>(), proj));
            for (unsigned c=0; c<pointsThisBatch; ++c) {
				*(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col[c]));
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
        if ((_writePointer + numPoints * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
			*(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawTriangles  (ProjectionMode::Enum proj, const Float3 v[],    uint32 numPoints,       const ColorB col[])
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + numPoints * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, numPoints, AsVertexFormat<Vertex>(), proj));
        for (unsigned c=0; c<numPoints; ++c) {
			*(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v[c], HardwareColor(col[c]));
            _writePointer += sizeof(Vertex);
        }
    }

    void ImmediateOverlayContext::DrawTriangle   (  ProjectionMode::Enum proj,
                                                    const Float3& v0,    const ColorB& colV0,    const Float3& v1,     
                                                    const ColorB& colV1, const Float3& v2,       const ColorB& colV2)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + 3 * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, 3, AsVertexFormat<Vertex>(), proj));
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v0, HardwareColor(colV0)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v1, HardwareColor(colV1)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(v2, HardwareColor(colV2)); _writePointer += sizeof(Vertex);
    }

    void    ImmediateOverlayContext::DrawQuad(
        ProjectionMode::Enum proj,
        const Float3& mins, const Float3& maxs, 
        ColorB color0, ColorB color1,
        const Float2& minTex0, const Float2& maxTex0, 
        const Float2& minTex1, const Float2& maxTex1,
        StringSection<char> pixelShader)
    {
        typedef Vertex_PCCTT Vertex;
        if ((_writePointer + 6 * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, 6, AsVertexFormat<Vertex>(), proj, pixelShader.AsString()));
        auto col0 = HardwareColor(color0);
        auto col1 = HardwareColor(color1);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], mins[1], mins[2]), col0, col1, Float2(minTex0[0], minTex0[1]), Float2(minTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col0, col1, Float2(minTex0[0], maxTex0[1]), Float2(minTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col0, col1, Float2(maxTex0[0], minTex0[1]), Float2(maxTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col0, col1, Float2(maxTex0[0], minTex0[1]), Float2(maxTex1[0], minTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col0, col1, Float2(minTex0[0], maxTex0[1]), Float2(minTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], maxs[1], mins[2]), col0, col1, Float2(maxTex0[0], maxTex0[1]), Float2(maxTex1[0], maxTex1[1])); _writePointer += sizeof(Vertex);
    }

    void    ImmediateOverlayContext::DrawQuad(
            ProjectionMode::Enum proj, 
            const Float3& mins, const Float3& maxs, 
            ColorB color,
            StringSection<char> pixelShader)
    {
        typedef Vertex_PC Vertex;
        if ((_writePointer + 6 * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, 6, AsVertexFormat<Vertex>(), proj, pixelShader.AsString()));
        auto col = HardwareColor(color);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], mins[1], mins[2]), col); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], maxs[1], mins[2]), col); _writePointer += sizeof(Vertex);
    }

    void ImmediateOverlayContext::DrawTexturedQuad(
        ProjectionMode::Enum proj, 
        const Float3& mins, const Float3& maxs, 
        const std::string& texture,
        ColorB color, const Float2& minTex0, const Float2& maxTex0)
    {
        typedef Vertex_PCCTT Vertex;
        if ((_writePointer + 6 * sizeof(Vertex)) > _workingBufferSize) {
            Flush();
        }

        PushDrawCall(DrawCall(Topology::TriangleList, _writePointer, 6, AsVertexFormat<Vertex>(), proj, std::string(), texture));
        auto col = HardwareColor(color);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], mins[1], mins[2]), col, col, Float2(minTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, col, Float2(minTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, col, Float2(maxTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], mins[1], mins[2]), col, col, Float2(maxTex0[0], minTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(mins[0], maxs[1], mins[2]), col, col, Float2(minTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
        *(Vertex*)&_workingBuffer.get()[_writePointer] = Vertex(Float3(maxs[0], maxs[1], mins[2]), col, col, Float2(maxTex0[0], maxTex0[1]), Float2(0.f, 0.f)); _writePointer += sizeof(Vertex);
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

    float ImmediateOverlayContext::DrawText      (  const std::tuple<Float3, Float3>& quad, TextStyle* textStyle, ColorB col, 
                                                    TextAlignment::Enum alignment, StringSection<char> text)
    {
            //
            //      Because _textStyle.Draw() will draw immediately, we need to flush out
            //      any pending draws
            //
        Flush();

		ucs4 unicharBuffer[4096];
        utf8_2_ucs4((const utf8*)text.begin(), text.size(), unicharBuffer, dimof(unicharBuffer));
		StringSection<ucs4> convertedText = unicharBuffer;

        //if (!textStyle)
        //    textStyle = &_defaultTextStyle;

        Quad q;
        q.min = Float2(std::get<0>(quad)[0], std::get<0>(quad)[1]);
        q.max = Float2(std::get<1>(quad)[0], std::get<1>(quad)[1]);
        Float2 alignedPosition = AlignText(*_defaultFont, q, AsUiAlign(alignment), convertedText);
        return Draw(
            *_deviceContext, 
			*_defaultFont, *textStyle,
            alignedPosition[0], alignedPosition[1],
            convertedText,
            0.f, 1.f, 0.f, 
            LinearInterpolate(std::get<0>(quad)[2], std::get<1>(quad)[2], 0.5f),
            col.AsUInt32(), UI_TEXT_STATE_NORMAL, true, nullptr); // &q);
    }

    float ImmediateOverlayContext::StringWidth    (float scale, TextStyle* textStyle, StringSection<char> text)
    {
        ucs4 unicharBuffer[4096];
        utf8_2_ucs4((const utf8*)text.begin(), text.size(), unicharBuffer, dimof(unicharBuffer));

        //if (!textStyle)
        //    textStyle = &_defaultTextStyle;

        return scale * RenderOverlays::StringWidth(*_defaultFont, unicharBuffer);
    }

    float ImmediateOverlayContext::TextHeight(TextStyle* textStyle) 
    {
        return _defaultFont->LineHeight();
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

    RenderCore::ConstantBufferElementDesc ReciprocalViewportDimensions_Elements[] = {
        { Hash64("ReciprocalViewportDimensions"), Format::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth) }
    };
    
    void ImmediateOverlayContext::SetState(const OverlayState& state) 
    {
        _metalContext->Bind(Techniques::CommonResources()._dssReadWrite);
        _metalContext->Bind(Techniques::CommonResources()._blendStraightAlpha);
        _metalContext->Bind(Techniques::CommonResources()._defaultRasterizer);
        // _metalContext->BindPS_G(ResourceList<Metal::SamplerState, 1>(std::make_tuple(Techniques::CommonResources()._defaultSampler)));

        Metal::ViewportDesc viewportDesc(*_metalContext);
        ReciprocalViewportDimensions reciprocalViewportDimensions = { 1.f / float(viewportDesc.Width), 1.f / float(viewportDesc.Height), 0.f, 0.f };
        _viewportConstantBuffer = MakeSharedPkt(
            (const uint8*)&reciprocalViewportDimensions, 
            (const uint8*)PtrAdd(&reciprocalViewportDimensions, sizeof(reciprocalViewportDimensions)));
    }

    void ImmediateOverlayContext::Flush()
    {
        if (_writePointer != 0) {
			auto temporaryBuffer = Metal::MakeVertexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(_workingBuffer.get(), PtrAdd(_workingBuffer.get(), _writePointer)));
            for (auto i=_drawCalls.cbegin(); i!=_drawCalls.cend(); ++i) {
                _metalContext->Bind(i->_topology);

                    //
                    //      Rebind the vertex buffer for each draw call
                    //          (because we need to reset offset and vertex stride
                    //          information)
                    //
				const VertexBufferView vbs[] = { VertexBufferView{ &temporaryBuffer, i->_vertexOffset } };

                    //
                    //      The shaders we need to use (and the vertex input
                    //      layout) are determined by the vertex format and 
                    //      topology selected.
                    //
                SetShader(i->_topology, i->_vertexFormat, i->_projMode, i->_pixelShaderName, MakeIteratorRange(vbs));
                if (!i->_textureName.empty()) {
                    _metalContext->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                        ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>(i->_textureName.c_str()).GetShaderResource()));
                }
                _metalContext->Draw(i->_vertexCount);
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
        case PCCTT: return sizeof(Vertex_PCCTT);
        }
        return 0;
    }

    void            ImmediateOverlayContext::PushDrawCall(const DrawCall& drawCall)
    {
        if (!drawCall._vertexCount) return; // (skip draw calls with zero vertices)

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

    class ImmediateOverlayContext::ShaderBox
    {
    public:
        class Desc
        {
        public:
            Topology _topology;
            VertexFormat _format;
            ProjectionMode::Enum _projMode;
            std::string _pixelShaderName;

            Desc(Topology topology, VertexFormat format, ProjectionMode::Enum projMode, const std::string& pixelShaderName)
                : _topology(topology), _format(format), _projMode(projMode), _pixelShaderName(pixelShaderName) {}
        };

        const Metal::ShaderProgram* _shaderProgram;
        Metal::BoundInputLayout _boundInputLayout;
        Metal::BoundUniforms _boundUniforms;
        Metal::BoundClassInterfaces _boundClassInterfaces;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const     
            { return _validationCallback; }
        
        ShaderBox(const Desc&);

    private:
        std::shared_ptr<::Assets::DependencyValidation>   _validationCallback;
    };

    ImmediateOverlayContext::ShaderBox::ShaderBox(const Desc& desc)
    {
        auto validationCallback = std::make_shared<::Assets::DependencyValidation>();
        InputLayout inputLayout;
        _shaderProgram = nullptr;

        const char* vertexShaderSource = nullptr;
        const char* geometryShaderSource = nullptr;
        const char* pixelShaderDefault = nullptr;

        if (desc._topology == Topology::PointList) {

            if (desc._format == PCR) {
                vertexShaderSource = (desc._projMode==ProjectionMode::P2D)?"xleres/basic2D.vsh:P2CR:vs_*":"xleres/basic3D.vsh:PCR:vs_*";
                geometryShaderSource = "xleres/basic.gsh:PCR:gs_*";
                pixelShaderDefault = "xleres/basic.psh:PC:ps_*";
                inputLayout = MakeIteratorRange(Vertex_PCR::inputElements);
            }

        } else {

            if (desc._format == PC) {
                vertexShaderSource = (desc._projMode==ProjectionMode::P2D)?"xleres/basic2D.vsh:P2C:vs_*":"xleres/basic3D.vsh:PC:vs_*";
                pixelShaderDefault = "xleres/basic.psh:PC:ps_*";
                inputLayout = MakeIteratorRange(Vertex_PC::inputElements);
            } else if (desc._format == PCT) {
                vertexShaderSource = (desc._projMode==ProjectionMode::P2D)?"xleres/basic2D.vsh:P2CT:vs_*":"xleres/basic3D.vsh:PCT:vs_*";
                pixelShaderDefault = "xleres/basic.psh:PCT:ps_*";
                inputLayout = MakeIteratorRange(Vertex_PCT::inputElements);
            } else if (desc._format == PCCTT) {
                vertexShaderSource = (desc._projMode==ProjectionMode::P2D)?"xleres/basic2D.vsh:P2CCTT:vs_*":"xleres/basic3D.vsh:PCCTT:vs_*";
                pixelShaderDefault = "xleres/basic.psh:PCT:ps_*";
                inputLayout = MakeIteratorRange(Vertex_PCCTT::inputElements);
            }

        }

        if (desc._pixelShaderName.empty()) {
			if (geometryShaderSource) {
				_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, geometryShaderSource, pixelShaderDefault, "");
			} else 
				_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, pixelShaderDefault, "");
        } else {
            StringMeld<MaxPath, ::Assets::ResChar> assetName;
            auto paramStart = desc._pixelShaderName.find_first_of(':');
            auto comma = desc._pixelShaderName.find_first_of(',', paramStart);
            if (paramStart != std::string::npos && comma != std::string::npos) {

                    // this shader name has extra parameters (in the form:
                    //   <file>:<entry point>,<interface>=<implementation>
                    //
                    // Build a version of this shader with dynamic linking enabled,
                    // and create a binding for the class interfaces we want.
                assetName << "xleres/" << desc._pixelShaderName.substr(0, comma) << ":!ps_*";

				if (geometryShaderSource) {
					_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, geometryShaderSource, assetName.get(), "");
				} else
					_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, assetName.get(), "");
                _boundClassInterfaces = Metal::BoundClassInterfaces(*_shaderProgram);

                auto i = desc._pixelShaderName.cbegin() + comma + 1;
                for (;;) {
                    while (i < desc._pixelShaderName.cend() && *i == ',') ++i;
                    auto start = i;
                    while (i < desc._pixelShaderName.cend() && *i != ',' && *i != '=') ++i;
                    if (i == start) break;

                    if (i < desc._pixelShaderName.cend() && *i == '=') {
                        auto classEnd = i;
                        auto instanceNameStart = i+1;
                        while (i < desc._pixelShaderName.cend() && *i != ',') ++i;
                        auto bindingHash = Hash64(AsPointer(start), AsPointer(classEnd));
                        _boundClassInterfaces.Bind(bindingHash, 0, 
                            desc._pixelShaderName.substr(
                                std::distance(desc._pixelShaderName.cbegin(), instanceNameStart), i-instanceNameStart).c_str());
                    } else {
                        Log(Warning) << "Malformed shader name in OverlayContext: " << desc._pixelShaderName << std::endl;
                    }
                }
            } else {
                assetName << "xleres/" << desc._pixelShaderName << ":ps_*";
				if (geometryShaderSource) {
					_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, geometryShaderSource, assetName.get(), "");
				} else
					_shaderProgram = &::Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, assetName.get(), "");
            }
        }

        if (_shaderProgram) {
            Metal::BoundInputLayout boundInputLayout(inputLayout, *_shaderProgram);
			UniformsStreamInterface uniformsInterf;
			uniformsInterf.BindConstantBuffer(0, { Hash64("ReciprocalViewportDimensionsCB"), MakeIteratorRange(ReciprocalViewportDimensions_Elements) });
			Metal::BoundUniforms boundUniforms(
				*_shaderProgram,
				Metal::PipelineLayoutConfig(),
				Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				uniformsInterf);

            ::Assets::RegisterAssetDependency(validationCallback, _shaderProgram->GetDependencyValidation());

            _boundInputLayout = std::move(boundInputLayout);
            _boundUniforms = std::move(boundUniforms);
        }

        _validationCallback = std::move(validationCallback);
    }

    void            ImmediateOverlayContext::SetShader(
		Topology topology, VertexFormat format, ProjectionMode::Enum projMode, const std::string& pixelShaderName, 
		IteratorRange<const VertexBufferView*> vertexBuffers)
    {
                // \todo --     we should cache the input layout result
                //              (since it's just the same every time)
        auto& box = ConsoleRig::FindCachedBoxDep<ShaderBox>(
            ShaderBox::Desc(topology, format, projMode, pixelShaderName));

        if (box._shaderProgram) {
            if (box._shaderProgram->DynamicLinkingEnabled()) {
                _metalContext->Bind(*box._shaderProgram, box._boundClassInterfaces);
            } else {
                _metalContext->Bind(*box._shaderProgram);
            }
			box._boundInputLayout.Apply(*_metalContext, vertexBuffers);

			ConstantBufferView stream0CBVs[] = { _globalTransformConstantBuffer };
			box._boundUniforms.Apply(*_metalContext.get(), 0, { MakeIteratorRange(stream0CBVs) });

			ConstantBufferView stream1CBVs[] = { _viewportConstantBuffer };
			box._boundUniforms.Apply(*_metalContext.get(), 1, { MakeIteratorRange(stream1CBVs) });
        } else {
            assert(0);
        }
    }

    IThreadContext*   ImmediateOverlayContext::GetDeviceContext()
    {
        return _deviceContext;
    }

    /*Techniques::ProjectionDesc    ImmediateOverlayContext::GetProjectionDesc() const
    {
        return _projDesc;
    }

    RenderCore::Techniques::AttachmentPool*     ImmediateOverlayContext::GetNamedResources() const
    {
        return _namedResources;
    }

    const Metal::UniformsStream&    ImmediateOverlayContext::GetGlobalUniformsStream() const
    {
        return _globalUniformsStream;
    }*/

    class DefaultFontBox
    {
    public:
        class Desc {};
        std::shared_ptr<Font> _font;
        DefaultFontBox(const Desc&) : _font(GetX2Font("Raleway", 16)) {}
    };

    ImmediateOverlayContext::ImmediateOverlayContext(
        IThreadContext& threadContext, 
        RenderCore::Techniques::AttachmentPool* namedRes,
        const Techniques::ProjectionDesc& projDesc)
    : _defaultFont(ConsoleRig::FindCachedBox2<DefaultFontBox>()._font)
    , _projDesc(projDesc)
    , _deviceContext(&threadContext)
    , _namedResources(namedRes)
    {
		_workingBufferSize = 16 * 1024;
		_workingBuffer = std::make_unique<uint8[]>(_workingBufferSize);
        _metalContext = Metal::DeviceContext::Get(*_deviceContext);

        _writePointer = 0;
        _drawCalls.reserve(64);

        auto trans = Techniques::BuildGlobalTransformConstants(projDesc);
        _globalTransformConstantBuffer = MakeSharedPkt(
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


	std::unique_ptr<ImmediateOverlayContext, AlignedDeletor<ImmediateOverlayContext>>
		MakeImmediateOverlayContext(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::AttachmentPool* namedRes,
			const RenderCore::Techniques::ProjectionDesc& projDesc)
	{
		auto overlayContext = std::unique_ptr<ImmediateOverlayContext, AlignedDeletor<ImmediateOverlayContext>>(
			(ImmediateOverlayContext*)XlMemAlign(sizeof(ImmediateOverlayContext), 16));
		#pragma push_macro("new")
		#undef new
			new(overlayContext.get()) ImmediateOverlayContext(threadContext, namedRes, projDesc);
		#pragma pop_macro("new")
		return overlayContext;
	}

    IOverlayContext::~IOverlayContext() {}



    const ColorB ColorB::White(0xff, 0xff, 0xff, 0xff);
    const ColorB ColorB::Black(0x0, 0x0, 0x0, 0xff);
    const ColorB ColorB::Red(0xff, 0x0, 0x0, 0xff);
    const ColorB ColorB::Green(0x0, 0xff, 0x0, 0xff);
    const ColorB ColorB::Blue(0x0, 0x0, 0xff, 0xff);
    const ColorB ColorB::Zero(0x0, 0x0, 0x0, 0x0);
}

namespace ConsoleRig
{
    template<> uint64 CalculateCachedBoxHash(const RenderOverlays::ImmediateOverlayContext::ShaderBox::Desc& desc)
    {
        return (uint64(desc._format) << 32) ^ (uint64(desc._projMode) << 16) ^ uint64(desc._topology) ^ Hash64(desc._pixelShaderName);
    }
}
