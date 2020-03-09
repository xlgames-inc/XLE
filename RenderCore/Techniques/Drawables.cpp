// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Drawables.h"
#include "DrawableDelegates.h"
#include "Techniques.h"
#include "TechniqueUtils.h"
#include "ParsingContext.h"
#include "RenderStateResolver.h"
#include "CompiledRenderStateSet.h"
#include "PipelineAccelerator.h"
#include "DescriptorSetAccelerator.h"
#include "BasicDelegates.h"
#include "CommonUtils.h"
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

namespace RenderCore { namespace Techniques
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void SetupDefaultUniforms(
		Techniques::ParsingContext& parserContext,
		UniformsStreamInterface& sequencerInterface,
		std::vector<ConstantBufferView>& sequencerCbvs,
		unsigned& cbSlot);

	static Metal::BoundUniforms* GetBoundUniforms(
		const Metal::GraphicsPipeline& pipeline,
        const Metal::PipelineLayoutConfig& pipelineLayout,
        const UniformsStreamInterface& interface0 = {},
        const UniformsStreamInterface& interface1 = {},
        const UniformsStreamInterface& interface2 = {},
		const UniformsStreamInterface& interface3 = {})
	{
		uint64_t hash = pipeline.GetGUID();
		hash = HashCombine(interface0.GetHash(), hash);
		hash = HashCombine(interface1.GetHash(), hash);
		hash = HashCombine(interface2.GetHash(), hash);
		hash = HashCombine(interface3.GetHash(), hash);

		static std::unordered_map<uint64_t, std::unique_ptr<Metal::BoundUniforms>> pipelines;
		auto i = pipelines.find(hash);
		if (i == pipelines.end())
			i = pipelines.insert(
				std::make_pair(
					hash, 
					std::make_unique<Metal::BoundUniforms>(pipeline, pipelineLayout, interface0, interface1, interface2, interface3))).first;
			
		return i->second.get();
	}

