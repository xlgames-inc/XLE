// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VisualizationUtils.h"
#include "SkeletonScaffoldInternal.h"
#include "../IThreadContext.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/CommonBindings.h"
#include "../Techniques/CommonResources.h"
#include "../Techniques/Techniques.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Shader.h"
#include "../Format.h"
#include "../Types.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/ResourceBox.h"

namespace RenderCore { namespace Assets
{

	class Vertex_PC
	{
	public:
		Float3      _position;
		unsigned    _color;
	};

	static InputElementDesc s_vertexInputLayout[] = {
        InputElementDesc( "POSITION",   0, Format::R32G32B32_FLOAT ),
        InputElementDesc( "COLOR",      0, Format::R8G8B8A8_UNORM )
    };

	class SkeletonPreviewResourceBox
	{
	public:
		Metal::BoundUniforms _boundUniforms;
		Metal::BoundInputLayout _boundLayout;
		std::shared_ptr<Metal::ShaderProgram> _shader;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _shader->GetDependencyValidation(); }

		class Desc {};
		SkeletonPreviewResourceBox(const Desc&)
		{
			auto shaderFuture = ::Assets::MakeAsset<Metal::ShaderProgram>( 
				"xleres/forward/illum.vsh:main:" VS_DefShaderModel, 
				"xleres/forward/unlit.psh:main", "GEO_HAS_COLOUR=1");
			shaderFuture->StallWhilePending();
			_shader = shaderFuture->Actualize();

			UniformsStreamInterface usi;
			usi.BindConstantBuffer(0, {Techniques::ObjectCB::LocalTransform});

			_boundUniforms = Metal::BoundUniforms(
				*_shader,
				{},
				Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
				usi);

			_boundLayout = Metal::BoundInputLayout(MakeIteratorRange(s_vertexInputLayout), *_shader);
		}
	};

	void    RenderSkeleton(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext, 
		const SkeletonMachine& skeleton,
		const Float4x4& localToWorld)
    {
        auto& metalContext = *Metal::DeviceContext::Get(context);
		auto& box = ConsoleRig::FindCachedBoxDep2<SkeletonPreviewResourceBox>();
        metalContext.Bind(*box._shader);
		
        box._boundUniforms.Apply(metalContext, 0, parserContext.GetGlobalUniformsStream());

		auto transformPkt = Techniques::MakeLocalTransformPacket(
            Identity<Float4x4>(),		// we do the local-to-world transform on CPU side to make the screenspace axes code a little easier to manage
            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));
		ConstantBufferView cbvs[] = { transformPkt };
		box._boundUniforms.Apply(metalContext, 1, UniformsStream{MakeIteratorRange(cbvs)});

        std::vector<Vertex_PC> workingVertices;

		std::vector<Float4x4> outputMatrices(skeleton.GetOutputMatrixCount());
		skeleton.GenerateOutputTransforms(
            MakeIteratorRange(outputMatrices), &skeleton.GetDefaultParameters());

		// Draw a sprite for each output matrix location
		auto cameraRight = Normalize(ExtractRight_Cam(parserContext.GetProjectionDesc()._cameraToWorld));
		auto cameraUp = Normalize(ExtractUp_Cam(parserContext.GetProjectionDesc()._cameraToWorld));
		auto cameraForward = Normalize(ExtractForward_Cam(parserContext.GetProjectionDesc()._cameraToWorld));
		auto worldToProjection = parserContext.GetProjectionDesc()._worldToProjection;
		auto s = XlTan(.5f * parserContext.GetProjectionDesc()._verticalFov);
		workingVertices.reserve(outputMatrices.size() * 6);
		for (const auto&o:outputMatrices) {
			auto position = TransformPoint(localToWorld, ExtractTranslation(o));
			float scale = 0.01f * (worldToProjection * Expand(position, 1.0f))[3] * s;	// normalize scale for screen space

			Float3 corners[] = {
				position - scale * cameraUp - scale * cameraRight,
				position - scale * cameraUp + scale * cameraRight,
				position + scale * cameraUp - scale * cameraRight,
				position + scale * cameraUp + scale * cameraRight
			};

			workingVertices.push_back({corners[0], 0x00af00});
			workingVertices.push_back({corners[1], 0x00af00});
			workingVertices.push_back({corners[2], 0x00af00});
			workingVertices.push_back({corners[2], 0x00af00});
			workingVertices.push_back({corners[1], 0x00af00});
			workingVertices.push_back({corners[3], 0x00af00});
		}

		std::vector<uint32_t> parents(skeleton.GetOutputMatrixCount());
		skeleton.CalculateParentPointers(MakeIteratorRange(parents));
		workingVertices.reserve(workingVertices.size() + parents.size() * 6);
		for (unsigned c=0; c<parents.size(); ++c) {
			if (parents[c] >= outputMatrices.size()) continue;

			auto childPosition = TransformPoint(localToWorld, ExtractTranslation(outputMatrices[c]));
			auto parentPosition = TransformPoint(localToWorld, ExtractTranslation(outputMatrices[parents[c]]));

			auto axis = parentPosition - childPosition;
			auto tangent = Normalize(Cross(cameraForward, axis));
			float scaleC = 0.33f * 0.01f * (worldToProjection * Expand(childPosition, 1.0f))[3] * s;	// normalize scale for screen space
			float scaleP = 0.33f * 0.01f * (worldToProjection * Expand(parentPosition, 1.0f))[3] * s;	// normalize scale for screen space

			Float3 corners[] = {
				childPosition - scaleC * tangent,
				childPosition + scaleC * tangent,
				parentPosition - scaleP * tangent,
				parentPosition + scaleP * tangent
			};

			workingVertices.push_back({corners[0], 0x00af00});
			workingVertices.push_back({corners[1], 0x00af00});
			workingVertices.push_back({corners[2], 0x00af00});
			workingVertices.push_back({corners[2], 0x00af00});
			workingVertices.push_back({corners[1], 0x00af00});
			workingVertices.push_back({corners[3], 0x00af00});
		}

        auto vertexBuffer = Metal::MakeVertexBuffer(Metal::GetObjectFactory(), MakeIteratorRange(workingVertices));
		
		VertexBufferView vbs[] = { {&vertexBuffer} };
		box._boundLayout.Apply(metalContext, MakeIteratorRange(vbs));

        metalContext.Bind(Techniques::CommonResources()._dssDisable);
        metalContext.Bind(Techniques::CommonResources()._blendOpaque);
        metalContext.Bind(Topology::TriangleList);
        metalContext.Draw((unsigned)workingVertices.size());
    }
}}
