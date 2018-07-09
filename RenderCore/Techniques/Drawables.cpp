// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Drawables.h"
#include "DrawableDelegates.h"
#include "Techniques.h"
#include "TechniqueUtils.h"
#include "ParsingContext.h"
#include "../UniformsStream.h"
#include "../BufferView.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/State.h"

namespace RenderCore { namespace Techniques
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	const ParameterBox* GetGeometrySelectors(const Techniques::DrawableGeo& geo)
	{
		return nullptr;
	}

	void Draw(
		IThreadContext& context,
        Techniques::ParsingContext& parserContext,
		unsigned techniqueIndex,
		const SequencerTechnique& sequencerTechnique,
		const ParameterBox* seqShaderSelectors,
		const Drawable& drawable)
	{
		auto& metalContext = *Metal::DeviceContext::Get(context);

		const ParameterBox* shaderSelectors[Techniques::ShaderSelectors::Source::Max] = {nullptr, nullptr, nullptr, nullptr};
		shaderSelectors[Techniques::ShaderSelectors::Source::Runtime] = seqShaderSelectors;
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
			shaderSelectors[Techniques::ShaderSelectors::Source::Material] = 
				sequencerTechnique._materialDelegate->GetShaderSelectors(drawable._material.get());

			ParameterBox geoSelectors;
			for (unsigned v=0; v<drawable._geo->_vertexStreamCount; ++v)
				SetGeoSelectors(geoSelectors, MakeIteratorRange(drawable._geo->_vertexStreams[v]._vertexElements));

			shaderSelectors[Techniques::ShaderSelectors::Source::Geometry] = &geoSelectors;

			auto* shaderProgram = sequencerTechnique._techniqueDelegate->GetShader(
				parserContext,
				MakeStringSection(drawable._techniqueConfig),
				shaderSelectors,
				techniqueIndex);

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

			Metal::BoundInputLayout inputLayout { MakeIteratorRange(slotBinding), *shaderProgram };
			inputLayout.Apply(metalContext, MakeIteratorRange(vbv));

			metalContext.Bind(
				Metal::AsResource(*drawable._geo->_ib.get()),
				drawable._geo->_ibFormat);

			//////////////////////////////////////////////////////////////////////////////

			Metal::BoundUniforms boundUniforms{
				*shaderProgram,
				Metal::PipelineLayoutConfig{},
				sequencerInterface,
				sequencerTechnique._materialDelegate->GetInterface(drawable._material.get()),
				UniformsStreamInterface{},	// geo stream,
				*drawable._uniformsInterface};

			boundUniforms.Apply(
				metalContext, 0, 
				UniformsStream {
					MakeIteratorRange(sequencerCbvs),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSrvs)),
					UniformsStream::MakeResources(MakeIteratorRange(sequencerSamplerStates))});
			sequencerTechnique._materialDelegate->ApplyUniforms(
				parserContext, metalContext, 
				boundUniforms, 1, drawable._material.get());

			//////////////////////////////////////////////////////////////////////////////

			drawable._drawFn(
				metalContext, parserContext, 
				drawable, boundUniforms, *shaderProgram);
		}
	}

	IMaterialContext::~IMaterialContext() {}

}}
