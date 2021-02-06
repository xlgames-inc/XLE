

# OpenGLES pretty much universal (at least emulatable via Desktop OpenGL)
add_subdirectory(../OpenGLES RenderCoreMetalOpenGLES)

# Vulkan supported everywhere except IOS & OSX
if (NOT APPLE)
    add_subdirectory(../Vulkan RenderCoreMetalVulkan)
endif ()

# DX11 only supported on Microsoft platforms
if (WIN32)
    add_subdirectory(../DX11 RenderCoreMetalDX11)
endif ()

# Apple Metal only on IOS & OSX
if (APPLE)
    add_subdirectory(../AppleMetal RenderCoreMetalAppleMetal)
endif ()
