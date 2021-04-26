

# OpenGLES pretty much universal (at least emulatable via Desktop OpenGL)
if (XLE_OPENGLES_ENABLE)
    add_subdirectory(../OpenGLES RenderCoreMetalOpenGLES)
endif ()

# Vulkan supported everywhere except IOS & OSX
if (NOT APPLE AND XLE_VULKAN_ENABLE)
    add_subdirectory(../Vulkan RenderCoreMetalVulkan)
endif ()

# DX11 only supported on Microsoft platforms
if (WIN32 AND XLE_DX11_ENABLE)
    #add_subdirectory(../DX11 RenderCoreMetalDX11)
endif ()

# Apple Metal only on IOS & OSX
if (APPLE AND XLE_APPLEMETAL_ENABLE)
    add_subdirectory(../AppleMetal RenderCoreMetalAppleMetal)
endif ()
