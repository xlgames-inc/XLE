// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/ResourceDesc.h"
#include <map>

#include "InputLayoutShaders_MSL.h"
#include <Metal/MTLRenderPipeline.h>

#if GFXAPI_TARGET != GFXAPI_APPLEMETAL
    #error this test is intended for Apple Metal only
#endif

namespace UnitTests
{

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    T E S T   I N P U T   D A T A

    #define ValuesStructs R"(
            struct Values0Struct
            {
                metal::float4x4 worldToClip;
            };

            struct Values1Struct
            {
            };
        )"

    static const char psText_ComplicatedUniformShader[] =
        VaryingsT
        ValuesStructs
        R"(
            fragment float4 fragmentShader(
                RasterizerData in [[stage_in]],
                constant Values0Struct* UsedValues,
                constant Values1Struct* UnusedValues)
            {
                return UsedValues->worldToClip[0];
            }
        )";

    static const char psText_ComplicatedUniformShader2[] =
        VaryingsT
        ValuesStructs
        R"(
            struct Globals
            {
                constant Values0Struct& UsedValues;
                constant Values1Struct& UnusedValues;

                metal::float4x4 getWorldToClip() { return transpose(UsedValues.worldToClip); }

                float4 CalculateResult()
                {
                    return getWorldToClip()[0];
                }

                void main(thread float4& res)
                {
                    res = CalculateResult();
                }
            };

            fragment float4 fragmentShader(RasterizerData in [[stage_in]], Globals gbls)
            {
                float4 v;
                gbls.main(v);
                return v;
            }
        )";

    static const char vsText_ComplicatedUniformShader[] =
        VaryingsT
        ValuesStructs
        R"(
            metal::float4x4 fake_transpose(metal::float4x4 inMatrix)
            {
                float4 i0 = inMatrix[0];
                float4 i1 = inMatrix[1];
                float4 i2 = inMatrix[2];
                float4 i3 = inMatrix[3];

                metal::float4x4 outMatrix = metal::float4x4(
                                float4(i0.x, i1.x, i2.x, i3.x),
                                float4(i0.y, i1.y, i2.y, i3.y),
                                float4(i0.z, i1.z, i2.z, i3.z),
                                float4(i0.w, i1.w, i2.w, i3.w));
                return outMatrix;
            }
        )"
        R"(
            struct Globals
            {
                constant Values0Struct& UsedValues;
                constant Values1Struct& UnusedValues;

                metal::float4x4 getWorldToClip() { return transpose(UsedValues.worldToClip); }

                void main(thread float4& res, float4 input)
                {
                    res = getWorldToClip() * input;
                }
            };

            vertex RasterizerData vertexShader(uint vertexID [[vertex_id]], Globals gbls)
            {
                RasterizerData out;
                out.texCoord = float2(
                    (vertexID&1)        ? 0.0f :  1.0f,
                    ((vertexID>>1)&1)   ? 0.0f :  1.0f);
                out.clipSpacePosition = float4(
                    out.texCoord.x *  2.0f - 1.0f,
                    out.texCoord.y * -2.0f + 1.0f,
                    0.0f, 1.0f
                );
                float4 offset; // = gbls.getWorldToClip() * out.clipSpacePosition;
                gbls.main(offset, out.clipSpacePosition);
                out.clipSpacePosition += offset;
                return out;
            }
        )";

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    C O D E

    OCPtr<MTLRenderPipelineReflection> MakeDefaultReflection(const RenderCore::Metal::ShaderProgram& shaderProgram)
    {
        using namespace RenderCore;
        FrameBufferProperties fbProps { 1024, 1024, TextureSamples::Create() };
        FrameBufferDesc::Attachment attachment { 0, { Format::R8G8B8A8_UNORM } };
        SubpassDesc sp;
        sp.AppendOutput(0);

        FrameBufferDesc fbDesc {
            std::vector<FrameBufferDesc::Attachment> { attachment },
            std::vector<SubpassDesc> { sp },
        };

        Metal::GraphicsPipelineBuilder pipelineBuilder;
        pipelineBuilder.SetRenderPassConfiguration(fbProps, fbDesc, 0);
        pipelineBuilder.Bind(shaderProgram);

        auto pipeline = pipelineBuilder.CreatePipeline(Metal::GetObjectFactory());
        return pipeline->_reflection;
    }

    TEST_CLASS(MSLSpecific)
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

		TEST_METHOD(ComplicatedBinding)
		{
            // -------------------------------------------------------------------------------------
            // Create some shaders that have a complicated uniform interface, and check that the
            // "active" flags on each uniform is as expected
            // -------------------------------------------------------------------------------------
			using namespace RenderCore;

            auto reflection1 = MakeDefaultReflection(
                MakeShaderProgram(*_testHelper, vsText_FullViewport, psText_ComplicatedUniformShader));

            Assert::IsTrue(reflection1.get().fragmentArguments.count == 2);
            Assert::IsTrue([reflection1.get().fragmentArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection1.get().fragmentArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::AreEqual(reflection1.get().fragmentArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection1.get().fragmentArguments[1].active, (BOOL)false);

            auto reflection2 = MakeDefaultReflection(
                MakeShaderProgram(*_testHelper, vsText_FullViewport, psText_ComplicatedUniformShader2));

            Assert::IsTrue(reflection2.get().fragmentArguments.count == 2);
            Assert::IsTrue([reflection2.get().fragmentArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection2.get().fragmentArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::AreEqual(reflection2.get().fragmentArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection2.get().fragmentArguments[1].active, (BOOL)false);

            auto reflection3 = MakeDefaultReflection(
                MakeShaderProgram(*_testHelper, vsText_ComplicatedUniformShader, psText_TextureBinding));

            Assert::IsTrue(reflection3.get().vertexArguments.count == 2);
            Assert::IsTrue([reflection3.get().vertexArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection3.get().vertexArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::AreEqual(reflection3.get().vertexArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection3.get().vertexArguments[1].active, (BOOL)false); /* Ideally, this assertion would pass.  However, it seems that there is a bug in the shader compilation or reflecting that results in unused values being active.  That is, we currently expect this to fail. */
		}
	};
}
