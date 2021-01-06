
set(Src
    "Device_EGL.cpp" 
    "EGLUtils.cpp"
    "Metal/Buffer.cpp"
    "Metal/DeviceContext.cpp"
    "Metal/FakeGLES.cpp"
    "Metal/Format.cpp"
    "Metal/FrameBuffer.cpp"
    "Metal/GPUSyncedAllocator.cpp"
    "Metal/InputLayout.cpp"
    "Metal/ObjectFactory.cpp"
    "Metal/PipelineLayout.cpp"
    "Metal/QueryPool.cpp"
    "Metal/Resource.cpp"
    "Metal/Shader.cpp"
    "Metal/ShaderIntrospection.cpp"
    "Metal/State.cpp"
    "Metal/TextureView.cpp")
add_definitions("-DGL_EXT_debug_marker=0")

add_library(RenderCore_OpenGLES STATIC ${Src} ${Headers})
configure_xle_library(RenderCore_OpenGLES)
target_link_libraries(RenderCore_OpenGLES PRIVATE ForeignMisc)