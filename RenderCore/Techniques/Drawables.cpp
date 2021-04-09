// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Drawables.h"
#include "DrawableDelegates.h"
#include "Techniques.h"
#include "TechniqueUtils.h"
#include "ParsingContext.h"
#include "RenderStateResolver.h"
#include "PipelineAccelerator.h"
#include "DescriptorSetAccelerator.h"
#include "BasicDelegates.h"
#include "CommonUtils.h"
#include "CompiledShaderPatchCollection.h"		// for DescriptorSetLayoutAndBinding
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../IThreadContext.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/State.h"
#include "../Metal/Shader.h"
#include "../Metal/Resource.h"
#include "../Metal/TextureView.h"
#include "../Assets/ShaderPatchCollection.h"
#include "../Assets/AsyncMarkerGroup.h"

namespace RenderCore { namespace Techniques
{
	static std::shared_ptr<IDescriptorSet> CreateSequencerDescriptorSet(
		IDevice& device,
		ParsingContext& parsingContext,
		const IPipelineAcceleratorPool& pipelineAccelerators,
		const SequencerContext& sequencerTechnique,
		const DescriptorSetSignature& sequenceDescSetSignature);

///////////////////////////////////////////////////////////////////////////////////////////////////

	struct FixedDescriptorSetBinding
	{
		unsigned _slot;
		uint64_t _hashName;
		const DescriptorSetSignature* _signature;
	};

	static Metal::BoundUniforms* GetBoundUniforms(
		const Metal::GraphicsPipeline& pipeline,
        const UniformsStreamInterface& group0,
		const UniformsStreamInterface& group1)
	{
		uint64_t hash = pipeline.GetInterfaceBindingGUID();
		hash = HashCombine(group0.GetHash(), hash);
		hash = HashCombine(group1.GetHash(), hash);

		static std::unordered_map<uint64_t, std::unique_ptr<Metal::BoundUniforms>> pipelines;
		auto i = pipelines.find(hash);
		if (i == pipelines.end())
			i = pipelines.insert(
				std::make_pair(
					hash, 
					std::make_unique<Metal::BoundUniforms>(pipeline, group0, group1))).first;
			
		return i->second.get();
	}

	class RealExecuteDrawableContext
	{
	public:
		Metal::DeviceContext*				_metalContext;
		Metal::GraphicsEncoder_Optimized*	_encoder;
		const Metal::GraphicsPipeline*		_pipeline;
		const Metal::BoundUniforms*			_boundUniforms;
	};

	static DescriptorSetSignature AsDescriptorSetSignature(const RenderCore::Assets::PredefinedDescriptorSetLayout& layout);

