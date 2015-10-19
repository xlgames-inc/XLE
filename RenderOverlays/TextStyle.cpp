// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Font.h"
#include "FontRendering.h"
#include "../RenderCore/RenderUtils.h"
#include "../Assets/Assets.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Math/Vector.h"
#include "../Core/Exceptions.h"
#include <initializer_list>
#include <tuple>        // We can't use variadic template parameters, or initializer_lists, so use tr1 tuples :(
#include <assert.h>
#include <algorithm>

#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"

#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ResourceBox.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace RenderOverlays
{

class WorkingVertexSetPCT
{
public:
    struct Vertex
    {
        Float3      p;
        unsigned    c;
        Float2      t;

        Vertex() {}
        Vertex(Float3 ip, unsigned ic, Float2 it) : p(ip), c(ic), t(it) {}
    };
    static const int    VertexSize = sizeof(Vertex);

    bool                PushQuad(const Quad& positions, unsigned color, const Quad& textureCoords, float depth, bool snap=true);
    void                Reset();
    size_t              VertexCount() const;
    RenderCore::Metal::VertexBuffer    CreateBuffer() const;

    WorkingVertexSetPCT();

private:
    Vertex              _vertices[512];
    Vertex *            _currentIterator;
};

bool            WorkingVertexSetPCT::PushQuad(const Quad& positions, unsigned color, const Quad& textureCoords, float depth, bool snap)
{
    if ((_currentIterator + 6) > &_vertices[dimof(_vertices)]) {
        return false;       // no more space!
    }

    float x0 = positions.min[0];
    float x1 = positions.max[0];
    float y0 = positions.min[1];
    float y1 = positions.max[1];

    Float3 p0(x0, y0, depth);
    Float3 p1(x1, y0, depth);
    Float3 p2(x0, y1, depth);
    Float3 p3(x1, y1, depth);

    if (snap) {
        p0[0] = (float)(int)(0.5f + p0[0]);
        p1[0] = (float)(int)(0.5f + p1[0]);
        p2[0] = (float)(int)(0.5f + p2[0]);
        p3[0] = (float)(int)(0.5f + p3[0]);

        p0[1] = (float)(int)(0.5f + p0[1]);
        p1[1] = (float)(int)(0.5f + p1[1]);
        p2[1] = (float)(int)(0.5f + p2[1]);
        p3[1] = (float)(int)(0.5f + p3[1]);
    }

        //
        //      Following behaviour from DrawQuad in Archeage display_list.cpp
        //
    *_currentIterator++ = Vertex(p0, color, Float2( textureCoords.min[0], textureCoords.min[1] ));
    *_currentIterator++ = Vertex(p2, color, Float2( textureCoords.min[0], textureCoords.max[1] ));
    *_currentIterator++ = Vertex(p1, color, Float2( textureCoords.max[0], textureCoords.min[1] ));
    *_currentIterator++ = Vertex(p1, color, Float2( textureCoords.max[0], textureCoords.min[1] ));
    *_currentIterator++ = Vertex(p2, color, Float2( textureCoords.min[0], textureCoords.max[1] ));
    *_currentIterator++ = Vertex(p3, color, Float2( textureCoords.max[0], textureCoords.max[1] ));

    return true;
}

RenderCore::Metal::VertexBuffer    WorkingVertexSetPCT::CreateBuffer() const
{
    return RenderCore::Metal::VertexBuffer(_vertices, size_t(_currentIterator) - size_t(_vertices));
}

size_t          WorkingVertexSetPCT::VertexCount() const
{
    return _currentIterator - _vertices;
}

void            WorkingVertexSetPCT::Reset()
{
    _currentIterator = _vertices;
}

WorkingVertexSetPCT::WorkingVertexSetPCT()
{
    _currentIterator = _vertices;
}

static void Flush(RenderCore::Metal::DeviceContext& renderer, WorkingVertexSetPCT& vertices)
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;
    if (vertices.VertexCount()) {
        auto vertexBuffer = vertices.CreateBuffer();
        renderer.Bind(MakeResourceList(vertexBuffer), WorkingVertexSetPCT::VertexSize, 0);
        renderer.Draw((unsigned)vertices.VertexCount(), 0);
        vertices.Reset();
    }
}

