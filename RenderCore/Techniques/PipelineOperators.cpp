// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PipelineOperators.h"
#include "CommonResources.h"
#include "RenderPass.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/InputLayout.h"
#include "../Metal/Shader.h"
#include "../Metal/ObjectFactory.h"
#include "../../Assets/Assets.h"
#include "../../xleres/FileList.h"

namespace RenderCore { namespace Techniques
{
	class FullViewportOperator : public IShaderOperator
	{
	public:
		std::shared_ptr<Metal::GraphicsPipeline> _pipeline;
		std::shared_ptr<ICompiledPipelineLayout> _pipelineLayout;
		Metal::BoundUniforms _boundUniforms;

		virtual void Draw(IThreadContext& threadContext, const UniformsStream& us) override
		{
			auto& metalContext = *Metal::DeviceContext::Get(threadContext);
			auto encoder = metalContext.BeginGraphicsEncoder(_pipelineLayout);
			_boundUniforms.ApplyLooseUniforms(metalContext, encoder, us);
			encoder.Draw(*_pipeline, 4);
		}

		static void ConstructToFuture(
			::Assets::AssetFuture<FullViewportOperator>& future,
			const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
			const FrameBufferTarget& fbTarget,
			StringSection<> pixelShader,
			const UniformsStreamInterface& usi)
		{
			auto shaderProgram = ::Assets::MakeAsset<Metal::ShaderProgram>(
				pipelineLayout,
				BASIC2D_VERTEX_HLSL ":fullscreen_viewfrustumvector",
				pixelShader);

			::Assets::WhenAll(shaderProgram).ThenConstructToFuture<FullViewportOperator>(
				future,
				[usi=usi, pipelineLayout, fbDesc=*fbTarget._fbDesc, subpassIdx=fbTarget._subpassIdx](std::shared_ptr<Metal::ShaderProgram> shader) {
					auto op = std::make_shared<FullViewportOperator>();
					op->_pipelineLayout = pipelineLayout;
					
					Metal::GraphicsPipelineBuilder pipelineBuilder;
					pipelineBuilder.Bind(*shader);
					pipelineBuilder.Bind(CommonResourceBox::s_dsDisable);
					AttachmentBlendDesc blends[] = { CommonResourceBox::s_abOpaque, CommonResourceBox::s_abOpaque };
					pipelineBuilder.Bind(MakeIteratorRange(blends));
					pipelineBuilder.Bind({}, Topology::TriangleStrip);
					pipelineBuilder.SetRenderPassConfiguration(fbDesc, subpassIdx);
					op->_pipeline = pipelineBuilder.CreatePipeline(Metal::GetObjectFactory());
					op->_boundUniforms = Metal::BoundUniforms{*op->_pipeline, usi};
					return op;
				});
		}
	};

	::Assets::FuturePtr<IShaderOperator> CreateFullViewportOperator(
		const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
		const RenderPassInstance& rpi,
		StringSection<> pixelShader,
		const UniformsStreamInterface& usi)
	{
		auto op = ::Assets::MakeAsset<FullViewportOperator>(
			pipelineLayout, 
			FrameBufferTarget{&rpi.GetFrameBufferDesc(), rpi.GetCurrentSubpassIndex()},
			pixelShader, usi);
		return *reinterpret_cast<::Assets::FuturePtr<IShaderOperator>*>(&op);
	}

	::Assets::FuturePtr<IShaderOperator> CreateFullViewportOperator(
		const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout,
		const FrameBufferTarget& fbTarget,
		StringSection<> pixelShader,
		const UniformsStreamInterface& usi)
	{
		auto op = ::Assets::MakeAsset<FullViewportOperator>(pipelineLayout, fbTarget, pixelShader, usi);
		return *reinterpret_cast<::Assets::FuturePtr<IShaderOperator>*>(&op);
	}

	IShaderOperator::~IShaderOperator() {}
}}
