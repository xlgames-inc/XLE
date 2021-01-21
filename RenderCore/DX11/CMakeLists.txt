
set(Src
    "Device.cpp" 
    "Annotator.cpp"
    "Metal/Buffer.cpp"
    "Metal/CompiledShaderByteCode.cpp"
    "Metal/DeviceContext.cpp"
    "Metal/DX11Utils.cpp"
    "Metal/Format.cpp"
    "Metal/FrameBuffer.cpp"
    "Metal/InputLayout.cpp"
    "Metal/ObjectFactory.cpp"
    "Metal/QueryPool.cpp"
    "Metal/Resource.cpp"
    "Metal/Shader.cpp"
    "Metal/State.cpp"
    "Metal/TextureView.cpp"
    "Metal/PipelineLayout.cpp")

add_library(RenderCore_DX11 STATIC ${Src} ${Headers})
xle_configure_library(RenderCore_DX11)
target_link_libraries(RenderCore_DX11 PRIVATE ForeignMisc)