static unsigned RGBA8(const Color4& color)
{
    return  (unsigned(Clamp(color.a, 0.f, 1.f) * 255.f) << 24)
        |   (unsigned(Clamp(color.b, 0.f, 1.f) * 255.f) << 16)
        |   (unsigned(Clamp(color.g, 0.f, 1.f) * 255.f) <<  8)
        |   (unsigned(Clamp(color.r, 0.f, 1.f) * 255.f)      )
        ;
}

static unsigned ToDigitValue(ucs4 chr, unsigned base)
{
    if (chr >= '0' && chr <= '9')                   { return       chr - '0'; }
    else if (chr >= 'a' && chr < ('a'+base-10))     { return 0xa + chr - 'a'; }
    else if (chr >= 'A' && chr < ('a'+base-10))     { return 0xa + chr - 'A'; }
    return 0xff;
}

static unsigned ParseColorValue(const ucs4 text[], unsigned* colorOverride)
{
    assert(text && colorOverride);

    unsigned digitCharacters = 0;
    while (     (text[digitCharacters] >= '0' && text[digitCharacters] <= '9')
            ||  (text[digitCharacters] >= 'A' && text[digitCharacters] <= 'F')
            ||  (text[digitCharacters] >= 'a' && text[digitCharacters] <= 'f')) {
        ++digitCharacters;
    }

    if (digitCharacters == 6 || digitCharacters == 8) {
        unsigned result = (digitCharacters == 6)?0xff000000:0x0;
        for (unsigned c=0; c<digitCharacters; ++c) {
            result |= ToDigitValue(text[c], 16) << ((digitCharacters-c-1)*4);
        }
        *colorOverride = result;
        return digitCharacters;
    }
    return 0;
}

class TextStyleResources
{
public:
    class Desc {};

    const RenderCore::Metal::ShaderProgram*   _shaderProgram;
    RenderCore::Metal::BoundInputLayout _boundInputLayout;
    RenderCore::Metal::BoundUniforms    _boundUniforms;

    TextStyleResources(const Desc& desc);
    ~TextStyleResources();

    const std::shared_ptr<Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
private:
    std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
};

struct ReciprocalViewportDimensions
{
public:
    float _reciprocalWidth, _reciprocalHeight;
    float _pad[2];
};

TextStyleResources::TextStyleResources(const Desc& desc)
{
    const char vertexShaderSource[]   = "game/xleres/basic2D.vsh:P2CT:" VS_DefShaderModel;
    const char pixelShaderSource[]    = "game/xleres/basic.psh:PCT_Text:" PS_DefShaderModel;

    using namespace RenderCore::Metal;
    const auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(vertexShaderSource, pixelShaderSource);
    BoundInputLayout boundInputLayout(GlobalInputLayouts::PCT, shaderProgram);

    ConstantBufferLayoutElement elements[] = {
        { "ReciprocalViewportDimensions", NativeFormat::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth), 0 }
    };

    BoundUniforms boundUniforms(shaderProgram);
    boundUniforms.BindConstantBuffer(Hash64("ReciprocalViewportDimensions"), 0, 1, elements, dimof(elements));

    auto validationCallback = std::make_shared<Assets::DependencyValidation>();
    Assets::RegisterAssetDependency(validationCallback, shaderProgram.GetDependencyValidation());

    _shaderProgram = &shaderProgram;
    _boundInputLayout = std::move(boundInputLayout);
    _boundUniforms = std::move(boundUniforms);
    _validationCallback = std::move(validationCallback);
}

TextStyleResources::~TextStyleResources()
{}

