// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace UnitTests
{
    #define InputVertexPC R"(
            typedef struct
            {
                float4 position [[attribute(0)]];
                float4 color [[attribute(1)]];
            } AAPLVertex;
        )"

    #define InputVertexPI2C R"(
            typedef struct
            {
                int2 position [[attribute(0)]];
                float4 color [[attribute(1)]];
            } AAPLVertex;
        )"

    #define VaryingsC R"(
            typedef struct
            {
                float4 clipSpacePosition [[position]];
                float4 color;
            } RasterizerData;
        )"

    #define VaryingsBasic R"(
            typedef struct
            {
                float4 clipSpacePosition [[position]];
            } RasterizerData;
        )"

	static const char vsText_clipInput[] =
        InputVertexPC
        VaryingsC
        R"(
            vertex RasterizerData vertexShader(AAPLVertex v_in [[stage_in]])
            {
                RasterizerData out;
                out.clipSpacePosition = v_in.position;
                out.color = v_in.color;
                return out;
            }
        )";

    static const char vsText[] =
        InputVertexPI2C
        VaryingsC
        R"(
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

    static const char vsText_Instanced[] =
        R"(
            typedef struct
            {
                int2 position [[attribute(0)]];
                float4 color [[attribute(1)]];
                int2 instanceOffset [[attribute(2)]];
            } AAPLVertex;
        )"
        VaryingsC
        R"(
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

    static const char vsText_FullViewport[] =
        VaryingsBasic
        R"(
            vertex RasterizerData vertexShader(uint vertexID [[vertex_id]])
            {
                RasterizerData out;
                out.clipSpacePosition = float4(
                    (vertexID&1)        ? -1.0f :  1.0f,
                    ((vertexID>>1)&1)   ? -1.0f :  1.0f,
                    0.0f, 1.0f
                );
                return out;
            }
        )";

    static const char psText[] =
        VaryingsC
        R"(
            fragment float4 fragmentShader(RasterizerData in [[stage_in]])
            {
                return in.color;
            }
        )";

    static const char psText_Uniforms[] =
        VaryingsBasic
        R"(
            struct ValuesStruct
            {
                float A, B, C;
                float4 vA;
            };

            fragment float4 fragmentShader(
                RasterizerData in [[stage_in]],
                constant ValuesStruct* Values [[buffer(1)]])
            {
                return float4(Values->A, Values->B, Values->vA.x, Values->vA.y);
            }
        )";
}
