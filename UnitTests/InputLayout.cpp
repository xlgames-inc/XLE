// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
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
	static const char vsText_clipInput[] = R"(
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

    static const char vsText[] = R"(
		typedef struct
		{
			int2 position [[attribute(0)]];
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
			out.clipSpacePosition = float4(
                (v_in.position.x / 1024.f) * 2.0f - 1.0f,
                (v_in.position.y / 1024.f) * 2.0f - 1.0f, 0.0f, 1.0f);
			out.color = v_in.color;
			return out;
		}
	)";

    static const char vsText_Instanced[] = R"(
        typedef struct
        {
            int2 position [[attribute(0)]];
            float4 color [[attribute(1)]];
            int2 instanceOffset [[attribute(2)]];
        } AAPLVertex;

        typedef struct
        {
            float4 clipSpacePosition [[position]];
            float4 color;
        } RasterizerData;

        vertex RasterizerData vertexShader(AAPLVertex v_in [[stage_in]])
        {
            RasterizerData out;
            out.clipSpacePosition = float4(
                ((v_in.position.x + v_in.instanceOffset.x) / 1024.f) * 2.0f - 1.0f,
                ((v_in.position.y + v_in.instanceOffset.y) / 1024.f) * 2.0f - 1.0f, 0.0f, 1.0f);
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
#else
    static const char vsText[] = R"(
        #if defined(GL_ES)
            precision highp float;
        #endif

        #if __VERSION__ >= 300
            #define ATTRIBUTE in     /** legacy **/
            #define VARYING_IN in
            #define VARYING_OUT out
        #else
            #define ATTRIBUTE attribute     /** legacy **/
            #define VARYING_IN varying
            #define VARYING_OUT varying
        #endif

        ATTRIBUTE vec4 position;
        ATTRIBUTE vec4 color;
        VARYING_OUT vec4 a_color;

        void main()
        {
            gl_Position = position;
            a_color = color;
        }
    )";

    static const char psText[] = R"(
        #if defined(GL_ES)
            precision highp float;
        #endif

        #if __VERSION__ >= 300
            #define ATTRIBUTE in     /** legacy **/
            #define VARYING_IN in
            #define VARYING_OUT out
        #else
            #define ATTRIBUTE attribute     /** legacy **/
            #define VARYING_IN varying
            #define VARYING_OUT varying
        #endif

        VARYING_IN vec4 a_color;

        void main()
        {
            gl_FragColor = a_color;
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

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    U T I L I T Y    F N S

    static std::shared_ptr<RenderCore::IResource> CreateVB(RenderCore::IDevice& device, IteratorRange<const void*> data)
    {
        using namespace RenderCore;
        return device.CreateResource(
            CreateDesc(
                BindFlag::VertexBuffer, 0, GPUAccess::Read,
                LinearBufferDesc::Create((unsigned)data.size()),
                "vertexBuffer"),
            [data](SubResourceId) -> SubResourceInitData { return SubResourceInitData { data }; });
    }

    static RenderCore::Metal::ShaderProgram MakeShaderProgram(MetalTestHelper& testHelper, StringSection<> vs, StringSection<> ps)
    {
        return RenderCore::Metal::ShaderProgram(RenderCore::Metal::GetObjectFactory(), testHelper.MakeShader(vs, "vs_*"), testHelper.MakeShader(ps, "ps_*"));
    }

    class UnitTestFBHelper
    {
    public:
        RenderCore::Techniques::AttachmentPool _namedResources;
        RenderCore::Techniques::FrameBufferPool _frameBufferPool;
        RenderCore::Techniques::TechniqueContext _techniqueContext;
        RenderCore::Techniques::ParsingContext _parsingContext;     // careful init-order rules

        std::shared_ptr<RenderCore::IResource> _target;

        UnitTestFBHelper(RenderCore::IDevice& device, RenderCore::IThreadContext& threadContext, const RenderCore::ResourceDesc& targetDesc)
        : _parsingContext(_techniqueContext, &_namedResources, &_frameBufferPool)     // careful init-order rules
        {
            auto* metalThreadContext = (RenderCore::ImplAppleMetal::ThreadContext*)threadContext.QueryInterface(typeid(RenderCore::ImplAppleMetal::ThreadContext).hash_code());
            if (metalThreadContext)
                metalThreadContext->BeginHeadlessFrame();

            _target = device.CreateResource(targetDesc);
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

        RenderCore::Techniques::RenderPassInstance BeginRenderPass()
        {
            auto targetDesc = _target->GetDesc();
            RenderCore::Metal::DeviceContext::Get(*_threadContext)->Bind(
                RenderCore::Metal::ViewportDesc{ 0.f, 0.f, (float)targetDesc._textureDesc._width, (float)targetDesc._textureDesc._height });
            return RenderCore::Techniques::RenderPassToPresentationTarget(*_threadContext, _target, _parsingContext, RenderCore::LoadStore::Clear);
        }

        struct ColorBreakdown
        {
            unsigned _blackPixels = 0;
            unsigned _coloredPixels[dimof(fixedColors)] = {};
            unsigned _otherPixels = 0;
        };

        ColorBreakdown GetColorBreakdown()
        {
            ColorBreakdown result;

            auto data = _target->ReadBack(*_threadContext);

            assert(data.size() == (size_t)RenderCore::ByteCount(_target->GetDesc()));
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

            auto colorBreakdown = fbHelper.GetColorBreakdown();
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
		}

        TEST_METHOD(BasicBinding_ShortForm)
        {
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

            auto colorBreakdown = fbHelper.GetColorBreakdown();
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
        }

        TEST_METHOD(BasicBinding_2VBs)
        {
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

            auto colorBreakdown = fbHelper.GetColorBreakdown();
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

            colorBreakdown = fbHelper.GetColorBreakdown();
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            Assert::AreEqual(colorBreakdown._coloredPixels[0], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[1], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[2], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[3], boxesSize);
            Assert::AreEqual(colorBreakdown._blackPixels, targetDesc._textureDesc._width * targetDesc._textureDesc._height - 4 * boxesSize);
        }

        TEST_METHOD(BasicBinding_DataRate)
        {
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

            auto colorBreakdown = fbHelper.GetColorBreakdown();
            Assert::AreEqual(colorBreakdown._otherPixels, 0u);
            Assert::AreEqual(colorBreakdown._coloredPixels[0], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[1], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[2], boxesSize);
            Assert::AreEqual(colorBreakdown._coloredPixels[3], boxesSize);
            Assert::AreEqual(colorBreakdown._blackPixels, targetDesc._textureDesc._width * targetDesc._textureDesc._height - 4 * boxesSize);
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
