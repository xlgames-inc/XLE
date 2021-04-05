// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Font.h"
#include "FT_FontTexture.h"
#include "../RenderCore/Techniques/ImmediateDrawables.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/RenderUtils.h"
#include "../RenderCore/Types.h"
#include "../RenderCore/Format.h"
// #include "../RenderCore/StateDesc.h"
// #include "../RenderCore/BufferView.h"
#include "../Assets/Assets.h"
#include "../Assets/AssetFutureContinuation.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Math/Vector.h"
// #include "../Core/Exceptions.h"
// #include "../xleres/FileList.h"
#include <assert.h>
#include <algorithm>

/*#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/ObjectFactory.h"

#include "../RenderCore/Techniques/CommonResources.h"*/

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
		static const int VertexSize = sizeof(Vertex);

		void PushQuad(const Quad& positions, unsigned color, const Quad& textureCoords, float depth, bool snap=true);
		void Complete();

		WorkingVertexSetPCT(
			RenderCore::Techniques::IImmediateDrawables& immediateDrawables,
			unsigned reservedQuads);

	private:
		RenderCore::Techniques::IImmediateDrawables* _immediateDrawables;
		IteratorRange<Vertex*> 	_currentAllocation;
		Vertex*            		_currentIterator;
	};

	static RenderCore::MiniInputElementDesc s_inputElements[] = 
	{
		RenderCore::MiniInputElementDesc{ RenderCore::Techniques::CommonSemantics::POSITION, RenderCore::Format::R32G32B32_FLOAT },
		RenderCore::MiniInputElementDesc{ RenderCore::Techniques::CommonSemantics::COLOR, RenderCore::Format::R8G8B8A8_UNORM },
		RenderCore::MiniInputElementDesc{ RenderCore::Techniques::CommonSemantics::TEXCOORD, RenderCore::Format::R32G32_FLOAT }
	};

	void WorkingVertexSetPCT::PushQuad(const Quad& positions, unsigned color, const Quad& textureCoords, float depth, bool snap)
	{
		if (__builtin_expect((_currentIterator + 6) > _currentAllocation.end(), false)) {
			auto reserveVertexCount = _currentAllocation.size() + 6 + (_currentAllocation.size() + 6)/2;
			auto iteratorPosition = _currentIterator - _currentAllocation.begin();
			_currentAllocation = _immediateDrawables->UpdateLastDrawCallVertexCount(reserveVertexCount).Cast<Vertex*>();
			_currentIterator = _currentAllocation.begin() + iteratorPosition;
			assert((_currentIterator + 6) <= _currentAllocation.end());
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
	}

	/*
	RenderCore::Metal::Resource WorkingVertexSetPCT::CreateBuffer(RenderCore::Metal::ObjectFactory& objectFactory) const
	{
		// todo -- we shouldn't create temporary buffers this small. We need a better way to push
		// small amounts of temporary buffer space
		unsigned size = unsigned(size_t(_currentIterator) - size_t(_vertices));
		return RenderCore::Metal::Resource {
			objectFactory, 
			RenderCore::CreateDesc(RenderCore::BindFlag::VertexBuffer, 0, RenderCore::GPUAccess::Read, RenderCore::LinearBufferDesc::Create(size), "tmp-font-buffer"),
			RenderCore::SubResourceInitData { MakeIteratorRange(_vertices, PtrAdd(_vertices, size)) } };
	}

	size_t          WorkingVertexSetPCT::VertexCount() const
	{
		return _currentIterator - _vertices;
	}

	void            WorkingVertexSetPCT::Reset()
	{
		_currentIterator = _vertices;
	}
	*/

	void WorkingVertexSetPCT::Complete()
	{
		// Update the vertex count to be where we ended up
		_immediateDrawables->UpdateLastDrawCallVertexCount(_currentIterator - _currentAllocation.begin());
	}

	WorkingVertexSetPCT::WorkingVertexSetPCT(
		RenderCore::Techniques::IImmediateDrawables& immediateDrawables,
		unsigned reservedQuads)
	: _immediateDrawables(&immediateDrawables)
	{
		assert(reservedQuads != 0);
		_currentAllocation = _immediateDrawables->QueueDraw(
			reservedQuads * 6,
			MakeIteratorRange(s_inputElements), 
			RenderCore::Assets::RenderStateSet{}).Cast<Vertex*>();
		_currentIterator = _currentAllocation.begin();
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

#if 0
	class TextStyleResources
	{
	public:
		std::shared_ptr<RenderCore::Metal::GraphicsPipeline> _pipeline;
		RenderCore::Metal::BoundUniforms _boundUniforms;

		~TextStyleResources();

		struct Desc 
		{
			uint64_t GetHash() const { return 0; }
		};
		static void ConstructToFuture(
			::Assets::AssetFuture<TextStyleResources>&,
			const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
			const Desc&);

		const std::shared_ptr<Assets::DependencyValidation>& GetDependencyValidation() const   { return _depVal; }
	protected:
		TextStyleResources();
		std::shared_ptr<Assets::DependencyValidation> _depVal;
	};

	struct ReciprocalViewportDimensions
	{
	public:
		float _reciprocalWidth, _reciprocalHeight;
		float _pad[2];
	};

	void TextStyleResources::ConstructToFuture(
		::Assets::AssetFuture<TextStyleResources>& future,
		const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
		const Desc& desc)
	{
		using namespace RenderCore;
		const char vertexShaderSource[]   = BASIC2D_VERTEX_HLSL ":P2CT:" VS_DefShaderModel;
		const char pixelShaderSource[]    = BASIC_PIXEL_HLSL ":PCT_Text:" PS_DefShaderModel;
		const auto& shaderFuture = ::Assets::MakeAsset<Metal::ShaderProgram>(pipelineLayout, vertexShaderSource, pixelShaderSource);

		::Assets::WhenAll(shaderFuture).ThenConstructToFuture<TextStyleResources>(
			future,
			[](const std::shared_ptr<Metal::ShaderProgram>& shader) {

				auto result = std::make_shared<TextStyleResources>();

				Metal::GraphicsPipelineBuilder builder;
				Metal::BoundInputLayout boundInputLayout(RenderCore::GlobalInputLayouts::PCT, *shader);
				builder.Bind(boundInputLayout, Topology::TriangleList);
				builder.Bind(Techniques::CommonResourceBox::s_dsDisable);
				builder.Bind(Techniques::CommonResourceBox::s_rsCullDisable);
				AttachmentBlendDesc attachmentBlends[] = { Techniques::CommonResourceBox::s_abStraightAlpha };
				builder.Bind(MakeIteratorRange(attachmentBlends));

				// We have to make an assumption about the render pass we're going to be drawing onto
				// in particular, we need to know the format we'll render to
				{
					SubpassDesc expectingSubpass;
					expectingSubpass.AppendOutput(AttachmentViewDesc{0});
					FrameBufferDesc::Attachment expectingAttachment;
					expectingAttachment._desc._format = Format::R8G8B8A8_UNORM_SRGB;
					FrameBufferDesc expectingRenderPass { 
						std::vector<FrameBufferDesc::Attachment>{ expectingAttachment },
						std::vector<SubpassDesc>{ expectingSubpass }
					};
					builder.SetRenderPassConfiguration(expectingRenderPass, 0);
				}

				result->_pipeline = builder.CreatePipeline(Metal::GetObjectFactory());

				ConstantBufferElementDesc elements[] = {
					{ Hash64("ReciprocalViewportDimensions"), RenderCore::Format::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth) }
				};

				UniformsStreamInterface usi;
				usi.BindImmediateData(0, Hash64("ReciprocalViewportDimensionsCB"), MakeIteratorRange(elements));
				usi.BindResourceView(0, Hash64("InputTexture"));
				result->_boundUniforms = Metal::BoundUniforms{*shader, usi};
				result->_depVal = shader->GetDependencyValidation();
				return result;
			});
	}

	TextStyleResources::TextStyleResources() {}
	TextStyleResources::~TextStyleResources() {}

	static void Flush(
		RenderCore::Metal::GraphicsEncoder_Optimized& encoder, 
		const RenderCore::Metal::GraphicsPipeline& pipeline,
		WorkingVertexSetPCT& vertices)
	{
		using namespace RenderCore;
		if (vertices.VertexCount()) {
			auto vertexBuffer = vertices.CreateBuffer(Metal::GetObjectFactory());
			VertexBufferView vbvs[] = { &vertexBuffer };
			encoder.Bind(MakeIteratorRange(vbvs), {});
			encoder.Draw(pipeline, (unsigned)vertices.VertexCount(), 0);
			vertices.Reset();
		}
	}
#endif

	float Draw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::IImmediateDrawables& immediateDrawables,
		const Font& font, const TextStyle& style,
		float x, float y, StringSection<ucs4> text,
		float spaceExtra, float scale, float mx, float depth,
		unsigned colorARGB, bool applyDescender, Quad* q)
	{
		using namespace RenderCore;
		// auto& metalContext = *Metal::DeviceContext::Get(threadContext);

		int prevGlyph = 0;
		float xScale = scale;
		float yScale = scale;

		if (style._options.snap) {
			x = xScale * (int)(0.5f + x / xScale);
			y = yScale * (int)(0.5f + y / yScale);
		}

		/*
		auto resFuture = ::Assets::MakeAsset<TextStyleResources>(
			pipelineLayout,
			TextStyleResources::Desc());
		auto* res = resFuture->TryActualize().get();
		if (!res) return 0.f;
		
		auto encoder = metalContext.BeginGraphicsEncoder(pipelineLayout);

		auto viewportDesc = metalContext.GetBoundViewport();
		ReciprocalViewportDimensions reciprocalViewportDimensions = { 1.f / float(viewportDesc.Width), 1.f / float(viewportDesc.Height), 0.f, 0.f };
		*/
			
		auto& textureMgr = GetFontTextureMgr();
		auto* texSRV = textureMgr.GetFontTexture().GetSRV().get();
		auto texDims = textureMgr.GetTextureDimensions();
		auto estimatedQuadCount = text.size();
		if (style._options.shadow)
			estimatedQuadCount += text.size();
		if (style._options.outline)
			estimatedQuadCount += 8 * text.size();
		WorkingVertexSetPCT workingVertices(immediateDrawables, estimatedQuadCount);

		/*
		const IResourceView* srvs[] = { texSRV };
		IteratorRange<const void*> cbvs[] = { MakeOpaqueIteratorRange(reciprocalViewportDimensions) };
		res->_boundUniforms.ApplyLooseUniforms(
			metalContext, encoder,
			UniformsStream{
				MakeIteratorRange(srvs),
				MakeIteratorRange(cbvs)
			});
		*/

		float descent = 0.0f;
		if (applyDescender) {
			descent = font.GetFontProperties()._descender;
		}
		float opacity = (colorARGB >> 24) / float(0xff);
		unsigned colorOverride = 0x0;

		for (auto i=text.begin(); i!=text.end(); ++i) {
			auto ch = *i;
			if (__builtin_expect(mx > 0.0f && x > mx, false)) {
				break;
			}

			if (__builtin_expect(!XlComparePrefixI((ucs4*)"{\0\0\0C\0\0\0o\0\0\0l\0\0\0o\0\0\0r\0\0\0:\0\0\0", i, 7), false)) {
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

			Quad pos = Quad::MinMax(
				baseX, baseY, 
				baseX + (bitmap._bottomRight[0] - bitmap._topLeft[0]) * xScale, baseY + (bitmap._bottomRight[1] - bitmap._topLeft[1]) * yScale);
			Quad tc = Quad::MinMax(
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
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[1] -= yScale;
				shadowPos.max[1] -= yScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] -= yScale;
				shadowPos.max[1] -= yScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[0] -= xScale;
				shadowPos.max[0] -= xScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[0] -= xScale;
				shadowPos.max[0] -= xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);

				shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				workingVertices.PushQuad(shadowPos, shadowColor, tc, depth);
			}

			if (style._options.shadow) {
				Quad shadowPos = pos;
				shadowPos.min[0] += xScale;
				shadowPos.max[0] += xScale;
				shadowPos.min[1] += yScale;
				shadowPos.max[1] += yScale;
				workingVertices.PushQuad(shadowPos, ColorB::FromNormalized(0,0,0,opacity).AsUInt32(), tc, depth);
			}

			workingVertices.PushQuad(pos, RenderCore::ARGBtoABGR(colorOverride?colorOverride:colorARGB), tc, depth);

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

		workingVertices.Complete();
		return x;
	}

}
