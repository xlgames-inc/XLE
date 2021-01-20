
set(Src
    Annotator.cpp
    DeviceVulkan.cpp
    Metal/Buffer.cpp
    Metal/CompiledShaderByteCode.cpp
    Metal/DescriptorSet.cpp
    Metal/DeviceContext.cpp
    Metal/Format.cpp
    Metal/FrameBuffer.cpp
    Metal/InputLayout.cpp
    Metal/NumericInterface.cpp
    Metal/ObjectFactory.cpp
    Metal/PipelineLayout.cpp
    Metal/PipelineLayoutSignatureFile.cpp
    Metal/Pools.cpp
    Metal/QueryPool.cpp
    Metal/Resource.cpp
    Metal/Shader.cpp
    Metal/ShaderReflection.cpp
    Metal/State.cpp
    Metal/TextureView.cpp)

add_library(RenderCore_Vulkan STATIC ${Src} ${Headers})
configure_xle_library(RenderCore_Vulkan)
target_link_libraries(RenderCore_Vulkan PRIVATE ForeignMisc)
