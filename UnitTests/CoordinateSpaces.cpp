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

		TEST_METHOD(WindowCoordSpaceOrientation)
		{
            // -------------------------------------------------------------------------------------
            // Render some pattern in clip coordinate space to check the orientation and
            // arrangement of window coordinate space
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(64, 64, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_topLeftQuad));

                // Using the InputElementDesc version of BoundInputLayout constructor
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

                VertexBufferView vbv { vertexBuffer.get() };
                auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

                metalContext.Bind(shaderProgram);
                metalContext.Bind(Topology::TriangleList);
                metalContext.Bind(Metal::RasterizationDesc{CullMode::None});
                metalContext.Draw(dimof(vertices_topLeftQuad));
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

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
        }
    };
}