float   TextStyle::Draw(    
    RenderCore::Metal::DeviceContext* renderer, 
    float x, float y, const ucs4 text[], int maxLen,
    float spaceExtra, float scale, float mx, float depth,
    unsigned colorARGB, UI_TEXT_STATE textState, bool applyDescender, Quad* q) const
{
    if (!_font) {
        return 0.f;
    }

    TRY {

        using namespace RenderCore::Metal;

        int prevGlyph = 0;
        float xScale = scale;
        float yScale = scale;

        if (_options.snap) {
            x = xScale * (int)(0.5f + x / xScale);
            y = yScale * (int)(0.5f + y / yScale);
        }

        float width     = _font->StringWidth(text, maxLen, spaceExtra, _options.outline);
        float height    = _font->LineHeight() * yScale;

        if (textState != UI_TEXT_STATE_NORMAL) {
            const float magicGap = 3.0f;

            Quad reverseBox;
            reverseBox.min[0] = x;
            reverseBox.min[1] = y - height + magicGap * yScale;
            reverseBox.max[0] = x + width;
            reverseBox.max[1] = y + magicGap * xScale;

            if(_options.outline) {
                reverseBox.min[0] -= xScale;
                reverseBox.min[1] -= yScale; 
                reverseBox.max[0] -= xScale;
                reverseBox.max[1] -= yScale;
            }

            if(mx > 0.0f) {
                reverseBox.max[0] = std::min(reverseBox.max[0], mx);
            }

            // ITexture* backTex = NULL;
            // if (textState == UI_TEXT_STATE_REVERSE) {
            //     backTex = desktop.greyTex;
            // }
            // else if( textState == UI_TEXT_STATE_INACTIVE_REVERSE) {
            //     backTex = desktop.greyTex;
            // }
            // if (backTex) {
            //     DrawQuad(list, reverseBox, Quad::MinMax(0,0,1,1), Color4::Create(1,1,1,1), backTex, true, false);
            // }
        }

        // VertexShader& vshader    = GetResource<VertexShader>(vertexShaderSource);
        // PixelShader& pshader     = GetResource<PixelShader>(pixelShaderSource);

        auto& res = RenderCore::Techniques::FindCachedBoxDep<TextStyleResources>(TextStyleResources::Desc());
        renderer->Bind(res._boundInputLayout);     // have to bind a standard P2CT input layout
        renderer->Bind(*res._shaderProgram);
        renderer->Bind(Topology::TriangleList);

        renderer->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
        renderer->Bind(RenderCore::Techniques::CommonResources()._cullDisable);

        {
            ViewportDesc viewportDesc(*renderer);
            ReciprocalViewportDimensions reciprocalViewportDimensions = { 1.f / float(viewportDesc.Width), 1.f / float(viewportDesc.Height), 0.f, 0.f };
            
            // ConstantBuffer constantBuffer(&reciprocalViewportDimensions, sizeof(reciprocalViewportDimensions));
            // std::shared_ptr<std::vector<uint8>> packet = constantBuffer.GetUnderlying();
            auto packet = RenderCore::MakeSharedPkt(
                (const uint8*)&reciprocalViewportDimensions, 
                (const uint8*)PtrAdd(&reciprocalViewportDimensions, sizeof(reciprocalViewportDimensions)));
            res._boundUniforms.Apply(*renderer, UniformsStream(), UniformsStream(&packet, nullptr, 1));
            
            // renderer->BindVS(boundLayout, constantBuffer);
            // renderer.BindVS(ResourceList<ConstantBuffer, 1>(std::make_tuple()));
        }
        const FontTexture2D *   currentBoundTexture = nullptr;
        WorkingVertexSetPCT     workingVertices;

        // bool batchFont = _font->GetTexKind() == FTK_IMAGETEXT ? false : true;
        float descent = 0.0f;
        if (applyDescender) {
            descent = _font->Descent();
        }
        float opacity = (colorARGB >> 24) / float(0xff);
        unsigned colorOverride = 0x0;
        for (uint32 i = 0; i < (uint32)maxLen; ++i) {
            ucs4 ch = text[i];
            if (!ch) break;
            if (ch == '\n' || ch == '\r' || ch == '\0') continue;
            if (mx > 0.0f && x > mx) {
                return x;
            }

            if (!XlComparePrefixI((ucs4*)"{\0\0\0C\0\0\0o\0\0\0l\0\0\0o\0\0\0r\0\0\0:\0\0\0", &text[i], 7)) {
                unsigned newColorOverride = 0;
                unsigned parseLength = ParseColorValue(&text[i+7], &newColorOverride);
                if (parseLength) {
                    colorOverride = newColorOverride;
                    i += 7 + parseLength;
                    while (i<(uint32)maxLen && text[i] && text[i] != '}') ++i;
                    continue;
                }
            }

            int curGlyph;
            Float2 v = _font->GetKerning(prevGlyph, ch, &curGlyph);
            x += xScale * v[0];
            y += yScale * v[1];
            prevGlyph = curGlyph;

            std::pair<const FontChar*, const FontTexture2D*> charAndTexture = _font->GetChar(ch);
            const FontChar* fc       = charAndTexture.first;
            const FontTexture2D* tex = charAndTexture.second;
            if(!fc) continue;

                // Set the new texture if needed (changing state requires flushing completed work)
            if (tex != currentBoundTexture) {
                Flush(*renderer, workingVertices);

                ShaderResourceView::UnderlyingResource sourceTexture = 
                    (ShaderResourceView::UnderlyingResource)tex->GetUnderlying();
                if (!sourceTexture) {
                    throw ::Assets::Exceptions::PendingAsset("", "Pending background upload of font texture");
                }

                ShaderResourceView shadRes(sourceTexture);
                renderer->BindPS(RenderCore::MakeResourceList(shadRes));
                currentBoundTexture = tex;
            }

            _font->TouchFontChar(fc);

            float baseX = x + fc->left * xScale;
            float baseY = y - (fc->top + descent) * yScale;
            if (_options.snap) {
                baseX = xScale * (int)(0.5f + baseX / xScale);
                baseY = yScale * (int)(0.5f + baseY / yScale);
            }

            Quad pos    = Quad::MinMax(baseX, baseY, baseX + fc->width * xScale, baseY + fc->height * yScale);
            Quad tc     = Quad::MinMax(fc->u0, fc->v0, fc->u1, fc->v1);

            if (_options.outline) {
                Quad shadowPos;
                unsigned shadowColor = RGBA8(Color4::Create(0, 0, 0, opacity));

                shadowPos = pos;
                shadowPos.min[0] -= xScale;
                shadowPos.max[0] -= xScale;
                shadowPos.min[1] -= yScale;
                shadowPos.max[1] -= yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[1] -= yScale;
                shadowPos.max[1] -= yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[0] += xScale;
                shadowPos.max[0] += xScale;
                shadowPos.min[1] -= yScale;
                shadowPos.max[1] -= yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[0] -= xScale;
                shadowPos.max[0] -= xScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[0] += xScale;
                shadowPos.max[0] += xScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[0] -= xScale;
                shadowPos.max[0] -= xScale;
                shadowPos.min[1] += yScale;
                shadowPos.max[1] += yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[1] += yScale;
                shadowPos.max[1] += yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }

                shadowPos = pos;
                shadowPos.min[0] += xScale;
                shadowPos.max[0] += xScale;
                shadowPos.min[1] += yScale;
                shadowPos.max[1] += yScale;
                if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
                }
            }

            if (_options.shadow) {
                Quad shadowPos = pos;
                shadowPos.min[0] += xScale;
                shadowPos.max[0] += xScale;
                shadowPos.min[1] += yScale;
                shadowPos.max[1] += yScale;
                if (!workingVertices.PushQuad(shadowPos, RGBA8(Color4::Create(0,0,0,opacity)), tc, depth)) {
                    Flush(*renderer, workingVertices);
                    workingVertices.PushQuad(shadowPos, RGBA8(Color4::Create(0,0,0,opacity)), tc, depth);
                }
            }

            if (!workingVertices.PushQuad(pos, RenderCore::ARGBtoABGR(colorOverride?colorOverride:colorARGB), tc, depth)) {
                Flush(*renderer, workingVertices);
                workingVertices.PushQuad(pos, RenderCore::ARGBtoABGR(colorOverride?colorOverride:colorARGB), tc, depth);
            }

            x += fc->xAdvance * xScale;
            if (_options.outline) {
                x += 2 * xScale;
            }
            if (ch == ' ') {
                x += spaceExtra;
            }

            if (q) {
                if (i == 0) {
                    *q = pos;
                } else {
                    if (q->min[0] > pos.min[0]) {
                        q->min[0] = pos.min[0];
                    }
                    if (q->min[1] > pos.min[1]) {
                        q->min[1] = pos.min[1];
                    }

                    if (q->max[0] < pos.max[0]) {
                        q->max[0] = pos.max[0];
                    }
                    if (q->max[1] < pos.max[1]) {
                        q->max[1] = pos.max[1];
                    }
                }
            }
        }

        Flush(*renderer, workingVertices);

    } CATCH(...) {
        // OutputDebugString("Suppressed exception while drawing text");
    } CATCH_END

    return x;
}

