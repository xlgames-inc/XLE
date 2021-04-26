// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/LightingEngine/LightingEngine.h"
#include "../../../RenderCore/LightingEngine/SharedTechniqueDelegates.h"
#include "../../../RenderCore/LightingEngine/LightDesc.h"
#include "../../../RenderCore/Techniques/RenderPass.h"
#include "../../../RenderCore/Techniques/Drawables.h"
#include "../../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../../RenderCore/Techniques/Services.h"
#include "../../../RenderCore/Techniques/ParsingContext.h"
#include "../../../RenderCore/Techniques/CommonResources.h"
#include "../../../RenderCore/Techniques/SystemUniformsDelegate.h"
#include "../../../RenderCore/Techniques/Techniques.h"
#include "../../../RenderCore/MinimalShaderSource.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetSetManager.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/CompilerLibrary.h"
#include "../../../Assets/Assets.h"
#include "../../../Math/Transformations.h"
#include "../../../Math/ProjectionMath.h"
#include "../../../ConsoleRig/Console.h"
#include "../../../OSServices/Log.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../Tools/ToolsRig/VisualisationGeo.h"
#include "../Metal/MetalTestHelper.h"
#include "thousandeyes/futures/then.h"
#include "thousandeyes/futures/DefaultExecutor.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <chrono>
#include <regex>

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{
	static RenderCore::Techniques::DescriptorSetLayoutAndBinding MakeMaterialDescriptorSetLayout()
	{
		const char* unitTestsMaterialDescSet = R"(
			UniformBuffer BasicMaterialConstants
			{
				float3  MaterialDiffuse = {1,1,1};
				float   Opacity = 1;
				float3  MaterialSpecular = {1,1,1};
				float   AlphaThreshold = .5f;

				float   RoughnessMin = 0.1f;
				float   RoughnessMax = 0.6f;
				float   SpecularMin = 0.0f;
				float   SpecularMax = 0.5f;
				float   MetalMin = 0.f;
				float   MetalMax = 1.f;
			};
			UniformBuffer cb1;						// 1
			UniformBuffer cb2;						// 2

			SampledTexture tex0;					// 3
			SampledTexture tex1;					// 4
			SampledTexture tex2;					// 5
			SampledTexture tex3;					// 6
			SampledTexture tex4;					// 7
			SampledTexture tex5;					// 8
			SampledTexture tex6;					// 9
			SampledTexture tex7;					// 10

			UnorderedAccessBuffer uab0;				// 11
			Sampler sampler0;						// 12
		)";

		auto layout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>(
			unitTestsMaterialDescSet, ::Assets::DirectorySearchRules{}, ::Assets::DependencyValidation{}
		);
		return RenderCore::Techniques::DescriptorSetLayoutAndBinding { layout, 1 };
	}

	static RenderCore::Techniques::DescriptorSetLayoutAndBinding MakeSequencerDescriptorSetLayout()
	{
		auto layout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>();
		layout->_slots = {
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"GlobalTransform"}, RenderCore::DescriptorType::UniformBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"LocalTransform"}, RenderCore::DescriptorType::UniformBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::UniformBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"BasicLightingEnvironment"}, RenderCore::DescriptorType::UniformBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"ArbitraryShadowProjection"}, RenderCore::DescriptorType::UniformBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::UniformBuffer },
			
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::SampledTexture },

			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Sampler },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Sampler },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Sampler },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Sampler }
		};

		return RenderCore::Techniques::DescriptorSetLayoutAndBinding { layout, 0 };
	}

	static RenderCore::LightingEngine::LightDesc CreateTestLight()
	{
		RenderCore::LightingEngine::LightDesc result;
		result._orientation = Identity<Float3x3>();
		result._position = Float3{0.f, 1.0f, 0.f};
		result._radii = Float2{1.0f, 1.0f};
		result._shape = RenderCore::LightingEngine::LightDesc::Shape::Directional;
		result._diffuseColor = Float3{1.0f, 1.0f, 1.0f};
		result._radii = Float2{100.f, 100.f};
		return result;
	}

	static RenderCore::LightingEngine::ShadowProjectionDesc CreateTestShadowProjection()
	{
		RenderCore::LightingEngine::ShadowProjectionDesc result;
		result._projections._normalProjCount = 1;
		result._projections._fullProj[0]._projectionMatrix = OrthogonalProjection(-1.f, -1.f, 1.f, 1.f, 0.01f, 100.f, GeometricCoordinateSpace::LeftHanded, RenderCore::Techniques::GetDefaultClipSpaceType());
		result._projections._fullProj[0]._viewMatrix = MakeCameraToWorld(Float3{0.f, -1.0f, 0.f}, Float3{0.f, 0.0f, 1.f}, Float3{0.f, 1.0f, 0.f}); 
		result._worldSpaceResolveBias = 0.f;
        result._tanBlurAngle = 0.00436f;
        result._minBlurSearch = 0.5f;
        result._maxBlurSearch = 25.f;
		result._lightId = 0;
		return result;
	}

	class DrawableWriter
	{
	public:
		std::shared_ptr<RenderCore::Techniques::DrawableGeo> _geo;
		std::shared_ptr<RenderCore::Techniques::PipelineAccelerator> _pipelineAccelerator;
		std::shared_ptr<RenderCore::Techniques::DescriptorSetAccelerator> _descriptorSetAccelerator;
		size_t _vertexCount;

		void WriteDrawable(RenderCore::Techniques::DrawablesPacket& pkt)
		{
			struct CustomDrawable : public RenderCore::Techniques::Drawable { unsigned _vertexCount; };
			auto* drawable = pkt._drawables.Allocate<CustomDrawable>();
			drawable->_pipeline = _pipelineAccelerator;
			drawable->_descriptorSet = _descriptorSetAccelerator;
			drawable->_geo = _geo;
			drawable->_vertexCount = _vertexCount;
			drawable->_drawFn = [](RenderCore::Techniques::ParsingContext&, const RenderCore::Techniques::ExecuteDrawableContext& drawFnContext, const RenderCore::Techniques::Drawable& drawable)
				{
					drawFnContext.Draw(((CustomDrawable&)drawable)._vertexCount);
				};
		}

		DrawableWriter(MetalTestHelper& testHelper, RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAcceleratorPool)
		{
			auto sphereGeo = ToolsRig::BuildGeodesicSphere();
			auto sphereVb = testHelper.CreateVB(sphereGeo);
			_geo = std::make_shared<RenderCore::Techniques::DrawableGeo>();
			_geo->_vertexStreams[0]._resource = sphereVb;
			_geo->_vertexStreamCount = 1;
			_vertexCount = sphereGeo.size();

			_pipelineAccelerator = pipelineAcceleratorPool.CreatePipelineAccelerator(
				nullptr,
				ParameterBox {},
				ToolsRig::Vertex3D_InputLayout,
				RenderCore::Topology::TriangleList,
				RenderCore::Assets::RenderStateSet{});

			_descriptorSetAccelerator = pipelineAcceleratorPool.CreateDescriptorSetAccelerator(
				nullptr,
				{}, {}, {});
		}
	};

	TEST_CASE( "LightingEngine-CompileTechnique", "[rendercore_lighting_engine]" )
	{
		using namespace RenderCore;

		using namespace RenderCore;
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto testHelper = MakeTestHelper();

		Verbose.SetConfiguration(OSServices::MessageTargetConfiguration{});

		auto techniqueServices = ConsoleRig::MakeAttachablePtr<Techniques::Services>(testHelper->_device);

		auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
		thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = ShaderSourceParser::RegisterShaderSelectorFilteringCompiler(compilers);
		auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(testHelper->_shaderSource, compilers);
		auto shaderCompiler2Registration = RenderCore::Techniques::RegisterInstantiateShaderGraphCompiler(testHelper->_shaderSource, compilers);

		auto pipelineAcceleratorPool = Techniques::CreatePipelineAcceleratorPool(
			testHelper->_device, testHelper->_pipelineLayout, Techniques::PipelineAcceleratorPoolFlags::RecordDescriptorSetBindingInfo,
			MakeMaterialDescriptorSetLayout(),
			MakeSequencerDescriptorSetLayout());

		auto techniqueSharedResources = RenderCore::Techniques::CreateTechniqueSharedResources(*testHelper->_device);
		auto techDelBox = std::make_shared<LightingEngine::SharedTechniqueDelegateBox>(techniqueSharedResources);

		auto lightingTechnique = LightingEngine::CreateForwardLightingTechnique(testHelper->_device, pipelineAcceleratorPool, techDelBox);
		(void)lightingTechnique;

		auto threadContext = testHelper->_device->GetImmediateContext();
		auto targetDesc = CreateDesc(
			BindFlag::RenderTarget | BindFlag::TransferSrc, 0, GPUAccess::Write,
			TextureDesc::Plain2D(256, 256, RenderCore::Format::R8G8B8A8_UNORM),
			"temporary-out");
		UnitTestFBHelper fbHelper(*testHelper->_device, *threadContext, targetDesc);

		auto techniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
		techniqueContext->_drawablesSharedResources = RenderCore::Techniques::CreateDrawablesSharedResources();
		auto commonResources = std::make_shared<RenderCore::Techniques::CommonResourceBox>(*testHelper->_device);
		techniqueContext->_systemUniformsDelegate = std::make_shared<RenderCore::Techniques::SystemUniformsDelegate>(*testHelper->_device, *commonResources);
		techniqueContext->_attachmentPool = std::make_shared<Techniques::AttachmentPool>(testHelper->_device);
		techniqueContext->_frameBufferPool = std::make_shared<Techniques::FrameBufferPool>();

		DrawableWriter drawableWriter(*testHelper, *pipelineAcceleratorPool);

		testHelper->BeginFrameCapture();

		{
			RenderCore::LightingEngine::SceneLightingDesc lightingDesc;
			lightingDesc._lights.push_back(CreateTestLight());
			lightingDesc._shadowProjections.push_back(CreateTestShadowProjection());

			RenderCore::Techniques::CameraDesc camera;
			camera._cameraToWorld = MakeCameraToWorld(Float3{1.0f, 0.0f, 0.0f}, Float3{0.0f, 1.0f, 0.0f}, Float3{-5.0f, 0.f, 0.f});
			
			Techniques::ParsingContext parsingContext{*techniqueContext};
			parsingContext.GetProjectionDesc() = BuildProjectionDesc(camera, UInt2{targetDesc._textureDesc._width, targetDesc._textureDesc._height});
			RenderCore::LightingEngine::LightingTechniqueInstance lightingIterator(
				*threadContext, parsingContext, *pipelineAcceleratorPool, lightingDesc, *lightingTechnique);

			for (;;) {
				auto next = lightingIterator.GetNextStep();
				if (next._type == LightingEngine::StepType::None || next._type == LightingEngine::StepType::Abort) break;
				assert(next._type == LightingEngine::StepType::ParseScene);
				assert(next._pkt);
				drawableWriter.WriteDrawable(*next._pkt);
			}
		}

		testHelper->EndFrameCapture();
	}
}
