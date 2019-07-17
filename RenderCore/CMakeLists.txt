list(APPEND Src
	Annotator.cpp
    Format.cpp
    FrameBufferDesc.cpp
    Init.cpp
    MinimalShaderSource.cpp
    RenderUtils.cpp
    ResourceUtils.cpp
    ShaderLangUtil.cpp
    ShaderService.cpp
    UniformsStream.cpp
	Version.cpp)
file(GLOB Headers "*.h")

BasicLibrary(RenderCore "${Src}" "${Headers}")

# Foreign Common 
target_include_directories(RenderCore PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-1_0_2 ${FOREIGN_DIR})
