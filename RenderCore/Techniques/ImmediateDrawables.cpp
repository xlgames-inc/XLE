// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ImmediateDrawables.h"
#include "PipelineAccelerator.h"
#include "DrawableDelegates.h"
#include "TechniqueDelegates.h"
#include "CommonResources.h"
#include "../Assets/MaterialScaffold.h"
#include "../Assets/AssetFutureContinuation.h"
#include "../Assets/Assets.h"
#include "../Format.h"
#include "../../ShaderParser/AutomaticSelectorFiltering.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../xleres/FileList.h"
#include <deque>

namespace RenderCore { namespace Techniques
{
	struct ReciprocalViewportDimensions
	{
	public:
		float _reciprocalWidth, _reciprocalHeight;
		float _pad[2];
	};

	RenderCore::ConstantBufferElementDesc ReciprocalViewportDimensions_Elements[] = {
		{ Hash64("ReciprocalViewportDimensions"), Format::R32G32_FLOAT, offsetof(ReciprocalViewportDimensions, _reciprocalWidth) }
	};

	class ImmediateRendererResourceDelegate : public IShaderResourceDelegate
	{
	public:
		ReciprocalViewportDimensions _rvd;

		virtual void WriteImmediateData(ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst)
		{
			switch (idx) {
			case 0:
				std::memcpy(dst.begin(), &_rvd, std::min(sizeof(_rvd), dst.size()));
				break;
			default:
				assert(0);
				break;
			}
		}

		virtual size_t GetImmediateDataSize(ParsingContext& context, const void* objectContext, unsigned idx)
		{
			switch (idx) {
			case 0: return sizeof(ReciprocalViewportDimensions);
			default: assert(0); return 0;
			}
		}

		void Configure(IThreadContext& context)
		{
			assert(0);
			// Viewport viewportDesc = _metalContext->GetBoundViewport();
			// _rvd = ReciprocalViewportDimensions { 1.f / float(viewportDesc._width), 1.f / float(viewportDesc._height), 0.f, 0.f };
		}

		UniformsStreamInterface _usi;
		const UniformsStreamInterface& GetInterface() { return _usi; }

		ImmediateRendererResourceDelegate()
		{
			_usi.BindImmediateData(0, Hash64("ReciprocalViewportDimensionsCB"), MakeIteratorRange(ReciprocalViewportDimensions_Elements));

			_rvd = ReciprocalViewportDimensions { 1.f / 25.f, 1.f / 25.f };
		}
	};

	class ImmediateRendererTechniqueDelegate : public ITechniqueDelegate
	{
	public:
		::Assets::FuturePtr<GraphicsPipelineDesc> Resolve(
			const CompiledShaderPatchCollection::Interface& shaderPatches,
			const RenderCore::Assets::RenderStateSet& renderStates)
		{
			return _pipelineDescFuture;
		}

		ImmediateRendererTechniqueDelegate() 
		{
			auto nascentDesc = std::make_shared<GraphicsPipelineDesc>();
			nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer = BASIC2D_VERTEX_HLSL ":P2C:vs_*";
			nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer = BASIC_PIXEL_HLSL ":P:ps_*";

			nascentDesc->_depthStencil = CommonResourceBox::s_dsDisable;
			nascentDesc->_rasterization = CommonResourceBox::s_rsCullDisable;
			nascentDesc->_blend.push_back(CommonResourceBox::s_abStraightAlpha);

			auto vsfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._initializer).AllExceptParameters();
			auto psfn = MakeFileNameSplitter(nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._initializer).AllExceptParameters();

			auto vsFilteringFuture = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(vsfn);
			auto psFilteringFuture = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(psfn);
			
			_pipelineDescFuture = std::make_shared<::Assets::AssetFuture<GraphicsPipelineDesc>>("immediate-renderer");
			::Assets::WhenAll(vsFilteringFuture, psFilteringFuture).ThenConstructToFuture<GraphicsPipelineDesc>(
				*_pipelineDescFuture,
				[nascentDesc](
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& vsFiltering,
					const std::shared_ptr<ShaderSourceParser::SelectorFilteringRules>& psFiltering) {
					
					nascentDesc->_shaders[(unsigned)ShaderStage::Vertex]._automaticFiltering = vsFiltering;
					nascentDesc->_shaders[(unsigned)ShaderStage::Pixel]._automaticFiltering = psFiltering;
					return nascentDesc;
				});
		}
		~ImmediateRendererTechniqueDelegate() {}
	private:
		::Assets::FuturePtr<GraphicsPipelineDesc> _pipelineDescFuture;
	};

	struct DrawableWithVertexCount : public Drawable { unsigned _vertexCount = 0; };

	class ImmediateDrawables : public IImmediateDrawables
	{
	public:
		IteratorRange<void*> QueueDraw(
			size_t vertexDataSize,
			IteratorRange<const InputElementDesc*> inputAssembly,
			const RenderCore::Assets::RenderStateSet& stateSet,
			Topology topology,
			const ParameterBox& shaderSelectors)
		{
			// auto vStride = CalculateVertexStride(inputAssembly);
			auto vStringA = CalculateVertexStrides(inputAssembly);
			auto vStride = vStringA[0];
			auto vertexCount = vertexDataSize / vStride;
			if (!vertexCount) return {};

			auto vertexStorage = _workingPkt.AllocateStorage(DrawablesPacket::Storage::VB, vertexDataSize);

			auto* drawable = _workingPkt._drawables.Allocate<DrawableWithVertexCount>();
			drawable->_geo = AllocateDrawableGeo();
			drawable->_geo->_vertexStreams[0]._resource = nullptr;
			drawable->_geo->_vertexStreams[0]._vbOffset = vertexStorage._startOffset;
			drawable->_geo->_vertexStreamCount = 1;
			drawable->_geo->_ibFormat = Format(0);
			drawable->_pipeline = GetPipelineAccelerator(inputAssembly, stateSet, topology, shaderSelectors);
			drawable->_vertexCount = vertexCount;
			drawable->_drawFn = [](ParsingContext&, const ExecuteDrawableContext& drawContext, const Drawable& drawable) {
				drawContext.Draw(((DrawableWithVertexCount&)drawable)._vertexCount);
			};

			return vertexStorage._data;
		}

