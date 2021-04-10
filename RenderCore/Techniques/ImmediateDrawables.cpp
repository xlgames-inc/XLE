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

			_rvd = ReciprocalViewportDimensions { 1.f / 1264.f, 1.f / 681.f };
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
			nascentDesc->_shaders[(unsigned)ShaderStage::Vertex] = BASIC2D_VERTEX_HLSL ":P2CT:vs_*";
			nascentDesc->_shaders[(unsigned)ShaderStage::Pixel] = BASIC_PIXEL_HLSL ":PCT:ps_*";

			nascentDesc->_depthStencil = CommonResourceBox::s_dsDisable;
			nascentDesc->_rasterization = CommonResourceBox::s_rsCullDisable;
			nascentDesc->_blend.push_back(CommonResourceBox::s_abStraightAlpha);

			_pipelineDescFuture = std::make_shared<::Assets::AssetFuture<GraphicsPipelineDesc>>("immediate-renderer");
			_pipelineDescFuture->SetAsset(std::move(nascentDesc), {});
		}
		~ImmediateRendererTechniqueDelegate() {}
	private:
		::Assets::FuturePtr<GraphicsPipelineDesc> _pipelineDescFuture;
	};

	struct DrawableWithVertexCount : public Drawable 
	{ 
		unsigned _vertexCount = 0, _vertexStride = 0, _bytesAllocated = 0;
		RetainedUniformsStream _uniforms;
	};

	class ImmediateDrawables : public IImmediateDrawables
	{
	public:
		IteratorRange<void*> QueueDraw(
			size_t vertexCount,
			IteratorRange<const MiniInputElementDesc*> inputAssembly,
			const ImmediateDrawableMaterial& material,
			Topology topology)
		{
			auto vStride = CalculateVertexStride(inputAssembly);
			auto vertexDataSize = vertexCount * vStride;
			if (!vertexDataSize) return {};	

			auto pipeline = GetPipelineAccelerator(inputAssembly, material._stateSet, topology, material._shaderSelectors);

			// check if we can just merge it into the previous draw call. If so we're just going to
			// increase the vertex count on that draw call
			bool compatibleWithLastDraw =
				    _lastQueuedDrawable && _lastQueuedDrawable->_pipeline == pipeline && _lastQueuedDrawable->_vertexStride == vStride
				&& topology != Topology::TriangleStrip
				&& topology != Topology::LineStrip
				;
			if (compatibleWithLastDraw) {
				if (material._uniformStreamInterface) {
					compatibleWithLastDraw &= _lastQueuedDrawable->_looseUniformsInterface && (material._uniformStreamInterface->GetHash() == _lastQueuedDrawable->_looseUniformsInterface->GetHash());
				} else
					compatibleWithLastDraw &= _lastQueuedDrawable->_looseUniformsInterface == nullptr;
			}
			if (compatibleWithLastDraw) {
				_lastQueuedDrawVertexCountOffset = _lastQueuedDrawable->_vertexCount;
				return UpdateLastDrawCallVertexCount(vertexCount);
			} else {
				auto vertexStorage = _workingPkt.AllocateStorage(DrawablesPacket::Storage::VB, vertexDataSize);
				auto* drawable = _workingPkt._drawables.Allocate<DrawableWithVertexCount>();
				drawable->_geo = AllocateDrawableGeo();
				drawable->_geo->_vertexStreams[0]._resource = nullptr;
				drawable->_geo->_vertexStreams[0]._vbOffset = vertexStorage._startOffset;
				drawable->_geo->_vertexStreamCount = 1;
				drawable->_geo->_ibFormat = Format(0);
				drawable->_pipeline = std::move(pipeline);
				drawable->_vertexCount = vertexCount;
				drawable->_vertexStride = vStride;
				drawable->_bytesAllocated = vertexDataSize;
				drawable->_drawFn = [](ParsingContext&, const ExecuteDrawableContext& drawContext, const Drawable& drawable) {
					auto* customDrawable = (DrawableWithVertexCount*)&drawable;
					if (drawContext.AtLeastOneBoundLooseUniform()) {
						const IResourceView* res[customDrawable->_uniforms._resourceViews.size()];
						for (size_t c=0; c<customDrawable->_uniforms._resourceViews.size(); ++c) res[c] = customDrawable->_uniforms._resourceViews[c].get();
						UniformsStream::ImmediateData immData[customDrawable->_uniforms._immediateData.size()];
						for (size_t c=0; c<customDrawable->_uniforms._immediateData.size(); ++c) immData[c] = customDrawable->_uniforms._immediateData[c];
						const ISampler* samplers[customDrawable->_uniforms._samplers.size()];
						for (size_t c=0; c<customDrawable->_uniforms._samplers.size(); ++c) samplers[c] = customDrawable->_uniforms._samplers[c].get();
						drawContext.ApplyLooseUniforms(
							UniformsStream { 
								MakeIteratorRange(res, &res[customDrawable->_uniforms._resourceViews.size()]),
								MakeIteratorRange(immData, &immData[customDrawable->_uniforms._immediateData.size()]),
								MakeIteratorRange(samplers, &samplers[customDrawable->_uniforms._samplers.size()]) });
					}
					drawContext.Draw(customDrawable->_vertexCount);
				};
				if (material._uniformStreamInterface) {
					drawable->_looseUniformsInterface = material._uniformStreamInterface;
					drawable->_uniforms = material._uniforms;
				}
				_lastQueuedDrawable = drawable;
				_lastQueuedDrawVertexCountOffset = 0;
				return vertexStorage._data;
			}
		}

		IteratorRange<void*> UpdateLastDrawCallVertexCount(size_t newVertexCount)
		{
			if (!_lastQueuedDrawable)
				Throw(std::runtime_error("Calling UpdateLastDrawCallVertexCount, but no previous draw call to update"));

			auto offsetPlusNewCount = _lastQueuedDrawVertexCountOffset + newVertexCount;
			if (offsetPlusNewCount == _lastQueuedDrawable->_vertexCount) {
				// no update necessary			
			} else if (offsetPlusNewCount > _lastQueuedDrawable->_vertexCount) {
				size_t allocationRequired = offsetPlusNewCount * _lastQueuedDrawable->_vertexStride;
				if (allocationRequired <= _lastQueuedDrawable->_bytesAllocated) {
					_lastQueuedDrawable->_vertexCount = offsetPlusNewCount;
				} else {
					auto extraStorage = _workingPkt.AllocateStorage(DrawablesPacket::Storage::VB, allocationRequired-_lastQueuedDrawable->_bytesAllocated);
					assert(_lastQueuedDrawable->_geo->_vertexStreams[0]._vbOffset + _lastQueuedDrawable->_bytesAllocated == extraStorage._startOffset);
					_lastQueuedDrawable->_bytesAllocated = allocationRequired;
					_lastQueuedDrawable->_vertexCount = offsetPlusNewCount;
				}
			} else {
				_lastQueuedDrawable->_vertexCount = offsetPlusNewCount;
			}

			auto fullStorage = _workingPkt.GetStorage(DrawablesPacket::Storage::VB);
			return MakeIteratorRange(
				const_cast<void*>(PtrAdd(fullStorage.begin(), _lastQueuedDrawable->_geo->_vertexStreams[0]._vbOffset + _lastQueuedDrawVertexCountOffset * _lastQueuedDrawable->_vertexStride)),
				const_cast<void*>(PtrAdd(fullStorage.begin(), _lastQueuedDrawable->_geo->_vertexStreams[0]._vbOffset + offsetPlusNewCount * _lastQueuedDrawable->_vertexStride)));
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
			_lastQueuedDrawable = nullptr;
			_lastQueuedDrawVertexCountOffset = 0;
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
			_lastQueuedDrawable = nullptr;
			_lastQueuedDrawVertexCountOffset = 0;
		}

	protected:
		DrawablesPacket _workingPkt;
		std::vector<std::shared_ptr<DrawableGeo>> _drawableGeosInWorkingPkt;
		std::deque<std::shared_ptr<DrawableGeo>> _reservedDrawableGeos;
		std::shared_ptr<IPipelineAcceleratorPool> _pipelineAcceleratorPool;
		std::shared_ptr<ImmediateRendererResourceDelegate> _resourceDelegate;
		std::vector<std::pair<uint64_t, std::shared_ptr<PipelineAccelerator>>> _pipelineAccelerators;
		std::shared_ptr<ITechniqueDelegate> _techniqueDelegate;
		DrawableWithVertexCount* _lastQueuedDrawable = nullptr;
		unsigned _lastQueuedDrawVertexCountOffset = 0;

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
			IteratorRange<const MiniInputElementDesc*> inputAssembly,
			const RenderCore::Assets::RenderStateSet& stateSet,
			Topology topology,
			const ParameterBox& shaderSelectors)
		{
			uint64_t hashCode = HashInputAssembly(inputAssembly, stateSet.GetHash());
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
			// Note that we keep this pipeline accelerator alive indefinitely 
			_pipelineAccelerators.insert(existing, std::make_pair(hashCode, newAccelerator));
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

