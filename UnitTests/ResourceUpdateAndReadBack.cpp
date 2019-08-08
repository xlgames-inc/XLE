// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/OpenGLES/IDeviceOpenGLES.h"
#include "../RenderCore/AppleMetal/Device.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Math/Vector.h"
#include "../Math/Transformations.h"
#include <map>
#include <deque>
#include <queue>

#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
    #include "InputLayoutShaders_MSL.h"
#elif GFXAPI_TARGET == GFXAPI_OPENGLES
    #include "InputLayoutShaders_GLSL.h"
#else
    #error Unit test shaders not written for this graphics API
#endif

namespace UnitTests
{

////////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned vertices_vIdx[] = { 0, 1, 2, 3 };

    static RenderCore::InputElementDesc inputEleVIdx[] = {
        RenderCore::InputElementDesc { "vertexID", 0, RenderCore::Format::R32_SINT }
    };

    struct Values
    {
        float A = 0.f, B = 0.f, C = 0.f;
        unsigned dummy = 0;
        Float4 vA = Float4 { 0.f, 0.f, 0.f, 0.f };

        Values(const Float4& c) 
        { 
            A = c[0]; B = c[1]; vA[0] = c[2]; vA[1] = c[3];
        }

        Values() {}

        unsigned ColorPackedForm() const 
        {
            return  (unsigned(A * 255.f) << 16)
                |   (unsigned(B * 255.f) <<  8)
                |   (unsigned(vA[0] * 255.f) <<  0)
                |   (unsigned(vA[1] * 255.f) << 24)
                ;
        }
    };

    const RenderCore::ConstantBufferElementDesc ConstantBufferElementDesc_Values[] {
        RenderCore::ConstantBufferElementDesc { Hash64("A"), RenderCore::Format::R32_FLOAT, offsetof(Values, A) },
        RenderCore::ConstantBufferElementDesc { Hash64("B"), RenderCore::Format::R32_FLOAT, offsetof(Values, B) },
        RenderCore::ConstantBufferElementDesc { Hash64("C"), RenderCore::Format::R32_FLOAT, offsetof(Values, C) },
        RenderCore::ConstantBufferElementDesc { Hash64("vA"), RenderCore::Format::R32G32B32A32_FLOAT, offsetof(Values, vA) }
    };

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    C O D E

    TEST_CLASS(ResourceUpdateAndReadBack)
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

