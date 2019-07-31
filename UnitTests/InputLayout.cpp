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

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    T E S T   S H A D E R S

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
			out.clipSpacePosition = v_in.position;
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

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    T E S T   I N P U T   D A T A

    class VertexPC
    {
    public:
        Float4      _position;
        unsigned    _color;
    };

    static const unsigned fixedColors[] = { 0xff7f7f7fu, 0xff007f7fu, 0xff7f0000u };

    static VertexPC inputVerticesPC[] = {
        VertexPC { Float4 {  -0.25f, -0.5f,  0.0f,  1.0f }, fixedColors[0] },
        VertexPC { Float4 {  -0.33f,  0.1f,  0.0f,  1.0f }, fixedColors[0] },
        VertexPC { Float4 {   0.33f, -0.2f,  0.0f,  1.0f }, fixedColors[0] },

        VertexPC { Float4 { -0.1f, -0.7f, 0.0f, 1.0f }, fixedColors[1] },
        VertexPC { Float4 {  0.5f, -0.4f, 0.0f, 1.0f }, fixedColors[1] },
        VertexPC { Float4 {  0.8f,  0.8f, 0.0f, 1.0f }, fixedColors[1] },

        VertexPC { Float4 { 0.25f, -0.6f, 0.0f, 1.0f }, fixedColors[2] },
        VertexPC { Float4 { 0.75f,  0.1f, 0.0f, 1.0f }, fixedColors[2] },
        VertexPC { Float4 { 0.4f,   0.7f, 0.0f, 1.0f }, fixedColors[2] }
    };

    static RenderCore::InputElementDesc inputElePC[] = {
        RenderCore::InputElementDesc { "position", 0, RenderCore::Format::R32G32B32A32_FLOAT },
        RenderCore::InputElementDesc { "color", 0, RenderCore::Format::R8G8B8A8_UNORM }
    };

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U T I L I T Y    F N S


    std::shared_ptr<RenderCore::IResource> CreateVB(RenderCore::IDevice& device, IteratorRange<const void*> data)
    {
        using namespace RenderCore;
        return device.CreateResource(
            CreateDesc(
                BindFlag::VertexBuffer, 0, GPUAccess::Read,
                LinearBufferDesc::Create((unsigned)data.size()),
                "vertexBuffer"),
            [data](SubResourceId) -> SubResourceInitData { return SubResourceInitData { data }; });
    }

    class UnitTestFBHelper
    {
    public:
        RenderCore::Techniques::AttachmentPool _namedResources;
        RenderCore::Techniques::FrameBufferPool _frameBufferPool;
        RenderCore::Techniques::TechniqueContext _techniqueContext;
        RenderCore::Techniques::ParsingContext _parsingContext;     // careful init-order rules

        std::shared_ptr<RenderCore::IResource> _target;
        RenderCore::Techniques::RenderPassInstance _rpi;

        UnitTestFBHelper(RenderCore::IDevice& device, RenderCore::IThreadContext& threadContext, const RenderCore::ResourceDesc& targetDesc)
        : _parsingContext(_techniqueContext, &_namedResources, &_frameBufferPool)     // careful init-order rules
        {
            auto* metalThreadContext = (RenderCore::ImplAppleMetal::ThreadContext*)threadContext.QueryInterface(typeid(RenderCore::ImplAppleMetal::ThreadContext).hash_code());
            if (metalThreadContext)
                metalThreadContext->BeginHeadlessFrame();

            _target = device.CreateResource(targetDesc);
            _rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, _target, _parsingContext, RenderCore::LoadStore::Clear);
            _threadContext = &threadContext;
        }

        ~UnitTestFBHelper()
        {
            if (_threadContext) {
                auto* metalThreadContext = (RenderCore::ImplAppleMetal::ThreadContext*)_threadContext->QueryInterface(typeid(RenderCore::ImplAppleMetal::ThreadContext).hash_code());
                if (metalThreadContext)
                    metalThreadContext->EndHeadlessFrame();
            }
        }

    private:
        RenderCore::IThreadContext* _threadContext;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    C O D E

    TEST_CLASS(InputLayout)
	{
	public:
		std::unique_ptr<MetalTestHelper> _testHelper;

		TEST_CLASS_INITIALIZE(Startup)
		{
			_testHelper = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::AppleMetal);
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_testHelper.reset();
		}

		TEST_METHOD(BasicBinding)
		{
			using namespace RenderCore;
			auto threadContext = _testHelper->_device->GetImmediateContext();

			auto vs = _testHelper->MakeShader(vsText, "vs_*");
			auto ps = _testHelper->MakeShader(psText, "ps_*");
			Metal::ShaderProgram shaderProgram(Metal::GetObjectFactory(), vs, ps);

            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);

			////////////////////////////////////////////////////////////////////////////////////////
			{
				auto vertexBuffer = CreateVB(*_testHelper->_device, MakeIteratorRange(inputVerticesPC));

				Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);

				VertexBufferView vbv { vertexBuffer.get() };
				auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

				metalContext.Bind(Metal::ViewportDesc{ 0.f, 0.f, (float)targetDesc._textureDesc._width, (float)targetDesc._textureDesc._height });
				metalContext.Bind(shaderProgram);
				metalContext.Bind(Topology::TriangleList);
				metalContext.Draw(dimof(inputVerticesPC));
			}
			////////////////////////////////////////////////////////////////////////////////////////

			fbHelper._rpi = {};     // end RPI

            auto data = fbHelper._target->ReadBack(*threadContext);

            // We're expecting all pixels either to be black or fixedColor
            unsigned blackPixels = 0;
            unsigned coloredPixels[dimof(fixedColors)] = {};
            unsigned otherPixels = 0;

            Assert::AreEqual(data.size(), (size_t)RenderCore::ByteCount(targetDesc));
            auto pixels = MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()));
            for (auto p:pixels) {
                if (p == 0xff000000) { ++blackPixels; continue; }

                for (unsigned c=0; c<dimof(fixedColors); ++c) {
                    if (p == fixedColors[c])
                        ++coloredPixels[c];
                }
            }
            otherPixels = (unsigned)pixels.size() - blackPixels;
            for (unsigned c=0; c<dimof(fixedColors); ++c) otherPixels -= coloredPixels[c];

            Assert::AreEqual(otherPixels, 0u);
		}
	};
}