		void ExecuteDraws(
			IThreadContext& context,
			ParsingContext& parserContext,
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex)
		{
			auto sequencerConfig = _pipelineAcceleratorPool->CreateSequencerConfig(
				_techniqueDelegate, ParameterBox{},
				fbDesc, subpassIndex);
			SequencerContext sequencerContext;
			sequencerContext._sequencerResources.push_back(_resourceDelegate);
			sequencerContext._sequencerConfig = sequencerConfig.get();
			Draw(
				context, parserContext,
				*_pipelineAcceleratorPool,
				sequencerContext, _workingPkt);

			_workingPkt.Reset();
			_reservedDrawableGeos.insert(_reservedDrawableGeos.end(), _drawableGeosInWorkingPkt.begin(), _drawableGeosInWorkingPkt.end());
			_drawableGeosInWorkingPkt.clear();
		}

		std::shared_ptr<::Assets::IAsyncMarker> PrepareResources(
			const FrameBufferDesc& fbDesc,
			unsigned subpassIndex)
		{
			auto sequencerConfig = _pipelineAcceleratorPool->CreateSequencerConfig(
				_techniqueDelegate, ParameterBox{},
				fbDesc, subpassIndex);
			return Techniques::PrepareResources(*_pipelineAcceleratorPool, *sequencerConfig, _workingPkt);
		}

		ImmediateDrawables(
			const std::shared_ptr<IDevice>& device,
			const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
			const DescriptorSetLayoutAndBinding& matDescSetLayout,
			const DescriptorSetLayoutAndBinding& sequencerDescSetLayout)
		{
			_pipelineAcceleratorPool = CreatePipelineAcceleratorPool(device, pipelineLayout, 0, matDescSetLayout, sequencerDescSetLayout);
			_resourceDelegate = std::make_shared<ImmediateRendererResourceDelegate>();
			_techniqueDelegate = std::make_shared<ImmediateRendererTechniqueDelegate>();
		}

	protected:
		DrawablesPacket _workingPkt;
		std::vector<std::shared_ptr<DrawableGeo>> _drawableGeosInWorkingPkt;
		std::deque<std::shared_ptr<DrawableGeo>> _reservedDrawableGeos;
		std::shared_ptr<IPipelineAcceleratorPool> _pipelineAcceleratorPool;
		std::shared_ptr<ImmediateRendererResourceDelegate> _resourceDelegate;
		std::vector<std::pair<uint64_t, std::shared_ptr<PipelineAccelerator>>> _pipelineAccelerators;
		std::shared_ptr<ITechniqueDelegate> _techniqueDelegate;

		std::shared_ptr<DrawableGeo> AllocateDrawableGeo()
		{
			// Not super efficient caching scheme here! But it's simple
			// Would be better if we just used std::shared_ptr<>s with custom deallocate functions,
			// and a contiguous custom heap
			std::shared_ptr<DrawableGeo> res;
			if (!_reservedDrawableGeos.empty()) {
				res = std::move(_reservedDrawableGeos.front());
				_reservedDrawableGeos.pop_front();
			} else 
				res = std::make_shared<DrawableGeo>();
			_drawableGeosInWorkingPkt.push_back(res);
			return res;
		}

		std::shared_ptr<PipelineAccelerator> GetPipelineAccelerator(
			IteratorRange<const InputElementDesc*> inputAssembly,
			const RenderCore::Assets::RenderStateSet& stateSet,
			Topology topology,
			const ParameterBox& shaderSelectors)
		{
			uint64_t hashCode = Hash64(inputAssembly.begin(), inputAssembly.end(), stateSet.GetHash());
			if (topology != Topology::TriangleList)
				hashCode = HashCombine((uint64_t)topology, hashCode);	// awkward because it's just a small integer value
			if (shaderSelectors.GetCount() != 0) {
				hashCode = HashCombine(shaderSelectors.GetParameterNamesHash(), hashCode);
				hashCode = HashCombine(shaderSelectors.GetHash(), hashCode);
			}

			auto existing = LowerBound(_pipelineAccelerators, hashCode);
			if (existing != _pipelineAccelerators.end() && existing->first == hashCode)
				return existing->second;

			auto newAccelerator = _pipelineAcceleratorPool->CreatePipelineAccelerator(
				nullptr, 
				shaderSelectors, inputAssembly,
				topology, stateSet);
			return newAccelerator;
		}
	};

	std::shared_ptr<IImmediateDrawables> CreateImmediateDrawables(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<RenderCore::ICompiledPipelineLayout>& pipelineLayout,
		const DescriptorSetLayoutAndBinding& matDescSetLayout,
		const DescriptorSetLayoutAndBinding& sequencerDescSetLayout)
	{
		return std::make_shared<ImmediateDrawables>(device, pipelineLayout, matDescSetLayout, sequencerDescSetLayout);
	}

	IImmediateDrawables::~IImmediateDrawables() {}

}}