        void DrawClipSpaceQuad(
            RenderCore::Metal::DeviceContext& metalContext,
            RenderCore::Metal::ShaderProgram& shaderProgram,
            const Float2& topLeft, const Float2& bottomRight, unsigned color = 0xffffffff)
        {
            using namespace RenderCore;

            class VertexPC
            {
            public:
                Float4      _position;
                unsigned    _color;
            };

            const VertexPC vertices[] = {
                VertexPC { Float4 {     topLeft[0],     topLeft[1], 0.0f, 1.0f }, color },
                VertexPC { Float4 {     topLeft[0], bottomRight[1], 0.0f, 1.0f }, color },
                VertexPC { Float4 { bottomRight[0],     topLeft[1], 0.0f, 1.0f }, color },
                VertexPC { Float4 { bottomRight[0], bottomRight[1], 0.0f, 1.0f }, color }
            };

            const RenderCore::InputElementDesc inputElePC[] = {
                RenderCore::InputElementDesc { "position", 0, RenderCore::Format::R32G32B32A32_FLOAT },
                RenderCore::InputElementDesc { "color", 0, RenderCore::Format::R8G8B8A8_UNORM }
            };

            auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices));

            Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);
            assert(inputLayout.AllAttributesBound());
            VertexBufferView vbvs[] = { vertexBuffer0.get() };            

            inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));
            metalContext.Bind(Topology::TriangleStrip);
            metalContext.Draw(4);           // Draw once, with CB contents initialized outside of the RPI
        }

		TEST_METHOD(UpdateConstantBuffer)
		{
            // -------------------------------------------------------------------------------------
            // Create a constant buffer and use it during rendering of several draw calls. Ensure
            // that the updates to the constant buffer affect rendering as expected
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;

            auto* glesDevice = (IDeviceOpenGLES*)_testHelper->_device->QueryInterface(typeid(IDeviceOpenGLES).hash_code());
            if (glesDevice) {
                if (!(glesDevice->GetFeatureSet() & Metal_OpenGLES::FeatureSet::GLES300)) {
                    XCTFail(@"Known issues running this code on non GLES300 OpenGL");
                }
            }

            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText_Uniforms);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            auto cbResource = _testHelper->_device->CreateResource(
                CreateDesc(
                    BindFlag::ConstantBuffer, CPUAccess::Write, GPUAccess::Read,
                    LinearBufferDesc::Create(sizeof(Values)),
                    "test-cbuffer"));

            Values testValue0(Float4(0.33f, 0.5f, 0.66f, 1.0f));
            Values testValue1(Float4(1.0f, 0.25f, 0.25f, 1.0f));
            Values testValue2(Float4(0.25f, 0.25f, 1.0f, 1.0f));
            Values testValue3(Float4(0.1f, 1.0f, 0.1f, 1.0f));
            Values testValueRedundant(Float4(0.0f, 0.0f, 0.0f, 1.0f));

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            ((Metal::Buffer*)cbResource.get())->Update(metalContext, &testValue0, sizeof(testValue0));

            // ............. Setup BoundInputLayout & BoundUniforms ................................

            UniformsStreamInterface usi;
            usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_Values) });
            Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

            // ............. Start RPI .............................................................
            
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);

            {
                auto rpi = fbHelper.BeginRenderPass();
                metalContext.Bind(shaderProgram);

                ConstantBufferView cbvs[] = { ConstantBufferView { cbResource } };
                UniformsStream uniformsStream;
                uniformsStream._constantBuffers = MakeIteratorRange(cbvs);
                uniforms.Apply(metalContext, 0, uniformsStream);

                // CB values set prior to the rpi
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2(-1.0f, -1.0f), Float2( 0.0f, 0.0f));

                // CB values set in the middle of the rpi
                ((Metal::Buffer*)cbResource.get())->Update(metalContext, &testValue1, sizeof(testValue1));
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2( 0.0f, -1.0f), Float2( 1.0f, 0.0f));

                // Set a value that will be unused, and then immediate reset with new data
                ((Metal::Buffer*)cbResource.get())->Update(metalContext, &testValueRedundant, sizeof(testValueRedundant));
                ((Metal::Buffer*)cbResource.get())->Update(metalContext, &testValue2, sizeof(testValue2));
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2(-1.0f,  0.0f), Float2( 0.0f, 1.0f));

                ((Metal::Buffer*)cbResource.get())->Update(metalContext, &testValue3, sizeof(testValue3));
            }

            {
                auto rpi = fbHelper.BeginRenderPass(RenderCore::LoadStore::Retain);
                metalContext.Bind(shaderProgram);

                ConstantBufferView cbvs[] = { ConstantBufferView { cbResource } };
                UniformsStream uniformsStream;
                uniformsStream._constantBuffers = MakeIteratorRange(cbvs);
                uniforms.Apply(metalContext, 0, uniformsStream);

                // CB values set in the previous rpi
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2( 0.0f,  0.0f), Float2( 1.0f, 1.0f));
            }

            auto breakdown = fbHelper.GetFullColorBreakdown();
            Assert::AreEqual(breakdown.size(), (size_t)4);
            for (auto i:breakdown) {
                Assert::IsTrue( i.second == testValue0.ColorPackedForm()
                            ||  i.second == testValue1.ColorPackedForm()
                            ||  i.second == testValue2.ColorPackedForm()
                            ||  i.second == testValue3.ColorPackedForm());
            }
		}

	};
}
