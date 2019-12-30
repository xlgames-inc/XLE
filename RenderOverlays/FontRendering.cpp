// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Font.h"
#include "FT_FontTexture.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/Format.h"
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
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/BufferView.h"

#include "../RenderCore/Techniques/CommonResources.h"
#include "../ConsoleRig/ResourceBox.h"

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
		RenderCore::Metal::Buffer    CreateBuffer(RenderCore::Metal::ObjectFactory&) const;

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

	RenderCore::Metal::Buffer    WorkingVertexSetPCT::CreateBuffer(RenderCore::Metal::ObjectFactory& objectFactory) const
	{
		unsigned size = unsigned(size_t(_currentIterator) - size_t(_vertices));
		return MakeVertexBuffer(objectFactory, MakeIteratorRange(_vertices, PtrAdd(_vertices, size)));
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

	static void Flush(RenderCore::Metal::DeviceContext& renderer, RenderCore::Metal::BoundInputLayout& inputLayout, WorkingVertexSetPCT& vertices)
	{
		using namespace RenderCore;
		if (vertices.VertexCount()) {
			auto vertexBuffer = vertices.CreateBuffer(renderer.GetFactory());
			VertexBufferView vbvs[] = { &vertexBuffer };
			inputLayout.Apply(renderer, MakeIteratorRange(vbvs));
			renderer.Draw((unsigned)vertices.VertexCount(), 0);
			vertices.Reset();
		}
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
		using namespace RenderCore;
		const char vertexShaderSource[]   = "xleres/basic2D.vsh:P2CT:" VS_DefShaderModel;
		const char pixelShaderSource[]    = "xleres/basic.psh:PCT_Text:" PS_DefShaderModel;

		const auto& shaderProgram = Assets::GetAssetDep<Metal::ShaderProgram>(vertexShaderSource, pixelShaderSource);
		Metal::BoundInputLayout boundInputLayout(RenderCore::GlobalInputLayouts::PCT, shaderProgram);

		ConstantBufferElementDesc elements[] = {
			{ Hash64("ReciprocalViewportDimensions"), RenderCore::Format::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth) }
		};

		UniformsStreamInterface usi;
		usi.BindConstantBuffer(0, {Hash64("ReciprocalViewportDimensionsCB"), MakeIteratorRange(elements)});
		usi.BindShaderResource(0, Hash64("InputTexture"));

		Metal::BoundUniforms boundUniforms(
			shaderProgram,
			Metal::PipelineLayoutConfig{},
			UniformsStreamInterface{},
			usi);

		auto validationCallback = std::make_shared<Assets::DependencyValidation>();
		Assets::RegisterAssetDependency(validationCallback, shaderProgram.GetDependencyValidation());

		_shaderProgram = &shaderProgram;
		_boundInputLayout = std::move(boundInputLayout);
		_boundUniforms = std::move(boundUniforms);
		_validationCallback = std::move(validationCallback);
	}

	TextStyleResources::~TextStyleResources()
	{}

	float Draw(    
		RenderCore::IThreadContext& threadContext, 
		const Font& font, const TextStyle& style,
		float x, float y, StringSection<ucs4> text,
		float spaceExtra, float scale, float mx, float depth,
		unsigned colorARGB, bool applyDescender, Quad* q)
	{
		using namespace RenderCore;
		auto& renderer = *Metal::DeviceContext::Get(threadContext);

		int prevGlyph = 0;
		float xScale = scale;
		float yScale = scale;

		if (style._options.snap) {
			x = xScale * (int)(0.5f + x / xScale);
			y = yScale * (int)(0.5f + y / yScale);
		}

		auto& res = ConsoleRig::FindCachedBoxDep<TextStyleResources>(TextStyleResources::Desc());
		res._shaderProgram->Apply(renderer);
		renderer.Bind(Topology::TriangleList);

		renderer.Bind(Techniques::CommonResources()._dssDisable);
		renderer.Bind(Techniques::CommonResources()._cullDisable);

		Metal::ViewportDesc viewportDesc = renderer.GetBoundViewport();
		ReciprocalViewportDimensions reciprocalViewportDimensions = { 1.f / float(viewportDesc.Width), 1.f / float(viewportDesc.Height), 0.f, 0.f };
            
		auto packet = RenderCore::MakeSharedPkt(
			(const uint8*)&reciprocalViewportDimensions, 
			(const uint8*)PtrAdd(&reciprocalViewportDimensions, sizeof(reciprocalViewportDimensions)));
            
		auto& textureMgr = GetFontTextureMgr();
		auto& texSRV = textureMgr.GetFontTexture().GetSRV();
		auto texDims = textureMgr.GetTextureDimensions();
		WorkingVertexSetPCT     workingVertices;

		const Metal::ShaderResourceView* srvs[] = { &texSRV };
		ConstantBufferView cbvs[] = { packet };
		res._boundUniforms.Apply(
			renderer, 1, 
			UniformsStream{
				MakeIteratorRange(cbvs),
				UniformsStream::MakeResources(MakeIteratorRange(srvs))
			});

		float descent = 0.0f;
		if (applyDescender) {
			descent = font.GetFontProperties()._descender;
		}
		float opacity = (colorARGB >> 24) / float(0xff);
		unsigned colorOverride = 0x0;
		for (auto i=text.begin(); i!=text.end(); ++i) {
			auto ch = *i;
			if (mx > 0.0f && x > mx) {
				return x;
			}

			if (!XlComparePrefixI((ucs4*)"{\0\0\0C\0\0\0o\0\0\0l\0\0\0o\0\0\0r\0\0\0:\0\0\0", i, 7)) {
				unsigned newColorOverride = 0;
				unsigned parseLength = ParseColorValue(i+7, &newColorOverride);
				if (parseLength) {
					colorOverride = newColorOverride;
					i += 7 + parseLength;
					while (i<text.end() && *i != '}') ++i;
					continue;
				}
			}

			int curGlyph;
			Float2 v = font.GetKerning(prevGlyph, ch, &curGlyph);
			x += xScale * v[0];
			y += yScale * v[1];
			prevGlyph = curGlyph;

			auto bitmap = font.GetBitmap(ch);

			float baseX = x + bitmap._bitmapOffsetX * xScale;
			float baseY = y + (bitmap._bitmapOffsetY - descent) * yScale;
			if (style._options.snap) {
				baseX = xScale * (int)(0.5f + baseX / xScale);
				baseY = yScale * (int)(0.5f + baseY / yScale);
			}

			Quad pos    = Quad::MinMax(
				baseX, baseY, 
				baseX + (bitmap._bottomRight[0] - bitmap._topLeft[0]) * xScale, baseY + (bitmap._bottomRight[1] - bitmap._topLeft[1]) * yScale);
			Quad tc     = Quad::MinMax(
				bitmap._topLeft[0] / float(texDims[0]), bitmap._topLeft[1] / float(texDims[1]), 
				bitmap._bottomRight[0] / float(texDims[0]), bitmap._bottomRight[1] / float(texDims[1]));

			if (style._options.outline) {
				Quad shadowPos;
				unsigned shadowColor = ColorB::FromNormalized(0, 0, 0, opacity).AsUInt32();

				shadowPos = pos;
				shadowPos.min[0] -= xScale;
				shadowPos.max[0] -= xScale;
				shadowPos.min[1] -= yScale;
				shadowPos.max[1] -= yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[1] -= yScale;
				shadowPos.max[1] -= yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] -= yScale;
				shadowPos.max[1] -= yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[0] -= xScale;
				shadowPos.max[0] -= xScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[0] -= xScale;
				shadowPos.max[0] -= xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				if (!workingVertices.PushQuad(shadowPos, shadowColor, tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
				}
			}

			if (style._options.shadow) {
				Quad shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				if (!workingVertices.PushQuad(shadowPos, ColorB::FromNormalized(0,0,0,opacity).AsUInt32(), tc, depth)) {
					Flush(renderer, res._boundInputLayout, workingVertices);
					workingVertices.PushQuad(shadowPos, ColorB::FromNormalized(0,0,0,opacity).AsUInt32(), tc, depth);
				}
			}

			if (!workingVertices.PushQuad(pos, RenderCore::ARGBtoABGR(colorOverride?colorOverride:colorARGB), tc, depth)) {
				Flush(renderer, res._boundInputLayout, workingVertices);
				workingVertices.PushQuad(pos, RenderCore::ARGBtoABGR(colorOverride?colorOverride:colorARGB), tc, depth);
			}

			x += bitmap._glyph._xAdvance * xScale;
			if (style._options.outline) {
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

		Flush(renderer, res._boundInputLayout, workingVertices);

		return x;
	}

}