	static void Draw(
		RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Metal::GraphicsEncoder_Optimized& encoder,
		ParsingContext& parserContext,
		const IPipelineAcceleratorPool& pipelineAccelerators,
		const SequencerContext& sequencerTechnique,
		const DrawablesPacket& drawablePkt,
		const IResourcePtr& temporaryVB, 
		const IResourcePtr& temporaryIB)
	{
		// Modern style drawing using GraphicsEncoder_Optimized
		//		-- descriptor sets & graphics pipelines

		const auto sequencerDescriptorSetSlot = 0;
		const auto materialDescriptorSetSlot = 1;
		static const auto sequencerDescSetName = Hash64("Sequencer");
		static const auto materialDescSetName = Hash64("Material");
		UniformsStreamInterface sequencerUSI;
		auto sequencerDescSetLayout = AsDescriptorSetSignature(*pipelineAccelerators.GetSequencerDescriptorSetLayout().GetLayout());
		auto matDescSetLayout = AsDescriptorSetSignature(*pipelineAccelerators.GetMaterialDescriptorSetLayout().GetLayout());
		sequencerUSI.BindFixedDescriptorSet(0, sequencerDescSetName, &sequencerDescSetLayout);
		sequencerUSI.BindFixedDescriptorSet(1, materialDescSetName, &matDescSetLayout);

		auto sequencerDescriptorSet = CreateSequencerDescriptorSet(*pipelineAccelerators.GetDevice(), parserContext, pipelineAccelerators, sequencerTechnique, sequencerDescSetLayout);

		for (auto d=drawablePkt._drawables.begin(); d!=drawablePkt._drawables.end(); ++d) {
			const auto& drawable = *(Drawable*)d.get();
			auto* pipeline = pipelineAccelerators.TryGetPipeline(*drawable._pipeline, *sequencerTechnique._sequencerConfig);
			if (!pipeline)
				continue;

			const IDescriptorSet* matDescSet = nullptr;
			if (drawable._descriptorSet) {
				matDescSet = pipelineAccelerators.TryGetDescriptorSet(*drawable._descriptorSet);
				if (!matDescSet)
					continue;
			}

			////////////////////////////////////////////////////////////////////////////// 
		 
			VertexBufferView vbv[4];
			if (drawable._geo) {
				for (unsigned c=0; c<drawable._geo->_vertexStreamCount; ++c) {
					auto& stream = drawable._geo->_vertexStreams[c];
					vbv[c]._resource = stream._resource ? stream._resource.get() : temporaryVB.get();
					vbv[c]._offset = stream._vbOffset;
				}

				if (drawable._geo->_ibFormat != Format(0)) {
					if (drawable._geo->_ib) {
						encoder.Bind(MakeIteratorRange(vbv, &vbv[drawable._geo->_vertexStreamCount]), IndexBufferView{drawable._geo->_ib.get(), drawable._geo->_ibFormat});
					} else {
						encoder.Bind(MakeIteratorRange(vbv, &vbv[drawable._geo->_vertexStreamCount]), IndexBufferView{temporaryIB.get(), drawable._geo->_ibFormat, drawable._geo->_dynIBBegin});
					}
				} else {
					encoder.Bind(MakeIteratorRange(vbv, &vbv[drawable._geo->_vertexStreamCount]), IndexBufferView{});
				}
			}

			//////////////////////////////////////////////////////////////////////////////

			auto* boundUniforms = GetBoundUniforms(
				*pipeline,
				sequencerUSI,
				drawable._looseUniformsInterface ? *drawable._looseUniformsInterface : UniformsStreamInterface{});

			const IDescriptorSet* descriptorSets[2];
			descriptorSets[0] = sequencerDescriptorSet.get();
			descriptorSets[1] = matDescSet;
			boundUniforms->ApplyDescriptorSets(
				metalContext, encoder,
				MakeIteratorRange(descriptorSets), 0);

			//////////////////////////////////////////////////////////////////////////////

			RealExecuteDrawableContext drawFnContext { &metalContext, &encoder, pipeline, boundUniforms };
			drawable._drawFn(parserContext, *(ExecuteDrawableContext*)&drawFnContext, drawable);
		}
	}

	void Draw(
		IThreadContext& context,
        ParsingContext& parserContext,
		const IPipelineAcceleratorPool& pipelineAccelerators,
		const SequencerContext& sequencerTechnique,
		const DrawablesPacket& drawablePkt)
	{
		assert(sequencerTechnique._sequencerConfig);

		IResourcePtr temporaryVB, temporaryIB;
		if (!drawablePkt.GetStorage(DrawablesPacket::Storage::VB).empty()) {
			temporaryVB = CreateStaticVertexBuffer(*context.GetDevice(), drawablePkt.GetStorage(DrawablesPacket::Storage::VB));
		}
		if (!drawablePkt.GetStorage(DrawablesPacket::Storage::IB).empty()) {
			temporaryIB = CreateStaticIndexBuffer(*context.GetDevice(), drawablePkt.GetStorage(DrawablesPacket::Storage::IB));
		}

		const bool useOptimizedPath = true;
		if (useOptimizedPath) {
			auto& metalContext = *Metal::DeviceContext::Get(context);
			auto encoder = metalContext.BeginGraphicsEncoder(pipelineAccelerators.GetPipelineLayout());
			Draw(metalContext, encoder, parserContext, pipelineAccelerators, sequencerTechnique, drawablePkt, temporaryVB, temporaryIB);
		} else {
			assert(0);
		}
	}

	static const std::string s_graphicsPipeline { "graphics-pipeline" };
	static const std::string s_descriptorSet { "descriptor-set" };

