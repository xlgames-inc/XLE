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
                float A, B, C;
                float4 vA;
            };

            struct Values1Struct
            {
                float4 vA[4];
            };

            struct Values2Struct
            {
                unsigned dummy;
                metal::float4x4 dummy0;
            };

            struct Values3Struct
            {
                metal::float4x4 values;
            };
        )"

    static const char psText_ComplicatedUniformShader[] =
        VaryingsT
        ValuesStructs
        R"(
            fragment float4 fragmentShader(
                RasterizerData in [[stage_in]],
                constant Values0Struct* UsedValues,
                constant Values1Struct* UnusedValues,
                constant Values2Struct* UnusedValues2,
                constant Values3Struct* UsedValue2
                )
            {
                return UsedValues->vA + UsedValue2->values[1];
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
                constant Values2Struct& UnusedValues2;
                constant Values3Struct& UsedValue2;

                float UnusedFunction()
                {
                    return UnusedValues.vA[0].x + (float)UnusedValues2.dummy;
                }

                float4 CalculateResult()
                {
                    return UsedValues.vA + UsedValue2.values[1];
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
        R"(
            struct Globals
            {
        )"
        ValuesStructs
        R"(
                constant Values0Struct& UsedValues;
                constant Values1Struct& UnusedValues;
                constant Values2Struct& UnusedValues2;
                constant Values3Struct& UsedValue2;

                float UnusedFunction()
                {
                    return UnusedValues.vA[0].x + (float)UnusedValues2.dummy;
                }

                metal::float4x4 UnusedFunction2()
                {
                    return transpose(UnusedValues2.dummy0);
                }

                float4 CalculateResult()
                {
                    return UsedValues.vA + UsedValue2.values[1];
                }

                void main(thread float4& res, float4 input)
                {
                    // res = CalculateResult();
                    res = transpose(UsedValue2.values) * input + UsedValues.vA;
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
                float4 offset;
                gbls.main(offset, out.clipSpacePosition);
                out.clipSpacePosition += offset;
                return out;
            }
        )";

////////////////////////////////////////////////////////////////////////////////////////////////////
            //    C O D E

    TBC::OCPtr<MTLRenderPipelineReflection> MakeDefaultReflection(const RenderCore::Metal::ShaderProgram& shaderProgram)
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

            Assert::IsTrue(reflection1.get().fragmentArguments.count == 4);
            Assert::IsTrue([reflection1.get().fragmentArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection1.get().fragmentArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::IsTrue([reflection1.get().fragmentArguments[2].name isEqualToString:@"UnusedValues2"]);
            Assert::IsTrue([reflection1.get().fragmentArguments[3].name isEqualToString:@"UsedValue2"]);
            Assert::AreEqual(reflection1.get().fragmentArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection1.get().fragmentArguments[1].active, (BOOL)false);
            Assert::AreEqual(reflection1.get().fragmentArguments[2].active, (BOOL)false);
            Assert::AreEqual(reflection1.get().fragmentArguments[3].active, (BOOL)true);

            auto reflection2 = MakeDefaultReflection(
                MakeShaderProgram(*_testHelper, vsText_FullViewport, psText_ComplicatedUniformShader2));

            Assert::IsTrue(reflection2.get().fragmentArguments.count == 4);
            Assert::IsTrue([reflection2.get().fragmentArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection2.get().fragmentArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::IsTrue([reflection2.get().fragmentArguments[2].name isEqualToString:@"UnusedValues2"]);
            Assert::IsTrue([reflection2.get().fragmentArguments[3].name isEqualToString:@"UsedValue2"]);
            Assert::AreEqual(reflection2.get().fragmentArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection2.get().fragmentArguments[1].active, (BOOL)false);
            Assert::AreEqual(reflection2.get().fragmentArguments[2].active, (BOOL)false);
            Assert::AreEqual(reflection2.get().fragmentArguments[3].active, (BOOL)true);

            auto reflection3 = MakeDefaultReflection(
                MakeShaderProgram(*_testHelper, vsText_ComplicatedUniformShader, psText_TextureBinding));

            Assert::IsTrue(reflection3.get().vertexArguments.count == 4);
            Assert::IsTrue([reflection3.get().vertexArguments[0].name isEqualToString:@"UsedValues"]);
            Assert::IsTrue([reflection3.get().vertexArguments[1].name isEqualToString:@"UnusedValues"]);
            Assert::IsTrue([reflection3.get().vertexArguments[2].name isEqualToString:@"UnusedValues2"]);
            Assert::IsTrue([reflection3.get().vertexArguments[3].name isEqualToString:@"UsedValue2"]);
            Assert::AreEqual(reflection3.get().vertexArguments[0].active, (BOOL)true);
            Assert::AreEqual(reflection3.get().vertexArguments[1].active, (BOOL)false);
            Assert::AreEqual(reflection3.get().vertexArguments[2].active, (BOOL)false);
            Assert::AreEqual(reflection3.get().vertexArguments[3].active, (BOOL)true);
		}
	};
}
