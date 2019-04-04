// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../SceneEngine/MetalStubs.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/MinimalShaderSource.h"
#include "../RenderCore/ShaderService.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SceneEngine
{
	void BufferBarrier0(RenderCore::Metal::DeviceContext& context, RenderCore::Metal::Resource& buffer);
	void BufferBarrier1(RenderCore::Metal::DeviceContext& context, RenderCore::Metal::Resource& buffer);
}

namespace UnitTests
{
	T2(OutputType, InputType) OutputType* QueryInterfaceCast(InputType& input)
	{
		return (OutputType*)input.QueryInterface(typeid(OutputType).hash_code());
	}

    TEST_CLASS(StreamOutput)
	{
	public:
		static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		static ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;
		static std::shared_ptr<RenderCore::IDevice> _device;
		static std::unique_ptr<RenderCore::ShaderService> _shaderService;
		static std::shared_ptr<RenderCore::ShaderService::IShaderSource> _shaderSource;

		TEST_CLASS_INITIALIZE(Startup)
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
			_assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);

			_device = RenderCore::CreateDevice(RenderCore::Techniques::GetTargetAPI());
			RenderCore::Techniques::SetThreadContext(_device->GetImmediateContext());

			_shaderService = std::make_unique<RenderCore::ShaderService>();
	        _shaderSource = std::make_shared<RenderCore::MinimalShaderSource>(_device->CreateShaderCompiler());
			_shaderService->AddShaderSource(_shaderSource);
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			RenderCore::Techniques::SetThreadContext(nullptr);
			_shaderSource.reset();
			_shaderService.reset();
			_device.reset();
			_globalServices.reset();
		}

		static RenderCore::CompiledShaderByteCode MakeShader(StringSection<> shader, StringSection<> shaderModel, StringSection<> defines = {})
		{
			auto future = _shaderSource->CompileFromMemory(shader, "main", shaderModel, defines);
			auto state = future->GetAssetState();
			if (state == ::Assets::AssetState::Invalid) {
				std::stringstream str;
				str << "Shader (" << shader << ") failed to compile. Message follows:" << std::endl;
				str << ::Assets::AsString(::Assets::GetErrorMessage(*future));
				Throw(std::runtime_error(str.str()));
			}
			assert(!future->GetArtifacts().empty());
			return RenderCore::CompiledShaderByteCode {
				future->GetArtifacts()[0].second->GetBlob(),
				future->GetArtifacts()[0].second->GetDependencyValidation(),
				future->GetArtifacts()[0].second->GetRequestParameters()
			};
		}

		TEST_METHOD(SimpleStreamOutput)
		{
			using namespace RenderCore;
			auto threadContext = _device->GetImmediateContext();
			auto& metalContext = *Metal::DeviceContext::Get(*threadContext);

			auto soBuffer = _device->CreateResource(
				CreateDesc(
					BindFlag::StreamOutput | BindFlag::TransferSrc, 0, GPUAccess::Read | GPUAccess::Write,
					LinearBufferDesc::Create(1024, 1024),
					"ModelIntersectionBuffer"));

			auto cpuAccessBuffer = _device->CreateResource(
				CreateDesc(
					BindFlag::TransferDst, CPUAccess::Read, 0,
					LinearBufferDesc::Create(1024, 1024),
					"ModelIntersectionCopyBuffer"));

			static const char vsText[] = R"(
				float4 main(uint vIndex : SV_VertexID) : POSITION
				{
					float f = vIndex;
					return float4(f / 0.1f, f / 0.7f, 0.0f, 1.0f);
				}
			)";
			static const char gsText[] = R"(
				struct GSOutput
				{
					float4 gsOut : POINT0;
				};
				struct VSOutput
				{
					float4 vsOut : POSITION;
				};

				[maxvertexcount(1)]
					void main(triangle VSOutput input[3], inout PointStream<GSOutput> outputStream)
				{
					GSOutput result;
					result.gsOut.x = max(max(input[0].vsOut.x, input[1].vsOut.x), input[2].vsOut.x);
					result.gsOut.y = max(max(input[0].vsOut.y, input[1].vsOut.y), input[2].vsOut.y);
					result.gsOut.z = max(max(input[0].vsOut.z, input[1].vsOut.z), input[2].vsOut.z);
					result.gsOut.w = max(max(input[0].vsOut.w, input[1].vsOut.w), input[2].vsOut.w);
					outputStream.Append(result);
				}
			)";

			const InputElementDesc soEles[] = {
				InputElementDesc("POINT",               0, Format::R32G32B32A32_FLOAT)
			};
			const unsigned soStrides[] = { (unsigned)sizeof(Float4) };

			std::stringstream str;
			str << "SO_OFFSETS=";
			unsigned rollingOffset = 0;
			for (const auto&e:soEles) {
				assert(e._alignedByteOffset == ~0x0u);		// expecting to use packed sequential ordering
				if (rollingOffset!=0) str << ",";
				str << Hash64(e._semanticName) + e._semanticIndex << "," << rollingOffset;
				rollingOffset += BitsPerPixel(e._nativeFormat) / 8;
			}

			auto vs = MakeShader(vsText, "vs_5_0");
			auto gs = MakeShader(gsText, "gs_5_0", str.str());
			Metal::ShaderProgram shaderProgram(
				Metal::GetObjectFactory(), 
				vs, gs, {},
				StreamOutputInitializers { MakeIteratorRange(soEles), MakeIteratorRange(soStrides) });

			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.BeginCommandList();
			#endif

			SceneEngine::MetalStubs::BindSO(metalContext, *soBuffer);

			Techniques::AttachmentPool dummyAttachmentPool;
			Techniques::FrameBufferPool frameBufferPool;
			Techniques::RenderPassInstance rpi {
				*threadContext,
				FrameBufferDesc::s_empty,
				frameBufferPool, dummyAttachmentPool };

			metalContext.Bind(shaderProgram);
			metalContext.Bind(Topology::TriangleList);
			metalContext.Draw(33);

			rpi = {};

			SceneEngine::MetalStubs::UnbindSO(metalContext);

			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.QueueCommandList(*_device, Metal::DeviceContext::QueueCommandListFlags::Stall);
			#endif

			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.BeginCommandList();
				SceneEngine::BufferBarrier0(metalContext, *QueryInterfaceCast<RenderCore::Metal::Resource>(*soBuffer));
				Metal::Copy(metalContext, *QueryInterfaceCast<RenderCore::Metal::Resource>(*cpuAccessBuffer), *QueryInterfaceCast<RenderCore::Metal::Resource>(*soBuffer));
				SceneEngine::BufferBarrier1(metalContext, *QueryInterfaceCast<RenderCore::Metal::Resource>(*soBuffer));
				metalContext.QueueCommandList(*_device, Metal::DeviceContext::QueueCommandListFlags::Stall);
			#else
				Metal::Copy(metalContext, *QueryInterfaceCast<RenderCore::Metal::Resource>(*cpuAccessBuffer), *QueryInterfaceCast<RenderCore::Metal::Resource>(*soBuffer));
			#endif

			Metal::ResourceMap map { *_device, *QueryInterfaceCast<RenderCore::Metal::Resource>(*cpuAccessBuffer), {} };
			float* data = (float*)map.GetData();
			size_t dataSize = map.GetDataSize();

			(void)data;
			(void)dataSize;
		}

	};

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> StreamOutput::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> StreamOutput::_assetServices;
	std::shared_ptr<RenderCore::IDevice> StreamOutput::_device;
	std::unique_ptr<RenderCore::ShaderService> StreamOutput::_shaderService;
	std::shared_ptr<RenderCore::ShaderService::IShaderSource> StreamOutput::_shaderSource;
}