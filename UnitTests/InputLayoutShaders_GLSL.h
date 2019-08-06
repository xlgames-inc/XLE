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
                #define VARYING_IN in
                #define VARYING_OUT out
            #else
                #define ATTRIBUTE attribute     /** legacy **/
                #define VARYING_IN varying
                #define VARYING_OUT varying
            #endif
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
            VARYING_OUT vec4 a_color;
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

    static const char vsText[] = 
        GLSLPrefix
        InputVertexPI2C
        VaryingsC
        R"(
            void main()
            {
                gl_Position.x = (position.x / 1024.0) *  2.0 - 1.0;
                gl_Position.y = (position.y / 1024.0) * -2.0 + 1.0;
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
                gl_Position.x = ((position.x + instanceOffset.x) / 1024.0) *  2.0 - 1.0;
                gl_Position.y = ((position.y + instanceOffset.y) / 1024.0) * -2.0 + 1.0;
                gl_Position.zw = vec2(0.0, 1.0);
                a_color = color;
            }
        )";

    static const char vsText_FullViewport[] =
        GLSLPrefix
        R"(
            void main()
            {
                #if __VERSION__ >= 300
                    gl_Position = vec4(
                        (gl_VertexID&1)        ? -1.0 :  1.0,
                        ((gl_VertexID>>1)&1)   ?  1.0 : -1.0,
                        0.0, 1.0
                    );
                #else
                    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
                #endif
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
}
