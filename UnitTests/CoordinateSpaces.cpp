// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Math/Vector.h"

#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
    #include "InputLayoutShaders_MSL.h"
#elif GFXAPI_TARGET == GFXAPI_OPENGLES
    #include "InputLayoutShaders_GLSL.h"
#else
    #error Unit test shaders not written for this graphics API
#endif

namespace UnitTests
{
    class VertexPC
    {
    public:
        Float4      _position;
        unsigned    _color;
    };

    static VertexPC vertices_topLeftQuad[] = {
        VertexPC { Float4 {  -1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {   1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {  -1.0f,  0.5f,  0.0f,  1.0f }, 0xffffffff },

        VertexPC { Float4 {   1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {  -1.0f,  0.5f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {   1.0f,  0.5f,  0.0f,  1.0f }, 0xffffffff }
    };

    static VertexPC vertices_fullViewport[] = {
        VertexPC { Float4 {  -1.0f, -1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {   1.0f, -1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {  -1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff },

        VertexPC { Float4 {  -1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {   1.0f, -1.0f,  0.0f,  1.0f }, 0xffffffff },
        VertexPC { Float4 {   1.0f,  1.0f,  0.0f,  1.0f }, 0xffffffff }
    };

    static RenderCore::InputElementDesc inputElePC[] = {
        RenderCore::InputElementDesc { "position", 0, RenderCore::Format::R32G32B32A32_FLOAT },
        RenderCore::InputElementDesc { "color", 0, RenderCore::Format::R8G8B8A8_UNORM }
    };

    TEST_CLASS(CoordinateSpaces)
	{
	public:
        std::unique_ptr<MetalTestHelper> _testHelper;

        TEST_CLASS_INITIALIZE(Startup)
        {
            #if GFXAPI_TARGET == GFXAPI_APPLEMETAL
                _testHelper = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::AppleMetal);
           #else
                _testHelper = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::OpenGLES);
           #endif
        }

        TEST_CLASS_CLEANUP(Shutdown)
        {
            _testHelper.reset();
        }

        void RenderQuad(
            RenderCore::Metal::DeviceContext& metalContext,
            IteratorRange<const VertexPC*> vertices,
            const RenderCore::Metal::RasterizationDesc& rasterizationDesc)
        {
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText);

            using namespace RenderCore;
            auto vertexBuffer = CreateVB(*_testHelper->_device, vertices);

            // Using the InputElementDesc version of BoundInputLayout constructor
            Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);
            assert(inputLayout.AllAttributesBound());

            VertexBufferView vbv { vertexBuffer.get() };
            inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

            metalContext.Bind(shaderProgram);
            metalContext.Bind(Topology::TriangleList);
            metalContext.Bind(rasterizationDesc);
            metalContext.Draw((unsigned)vertices.size());
        }

		TEST_METHOD(WindowCoordSpaceOrientation)
		{
            // -------------------------------------------------------------------------------------
            // Render some pattern in clip coordinate space to check the orientation and
            // arrangement of window coordinate space
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(64, 64, Format::R8G8B8A8_UNORM),
                "temporary-out");
            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            {
                auto rpi = fbHelper.BeginRenderPass();
                RenderQuad(metalContext, MakeIteratorRange(vertices_topLeftQuad), Metal::RasterizationDesc{CullMode::None});
            }

            auto data = fbHelper._target->ReadBack(*threadContext);
            unsigned lastPixel = *(unsigned*)PtrAdd(AsPointer(data.end()), -sizeof(unsigned));
            unsigned firstPixel = *(unsigned*)data.data();

            // The orientation of window coordinate space is different in GLES vs other APIs
            // In this test, we draw a horizontal strip between Y = 1.0 and y = 0.5 in clip space
            // In most APIs, when we readback from the texture, this is at the start of texture
            // memory, but in OpenGLES / OpenGL, it's at the end
            #if GFXAPI_TARGET == GFXAPI_OPENGLES
                Assert::AreEqual(firstPixel, 0xff000000);
                Assert::AreEqual(lastPixel, 0xffffffff);
            #else
                Assert::AreEqual(firstPixel, 0xffffffff);
                Assert::AreEqual(lastPixel, 0xff000000);
            #endif
            (void)firstPixel; (void)lastPixel;
        }

        TEST_METHOD(WindowCoordSpaceWindingOrder)
        {
            // -------------------------------------------------------------------------------------
            // Render a quad to check the impact of window coordinates on winding oder
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);

            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(64, 64, Format::R8G8B8A8_UNORM),
                "temporary-out0");
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            {
                auto rpi = fbHelper.BeginRenderPass();
                RenderQuad(metalContext, MakeIteratorRange(vertices_fullViewport), Metal::RasterizationDesc{CullMode::Back, FaceWinding::CCW});
            }
            auto breakdown0 = fbHelper.GetFullColorBreakdown();

            {
                auto rpi = fbHelper.BeginRenderPass();
                RenderQuad(metalContext, MakeIteratorRange(vertices_fullViewport), Metal::RasterizationDesc{CullMode::Back, FaceWinding::CW});
            }
            auto breakdown1 = fbHelper.GetFullColorBreakdown();

            // The differences in the window coordinate definition does not impact the winding
            // mode. Even though the handiness of window coordinates is different, the winding
            // order calculate is determined in cull space.
            Assert::AreEqual(breakdown0.size(), (size_t)1);
            Assert::AreEqual(breakdown0.begin()->first, 0xffffffff);
            Assert::AreEqual(breakdown1.begin()->first, 0xff000000);
            (void)breakdown0; (void)breakdown1;

            {
                auto rpi = fbHelper.BeginRenderPass();
                RenderQuad(metalContext, MakeIteratorRange(vertices_fullViewport, &vertices_fullViewport[3]), Metal::RasterizationDesc{CullMode::Back, FaceWinding::CCW});
            }
            auto breakdown2 = fbHelper.GetFullColorBreakdown();

            // If we draw only one triangle of the full screen quad, we will draw to approximately
            // half the screen. It's not exactly half, though, because of the rules for when a
            // triangle goes through the center of a pixel. We draw to slightly fewer than half
            // of the pixels. And we should get the same results regardless of API and regardless
            // of window coordinate space definition
            Assert::AreEqual(breakdown2.size(), (size_t)2);
            Assert::AreEqual(breakdown2[0xff000000], 2080u);
            Assert::AreEqual(breakdown2[0xffffffff], unsigned((64*64) - 2080));
        }
    };
}

