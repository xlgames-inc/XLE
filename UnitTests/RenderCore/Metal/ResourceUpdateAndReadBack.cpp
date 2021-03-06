// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "MetalTestShaders.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/Metal/QueryPool.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/OpenGLES/IDeviceOpenGLES.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Math/Vector.h"
#include "../Math/Transformations.h"
#include <map>
#include <deque>
#include <queue>

#if !defined(XC_TEST_ADAPTER)
    #include <CppUnitTest.h>
    using namespace Microsoft::VisualStudio::CppUnitTestFramework;
	#define ThrowsException ExpectException<const std::exception&>
	#define XCTestCase unsigned
	static const unsigned self = 0;
	#pragma warning(disable:4459)
#endif

namespace UnitTests
{

	// See comments in ColorPackedForm below. We can't predict the exact value, so we have to test +/- 1.
	static bool ComponentsMatch(uint32_t c1, uint32_t c2) {
		return (c1 == c2 || c1+1 == c2 || c1 == c2+1);
	}

	static bool ColorsMatch(uint32_t c1, uint32_t c2) {
		unsigned char *p1 = reinterpret_cast<unsigned char *>(&c1);
		unsigned char *p2 = reinterpret_cast<unsigned char *>(&c2);
		return (
			ComponentsMatch(p1[0], p2[0]) &&
			ComponentsMatch(p1[1], p2[1]) &&
			ComponentsMatch(p1[2], p2[2]) &&
			ComponentsMatch(p1[3], p2[3])
		);
	}

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

        // The way float32 colors get rounded when drawn to normalized U8 textures may differ between GFXAPIs. Metal documents that each component gets normalized and then rounded to nearest even, but this isn't actually what happens--while 0.3, 0.5, and 0.7 get rounded up to 77, 128, and 179 as you'd expect, 0.1 and 0.9 get rounded down to 25 and 229. So, rather than worry about rounding here, we just truncate, and then check +/-1 in the comparison.
        unsigned ColorPackedForm() const
        {
            return  (unsigned(A * 255.f) << 0)
                |   (unsigned(B * 255.f) <<  8)
                |   (unsigned(vA[0] * 255.f) <<  16)
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

		ResourceUpdateAndReadBack()
		{
            _testHelper = MakeTestHelper();
		}

		~ResourceUpdateAndReadBack()
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

        const Values testValue0 = Float4(0.1f, 0.2f, 0.95f, 1.0f);
        const Values testValue1 = Float4(0.9f, 0.4f, 0.3f, 1.0f);
        const Values testValue2 = Float4(0.5f, 0.85f, 0.6f, 1.0f);
        const Values testValue3 = Float4(0.7f, 0.8f, 0.75f, 1.0f);
        const Values testValueRedundant = Float4(0.65f, 0.33f, 0.42f, 1.0f);

        std::map<unsigned, unsigned> _UpdateConstantBufferHelper(XCTestCase* self, bool unsynchronized)
        {
            // -------------------------------------------------------------------------------------
            // Create a constant buffer and use it during rendering of several draw calls. Ensure
            // that the updates to the constant buffer affect rendering as expected
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;

            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText_Uniforms);
            auto targetDesc = CreateDesc(
                                         BindFlag::RenderTarget, 0, GPUAccess::Write,
                                         TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                                         "temporary-out");

            auto cbResource = _testHelper->_device->CreateResource(
                                                                   CreateDesc(
                                                                              BindFlag::ConstantBuffer, CPUAccess::WriteDynamic, GPUAccess::Read,
                                                                              LinearBufferDesc::Create(sizeof(Values)),
                                                                              "test-cbuffer"));

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            auto *cBuffer = (Metal::Buffer*)cbResource.get();
            int flags = unsynchronized ? Metal::Buffer::UpdateFlags::UnsynchronizedWrite : 0;
            cBuffer->Update(metalContext, &testValue0, sizeof(testValue0), 0, flags);

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

                // CB values set in the middle of the rpi--illegal for synchronized
                if (unsynchronized) {
                    cBuffer->Update(metalContext, &testValue1, sizeof(testValue1), 0, flags);
                } else {
                    Assert::ThrowsException([&]() {
                        cBuffer->Update(metalContext, &testValue1, sizeof(testValue1), 0, flags);
                    });
                }
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2( 0.0f, -1.0f), Float2( 1.0f, 0.0f));

                // Set a value that will be unused, and then immediate reset with new data--still illegal for synchronized
                if (unsynchronized) {
                    cBuffer->Update(metalContext, &testValueRedundant, sizeof(testValueRedundant), 0, flags);
                    cBuffer->Update(metalContext, &testValue2, sizeof(testValue2), 0, flags);
                }
                DrawClipSpaceQuad(metalContext, shaderProgram, Float2(-1.0f,  0.0f), Float2( 0.0f, 1.0f));

                // Set a value to be used in the next render pass--still illegal for synchronized
                if (unsynchronized) {
                    cBuffer->Update(metalContext, &testValue3, sizeof(testValue3), 0, flags);
                }
            }

            // Set a value to be used in the next render pass--the right place for unsynchronized
            if (!unsynchronized) {
                cBuffer->Update(metalContext, &testValue3, sizeof(testValue3), 0, flags);
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

            return fbHelper.GetFullColorBreakdown();
        }

        TEST_METHOD(UpdateConstantBufferUnsynchronized)
		{
            using namespace RenderCore;

            auto* glesDevice = (IDeviceOpenGLES*)_testHelper->_device->QueryInterface(typeid(IDeviceOpenGLES).hash_code());
            if (glesDevice) {
                if (!(glesDevice->GetFeatureSet() & Metal_OpenGLES::FeatureSet::GLES300)) {
					Assert::Fail(ToString("Known issues running this code on non GLES300 OpenGL: unsynchronized writes are simulated with synchronized writes, so we don't get the expected results").c_str());
                }
            }

            auto breakdown = _UpdateConstantBufferHelper(self, true);
            // Since we're not synchronizing anywhere, and doing virtually no CPU work,
            // it's incredibly unlikely that anything in either render pass will get
            // drawn before the last update, so all four quadrants should have the
            // last value set, even though testValue0, 1, and 2 were the current
            // values at the times we actually issued the draws.
            Assert::AreEqual(breakdown.size(), (size_t)1);
            for (auto i:breakdown) {
                auto color = i.first;
                Assert::IsTrue(ColorsMatch(color, testValue3.ColorPackedForm()));
            }
		}

        TEST_METHOD(UpdateConstantBufferSynchronized)
        {
            auto breakdown = _UpdateConstantBufferHelper(self, false);
            // With synchronized writes that happen on render-pass boundaries, we're
            // expecting that the first three quadrants (in the first render pass)
            // will have testValue0, and the last quadrant (in the second) will have
            // testValue3.
            Assert::AreEqual(breakdown.size(), (size_t)2);
            for (auto i:breakdown) {
                auto color = i.first;
                Assert::IsTrue(ColorsMatch(color, testValue0.ColorPackedForm()) ||
                               ColorsMatch(color, testValue3.ColorPackedForm()));
            }
        }
	};
}