static Float2 GetAlignPos(const Quad& q, const Float2& extent, UiAlign align)
{
    Float2 pos;
    pos[0] = q.min[0];
    pos[1] = q.min[1];
    switch (align) {
    case UIALIGN_TOP_LEFT:
        pos[0] = q.min[0];
        pos[1] = q.min[1];
        break;
    case UIALIGN_TOP:
        pos[0] = 0.5f * (q.min[0] + q.max[0] - extent[0]);
        pos[1] = q.min[1];
        break;
    case UIALIGN_TOP_RIGHT:
        pos[0] = q.max[0] - extent[0];
        pos[1] = q.min[1];
        break;
    case UIALIGN_LEFT:
        pos[0] = q.min[0];
        pos[1] = 0.5f * (q.min[1] + q.max[1] - extent[1]);
        break;
    case UIALIGN_CENTER:
        pos[0] = 0.5f * (q.min[0] + q.max[0] - extent[0]);
        pos[1] = 0.5f * (q.min[1] + q.max[1] - extent[1]);
        break;
    case UIALIGN_RIGHT:
        pos[0] = q.max[0] - extent[0];
        pos[1] = 0.5f * (q.min[1] + q.max[1] - extent[1]);
        break;
    case UIALIGN_BOTTOM_LEFT:
        pos[0] = q.min[0];
        pos[1] = q.max[1] - extent[1];
        break;
    case UIALIGN_BOTTOM:
        pos[0] = 0.5f * (q.min[0] + q.max[0] - extent[0]);
        pos[1] = q.max[1] - extent[1];
        break;
    case UIALIGN_BOTTOM_RIGHT:
        pos[0] = q.max[0] - extent[0];
        pos[1] = q.max[1] - extent[1];
        break;
    }
    return pos;
}