	std::shared_ptr<::Assets::IAsyncMarker> PrepareResources(
		const IPipelineAcceleratorPool& pipelineAccelerators,
		SequencerConfig& sequencerConfig,
		const DrawablesPacket& drawablePkt)
	{
		std::shared_ptr<::Assets::AsyncMarkerGroup> result;

		for (auto d=drawablePkt._drawables.begin(); d!=drawablePkt._drawables.end(); ++d) {
			const auto& drawable = *(Drawable*)d.get();
			auto pipelineFuture = pipelineAccelerators.GetPipeline(*drawable._pipeline, sequencerConfig);
			if (pipelineFuture->GetAssetState() != ::Assets::AssetState::Ready) {
				if (!result)
					result = std::make_shared<::Assets::AsyncMarkerGroup>();
				result->Add(pipelineFuture, s_graphicsPipeline);
			}

			if (drawable._descriptorSet) {
				auto descriptorSetFuture = pipelineAccelerators.GetDescriptorSet(*drawable._descriptorSet);
				if (descriptorSetFuture->GetAssetState() != ::Assets::AssetState::Ready) {
					if (!result)
						result = std::make_shared<::Assets::AsyncMarkerGroup>();
					result->Add(pipelineFuture, s_descriptorSet);
				}
			}
		}

		return result;
	}

