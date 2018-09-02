
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
    "Metal/TextureView.cpp")

BasicLibrary(RenderCore_DX11 "${Src}" "${Headers}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen)
