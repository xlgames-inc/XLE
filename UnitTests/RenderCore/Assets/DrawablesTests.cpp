// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../ReusableDataFiles.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/Techniques/Drawables.h"
#include "../../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../../RenderCore/Techniques/DrawableDelegates.h"
#include "../../../RenderCore/Techniques/Services.h"
#include "../../../RenderCore/Techniques/TextureLoaders.h"
#include "../../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../../RenderCore/Techniques/ParsingContext.h"
#include "../../../RenderCore/Techniques/SimpleModelRenderer.h"
#include "../../../RenderCore/Assets/MaterialCompiler.h"
#include "../../../RenderCore/MinimalShaderSource.h"
#include "../../../RenderCore/Format.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetSetManager.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/CompilerLibrary.h"
#include "../../../Assets/Assets.h"
#include "../../../Math/MathSerialization.h"
#include "../../../Math/Transformations.h"
#include "../../../Tools/ToolsRig/VisualisationGeo.h"
#include "../../../ConsoleRig/Console.h"
#include "../../../OSServices/Log.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../Utility/StringFormat.h"
#include "thousandeyes/futures/then.h"
#include "thousandeyes/futures/DefaultExecutor.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <regex>
#include <thread>
#include <chrono>

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{
	static const char* s_basicTexturingGraph = R"--(
		import templates = "xleres/Nodes/Templates.sh"
		import output = "xleres/Nodes/Output.sh"
		import texture = "xleres/Nodes/Texture.sh"
		import basic = "xleres/Nodes/Basic.sh"
		import materialParam = "xleres/Nodes/MaterialParam.sh"

		GBufferValues Bind_PerPixel(VSOUT geo) implements templates::PerPixel
		{
			captures MaterialUniforms = ( float3 Multiplier = "{1,1,1}", float Adder = "{0,0,0}", float2 CoordFreq = "{.1, .1}", Texture2D BoundTexture, SamplerState BoundSampler );
			node samplingCoords = basic::Multiply2(lhs:texture::GetPixelCoords(geo:geo).result, rhs:MaterialUniforms.CoordFreq);
			node loadFromTexture = texture::SampleWithSampler(
				inputTexture:MaterialUniforms.BoundTexture, 
				inputSampler:MaterialUniforms.BoundSampler,
				texCoord:samplingCoords.result);
			node multiply = basic::Multiply3(lhs:loadFromTexture.result, rhs:MaterialUniforms.Multiplier);
			node add = basic::Add3(lhs:multiply.result, rhs:MaterialUniforms.Adder);
			node mat = materialParam::CommonMaterialParam_Make(roughness:"1", specular:"1", metal:"1");
			return output::Output_PerPixel(
				diffuseAlbedo:add.result, 
				material:mat.result).result;
		}
	)--";

	static const char s_patchCollectionBasicTexturing[] = R"--(
		main=~
			ut-data/basicTexturingGraph.graph::Bind_PerPixel
	)--";

	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("basic.tech", ::Assets::AsBlob(s_basicTechniqueFile)),
		std::make_pair("basicTexturingGraph.graph", ::Assets::AsBlob(s_basicTexturingGraph))
	};

	static std::shared_ptr<RenderCore::Assets::ShaderPatchCollection> GetPatchCollectionFromText(StringSection<> techniqueText)
	{
		using namespace RenderCore;

		InputStreamFormatter<utf8> formattr { techniqueText.Cast<utf8>() };
		return std::make_shared<RenderCore::Assets::ShaderPatchCollection>(formattr, ::Assets::DirectorySearchRules{}, std::make_shared<::Assets::DependencyValidation>());
	}

	template<typename Type>
		static void StallForDescriptorSet(RenderCore::IThreadContext& threadContext, ::Assets::AssetFuture<Type>& descriptorSetFuture)
	{
		// If we're running buffer uploads in single thread mode, we need to pump it while
		// waiting for the descriptor set
		for (unsigned c=0; c<5; ++c) {
			RenderCore::Techniques::Services::GetBufferUploads().Update(threadContext);
			using namespace std::chrono_literals;
			descriptorSetFuture.StallWhilePending(16ms);
		}

		descriptorSetFuture.StallWhilePending();

		// hack -- 
		// we need to pump buffer uploads a bit to ensure the texture load gets completed
		for (unsigned c=0; c<5; ++c) {
			RenderCore::Techniques::Services::GetBufferUploads().Update(threadContext);
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(16ms);
		}
	}

	template<typename Type>
		void RequireReady(::Assets::AssetFuture<Type>& future)
	{
		INFO(::Assets::AsString(future.GetActualizationLog()));
		REQUIRE(future.GetAssetState() == ::Assets::AssetState::Ready);
	}

	static RenderCore::Techniques::GlobalTransformConstants MakeGlobalTransformConstants(const RenderCore::ResourceDesc& targetDesc)
	{
		using namespace RenderCore;
		Techniques::CameraDesc cameraDesc;
		Float3 fwd = Normalize(Float3 { 1.0f, -1.0f, 1.0f });
		cameraDesc._cameraToWorld = MakeCameraToWorld(fwd, Float3{0.f, 1.f, 0.f}, -5.0f * fwd);
		cameraDesc._projection = Techniques::CameraDesc::Projection::Orthogonal;
		cameraDesc._left = -2.0f; cameraDesc._top = -2.0f;
		cameraDesc._right = 2.0f; cameraDesc._bottom = 2.0f;
		auto projDesc = Techniques::BuildProjectionDesc(cameraDesc, UInt2{ targetDesc._textureDesc._width, targetDesc._textureDesc._height });
		return Techniques::BuildGlobalTransformConstants(projDesc);
	}

	class UnitTestGlobalUniforms : public RenderCore::Techniques::IShaderResourceDelegate
	{
	public:
		const RenderCore::UniformsStreamInterface& GetInterface() { return _interface; }

		void WriteImmediateData(RenderCore::Techniques::ParsingContext& context, const void* objectContext, unsigned idx, IteratorRange<void*> dst)
		{
			switch (idx) {
			case 0:
				*(RenderCore::Techniques::GlobalTransformConstants*)dst.begin() = MakeGlobalTransformConstants(_targetDesc);
				break;
			case 1:
				*(RenderCore::Techniques::LocalTransformConstants*)dst.begin() = RenderCore::Techniques::MakeLocalTransform(Identity<Float4x4>(), Zero<Float3>());
				break;
			}
		}

		size_t GetImmediateDataSize(RenderCore::Techniques::ParsingContext& context, const void* objectContext, unsigned idx)
		{
			switch (idx) {
			case 0:
				return sizeof(RenderCore::Techniques::GlobalTransformConstants);
			case 1:
				return sizeof(RenderCore::Techniques::LocalTransformConstants);
			default:
				return 0;
			}
		}

		UnitTestGlobalUniforms(const RenderCore::ResourceDesc& targetDesc) : _targetDesc(targetDesc)
		{
			_interface.BindImmediateData(0, Hash64("GlobalTransform"));
			_interface.BindImmediateData(1, Hash64("LocalTransform"));
		}

		RenderCore::UniformsStreamInterface _interface;
		RenderCore::ResourceDesc _targetDesc;
	};

	static RenderCore::Techniques::DescriptorSetLayoutAndBinding MakeMaterialDescriptorSetLayout()
	{
		auto layout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>();
		layout->_slots = {
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },

			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::UnorderedAccessBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Sampler }
		};

		return RenderCore::Techniques::DescriptorSetLayoutAndBinding { layout, 1 };
	}

	static RenderCore::Techniques::DescriptorSetLayoutAndBinding MakeSequencerDescriptorSetLayout()
	{
		auto layout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>();
		layout->_slots = {
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"GlobalTransform"}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{"LocalTransform"}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::ConstantBuffer },
			
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture },
			RenderCore::Assets::PredefinedDescriptorSetLayout::ConditionalDescriptorSlot { std::string{}, RenderCore::DescriptorType::Texture }
		};

		return RenderCore::Techniques::DescriptorSetLayoutAndBinding { layout, 0 };
	}

	static unsigned s_sphereVertexCount = 0;

	TEST_CASE( "Drawables-RenderImages", "[rendercore_techniques]" )
	{
		using namespace RenderCore;
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto utdatamnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData, s_defaultFilenameRules, ::Assets::FileSystemMemoryFlags::UseModuleModificationTime));
		auto testHelper = MakeTestHelper();

		Verbose.SetConfiguration(OSServices::MessageTargetConfiguration{});

		auto techniqueServices = ConsoleRig::MakeAttachablePtr<Techniques::Services>(testHelper->_device);
		techniqueServices->RegisterTextureLoader(std::regex(R"(.*\.[dD][dD][sS])"), Techniques::CreateDDSTextureLoader());
		techniqueServices->RegisterTextureLoader(std::regex(R"(.*)"), Techniques::CreateWICTextureLoader());

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

		auto cleanup = AutoCleanup([]() {
			::Assets::Services::GetAssetSets().Clear();
		});

		auto threadContext = testHelper->_device->GetImmediateContext();
		auto targetDesc = CreateDesc(
			BindFlag::RenderTarget | BindFlag::TransferSrc, 0, GPUAccess::Write,
			TextureDesc::Plain2D(256, 256, Format::R8G8B8A8_UNORM),
			"temporary-out");
		UnitTestFBHelper fbHelper(*testHelper->_device, *threadContext, targetDesc);
		
		/////////////////////////////////////////////////////////////////

		SECTION("Draw Basic Sphere")
		{
			auto sphereGeo = ToolsRig::BuildGeodesicSphere();
			auto sphereVb = testHelper->CreateVB(sphereGeo);
			auto drawableGeo = std::make_shared<Techniques::DrawableGeo>();
			drawableGeo->_vertexStreams[0]._resource = sphereVb;
			drawableGeo->_vertexStreamCount = 1;
			s_sphereVertexCount = sphereGeo.size();

			auto patches = GetPatchCollectionFromText(s_patchCollectionBasicTexturing);

			ParameterBox constantBindings;
			constantBindings.SetParameter("CoordFreq", Float2{.025f, .025f});
			ParameterBox resourceBindings;
			resourceBindings.SetParameter("BoundTexture", "xleres/DefaultResources/waternoise.png");
			std::vector<std::pair<uint64_t, SamplerDesc>> samplerBindings;
			samplerBindings.push_back(std::make_pair(Hash64("BoundSampler"), SamplerDesc{}));
			auto descriptorSetAccelerator = pipelineAcceleratorPool->CreateDescriptorSetAccelerator(
				patches,
				ParameterBox {}, constantBindings, resourceBindings, MakeIteratorRange(samplerBindings));

			auto techniqueSetFile = ::Assets::MakeAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");
			auto cfgId = pipelineAcceleratorPool->CreateSequencerConfig(
				Techniques::CreateTechniqueDelegate_Deferred(techniqueSetFile, Techniques::MakeTechniqueSharedResources(*testHelper->_device)),
				ParameterBox {},
				fbHelper.GetDesc());

			auto pipelineWithTexCoord = pipelineAcceleratorPool->CreatePipelineAccelerator(
				patches,
				ParameterBox {},
				ToolsRig::Vertex3D_InputLayout,
				Topology::TriangleList,
				RenderCore::Assets::RenderStateSet{});

			StallForDescriptorSet(*threadContext, *pipelineAcceleratorPool->GetDescriptorSet(*descriptorSetAccelerator));
			RequireReady(*pipelineAcceleratorPool->GetDescriptorSet(*descriptorSetAccelerator));
			pipelineAcceleratorPool->GetPipeline(*pipelineWithTexCoord, *cfgId)->StallWhilePending();
			RequireReady(*pipelineAcceleratorPool->GetPipeline(*pipelineWithTexCoord, *cfgId));

			struct CustomDrawable : public Techniques::Drawable { unsigned _vertexCount; };
			Techniques::DrawablesPacket pkt;
			auto* drawable = pkt._drawables.Allocate<CustomDrawable>();
			drawable->_pipeline = pipelineWithTexCoord;
			drawable->_descriptorSet = descriptorSetAccelerator;
			drawable->_geo = drawableGeo;
			drawable->_vertexCount = sphereGeo.size();
			drawable->_drawFn = [](Techniques::ParsingContext&, const Techniques::Drawable::DrawFunctionContext& drawFnContext, const Techniques::Drawable& drawable)
				{
					drawFnContext.Draw(((CustomDrawable&)drawable)._vertexCount);
				};

			auto globalDelegate = std::make_shared<UnitTestGlobalUniforms>(targetDesc);

			{
				auto rpi = fbHelper.BeginRenderPass(*threadContext);
				Techniques::TechniqueContext techContext;
				Techniques::ParsingContext parsingContext{techContext};
				parsingContext.AddShaderResourceDelegate(globalDelegate);
				Techniques::SequencerContext sequencerContext;
				sequencerContext._sequencerConfig = cfgId.get();
				Techniques::Draw(
					*threadContext,
					parsingContext, 
					*pipelineAcceleratorPool,
					sequencerContext,
					pkt);
			}
			fbHelper.SaveImage(*threadContext, "drawables-render-sphere");
		}

		SECTION("Draw model file")
		{
			testHelper->BeginFrameCapture();

			auto matRegistration = RenderCore::Assets::RegisterMaterialCompiler(compilers);
			auto discoveredCompilations = ::Assets::DiscoverCompileOperations(compilers, "../ColladaConversion/*.dll");
			REQUIRE(!discoveredCompilations.empty());

			auto techniqueSetFile = ::Assets::MakeAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");
			auto cfgId = pipelineAcceleratorPool->CreateSequencerConfig(
				Techniques::CreateTechniqueDelegate_Deferred(techniqueSetFile, Techniques::MakeTechniqueSharedResources(*testHelper->_device)),
				ParameterBox {},
				fbHelper.GetDesc());

			auto renderer = ::Assets::MakeAsset<Techniques::SimpleModelRenderer>(
				pipelineAcceleratorPool,
				"xleres/DefaultResources/materialsphere.dae",
				"xleres/DefaultResources/materialsphere.material");
			StallForDescriptorSet(*threadContext, *renderer);
			INFO(::Assets::AsString(renderer->GetActualizationLog()));
			REQUIRE(renderer->GetAssetState() == ::Assets::AssetState::Ready);

			Techniques::DrawablesPacket pkts[(unsigned)Techniques::BatchFilter::Max];
			Techniques::DrawablesPacket* drawablePktsPtrs[] = { &pkts[0], &pkts[1], &pkts[2], &pkts[3] };
			static_assert(dimof(pkts) == dimof(drawablePktsPtrs));
			renderer->Actualize()->BuildDrawables(MakeIteratorRange(drawablePktsPtrs));
				
			auto globalDelegate = std::make_shared<UnitTestGlobalUniforms>(targetDesc);

			for (unsigned c=0; c<32; ++c) {
				{
					auto rpi = fbHelper.BeginRenderPass(*threadContext);
					Techniques::TechniqueContext techContext;
					Techniques::ParsingContext parsingContext{techContext};
					parsingContext.AddShaderResourceDelegate(globalDelegate);
					Techniques::SequencerContext sequencerContext;
					sequencerContext._sequencerConfig = cfgId.get();
					
					auto* d = (Techniques::Drawable*)pkts[0]._drawables.begin().get();
					auto future = pipelineAcceleratorPool->GetPipeline(*d->_pipeline, *cfgId);
					future->StallWhilePending();
					INFO(::Assets::AsString(future->GetActualizationLog()));
					REQUIRE(future->GetAssetState() == ::Assets::AssetState::Ready);

					for (const auto&pkt:pkts)
						Techniques::Draw(
							*threadContext,
							parsingContext, 
							*pipelineAcceleratorPool,
							sequencerContext,
							pkt);
				}
				fbHelper.SaveImage(*threadContext, "drawables-render-model");
				std::this_thread::sleep_for(16ms);
			}

			for (const auto&compiler:discoveredCompilations)
				compilers.DeregisterCompiler(compiler);
			compilers.DeregisterCompiler(matRegistration._registrationId);

			testHelper->EndFrameCapture();
		}

		/////////////////////////////////////////////////////////////////

		pipelineAcceleratorPool.reset();
		compilers.DeregisterCompiler(shaderCompiler2Registration._registrationId);
		compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		compilers.DeregisterCompiler(filteringRegistration._registrationId);

		::Assets::MainFileSystem::GetMountingTree()->Unmount(utdatamnt);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
	}
}