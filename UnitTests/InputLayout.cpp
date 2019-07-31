// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/AppleMetal/Device.h"
#include "../RenderCore/Techniques/RenderPassUtils.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/ResourceUtils.h"
#include "../Math/Vector.h"

#include "Metal/MTLCaptureManager.h"

namespace UnitTests
{
#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
	static const char vsText[] = R"(
		
		typedef struct
		{
			float4 position [[attribute(0)]];
			float4 color [[attribute(1)]];
		} AAPLVertex;

		typedef struct
		{
			float4 clipSpacePosition [[position]];
			float4 color;
		} RasterizerData;

		vertex RasterizerData vertexShader(AAPLVertex v_in [[stage_in]])
		{
			RasterizerData out;
			out.clipSpacePosition = float4(v_in.position.x / 50.f, v_in.position.y / 50.f, 0.0f, 1.0f);
			out.color = v_in.color;
			return out;
		}
	)";

	static const char psText[] = R"(
		typedef struct
		{
			float4 clipSpacePosition [[position]];
			float4 color;
		} RasterizerData;

		fragment float4 fragmentShader(RasterizerData in [[stage_in]])
		{
			return in.color;
		}
	)";
#endif

    TEST_CLASS(InputLayout)
	{
	public:
		std::unique_ptr<MetalTestHelper> _testHelper;

		TEST_CLASS_INITIALIZE(Startup)
		{
			_testHelper = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::AppleMetal);
			// ::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_testHelper.reset();
		}

		TEST_METHOD(BasicBinding)
		{
			using namespace RenderCore;
			auto& device = *_testHelper->_device;
			auto threadContext = device.GetImmediateContext();
			auto& metalContext = *Metal::DeviceContext::Get(*threadContext);

			auto vs = _testHelper->MakeShader(vsText, "vs_5_0");
			auto ps = _testHelper->MakeShader(psText, "ps_5_0");

			Metal::ShaderProgram shaderProgram(Metal::GetObjectFactory(), vs, ps);

            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");
			auto target = device.CreateResource(targetDesc);

            std::vector<uint8_t> data;
            const unsigned fixedColor = 0xff7f7f7fu;

            auto* metalThreadContext = (RenderCore::ImplAppleMetal::ThreadContext*)threadContext->QueryInterface(typeid(RenderCore::ImplAppleMetal::ThreadContext).hash_code());
            if (metalThreadContext) {
                metalThreadContext->BeginHeadlessFrame();
            }

            RenderCore::Techniques::AttachmentPool namedResources;
            RenderCore::Techniques::FrameBufferPool frameBufferPool;
			RenderCore::Techniques::TechniqueContext techniqueContext;
			RenderCore::Techniques::ParsingContext parsingContext(techniqueContext, &namedResources, &frameBufferPool);
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(*threadContext, target, parsingContext, LoadStore::Clear);

			////////////////////////////////////////////////////////////////////////
			{
                class Vertex
                {
                public:
                    Float4      _position;
                    unsigned    _color;
                };
				Vertex inputVertices[] = {
					Vertex { Float4 {  1.0f,  2.0f,  3.0f,  4.0f }, fixedColor },
					Vertex { Float4 {  5.0f,  6.0f,  7.0f,  8.0f }, fixedColor },
					Vertex { Float4 { 11.0f, 12.0f, 13.0f, 14.0f }, fixedColor },

					Vertex { Float4 { 15.0f, 16.0f, 17.0f, 18.0f }, fixedColor },
					Vertex { Float4 { 21.0f, 22.0f, 23.0f, 24.0f }, fixedColor },
					Vertex { Float4 { 25.0f, 26.0f, 27.0f, 28.0f }, fixedColor },

					Vertex { Float4 { 31.0f, 32.0f, 33.0f, 34.0f }, fixedColor },
					Vertex { Float4 { 35.0f, 36.0f, 37.0f, 38.0f }, fixedColor },
					Vertex { Float4 { 41.0f, 42.0f, 43.0f, 44.0f }, fixedColor }
				};

				auto vertexBuffer = device.CreateResource(
					CreateDesc(
						BindFlag::VertexBuffer, 0, GPUAccess::Read,
						LinearBufferDesc::Create(sizeof(inputVertices)),
						"vertexBuffer"),
					[inputVertices](SubResourceId) -> SubResourceInitData { return SubResourceInitData { MakeIteratorRange(inputVertices) }; });
				InputElementDesc inputEle[] = {
                    InputElementDesc { "position", 0, Format::R32G32B32A32_FLOAT },
                    InputElementDesc { "color", 0, Format::R8G8B8A8_UNORM }
                };
				Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEle), shaderProgram);

				VertexBufferView vbv { vertexBuffer.get() };
				inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

				metalContext.Bind(Metal::ViewportDesc{ 0.f, 0.f, (float)targetDesc._textureDesc._width, (float)targetDesc._textureDesc._height });
				metalContext.Bind(shaderProgram);
				metalContext.Bind(Topology::TriangleList);
				metalContext.Draw(dimof(inputVertices));				
			}
			////////////////////////////////////////////////////////////////////////

			rpi = {};

            data = target->ReadBack(*threadContext);

            if (metalThreadContext) {
                metalThreadContext->EndHeadlessFrame();
            }

            // We're expecting all pixels either to be black or fixedColor
            unsigned blackPixels = 0;
            unsigned coloredPixels = 0;
            unsigned otherPixels = 0;

            Assert::AreEqual(data.size(), (size_t)RenderCore::ByteCount(targetDesc));
            for (auto p:MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()))) {
                if (p == 0xff000000) ++blackPixels;
                else if (p == fixedColor) ++coloredPixels;
                else ++otherPixels;
            }

            Assert::AreEqual(otherPixels, 0u);
		}
	};
}