	std::shared_ptr<IDescriptorSet> CreateSequencerDescriptorSet(
		IDevice& device,
		ParsingContext& parsingContext,
		const IPipelineAcceleratorPool& pipelineAccelerators,
		const SequencerContext& sequencerTechnique,
		const DescriptorSetSignature& sequenceDescSetSignature)
	{
		// Create a temporary descriptor set, with per-sequencer bindings
		// We need to look for something providing data for this:
		// * parsingContext uniform buffer delegate
		// * sequencer technique uniform buffer delegate
		// * sequencer technique shader resource delegate
		// Unfortunately we have to do a make a lot of small temporary allocations in order to
		// calculate how the various delegates map onto the descriptor set layout. It might be
		// worth considering caching this result, because there should actually only be a finite
		// number of different configurations in most use cases
		const auto& layout = *pipelineAccelerators.GetSequencerDescriptorSetLayout().GetLayout();

		struct ShaderResourceDelegateBinding
		{
			IShaderResourceDelegate* _delegate = nullptr;
			const UniformsStreamInterface* _usi = nullptr;
			uint64_t _resourceViewBindingFlags = 0ull;
			uint64_t _immediateDataBindingFlags = 0ull;
			uint64_t _samplerBindingFlags = 0ull;
			std::vector<unsigned> _resourceInterfaceToDescSet;
			std::vector<unsigned> _immediateDataInterfaceToDescSet;
			std::vector<size_t> _immediateDataSizes;
			std::vector<unsigned> _samplerInterfaceToDescSet;
		};
		std::vector<ShaderResourceDelegateBinding> srBindings;
		srBindings.reserve(parsingContext.GetShaderResourceDelegates().size() + sequencerTechnique._sequencerResources.size());
		for (const auto& sr:parsingContext.GetShaderResourceDelegates()) {
			ShaderResourceDelegateBinding b;
			b._delegate = sr.get();
			b._usi = &sr->GetInterface();
			assert(b._usi->_fixedDescriptorSetBindings.empty());
			b._resourceInterfaceToDescSet.resize(b._usi->_resourceViewBindings.size(), ~0u);
			b._immediateDataInterfaceToDescSet.resize(b._usi->_immediateDataBindings.size(), ~0u);
			b._immediateDataSizes.resize(b._usi->_immediateDataBindings.size(), 0);
			b._samplerInterfaceToDescSet.resize(b._usi->_samplerBindings.size(), ~0u);
			srBindings.push_back(std::move(b));
		}
		for (const auto& sr:sequencerTechnique._sequencerResources) {
			ShaderResourceDelegateBinding b;
			b._delegate = sr.get();
			b._usi = &sr->GetInterface();
			assert(b._usi->_fixedDescriptorSetBindings.empty());
			b._resourceInterfaceToDescSet.resize(b._usi->_resourceViewBindings.size(), ~0u);
			b._immediateDataInterfaceToDescSet.resize(b._usi->_immediateDataBindings.size(), ~0u);
			b._immediateDataSizes.resize(b._usi->_immediateDataBindings.size(), 0);
			b._samplerInterfaceToDescSet.resize(b._usi->_samplerBindings.size(), ~0u);
			srBindings.push_back(std::move(b));
		}

		struct UniformBufferDelegateBinding
		{
			IUniformBufferDelegate* _delegate = nullptr;
			size_t _size = 0;
			unsigned _descSetSlot;
		};
		std::vector<UniformBufferDelegateBinding> uBindings;
		auto uniformBufferInput0 = parsingContext.GetUniformDelegates();
		auto uniformBufferInput1 = MakeIteratorRange(sequencerTechnique._sequencerUniforms);

		for (unsigned slotIdx=0; slotIdx<layout._slots.size(); ++slotIdx) {
			auto hashName = Hash64(layout._slots[slotIdx]._name);
			bool foundBinding = false;
			for (signed c=srBindings.size()-1; c>=0; --c) {
				auto& binding = srBindings[c];

				auto ri = std::find(binding._usi->_resourceViewBindings.begin(), binding._usi->_resourceViewBindings.end(), hashName);
				if (ri != binding._usi->_resourceViewBindings.end()) {
					auto idx = std::distance(binding._usi->_resourceViewBindings.begin(), ri);
					binding._resourceViewBindingFlags |= 1ull<<uint64_t(idx);
					binding._resourceInterfaceToDescSet[idx] = slotIdx;
					foundBinding = true;
					break;
				}

				auto ii = std::find(binding._usi->_immediateDataBindings.begin(), binding._usi->_immediateDataBindings.end(), hashName);
				if (ii != binding._usi->_immediateDataBindings.end()) {
					auto idx = std::distance(binding._usi->_immediateDataBindings.begin(), ii);
					binding._immediateDataBindingFlags |= 1ull<<uint64_t(idx);
					binding._immediateDataInterfaceToDescSet[idx] = slotIdx;
					binding._immediateDataSizes[idx] = binding._delegate->GetImmediateDataSize(parsingContext, nullptr, idx);
					foundBinding = true;
					break;
				}

				auto si = std::find(binding._usi->_samplerBindings.begin(), binding._usi->_samplerBindings.end(), hashName);
				if (si != binding._usi->_samplerBindings.end()) {
					auto idx = std::distance(binding._usi->_samplerBindings.begin(), si);
					binding._samplerBindingFlags |= 1ull<<uint64_t(idx);
					binding._samplerInterfaceToDescSet[idx] = slotIdx;
					foundBinding = true;
					break;
				}
			}

			if (foundBinding) continue;

			auto i = std::find_if(uniformBufferInput1.begin(), uniformBufferInput1.end(), [hashName](const auto& c) { return c.first == hashName; });
			if (i == uniformBufferInput1.end()) {
				i = std::find_if(uniformBufferInput0.begin(), uniformBufferInput0.end(), [hashName](const auto& c) { return c.first == hashName; });
				if (i == uniformBufferInput0.end()) continue;
			}

			UniformBufferDelegateBinding b;
			b._delegate = i->second.get();
			b._size = i->second->GetSize();
			b._descSetSlot = slotIdx;
			uBindings.push_back(b);
		}

		std::vector<DescriptorSetInitializer::BindTypeAndIdx> bindTypesAndIdx;
		std::vector<const IResourceView*> finalResourceViews;
		std::vector<const ISampler*> finalSamplers;
		std::vector<UniformsStream::ImmediateData> finalImmediateData;
		bindTypesAndIdx.resize(layout._slots.size());
		finalResourceViews.reserve(layout._slots.size());
		finalSamplers.reserve(layout._slots.size());
		finalImmediateData.reserve(layout._slots.size());

		size_t totalImmediateDataSize = 0;
		for (const auto&srDelegate:srBindings)
			for (const auto&s:srDelegate._immediateDataSizes)
				totalImmediateDataSize += s;
		for (const auto&u:uBindings)
			totalImmediateDataSize += u._size;

		std::vector<uint8_t> tempDataBuffer;
		tempDataBuffer.resize(totalImmediateDataSize);
		totalImmediateDataSize = 0;

		std::vector<void*> temporaryBuffer(16);
		
		for (const auto&srDelegate:srBindings) {
			if (srDelegate._resourceViewBindingFlags) {
				if (temporaryBuffer.size() < srDelegate._resourceInterfaceToDescSet.size()) temporaryBuffer.resize(srDelegate._resourceInterfaceToDescSet.size());
				IteratorRange<IResourceView**> range { (IResourceView**)AsPointer(temporaryBuffer.begin()), (IResourceView**)AsPointer(temporaryBuffer.begin() + srDelegate._resourceInterfaceToDescSet.size()) };
				srDelegate._delegate->WriteResourceViews(parsingContext, nullptr, srDelegate._resourceViewBindingFlags, range);
				for (unsigned c=0; c<srDelegate._resourceInterfaceToDescSet.size(); ++c) {
					auto b = srDelegate._resourceInterfaceToDescSet[c];
					if (b == ~0u) continue;
					assert(bindTypesAndIdx[b]._type == DescriptorSetInitializer::BindType::Empty);
					bindTypesAndIdx[b] = DescriptorSetInitializer::BindTypeAndIdx{
						DescriptorSetInitializer::BindType::ResourceView,
						(unsigned)finalResourceViews.size()
					};
					finalResourceViews.push_back(range[c]);
				}
			}

			if (srDelegate._samplerBindingFlags) {
				if (temporaryBuffer.size() < srDelegate._samplerInterfaceToDescSet.size()) temporaryBuffer.resize(srDelegate._samplerInterfaceToDescSet.size());
				IteratorRange<ISampler**> range { (ISampler**)AsPointer(temporaryBuffer.begin()), (ISampler**)AsPointer(temporaryBuffer.begin() + srDelegate._samplerInterfaceToDescSet.size()) };
				srDelegate._delegate->WriteSamplers(parsingContext, nullptr, srDelegate._samplerBindingFlags, range);
				for (unsigned c=0; c<srDelegate._samplerInterfaceToDescSet.size(); ++c) {
					auto b = srDelegate._samplerInterfaceToDescSet[c];
					if (b == ~0u) continue;
					assert(bindTypesAndIdx[b]._type == DescriptorSetInitializer::BindType::Empty);
					bindTypesAndIdx[b] = DescriptorSetInitializer::BindTypeAndIdx{
						DescriptorSetInitializer::BindType::Sampler,
						(unsigned)finalSamplers.size()
					};
					finalSamplers.push_back(range[c]);
				}
			}

			if (srDelegate._immediateDataBindingFlags) {
				for (unsigned c=0; c<srDelegate._immediateDataInterfaceToDescSet.size(); ++c) {
					auto b = srDelegate._immediateDataInterfaceToDescSet[c];
					if (b == ~0u) continue;
					auto size = srDelegate._immediateDataSizes[c];
					auto targetRange = MakeIteratorRange(tempDataBuffer.data() + totalImmediateDataSize, tempDataBuffer.data() + totalImmediateDataSize + size);
					srDelegate._delegate->WriteImmediateData(parsingContext, nullptr, c, targetRange);
					totalImmediateDataSize += size;

					assert(bindTypesAndIdx[b]._type == DescriptorSetInitializer::BindType::Empty);
					bindTypesAndIdx[b] = DescriptorSetInitializer::BindTypeAndIdx{
						DescriptorSetInitializer::BindType::ImmediateData,
						(unsigned)finalImmediateData.size()
					};
					finalImmediateData.push_back(targetRange);
				}
			}
		}

		for (const auto&uDelegate:uBindings) {
			auto targetRange = MakeIteratorRange(tempDataBuffer.data() + totalImmediateDataSize, tempDataBuffer.data() + totalImmediateDataSize + uDelegate._size);
			uDelegate._delegate->WriteImmediateData(parsingContext, nullptr, targetRange);
			totalImmediateDataSize += uDelegate._size;

			auto b = uDelegate._descSetSlot;
			assert(bindTypesAndIdx[b]._type == DescriptorSetInitializer::BindType::Empty);
			bindTypesAndIdx[b] = DescriptorSetInitializer::BindTypeAndIdx{
				DescriptorSetInitializer::BindType::ImmediateData,
				(unsigned)finalImmediateData.size()
			};
			finalImmediateData.push_back(targetRange);
		}

		DescriptorSetInitializer initializer;
		initializer._slotBindings = MakeIteratorRange(bindTypesAndIdx);
		initializer._bindItems._resourceViews = MakeIteratorRange(finalResourceViews);
		initializer._bindItems._samplers = MakeIteratorRange(finalSamplers);
		initializer._bindItems._immediateData = MakeIteratorRange(finalImmediateData);
		initializer._signature = &sequenceDescSetSignature;
		return device.CreateDescriptorSet(initializer);
	}

