set(Src
    "${Src}"
    "Annotator.cpp"
    "Format.cpp"
    "FrameBufferDesc.cpp"
    "Init.cpp"
    "MinimalShaderSource.cpp"
    "RenderUtils.cpp"
    "ResourceUtils.cpp"
    "ShaderLangUtil.cpp"
    "ShaderService.cpp"
    "UniformsStream.cpp"
    "Version.cpp"
    )
if (APPLE)
    if (UNIX)
        set(Src "${Src}" "OpenGLES/Device_CGL.mm") # macos
    else()
        set(Src "${Src}" "OpenGLES/Device_EAGL.mm") # ios
    endif()
else()
    set(Src "${Src}" "OpenGLES/Device_EGL.cpp" "OpenGLES/EGLUtils.cpp") # win32, android, linux
endif()
set(Src
    "${Src}"
    "OpenGLES/Metal/Buffer.cpp"
    "OpenGLES/Metal/DeviceContext.cpp"
    "OpenGLES/Metal/Format.cpp"
    "OpenGLES/Metal/FrameBuffer.cpp"
    "OpenGLES/Metal/InputLayout.cpp"
    "OpenGLES/Metal/ObjectFactory.cpp"
    "OpenGLES/Metal/PipelineLayout.cpp"
    "OpenGLES/Metal/QueryPool.cpp"
    "OpenGLES/Metal/Resource.cpp"
    "OpenGLES/Metal/Shader.cpp"
    "OpenGLES/Metal/ShaderIntrospection.cpp"
    "OpenGLES/Metal/State.cpp"
    "OpenGLES/Metal/TextureView.cpp"
    )
file(GLOB Headers "*.h")

add_definitions("-DGL_EXT_debug_marker=0")

BasicLibrary(RenderCore "${Src}" "${Headers}")

# Foreign Common 
target_include_directories(RenderCore PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-2_1_2 ${FOREIGN_DIR})
