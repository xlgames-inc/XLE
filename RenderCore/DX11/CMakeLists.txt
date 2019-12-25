
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

BasicLibrary(RenderCore_DX11 "${Src}" "${Headers}")
target_link_libraries(RenderCore_DX11 ForeignMisc)  # required to pull in interface include directories (etc)