	DescriptorSetSignature AsDescriptorSetSignature(const RenderCore::Assets::PredefinedDescriptorSetLayout& layout)
	{
		DescriptorSetSignature result;
		result._slots.reserve(layout._slots.size());
		for (const auto&s:layout._slots)
			result._slots.push_back(DescriptorSlot{s._type, s._arrayElementCount ? s._arrayElementCount : 1});
		return result;
	}

	void ExecuteDrawableContext::ApplyLooseUniforms(const UniformsStream& stream) const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._boundUniforms->ApplyLooseUniforms(*realContext._metalContext, *realContext._encoder, stream, 1);
	}

	void ExecuteDrawableContext::ApplyDescriptorSets(IteratorRange<const IDescriptorSet* const*> descSets) const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._boundUniforms->ApplyDescriptorSets(*realContext._metalContext, *realContext._encoder, descSets, 1);
	}

	uint64_t ExecuteDrawableContext::GetBoundLooseImmediateDatas() const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		return realContext._boundUniforms->GetBoundLooseImmediateDatas(1);
	}

	uint64_t ExecuteDrawableContext::GetBoundLooseResources() const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		return realContext._boundUniforms->GetBoundLooseResources(1);
	}

	uint64_t ExecuteDrawableContext::GetBoundLooseSamplers() const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		return realContext._boundUniforms->GetBoundLooseSamplers(1);
	}

	bool ExecuteDrawableContext::AtLeastOneBoundLooseUniform() const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		return (realContext._boundUniforms->GetBoundLooseImmediateDatas(1) | realContext._boundUniforms->GetBoundLooseResources(1) | realContext._boundUniforms->GetBoundLooseSamplers(1)) != 0;
	}

	void ExecuteDrawableContext::Draw(unsigned vertexCount, unsigned startVertexLocation) const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._encoder->Draw(*realContext._pipeline, vertexCount, startVertexLocation);
	}

	void ExecuteDrawableContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation) const
	{
		assert(baseVertexLocation == 0);		// parameter deprecated
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._encoder->DrawIndexed(*realContext._pipeline, indexCount, startIndexLocation);
	}

	void ExecuteDrawableContext::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation) const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._encoder->DrawInstances(*realContext._pipeline, vertexCount, instanceCount, startVertexLocation);
	}

	void ExecuteDrawableContext::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation) const
	{
		assert(baseVertexLocation == 0);		// parameter deprecated
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._encoder->DrawIndexedInstances(*realContext._pipeline, indexCount, instanceCount, startIndexLocation);
	}

	void ExecuteDrawableContext::DrawAuto() const
	{
		auto& realContext = *(RealExecuteDrawableContext*)this;
		realContext._encoder->DrawAuto(*realContext._pipeline);
	}

	static DrawablesPacket::AllocateStorageResult AllocateFrom(std::vector<uint8_t>& vector, size_t size, unsigned alignment)
	{
		unsigned preAlignmentBuffer = 0;
		if (alignment != 0) {
			preAlignmentBuffer = alignment - (vector.size() % alignment);
			if (preAlignmentBuffer == alignment) preAlignmentBuffer = 0;
		}

		size_t startOffset = vector.size() + preAlignmentBuffer;
		vector.resize(vector.size() + preAlignmentBuffer + size);
		return {
			MakeIteratorRange(AsPointer(vector.begin() + startOffset), AsPointer(vector.begin() + startOffset + size)),
			(unsigned)startOffset
		};
	}

	auto DrawablesPacket::AllocateStorage(Storage storageType, size_t size) -> AllocateStorageResult
	{
		if (storageType == Storage::IB) {
			return AllocateFrom(_ibStorage, size, _storageAlignment);
		} else {
			assert(storageType == Storage::VB);
			return AllocateFrom(_vbStorage, size, _storageAlignment);
		}
	}

	IteratorRange<const void*> DrawablesPacket::GetStorage(Storage storageType) const
	{
		if (storageType == Storage::IB) {
			return MakeIteratorRange(_ibStorage);
		} else {
			assert(storageType == Storage::VB);
			return MakeIteratorRange(_vbStorage);
		}
	}

}}
