// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/AppleMetal/Device.h"
#include "../RenderCore/ResourceDesc.h"
#include "../Math/Vector.h"
#include <map>

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
            //    T E S T   I N P U T   D A T A

    class VertexPC
    {
    public:
        Float4      _position;
        unsigned    _color;
    };

    static const unsigned fixedColors[] = { 0xff7f7f7fu, 0xff007f7fu, 0xff7f0000u, 0xff7f007fu };

    static VertexPC vertices_randomTriangle[] = {
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

    static RenderCore::MiniInputElementDesc miniInputElePC[] = {
        { Hash64("position"), RenderCore::Format::R32G32B32A32_FLOAT },
        { Hash64("color"), RenderCore::Format::R8G8B8A8_UNORM }
    };

    static unsigned boxesSize = (96 - 32) * (96 - 32);
    static Int2 boxOffsets[] = { Int2(0, 0), Int2(768, 0), Int2(0, 768), Int2(768, 768) };
    static Int2 vertices_4Boxes[] = {
        Int2(32, 32), Int2(96, 32), Int2(32, 96),
        Int2(32, 96), Int2(96, 32), Int2(96, 96),

        Int2(768 + 32, 32), Int2(768 + 96, 32), Int2(768 + 32, 96),
        Int2(768 + 32, 96), Int2(768 + 96, 32), Int2(768 + 96, 96),

        Int2(32, 768 + 32), Int2(96, 768 + 32), Int2(32, 768 + 96),
        Int2(32, 768 + 96), Int2(96, 768 + 32), Int2(96, 768 + 96),

        Int2(768 + 32, 768 + 32), Int2(768 + 96, 768 + 32), Int2(768 + 32, 768 + 96),
        Int2(768 + 32, 768 + 96), Int2(768 + 96, 768 + 32), Int2(768 + 96, 768 + 96)
    };

    static unsigned vertices_colors[] = {
        fixedColors[0], fixedColors[0], fixedColors[0], fixedColors[0], fixedColors[0], fixedColors[0],
        fixedColors[1], fixedColors[1], fixedColors[1], fixedColors[1], fixedColors[1], fixedColors[1],
        fixedColors[2], fixedColors[2], fixedColors[2], fixedColors[2], fixedColors[2], fixedColors[2],
        fixedColors[3], fixedColors[3], fixedColors[3], fixedColors[3], fixedColors[3], fixedColors[3]
    };

    static RenderCore::InputElementDesc inputEleVIdx[] = {
        RenderCore::InputElementDesc { "vertexID", 0, RenderCore::Format::R32_SINT }
    };

    static unsigned vertices_vIdx[] = { 0, 1, 2, 3 };

    struct Values
    {
        float A, B, C;
        unsigned dummy;
        Float4 vA;
    };

    const RenderCore::ConstantBufferElementDesc ConstantBufferElementDesc_Values[] {
        RenderCore::ConstantBufferElementDesc { Hash64("A"), RenderCore::Format::R32_FLOAT, offsetof(Values, A) },
        RenderCore::ConstantBufferElementDesc { Hash64("B"), RenderCore::Format::R32_FLOAT, offsetof(Values, B) },
        RenderCore::ConstantBufferElementDesc { Hash64("C"), RenderCore::Format::R32_FLOAT, offsetof(Values, C) },
        RenderCore::ConstantBufferElementDesc { Hash64("vA"), RenderCore::Format::R32G32B32A32_FLOAT, offsetof(Values, vA) }
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    struct ColorBreakdown
    {
        unsigned _blackPixels = 0;
        unsigned _coloredPixels[dimof(fixedColors)] = {};
        unsigned _otherPixels = 0;
    };

    static ColorBreakdown GetColorBreakdown(RenderCore::IThreadContext& threadContext, UnitTestFBHelper& fbHelper)
    {
        ColorBreakdown result;

        auto data = fbHelper._target->ReadBack(threadContext);

        assert(data.size() == (size_t)RenderCore::ByteCount(fbHelper._target->GetDesc()));
        auto pixels = MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()));
        for (auto p:pixels) {
            if (p == 0xff000000) { ++result._blackPixels; continue; }

            for (unsigned c=0; c<dimof(fixedColors); ++c) {
                if (p == fixedColors[c])
                    ++result._coloredPixels[c];
            }
        }
        result._otherPixels = (unsigned)pixels.size() - result._blackPixels;
        for (unsigned c=0; c<dimof(fixedColors); ++c) result._otherPixels -= result._coloredPixels[c];

        return result;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    C O D E

    TEST_CLASS(InputLayout)
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

		TEST_METHOD(BasicBinding_LongForm)
		{
            // -------------------------------------------------------------------------------------
            // Bind some geometry and render it using the "InputElementDesc" version of the
            // BoundInputLayout constructor
            // -------------------------------------------------------------------------------------
			using namespace RenderCore;
			auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

			////////////////////////////////////////////////////////////////////////////////////////
			{
				auto vertexBuffer = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_randomTriangle));

                // Using the InputElementDesc version of BoundInputLayout constructor
				Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

				VertexBufferView vbv { vertexBuffer.get() };
				auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

				metalContext.Bind(shaderProgram);
				metalContext.Bind(Topology::TriangleList);
				metalContext.Draw(dimof(vertices_randomTriangle));
			}
			////////////////////////////////////////////////////////////////////////////////////////

			rpi = {};     // end RPI

            auto colorBreakdown = GetColorBreakdown(*threadContext, fbHelper);
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            (void)colorBreakdown;
		}

        TEST_METHOD(BasicBinding_ShortForm)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and render it using the "MiniInputElementDesc" version of the
            // BoundInputLayout constructor
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_clipInput, psText);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_randomTriangle));

                // Using the MiniInputElementDesc version of BoundInputLayout constructor
                Metal::BoundInputLayout::SlotBinding slotBinding { MakeIteratorRange(miniInputElePC), 0 };
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(&slotBinding, &slotBinding+1), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

                VertexBufferView vbv { vertexBuffer.get() };
                auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

                metalContext.Bind(shaderProgram);
                metalContext.Bind(Topology::TriangleList);
                metalContext.Draw(dimof(vertices_randomTriangle));
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            auto colorBreakdown = GetColorBreakdown(*threadContext, fbHelper);
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            (void)colorBreakdown;
        }

        TEST_METHOD(BasicBinding_2VBs)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and render it using both the "InputElementDesc" the and
            // "MiniInputElementDesc" versions of the BoundInputLayout constructor, but with 2
            // separate vertex buffers (each containing a different geometry stream)
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText, psText);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);

            auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_4Boxes));
            auto vertexBuffer1 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_colors));

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto rpi = fbHelper.BeginRenderPass();

                InputElementDesc inputEles[] = {
                    InputElementDesc { "position", 0, Format::R32G32_SINT, 0 },
                    InputElementDesc { "color", 0, Format::R8G8B8A8_UNORM, 1 }
                };

                // Using the InputElementDesc version of BoundInputLayout constructor
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEles), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

                VertexBufferView vbvs[] {
                    VertexBufferView { vertexBuffer0.get() },
                    VertexBufferView { vertexBuffer1.get() }
                };
                auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                metalContext.Bind(shaderProgram);
                metalContext.Bind(Topology::TriangleList);
                metalContext.Draw(dimof(vertices_4Boxes));
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            auto colorBreakdown = GetColorBreakdown(*threadContext, fbHelper);
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            Assert::AreEqual(colorBreakdown._coloredPixels[0], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[1], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[2], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[3], boxesSize);
            Assert::AreEqual(colorBreakdown._blackPixels, targetDesc._textureDesc._width * targetDesc._textureDesc._height - 4 * boxesSize);

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto rpi = fbHelper.BeginRenderPass();

                MiniInputElementDesc miniInputElePC1[] { { Hash64("position"), Format::R32G32_SINT } };
                MiniInputElementDesc miniInputElePC2[] { { Hash64("color"), Format::R8G8B8A8_UNORM } };

                Metal::BoundInputLayout::SlotBinding slotBindings[] {
                    { MakeIteratorRange(miniInputElePC1), 0 },
                    { MakeIteratorRange(miniInputElePC2), 0 }
                };

                // Using the InputElementDesc version of BoundInputLayout constructor
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(slotBindings), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

                VertexBufferView vbvs[] {
                    VertexBufferView { vertexBuffer0.get() },
                    VertexBufferView { vertexBuffer1.get() }
                };
                auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                metalContext.Bind(shaderProgram);
                metalContext.Bind(Topology::TriangleList);
                metalContext.Draw(dimof(vertices_4Boxes));
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            colorBreakdown = GetColorBreakdown(*threadContext, fbHelper);
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            Assert::AreEqual(colorBreakdown._coloredPixels[0], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[1], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[2], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[3], boxesSize);
            Assert::AreEqual(colorBreakdown._blackPixels, targetDesc._textureDesc._width * targetDesc._textureDesc._height - 4 * boxesSize);
        }

        TEST_METHOD(BasicBinding_DataRate)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and render it using the "InputElementDesc" version of the
            // BoundInputLayout constructor, with 3 separate vertex buffers, and some attributes
            // using per instance data rate settings
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_Instanced, psText);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_4Boxes));
                auto vertexBuffer1 = CreateVB(*_testHelper->_device, MakeIteratorRange(fixedColors));
                auto vertexBuffer2 = CreateVB(*_testHelper->_device, MakeIteratorRange(boxOffsets));

                InputElementDesc inputEles[] = {
                    InputElementDesc { "position", 0, Format::R32G32_SINT, 0 },
                    InputElementDesc { "color", 0, Format::R8G8B8A8_UNORM, 1, ~0u, InputDataRate::PerInstance, 1 },
                    InputElementDesc { "instanceOffset", 0, Format::R32G32_SINT, 2, ~0u, InputDataRate::PerInstance, 1 }
                };

                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEles), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());

                VertexBufferView vbvs[] {
                    VertexBufferView { vertexBuffer0.get() },
                    VertexBufferView { vertexBuffer1.get() },
                    VertexBufferView { vertexBuffer2.get() },
                };
                auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                metalContext.Bind(shaderProgram);
                metalContext.Bind(Topology::TriangleList);
                metalContext.DrawInstances(6, 4);
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            auto colorBreakdown = GetColorBreakdown(*threadContext, fbHelper);
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            Assert::AreEqual(colorBreakdown._coloredPixels[0], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[1], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[2], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[3], boxesSize);
            Assert::AreEqual(colorBreakdown._blackPixels, targetDesc._textureDesc._width * targetDesc._textureDesc._height - 4 * boxesSize);
            (void)colorBreakdown;
        }

        TEST_METHOD(BasicBinding_BindAttributeToGeneratorShader)
        {
            // -------------------------------------------------------------------------------------
            // Bind an attribute (of any kind) to some shader that doesn't take any attributes as
            // input at all
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_FullViewport, psText);
            Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputElePC), shaderProgram);
            Assert::IsTrue(inputLayout.AllAttributesBound());

            auto vertexBuffer = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_randomTriangle));
            VertexBufferView vbv { vertexBuffer.get() };
            auto& metalContext = *Metal::DeviceContext::Get(*_testHelper->_device->GetImmediateContext());
            inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));
        }

        TEST_METHOD(BasicBinding_BindMissingAttribute)
        {
            // -------------------------------------------------------------------------------------
            // Bind an attribute (and actually a full VB) to a shader that doesn't actually need
            // that attribute. In this case, the entire VB binding gets rejected because none of
            // that attributes from that VB are needed (but other attribute bindings -- from other
            // VBs -- do apply)
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText, psText);
            InputElementDesc inputEles[] = {
                InputElementDesc { "position", 0, Format::R32G32_SINT, 0 },
                InputElementDesc { "color", 0, Format::R8G8B8A8_UNORM, 1, ~0u, InputDataRate::PerInstance, 1 },
                InputElementDesc { "instanceOffset", 0, Format::R32G32_SINT, 2, ~0u, InputDataRate::PerInstance, 1 }
            };

            Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEles), shaderProgram);
            Assert::IsTrue(inputLayout.AllAttributesBound());

            auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_4Boxes));
            auto vertexBuffer1 = CreateVB(*_testHelper->_device, MakeIteratorRange(fixedColors));
            auto vertexBuffer2 = CreateVB(*_testHelper->_device, MakeIteratorRange(boxOffsets));
            VertexBufferView vbvs[] {
                VertexBufferView { vertexBuffer0.get() },
                VertexBufferView { vertexBuffer1.get() },
                VertexBufferView { vertexBuffer2.get() },
            };
            auto& metalContext = *Metal::DeviceContext::Get(*_testHelper->_device->GetImmediateContext());
            inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));
        }

        TEST_METHOD(BasicBinding_Uniforms)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and render it, and bind some uniforms using the the BoundUniforms
            // class. Also render using a "vertex generator" shader with no input attributes.
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_FullViewport, psText_Uniforms);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                Metal::BoundInputLayout inputLayout(IteratorRange<const InputElementDesc*>{}, shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());
                inputLayout.Apply(metalContext, {});

                // NOTE -- special case in AppleMetal implementation -- the shader must be bound
                // here first, before we get to the uniform binding
                metalContext.Bind(shaderProgram);

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, {Hash64("Values")});
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

                Values v { 0.4f, 0.5f, 0.2f, 0, Float4 { 0.1f, 1.0f, 1.0f, 1.0f } };
                ConstantBufferView cbvs[] = { MakeSharedPkt(v) };
                uniforms.Apply(metalContext, 0, UniformsStream { MakeIteratorRange(cbvs) });

                metalContext.Bind(Topology::TriangleStrip);
                metalContext.Draw(4);
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            // we should have written the same color to every pixel, based on the uniform inputs we gave
            auto colorBreakdown = fbHelper.GetFullColorBreakdown();
            Assert::AreEqual(colorBreakdown.size(), (size_t)1);
            Assert::AreEqual(colorBreakdown.begin()->first, 0xff198066);
            Assert::AreEqual(colorBreakdown.begin()->second, targetDesc._textureDesc._width * targetDesc._textureDesc._height);

            ////////////////////////////////////////////////////////////////////////////////////////
            //  Do it again, this time with the full CB layout provided in the binding call

            rpi = fbHelper.BeginRenderPass();

            {
                Metal::BoundInputLayout inputLayout(IteratorRange<const InputElementDesc*>{}, shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());
                inputLayout.Apply(metalContext, {});
                metalContext.Bind(shaderProgram);

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_Values) });
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

                Values v { 0.1f, 0.7f, 0.4f, 0, Float4 { 0.8f, 1.0f, 1.0f, 1.0f } };
                ConstantBufferView cbvs[] = { MakeSharedPkt(v) };
                uniforms.Apply(metalContext, 0, UniformsStream { MakeIteratorRange(cbvs) });

                metalContext.Draw(4);
            }

            rpi = {};     // end RPI

            // we should have written the same color to every pixel, based on the uniform inputs we gave
            colorBreakdown = fbHelper.GetFullColorBreakdown();
            Assert::AreEqual(colorBreakdown.size(), (size_t)1);
            Assert::AreEqual(colorBreakdown.begin()->first, 0xffccb219);
            Assert::AreEqual(colorBreakdown.begin()->second, targetDesc._textureDesc._width * targetDesc._textureDesc._height);
        }

        TEST_METHOD(BasicBinding_IncorrectUSI)
        {
            // -------------------------------------------------------------------------------------
            // Bind uniform buffers using the BoundUniforms interface with various error conditions
            // (such as incorrect arrangement of uniform buffer elements, missing values, etc)
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_FullViewport2, psText_Uniforms);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            metalContext.Bind(shaderProgram);

            {
                // incorrect arrangement of constant buffer elements
                const ConstantBufferElementDesc ConstantBufferElementDesc_IncorrectBinding[] {
                    ConstantBufferElementDesc { Hash64("A"), Format::R32_FLOAT, sizeof(Values) - sizeof(Values::A) - offsetof(Values, A) },
                    ConstantBufferElementDesc { Hash64("B"), Format::R32_FLOAT, sizeof(Values) - sizeof(Values::A) - offsetof(Values, B) },
                    ConstantBufferElementDesc { Hash64("C"), Format::R32_FLOAT, sizeof(Values) - sizeof(Values::A) - offsetof(Values, C) },
                    ConstantBufferElementDesc { Hash64("vA"), Format::R32G32B32A32_FLOAT, sizeof(Values) - sizeof(Values::vA) - offsetof(Values, vA) }
                };

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_IncorrectBinding) });
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };
                (void)uniforms;
            }

            {
                // some missing constant buffer elements
                const ConstantBufferElementDesc ConstantBufferElementDesc_MissingValues[] {
                    RenderCore::ConstantBufferElementDesc { Hash64("A"), RenderCore::Format::R32_FLOAT, offsetof(Values, A) },
                    RenderCore::ConstantBufferElementDesc { Hash64("vA"), RenderCore::Format::R32G32B32A32_FLOAT, offsetof(Values, vA) }
                };

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_MissingValues) });
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };
                (void)uniforms;
            }

            {
                // Incorrect formats of elements within the constant buffer
                const ConstantBufferElementDesc ConstantBufferElementDesc_IncorrectFormats[] {
                    RenderCore::ConstantBufferElementDesc { Hash64("A"), RenderCore::Format::R32G32_FLOAT, offsetof(Values, A) },
                    RenderCore::ConstantBufferElementDesc { Hash64("C"), RenderCore::Format::R8G8B8A8_UNORM, offsetof(Values, C) },
                    RenderCore::ConstantBufferElementDesc { Hash64("vA"), RenderCore::Format::R32G32B32_FLOAT, offsetof(Values, vA) }
                };

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_IncorrectFormats) });
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };
                (void)uniforms;
            }

            {
                // overlapping values in the constant buffer elements
                const ConstantBufferElementDesc ConstantBufferElementDesc_OverlappingValues[] {
                    RenderCore::ConstantBufferElementDesc { Hash64("A"), RenderCore::Format::R32G32_FLOAT, offsetof(Values, A) },
                    RenderCore::ConstantBufferElementDesc { Hash64("B"), RenderCore::Format::R32G32_FLOAT, offsetof(Values, B) },
                    RenderCore::ConstantBufferElementDesc { Hash64("C"), RenderCore::Format::R32G32_FLOAT, offsetof(Values, C) },
                    RenderCore::ConstantBufferElementDesc { Hash64("vA"), RenderCore::Format::R32G32B32A32_FLOAT, offsetof(Values, vA) }
                };

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, { Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_OverlappingValues) });
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };
                (void)uniforms;
            }

            {
                // missing CB binding
                UniformsStreamInterface usi;
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };
                (void)uniforms;
            }

            rpi = {};     // end RPI
        }

        class TestTexture
        {
        public:
            RenderCore::ResourceDesc _resDesc;
            std::vector<unsigned> _initData;
            std::shared_ptr<RenderCore::IResource> _res;

            TestTexture(RenderCore::IDevice& device)
            {
                using namespace RenderCore;
                _resDesc = CreateDesc(
                    BindFlag::ShaderResource, 0, GPUAccess::Read,
                    TextureDesc::Plain2D(16, 16, Format::R8G8B8A8_UNORM),
                    "input-tex");
                for (unsigned y=0; y<16; ++y)
                    for (unsigned x=0; x<16; ++x)
                        _initData.push_back(((x+y)&1) ? 0xff7f7f7f : 0xffcfcfcf);
                _res = device.CreateResource(
                    _resDesc,
                    [this](SubResourceId subResId) {
                        assert(subResId._mip == 0 && subResId._arrayLayer == 0);
                        return SubResourceInitData { MakeIteratorRange(_initData) };
                    });
            }
        };

        TEST_METHOD(BasicBinding_IncorrectUniformsStream)
        {
            // -------------------------------------------------------------------------------------
            // Bind uniform buffers using the BoundUniforms interface with various error conditions
            // But this time, the errors are in the UniformsStream object passed to the Apply method
            // (With Apple Metal, the Apply method only queues up uniforms to be applied at Draw,
            // so it's the Draw that will throw.)
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgramCB = MakeShaderProgram(*_testHelper, vsText_FullViewport2, psText_Uniforms);
            auto shaderProgramSRV = MakeShaderProgram(*_testHelper, vsText_FullViewport2, psText_TextureBinding);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            TestTexture testTexture(*_testHelper->_device);

            // -------------------------------------------------------------------------------------

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_vIdx));
            Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEleVIdx), shaderProgramCB);
            Assert::IsTrue(inputLayout.AllAttributesBound());
            VertexBufferView vbvs[] = { vertexBuffer0.get() };
            inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

            {
                // Shader takes a CB called "Values", but we will incorrectly attempt to bind
                // a shader resource there (and not bind the CB)
                metalContext.Bind(shaderProgramCB);

                UniformsStreamInterface usi;
                usi.BindShaderResource(0, Hash64("Values"));
                Metal::BoundUniforms uniforms { shaderProgramCB, Metal::PipelineLayoutConfig {}, usi };
                
                Metal::ShaderResourceView srv { Metal::GetObjectFactory(), testTexture._res };
                Metal::SamplerState pointSampler { FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp };

                UniformsStream uniformsStream;
                const Metal::ShaderResourceView* srvs[] = { &srv };
                const Metal::SamplerState* samplers[] = { &pointSampler };
                uniformsStream._resources = UniformsStream::MakeResources(MakeIteratorRange(srvs));
                uniformsStream._samplers = UniformsStream::MakeResources(MakeIteratorRange(samplers));
                uniforms.Apply(metalContext, 0, uniformsStream);                
            }

            {
                // Shader takes a SRV called "Texture", but we will incorrectly attempt to bind
                // a constant buffer there (and not bind the SRV)
                metalContext.Bind(shaderProgramSRV);

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, {Hash64("Texture")});
                Metal::BoundUniforms uniforms { shaderProgramSRV, Metal::PipelineLayoutConfig {}, usi };
                
                ConstantBufferView cbvs[] = { ConstantBufferView {  MakeSubFramePkt(Values{}) } };
                UniformsStream uniformsStream;
                uniformsStream._constantBuffers = MakeIteratorRange(cbvs);
                uniforms.Apply(metalContext, 0, uniformsStream);                
            }

            {
                // Shader takes a CB called "Values", we will promise to bind it, but then not
                // actually include it into the UniformsStream
                metalContext.Bind(shaderProgramCB);

                UniformsStreamInterface usi;
                usi.BindConstantBuffer(0, {Hash64("Values"), MakeIteratorRange(ConstantBufferElementDesc_Values)});
                Metal::BoundUniforms uniforms { shaderProgramCB, Metal::PipelineLayoutConfig {}, usi };

                Assert::ThrowsException(
                    [&]() {
                        uniforms.Apply(metalContext, 0, UniformsStream {});
                        metalContext.Bind(Topology::TriangleStrip);
                        metalContext.Draw(4);
                    });
            }

            {
                // Shader takes a SRV called "Texture", we will promise to bind it, but then not
                // actually include it into the UniformsStream
                metalContext.Bind(shaderProgramSRV);

                UniformsStreamInterface usi;
                usi.BindShaderResource(0, Hash64("Texture"));
                Metal::BoundUniforms uniforms { shaderProgramSRV, Metal::PipelineLayoutConfig {}, usi };

                Assert::ThrowsException(
                    [&]() {
                        uniforms.Apply(metalContext, 0, UniformsStream {});
                        metalContext.Bind(Topology::TriangleStrip);
                        metalContext.Draw(4);
                    });
            }

            rpi = {};     // end RPI
        }

        TEST_METHOD(BasicBinding_TextureBinding)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and bind a texture using the BoundUniforms interface
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_FullViewport2, psText_TextureBinding);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            TestTexture testTexture(*_testHelper->_device);

            // -------------------------------------------------------------------------------------

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_vIdx));
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEleVIdx), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());
                VertexBufferView vbvs[] = { vertexBuffer0.get() };
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                // NOTE -- special case in AppleMetal implementation -- the shader must be bound
                // here first, before we get to the uniform binding
                metalContext.Bind(shaderProgram);

                UniformsStreamInterface usi;
                usi.BindShaderResource(0, Hash64("Texture"));
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

                Metal::ShaderResourceView srv { Metal::GetObjectFactory(), testTexture._res };
                Metal::SamplerState pointSampler { FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp };

                UniformsStream uniformsStream;
                const Metal::ShaderResourceView* srvs[] = { &srv };
                const Metal::SamplerState* samplers[] = { &pointSampler };
                uniformsStream._resources = UniformsStream::MakeResources(MakeIteratorRange(srvs));
                uniformsStream._samplers = UniformsStream::MakeResources(MakeIteratorRange(samplers));
                uniforms.Apply(metalContext, 0, uniformsStream);

                metalContext.Bind(Topology::TriangleStrip);
                metalContext.Draw(4);
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            // We're expecting the output texture to directly match the input, just scaled up by
            // the dimensional difference. Since we're using point sampling, there should be no
            // filtering applied
            auto data = fbHelper._target->ReadBack(*threadContext);
            assert(data.size() == (size_t)RenderCore::ByteCount(targetDesc));
            auto pixels = MakeIteratorRange((unsigned*)AsPointer(data.begin()), (unsigned*)AsPointer(data.end()));

            for (unsigned y=0; y<targetDesc._textureDesc._height; ++y)
                for (unsigned x=0; x<targetDesc._textureDesc._width; ++x) {
                    unsigned inputX = x * 16 / targetDesc._textureDesc._width;
                    unsigned inputY = y * 16 / targetDesc._textureDesc._height;
                    Assert::AreEqual(pixels[y*targetDesc._textureDesc._width+x], testTexture._initData[inputY*16+inputX]);
                }
        }

        TEST_METHOD(BasicBinding_TextureSampling)
        {
            // -------------------------------------------------------------------------------------
            // Bind some geometry and sample a texture using a filtering sampler
            // -------------------------------------------------------------------------------------
            using namespace RenderCore;
            auto threadContext = _testHelper->_device->GetImmediateContext();
            auto shaderProgram = MakeShaderProgram(*_testHelper, vsText_FullViewport2, psText_TextureBinding);
            auto targetDesc = CreateDesc(
                BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
                TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
                "temporary-out");

            TestTexture testTexture(*_testHelper->_device);

            // -------------------------------------------------------------------------------------

            auto& metalContext = *Metal::DeviceContext::Get(*threadContext);
            UnitTestFBHelper fbHelper(*_testHelper->_device, *threadContext, targetDesc);
            auto rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_vIdx));
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEleVIdx), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());
                VertexBufferView vbvs[] = { vertexBuffer0.get() };
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                metalContext.Bind(shaderProgram);

                UniformsStreamInterface usi;
                usi.BindShaderResource(0, Hash64("Texture"));
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

                Metal::ShaderResourceView srv { Metal::GetObjectFactory(), testTexture._res };
                Metal::SamplerState pointSampler { FilterMode::Point, AddressMode::Clamp, AddressMode::Clamp };

                UniformsStream uniformsStream;
                const Metal::ShaderResourceView* srvs[] = { &srv };
                const Metal::SamplerState* samplers[] = { &pointSampler };
                uniformsStream._resources = UniformsStream::MakeResources(MakeIteratorRange(srvs));
                uniformsStream._samplers = UniformsStream::MakeResources(MakeIteratorRange(samplers));
                uniforms.Apply(metalContext, 0, uniformsStream);

                metalContext.Bind(Topology::TriangleStrip);
                metalContext.Draw(4);
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            auto breakdown = fbHelper.GetFullColorBreakdown();
            Assert::IsTrue(breakdown.size() == 2);       // if point sampling is working, we should have two colors

            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = fbHelper.BeginRenderPass();

            ////////////////////////////////////////////////////////////////////////////////////////
            {
                auto vertexBuffer0 = CreateVB(*_testHelper->_device, MakeIteratorRange(vertices_vIdx));
                Metal::BoundInputLayout inputLayout(MakeIteratorRange(inputEleVIdx), shaderProgram);
                Assert::IsTrue(inputLayout.AllAttributesBound());
                VertexBufferView vbvs[] = { vertexBuffer0.get() };
                inputLayout.Apply(metalContext, MakeIteratorRange(vbvs));

                metalContext.Bind(shaderProgram);

                UniformsStreamInterface usi;
                usi.BindShaderResource(0, Hash64("Texture"));
                Metal::BoundUniforms uniforms { shaderProgram, Metal::PipelineLayoutConfig {}, usi };

                Metal::ShaderResourceView srv { Metal::GetObjectFactory(), testTexture._res };
                Metal::SamplerState linearSampler { FilterMode::Bilinear, AddressMode::Clamp, AddressMode::Clamp };

                UniformsStream uniformsStream;
                const Metal::ShaderResourceView* srvs[] = { &srv };
                const Metal::SamplerState* samplers[] = { &linearSampler };
                uniformsStream._resources = UniformsStream::MakeResources(MakeIteratorRange(srvs));
                uniformsStream._samplers = UniformsStream::MakeResources(MakeIteratorRange(samplers));
                uniforms.Apply(metalContext, 0, uniformsStream);

                metalContext.Bind(Topology::TriangleStrip);
                metalContext.Draw(4);
            }
            ////////////////////////////////////////////////////////////////////////////////////////

            rpi = {};     // end RPI

            breakdown = fbHelper.GetFullColorBreakdown();
            Assert::IsTrue(breakdown.size() > 2);       // if filtering is working, we will get a large variety of colors
        }

        // error cases we could try:
        //      * not binding all attributes
        //      * refering to a vertex buffer in the InputElementDesc, and then not providing it
        //          in the Apply() method
        //      * providing a vertex buffer that isn't used at all (eg, unused attribute)
        //      * overlapping elements in the input binding
        //      * mismatched attribute
	};
}
