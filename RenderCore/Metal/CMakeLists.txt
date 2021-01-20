

add_subdirectory(../OpenGLES RenderCoreMetalOpenGLES)
add_subdirectory(../Vulkan RenderCoreMetalVulkan)

# DX11 only supported on Microsoft platforms
if (WIN32)
    add_subdirectory(../DX11 RenderCoreMetalDX11)
endif ()
