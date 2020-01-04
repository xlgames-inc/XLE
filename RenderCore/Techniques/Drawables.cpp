// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Drawables.h"
#include "DrawableDelegates.h"
#include "DrawableMaterial.h"
#include "Techniques.h"
#include "TechniqueUtils.h"
#include "ParsingContext.h"
#include "RenderStateResolver.h"
#include "TechniqueMaterial.h"
#include "CompiledRenderStateSet.h"
#include "PipelineAccelerator.h"
#include "DescriptorSetAccelerator.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
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

	void Draw(
		IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		unsigned techniqueIndex,
		const SequencerTechnique& sequencerTechnique,
		const Drawable& drawable)
	{
		auto& metalContext = *Metal::DeviceContext::Get(context);

		const ParameterBox* shaderSelectors[Techniques::ShaderSelectors::Source::Max] = {nullptr, nullptr, nullptr, nullptr};
		shaderSelectors[Techniques::ShaderSelectors::Source::Runtime] = &parserContext.GetSubframeShaderSelectors();
		shaderSelectors[Techniques::ShaderSelectors::Source::GlobalEnvironment] = &parserContext.GetTechniqueContext()._globalEnvironmentState;

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

		// this part would normally be a loop -- 
		{
			auto* pipeline = drawable._pipeline->TryGetPipeline(sequencerTechnique._sequencerConfigId);
			if (!pipeline)
				return;

			if (!drawable._descriptorSet)
				return;

			//////////////////////////////////////////////////////////////////////////////

			Metal::BoundInputLayout::SlotBinding slotBinding[4];
			VertexBufferView vbv[4];
			for (unsigned c=0; c<drawable._geo->_vertexStreamCount; ++c) {
				auto& stream = drawable._geo->_vertexStreams[c];
				vbv[c]._resource = stream._resource.get();
				vbv[c]._offset = stream._vbOffset;
				slotBinding[c]._elements = MakeIteratorRange(stream._vertexElements);
				slotBinding[c]._instanceStepDataRate = stream._instanceStepDataRate;
			}

			Metal::BoundInputLayout inputLayout { MakeIteratorRange(slotBinding, &slotBinding[drawable._geo->_vertexStreamCount]), pipeline->GetShaderProgram() };
			inputLayout.Apply(metalContext, MakeIteratorRange(vbv));

			if (drawable._geo->_ib)
				metalContext.Bind(
					Metal::AsResource(*drawable._geo->_ib.get()),
					drawable._geo->_ibFormat);

			//////////////////////////////////////////////////////////////////////////////

			Metal::BoundUniforms boundUniforms{
				*pipeline,
				Metal::PipelineLayoutConfig{},
				sequencerInterface,
				drawable._descriptorSet->_usi,	// mat stream
				UniformsStreamInterface{},		// geo stream
				drawable._uniformsInterface ? *drawable._uniformsInterface : UniformsStreamInterface{}};

			boundUniforms.Apply(
				metalContext, 0, 
				UniformsStream {
					MakeIteratorRange(sequencerCbvs),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSrvs)),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSamplerStates))});
			drawable._descriptorSet->Apply(metalContext, boundUniforms, 1);

			//////////////////////////////////////////////////////////////////////////////

			drawable._drawFn(
				parserContext, 
				Drawable::DrawFunctionContext { &metalContext, pipeline, &boundUniforms },
				drawable);
		}
	}

	void Drawable::DrawFunctionContext::ApplyUniforms(const UniformsStream& stream) const
	{
		_boundUniforms->Apply(*_metalContext, 3, stream);
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

}}