	void Draw(
		IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		const SequencerContext& sequencerTechnique,
		const DrawablesPacket& drawablePkt)
	{
		assert(sequencerTechnique._sequencerConfig);

		auto& metalContext = *Metal::DeviceContext::Get(context);

		UniformsStreamInterface sequencerInterface;
		std::vector<ConstantBufferView> sequencerCbvs;
		std::vector<Metal::ShaderResourceView> sequencerSrvs;
		std::vector<Metal::SamplerState> sequencerSamplerStates;
		{
			unsigned cbSlot = 0, srvSlot = 0;
			for (auto& d:sequencerTechnique._sequencerUniforms) {
				sequencerInterface.BindConstantBuffer(cbSlot++, {d.first, d.second->GetLayout()});
				sequencerCbvs.push_back(d.second->WriteBuffer(parserContext, nullptr));
			}

			SetupDefaultUniforms(parserContext, sequencerInterface, sequencerCbvs, cbSlot);

			for (auto& d:sequencerTechnique._sequencerResources) {
				auto bindings = d->GetBindings();
				for (auto b:bindings)
					sequencerInterface.BindShaderResource(srvSlot++, b);

				auto start = sequencerSrvs.size();
				sequencerSrvs.resize(start+bindings.size());
				sequencerSamplerStates.resize(start+bindings.size());

				d->GetShaderResources(
					parserContext, nullptr,
					MakeIteratorRange(AsPointer(sequencerSrvs.begin()+start), AsPointer(sequencerSrvs.end())),
					MakeIteratorRange(AsPointer(sequencerSamplerStates.begin()+start), AsPointer(sequencerSamplerStates.end())));
			}
		}

		IResourcePtr temporaryVB, temporaryIB;
		if (!drawablePkt.GetStorage(DrawablesPacket::Storage::VB).empty()) {
			temporaryVB = CreateStaticVertexBuffer(*context.GetDevice(), drawablePkt.GetStorage(DrawablesPacket::Storage::VB));
		}
		if (!drawablePkt.GetStorage(DrawablesPacket::Storage::IB).empty()) {
			temporaryIB = CreateStaticIndexBuffer(*context.GetDevice(), drawablePkt.GetStorage(DrawablesPacket::Storage::IB));
		}

		for (auto d=drawablePkt._drawables.begin(); d!=drawablePkt._drawables.end(); ++d) {
			const auto& drawable = *(Drawable*)d.get();
			auto* pipeline = parserContext._pipelineAcceleratorPool->TryGetPipeline(
				*drawable._pipeline,
				*sequencerTechnique._sequencerConfig);

			if (!pipeline || !drawable._descriptorSet)
				continue;

			//////////////////////////////////////////////////////////////////////////////

			VertexBufferView vbv[4];
			if (drawable._geo) {
				for (unsigned c=0; c<drawable._geo->_vertexStreamCount; ++c) {
					auto& stream = drawable._geo->_vertexStreams[c];
					vbv[c]._resource = stream._resource ? stream._resource.get() : temporaryVB.get();
					vbv[c]._offset = stream._vbOffset;
				}

				pipeline->ApplyVertexBuffers(metalContext, MakeIteratorRange(vbv));

				if (drawable._geo->_ibFormat != Format(0)) {
					auto* ib = drawable._geo->_ib ? drawable._geo->_ib.get() : temporaryIB.get();
					metalContext.Bind(Metal::AsResource(*ib), drawable._geo->_ibFormat);
				}
			} else {
				metalContext.UnbindInputLayout();
			}

			//////////////////////////////////////////////////////////////////////////////

			auto* boundUniforms = GetBoundUniforms(
				*pipeline,
				Metal::PipelineLayoutConfig{},
				sequencerInterface,
				drawable._descriptorSet->_usi,	// mat stream
				UniformsStreamInterface{},		// geo stream
				drawable._uniformsInterface ? *drawable._uniformsInterface : UniformsStreamInterface{});

			boundUniforms->Apply(
				metalContext, 0, 
				UniformsStream {
					MakeIteratorRange(sequencerCbvs),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSrvs)),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSamplerStates))});
			drawable._descriptorSet->Apply(metalContext, *boundUniforms, 1);

			//////////////////////////////////////////////////////////////////////////////

			drawable._drawFn(
				parserContext, 
				Drawable::DrawFunctionContext { &metalContext, pipeline, boundUniforms },
				drawable);
		}
	}

	void SetupDefaultUniforms(
		Techniques::ParsingContext& parserContext,
		UniformsStreamInterface& sequencerInterface,
		std::vector<ConstantBufferView>& sequencerCbvs,
		unsigned& cbSlot)
	{
		auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c) {
			auto bindingName = techUSI._cbBindings[c]._hashName;
			RenderCore::Techniques::GlobalCBDelegate delegate{c};
			sequencerInterface.BindConstantBuffer(cbSlot++, {bindingName, delegate.GetLayout()});
			sequencerCbvs.push_back(delegate.WriteBuffer(parserContext, nullptr));
		}

		for (const auto& d:parserContext.GetUniformDelegates()) {
			auto bindingName = d.first;
			auto& delegate = *d.second;
			sequencerInterface.BindConstantBuffer(cbSlot++, {bindingName, delegate.GetLayout()});
			sequencerCbvs.push_back(delegate.WriteBuffer(parserContext, nullptr));
		}
	}

	void Drawable::DrawFunctionContext::ApplyUniforms(const UniformsStream& stream) const
	{
		_boundUniforms->Apply(*_metalContext, 3, stream);
	}

	uint64_t Drawable::DrawFunctionContext::UniformBindingBitField() const
	{
		return _boundUniforms->_boundUniformBufferSlots[3];
	}

	void Drawable::DrawFunctionContext::Draw(unsigned vertexCount, unsigned startVertexLocation) const
	{
		_metalContext->Draw(*_pipeline, vertexCount, startVertexLocation);
	}

	void Drawable::DrawFunctionContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation) const
	{
		_metalContext->DrawIndexed(*_pipeline, indexCount, startIndexLocation, baseVertexLocation);
	}

	void Drawable::DrawFunctionContext::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation) const
	{
		_metalContext->DrawInstances(*_pipeline, vertexCount, instanceCount, startVertexLocation);
	}

	void Drawable::DrawFunctionContext::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation, unsigned baseVertexLocation) const
	{
		_metalContext->DrawIndexedInstances(*_pipeline, indexCount, instanceCount, startIndexLocation, baseVertexLocation);
	}

	void Drawable::DrawFunctionContext::DrawAuto() const
	{
		_metalContext->DrawAuto(*_pipeline);
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
