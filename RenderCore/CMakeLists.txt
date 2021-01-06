list(APPEND Src
    Format.cpp
    FrameBufferDesc.cpp
    Init.cpp
    MinimalShaderSource.cpp
    RenderUtils.cpp
    ResourceUtils.cpp
    ShaderLangUtil.cpp
    ShaderService.cpp
    StateDesc.cpp
    UniformsStream.cpp)
file(GLOB Headers "*.h")

add_library(RenderCore STATIC ${Src} ${Headers})
configure_xle_library(RenderCore)

target_link_libraries(RenderCore ForeignMisc)  # required to pull in interface include directories (etc)
