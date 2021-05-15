// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingEngineTestHelper.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/LightingEngine/LightingEngine.h"
#include "../../../RenderCore/LightingEngine/LightingEngineApparatus.h"
#include "../../../RenderCore/LightingEngine/LightDesc.h"
#include "../../../RenderCore/LightingEngine/ForwardLightingDelegate.h"
#include "../../../RenderCore/LightingEngine/DeferredLightingDelegate.h"
#include "../../../RenderCore/Techniques/ParsingContext.h"
#include "../../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../../RenderCore/Techniques/CommonBindings.h"
#include "../../../RenderCore/Techniques/Techniques.h"
#include "../../../RenderCore/Techniques/RenderPass.h"
#include "../../../RenderCore/Techniques/PipelineCollection.h"
#include "../../../RenderCore/Assets/PredefinedPipelineLayout.h"
#include "../../../Math/Transformations.h"
#include "../../../Math/ProjectionMath.h"
#include "../../../Assets/IAsyncMarker.h"
#include "../../../Assets/Assets.h"
#include "../../../xleres/FileList.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{
	static RenderCore::LightingEngine::LightDesc CreateTestLight()
	{
		RenderCore::LightingEngine::LightDesc result;
		result._orientation = Identity<Float3x3>();
		result._position = Float3{0.f, 1.0f, 0.f};
		result._radii = Float2{1.0f, 1.0f};
		result._shape = RenderCore::LightingEngine::LightSourceShape::Directional;
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

	static RenderCore::Techniques::ParsingContext InitializeParsingContext(
		RenderCore::Techniques::TechniqueContext& techniqueContext,
		const RenderCore::ResourceDesc& targetDesc,
		const RenderCore::Techniques::CameraDesc& camera)
	{
		using namespace RenderCore;

		Techniques::PreregisteredAttachment preregisteredAttachments[] {
			Techniques::PreregisteredAttachment {
				Techniques::AttachmentSemantics::ColorLDR,
				targetDesc,
				Techniques::PreregisteredAttachment::State::Uninitialized
			}
		};
		FrameBufferProperties fbProps { targetDesc._textureDesc._width, targetDesc._textureDesc._height };

		Techniques::ParsingContext parsingContext{techniqueContext};
		parsingContext.GetProjectionDesc() = BuildProjectionDesc(camera, UInt2{targetDesc._textureDesc._width, targetDesc._textureDesc._height});
		
		auto& stitchingContext = parsingContext.GetFragmentStitchingContext();
		stitchingContext._workingProps = fbProps;
		for (const auto&a:preregisteredAttachments)
			stitchingContext.DefineAttachment(a._semantic, a._desc, a._state, a._layoutFlags);
		return parsingContext;
	}

	template<typename Type>
		static std::shared_ptr<Type> StallAndRequireReady(::Assets::AssetFuture<Type>& future)
	{
		future.StallWhilePending();
		INFO(::Assets::AsString(future.GetActualizationLog()));
		REQUIRE(future.GetAssetState() == ::Assets::AssetState::Ready);
		return future.Actualize();
	}

	TEST_CASE( "LightingEngine-ExecuteTechnique", "[rendercore_lighting_engine]" )
	{
		using namespace RenderCore;
		LightingEngineTestApparatus testApparatus;
		auto testHelper = testApparatus._metalTestHelper.get();

		auto targetDesc = CreateDesc(
			BindFlag::RenderTarget | BindFlag::TransferSrc, 0, GPUAccess::Write,
			TextureDesc::Plain2D(256, 256, RenderCore::Format::R8G8B8A8_UNORM),
			"temporary-out");
		
		auto threadContext = testHelper->_device->GetImmediateContext();
		UnitTestFBHelper fbHelper(*testHelper->_device, *threadContext, targetDesc);

		auto drawableWriter = CreateSphereDrawablesWriter(*testHelper, *testApparatus._pipelineAcceleratorPool);

		RenderCore::LightingEngine::SceneLightingDesc lightingDesc;
		lightingDesc._lights.push_back(CreateTestLight());
		lightingDesc._shadowProjections.push_back(CreateTestShadowProjection());

		RenderCore::Techniques::CameraDesc camera;
		camera._cameraToWorld = MakeCameraToWorld(Float3{1.0f, 0.0f, 0.0f}, Float3{0.0f, 1.0f, 0.0f}, Float3{-3.33f, 0.f, 0.f});
		
		auto parsingContext = InitializeParsingContext(*testApparatus._techniqueContext, targetDesc, camera);
		parsingContext.GetTechniqueContext()._attachmentPool->Bind(Techniques::AttachmentSemantics::ColorLDR, fbHelper.GetMainTarget());

		testHelper->BeginFrameCapture();

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		SECTION("Forward lighting")
		{
			auto& stitchingContext = parsingContext.GetFragmentStitchingContext();
			auto lightingTechniqueFuture = LightingEngine::CreateForwardLightingTechnique(
				testHelper->_device,
				testApparatus._pipelineAcceleratorPool, testApparatus._techDelBox,
				stitchingContext.GetPreregisteredAttachments(), stitchingContext._workingProps);
			auto lightingTechnique = StallAndRequireReady(*lightingTechniqueFuture);

			// stall until all resources are ready
			{
				RenderCore::LightingEngine::LightingTechniqueInstance prepareLightingIterator(*testApparatus._pipelineAcceleratorPool, *lightingTechnique);
				ParseScene(prepareLightingIterator, *drawableWriter);
				auto prepareMarker = prepareLightingIterator.GetResourcePreparationMarker();
				if (prepareMarker) {
					prepareMarker->StallWhilePending();
					REQUIRE(prepareMarker->GetAssetState() == ::Assets::AssetState::Ready);
				}
			}

			{
				RenderCore::LightingEngine::LightingTechniqueInstance lightingIterator(
					*threadContext, parsingContext, *testApparatus._pipelineAcceleratorPool, lightingDesc, *lightingTechnique);
				ParseScene(lightingIterator, *drawableWriter);
			}

			fbHelper.SaveImage(*threadContext, "forward-lighting-output");
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		SECTION("Deferred lighting")
		{
			auto pipelineLayoutFileFuture = ::Assets::MakeAsset<RenderCore::Assets::PredefinedPipelineLayoutFile>(LIGHTING_OPERATOR_PIPELINE);
			auto pipelineLayoutFile = StallAndRequireReady(*pipelineLayoutFileFuture);

			const std::string pipelineLayoutName = "LightingOperator";
			auto i = pipelineLayoutFile->_pipelineLayouts.find(pipelineLayoutName);
			if (i == pipelineLayoutFile->_pipelineLayouts.end())
				Throw(std::runtime_error("Did not find pipeline layout with the name " + pipelineLayoutName + " in the given pipeline layout file"));
			auto pipelineInit = i->second->MakePipelineLayoutInitializer(testHelper->_shaderCompiler->GetShaderLanguage());
			auto lightingOperatorLayout = testHelper->_device->CreatePipelineLayout(pipelineInit);

			auto pipelineCollection = std::make_shared<Techniques::GraphicsPipelineCollection>(testHelper->_device, lightingOperatorLayout);

			LightingEngine::LightResolveOperatorDesc resolveOperators[] {
				LightingEngine::LightResolveOperatorDesc{}
			};
			LightingEngine::ShadowGeneratorDesc shadowGenerator[] {
				LightingEngine::ShadowGeneratorDesc{}
			};

			auto& stitchingContext = parsingContext.GetFragmentStitchingContext();
			auto lightingTechniqueFuture = LightingEngine::CreateDeferredLightingTechnique(
				testHelper->_device,
				testApparatus._pipelineAcceleratorPool, testApparatus._techDelBox, pipelineCollection,
				MakeIteratorRange(resolveOperators), MakeIteratorRange(shadowGenerator), 
				stitchingContext.GetPreregisteredAttachments(), stitchingContext._workingProps);
			auto lightingTechnique = StallAndRequireReady(*lightingTechniqueFuture);

			testApparatus._bufferUploads->Update(*threadContext);
			Threading::Sleep(16);
			testApparatus._bufferUploads->Update(*threadContext);

			// stall until all resources are ready
			{
				RenderCore::LightingEngine::LightingTechniqueInstance prepareLightingIterator(*testApparatus._pipelineAcceleratorPool, *lightingTechnique);
				ParseScene(prepareLightingIterator, *drawableWriter);
				auto prepareMarker = prepareLightingIterator.GetResourcePreparationMarker();
				if (prepareMarker) {
					prepareMarker->StallWhilePending();
					REQUIRE(prepareMarker->GetAssetState() == ::Assets::AssetState::Ready);
				}
			}

			{
				RenderCore::LightingEngine::LightingTechniqueInstance lightingIterator(
					*threadContext, parsingContext, *testApparatus._pipelineAcceleratorPool, lightingDesc, *lightingTechnique);
				ParseScene(lightingIterator, *drawableWriter);
			}

			fbHelper.SaveImage(*threadContext, "deferred-lighting-output");
		}

		testHelper->EndFrameCapture();
	}
}
