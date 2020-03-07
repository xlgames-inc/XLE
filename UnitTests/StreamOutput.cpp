// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../SceneEngine/MetalStubs.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/Metal/Resource.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/BufferView.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IArtifact.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
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

	static const char vsText[] = R"(
		float4 main(float4 input : INPUT) : SV_Position { return input; }
	)";
	static const char gsText[] = R"(
		struct GSOutput
		{
			float4 gsOut : POINT0;
		};
		struct VSOUT
		{
			float4 vsOut : SV_Position;
		};

		[maxvertexcount(1)]
			void main(triangle VSOUT input[3], inout PointStream<GSOutput> outputStream)
		{
			GSOutput result;
			result.gsOut.x = max(max(input[0].vsOut.x, input[1].vsOut.x), input[2].vsOut.x);
			result.gsOut.y = max(max(input[0].vsOut.y, input[1].vsOut.y), input[2].vsOut.y);
			result.gsOut.z = max(max(input[0].vsOut.z, input[1].vsOut.z), input[2].vsOut.z);
			result.gsOut.w = max(max(input[0].vsOut.w, input[1].vsOut.w), input[2].vsOut.w);
			outputStream.Append(result);
		}
	)";

    TEST_CLASS(StreamOutput)
	{
	public:
		std::unique_ptr<MetalTestHelper> _testHelper;
		::Assets::MountingTree::MountID _mountedFS;

		StreamOutput()
		{
			_testHelper = std::make_unique<MetalTestHelper>(RenderCore::Techniques::GetTargetAPI());
			_mountedFS = ::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
		}

		~StreamOutput()
		{
			::Assets::MainFileSystem::GetMountingTree()->Unmount(_mountedFS);
			_testHelper.reset();
		}

		static std::string BuildSODefinesString(IteratorRange<const RenderCore::InputElementDesc*> desc)
		{
			std::stringstream str;
			str << "SO_OFFSETS=";
			unsigned rollingOffset = 0;
			for (const auto&e:desc) {
				assert(e._alignedByteOffset == ~0x0u);		// expecting to use packed sequential ordering
				if (rollingOffset!=0) str << ",";
				str << Hash64(e._semanticName) + e._semanticIndex << "," << rollingOffset;
				rollingOffset += BitsPerPixel(e._nativeFormat) / 8;
			}
			return str.str();
		}

		TEST_METHOD(SimpleStreamOutput)
		{
			using namespace RenderCore;
			auto threadContext = _testHelper->_device->GetImmediateContext();
			auto& metalContext = *Metal::DeviceContext::Get(*threadContext);

			auto soBuffer = _testHelper->_device->CreateResource(
				CreateDesc(
					BindFlag::StreamOutput | BindFlag::TransferSrc, 0, GPUAccess::Read | GPUAccess::Write,
					LinearBufferDesc::Create(1024, 1024),
					"soBuffer"));

			auto cpuAccessBuffer = _testHelper->_device->CreateResource(
				CreateDesc(
					BindFlag::TransferDst, CPUAccess::Read, 0,
					LinearBufferDesc::Create(1024, 1024),
					"cpuAccessBuffer"));

			const InputElementDesc soEles[] = { InputElementDesc("POINT", 0, Format::R32G32B32A32_FLOAT) };
			const unsigned soStrides[] = { (unsigned)sizeof(Float4) };
			
			auto vs = _testHelper->MakeShader(vsText, "vs_5_0");
			auto gs = _testHelper->MakeShader(gsText, "gs_5_0", BuildSODefinesString(MakeIteratorRange(soEles)));
			Metal::ShaderProgram shaderProgram(
				Metal::GetObjectFactory(), 
				vs, gs, {},
				StreamOutputInitializers { MakeIteratorRange(soEles), MakeIteratorRange(soStrides) });

			Float4 inputVertices[] = {
				Float4{ 1.0f, 2.0f, 3.0f, 4.0f },
				Float4{ 5.0f, 6.0f, 7.0f, 8.0f },
				Float4{ 11.0f, 12.0f, 13.0f, 14.0f },

				Float4{ 15.0f, 16.0f, 17.0f, 18.0f },
				Float4{ 21.0f, 22.0f, 23.0f, 24.0f },
				Float4{ 25.0f, 26.0f, 27.0f, 28.0f },

				Float4{ 31.0f, 32.0f, 33.0f, 34.0f },
				Float4{ 35.0f, 36.0f, 37.0f, 38.0f },
				Float4{ 41.0f, 42.0f, 43.0f, 44.0f }
			};

			auto vertexBuffer = _testHelper->_device->CreateResource(
				CreateDesc(
					BindFlag::VertexBuffer, 0, GPUAccess::Read,
					LinearBufferDesc::Create(1024, 1024),
					"vertexBuffer"),
				[inputVertices](SubResourceId) -> SubResourceInitData { return MakeIteratorRange(inputVertices); });
			InputElementDesc inputEle[] = { InputElementDesc{"INPUT", 0, Format::R32G32B32A32_FLOAT} };
			Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEle), shaderProgram);

			#if GFXAPI_TARGET == GFXAPI_VULKAN
				metalContext.BeginCommandList();
			#endif

			VertexBufferView vbv { vertexBuffer.get() };
			inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

			SceneEngine::MetalStubs::BindSO(metalContext, *soBuffer);

			Techniques::AttachmentPool dummyAttachmentPool;
			dummyAttachmentPool.Bind(FrameBufferProperties{256, 256});
			Techniques::FrameBufferPool frameBufferPool;
			Techniques::RenderPassInstance rpi {
				*threadContext,
				FrameBufferDesc::s_empty,
				frameBufferPool, dummyAttachmentPool };

			metalContext.Bind(Metal::ViewportDesc{ 0.f, 0.f, 256.f, 256.f });
			metalContext.Bind(shaderProgram);
			metalContext.Bind(Topology::TriangleList);
			metalContext.Draw(dimof(inputVertices));

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

			Metal::ResourceMap map { metalContext, *QueryInterfaceCast<RenderCore::Metal::Resource>(*cpuAccessBuffer), Metal::ResourceMap::Mode::Read };
			auto* readbackData = (Float4*)map.GetData().begin();
			size_t readbackDataSize = map.GetData().size();

			Assert::IsTrue(Equivalent(readbackData[0], Float4{11.f, 12.f, 13.f, 14.f}, 1e-6f));
			Assert::IsTrue(Equivalent(readbackData[1], Float4{25.f, 26.f, 27.f, 28.f}, 1e-6f));
			Assert::IsTrue(Equivalent(readbackData[2], Float4{41.f, 42.f, 43.f, 44.f}, 1e-6f));
			Assert::IsTrue(1024 == readbackDataSize);
		}
	};
}