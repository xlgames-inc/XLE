
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

BasicLibrary(RenderCore_OpenGLES "${Src}" "${Headers}")

# Foreign Common 
target_include_directories(RenderCore_OpenGLES PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen)