static Float2 AlignText(const Quad& q, Font* font, float stringWidth, float indent, UiAlign align)
{
    Float2 extent = Float2(stringWidth, font->Ascent(false));
    Float2 pos = GetAlignPos(q, extent, align);
    pos[0] += indent;
    pos[1] += extent[1];
    switch (align) {
    case UIALIGN_TOP_LEFT:
    case UIALIGN_TOP:
    case UIALIGN_TOP_RIGHT:
        pos[1] += font->Ascent(true) - extent[1];
        break;
    case UIALIGN_BOTTOM_LEFT:
    case UIALIGN_BOTTOM:
    case UIALIGN_BOTTOM_RIGHT:
        pos[1] -= font->Descent();
        break;
    }
    return pos;
}

Float2 TextStyle::AlignText(const Quad& q, UiAlign align, const ucs4* text, int maxLen /*= -1*/)
{
    assert(_font);
    return RenderOverlays::AlignText(q, _font.get(), _font->StringWidth(text, maxLen), 0, align);
}

Float2 TextStyle::AlignText(const Quad& q, UiAlign align, float width, float indent)
{
    assert(_font);
    return RenderOverlays::AlignText(q, _font.get(), width, indent, align);
}

float TextStyle::StringWidth(const ucs4* text, int maxlen)
{
    return _font->StringWidth(text, maxlen);
}

int TextStyle::CharCountFromWidth(const ucs4* text, float width)
{
    return _font->CharCountFromWidth(text, width);
}

float TextStyle::SetStringEllipis(const ucs4* inText, ucs4* outText, size_t outTextSize, float width)
{
    return _font->StringEllipsis(inText, outText, outTextSize, width);
}

float TextStyle::CharWidth(ucs4 ch, ucs4 prev)
{
    return _font->CharWidth(ch, prev);
}

TextStyle::TextStyle(Font& font, const DrawTextOptions& options) 
: _font(&font), _options(options)
{
}
    
TextStyle::~TextStyle()
{
}

}

