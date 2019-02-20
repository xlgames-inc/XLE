// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AnimationVisualization.h"
#include "OverlayContext.h"
#include "../RenderCore/Assets/SkeletonScaffoldInternal.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Types.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"

namespace RenderOverlays
{
	using namespace RenderCore;

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

		std::shared_ptr<Font> _font;

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

			_font = RenderOverlays::GetX2Font("Vera", 12);
		}
	};

	void    RenderSkeleton(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext, 
		const RenderCore::Assets::SkeletonMachine& skeleton,
		const RenderCore::Assets::TransformationParameterSet& params,
		const Float4x4& localToWorld, 
		bool drawBoneNames)
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
		skeleton.GenerateOutputTransforms(MakeIteratorRange(outputMatrices), &params);

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

		if (drawBoneNames) {
			RenderOverlays::ImmediateOverlayContext overlayContext(context);
            overlayContext.CaptureState();

			auto contextStateDesc = context.GetStateDesc();

			for (unsigned idx=0; idx<outputMatrices.size(); ++idx) {
				auto o = outputMatrices[idx];
				auto hposition = worldToProjection * Expand(TransformPoint(localToWorld, ExtractTranslation(o)), 1.0f);
				if (	hposition[0] < -hposition[3] || hposition[0] > hposition[3]
					||	hposition[1] < -hposition[3] || hposition[1] > hposition[3]
					||	hposition[2] < -hposition[3] || hposition[2] > hposition[3])
					continue;

				Float2 viewportSpace { hposition[0] / hposition[3], hposition[1] / hposition[3] };
				Float2 screenSpace {
					(viewportSpace[0] * 0.5f + 0.5f) * contextStateDesc._viewportDimensions[0],
					(viewportSpace[1] * -0.5f + 0.5f) * contextStateDesc._viewportDimensions[1] };

				auto name = std::to_string(idx);
				overlayContext.DrawText(
					std::make_tuple(
						Float3(screenSpace[0] - 100.f, screenSpace[1] - 20.f, 0.f), 
						Float3(screenSpace[0] + 100.f, screenSpace[1] + 20.f, 0.f)),
					box._font, {}, RenderOverlays::ColorB{0xffffffff}, RenderOverlays::TextAlignment::Center, 
					MakeStringSection(name));
			}
		}
    }

	void    RenderSkeleton(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext, 
		const RenderCore::Assets::SkeletonMachine& skeleton,
		const Float4x4& localToWorld,
		bool drawBoneNames)
	{
		RenderSkeleton(context, parserContext, skeleton, skeleton.GetDefaultParameters(), localToWorld, drawBoneNames);
	}
}
