// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace UnitTests
{
    #define GLSLPrefix R"(
            #if defined(GL_ES)
                precision highp float;
            #endif

            #if __VERSION__ >= 300
                #define ATTRIBUTE in     /** legacy **/
                #if defined(FRAGMENT_SHADER)
                    #define VARYING in
                #else
                    #define VARYING out
                #endif
            #else
                #define ATTRIBUTE attribute     /** legacy **/
                #define VARYING varying
            #endif

            int fakeMod(int lhs, int rhs)
            {
                // only valid for positive values
                float A = float(lhs) / float(rhs);
                return int((A - floor(A)) * float(rhs));
            }
        )"

    #define InputVertexPC R"(
            ATTRIBUTE vec4 position;
            ATTRIBUTE vec4 color;
        )"

    #define InputVertexPI2C R"(
            ATTRIBUTE vec2 position;
            ATTRIBUTE vec4 color;
        )"

    #define VaryingsC R"(
            VARYING vec4 a_color;
        )"

    #define VaryingsT R"(
            VARYING vec2 a_texCoord;
        )"

    #define VaryingsBasic R"(
        )"

    static const char vsText_clipInput[] = 
        GLSLPrefix
        InputVertexPC
        VaryingsC
        R"(
            void main()
            {
                gl_Position = position;
                a_color = color;
            }
        )";

    static const char vsText_clipInputTransform[] = 
        GLSLPrefix
        InputVertexPC
        VaryingsC
        R"(
            uniform struct
            {
                mat4 inputToClip;
            } Transform;

            void main()
            {
                gl_Position = transpose(Transform.inputToClip) * position;
                a_color = color;
            }
        )";

    static const char vsText[] = 
        GLSLPrefix
        InputVertexPI2C
        VaryingsC
        R"(
            void main()
            {
                gl_Position.x = (position.x / 1024.0) * 2.0 - 1.0;
                gl_Position.y = (position.y / 1024.0) * 2.0 - 1.0;
                gl_Position.zw = vec2(0.0, 1.0);
                a_color = color;
            }
        )";

    static const char vsText_Instanced[] =
        GLSLPrefix
        R"(
            ATTRIBUTE vec2 position;
            ATTRIBUTE vec4 color;
            ATTRIBUTE vec2 instanceOffset;
        )"
        VaryingsC
        R"(
            void main()
            {
                gl_Position.x = ((position.x + instanceOffset.x) / 1024.0) * 2.0 - 1.0;
                gl_Position.y = ((position.y + instanceOffset.y) / 1024.0) * 2.0 - 1.0;
                gl_Position.zw = vec2(0.0, 1.0);
                a_color = color;
            }
        )";

    static const char vsText_FullViewport[] =
        GLSLPrefix
        VaryingsT
        R"(
            void main()
            {
                #if __VERSION__ >= 300
                    int in_vertexID = gl_VertexID;
                #else
                    int in_vertexID = 0;
                    #error Vertex Generator shaders not supported in this version of GLSL
                #endif

                a_texCoord = vec2(
                    (fakeMod(in_vertexID, 2) == 1)     ? 0.0f :  1.0f,
                    (fakeMod(in_vertexID/2, 2) == 1) ? 0.0f :  1.0f);
                gl_Position = vec4(
                    a_texCoord.x *  2.0f + 1.0f,
                    a_texCoord.y *  2.0f + 1.0f,
                    0.0, 1.0
                );
            }
        )";

    static const char vsText_FullViewport2[] =
        GLSLPrefix
        VaryingsT
        R"(
            ATTRIBUTE float vertexID;
            void main()
            {
                int in_vertexID = int(vertexID);
                a_texCoord = vec2(
                    (fakeMod(in_vertexID, 2) == 1)     ? 0.0f :  1.0f,
                    (fakeMod(in_vertexID/2, 2) == 1) ? 0.0f :  1.0f);
                gl_Position = vec4(
                    a_texCoord.x *  2.0f - 1.0f,
                    a_texCoord.y *  2.0f - 1.0f,
                    0.0, 1.0
                );
            }
        )";

    static const char psText[] = 
        GLSLPrefix
        VaryingsC
        R"(
            void main()
            {
                gl_FragColor = a_color;
            }
        )";

    static const char psText_Uniforms[] =
        VaryingsBasic
        R"(
            uniform struct
            {
                float A, B, C;
                vec4 vA;
            } Values;

            void main()
            {
                gl_FragColor = vec4(Values.A, Values.B, Values.vA.x, Values.vA.y);
            }
        )";

    static const char psText_TextureBinding[] = 
        GLSLPrefix
        VaryingsT
        R"(
            uniform sampler2D Texture;
            void main()
            {
                gl_FragColor = texture2D(Texture, a_texCoord);
            }
        )";
}
