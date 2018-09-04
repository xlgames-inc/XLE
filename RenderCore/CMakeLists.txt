set(Src
    Format.cpp
    FrameBufferDesc.cpp
    Init.cpp
    MinimalShaderSource.cpp
    RenderUtils.cpp
    ResourceUtils.cpp
    ShaderLangUtil.cpp
    ShaderService.cpp
    UniformsStream.cpp)
file(GLOB Headers "*.h")

BasicLibrary(RenderCore "${Src}" "${Headers}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen)
