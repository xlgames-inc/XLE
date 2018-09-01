
if (GFX_API MATCHES "OpenGLES")
    # include("../OpenGLES/CMakeLists.txt")
    add_subdirectory(../OpenGLES RenderCoreMetalOpenGLES)
else ()
    # include("../DX11/CMakeLists.txt")
    add_subdirectory(../DX11 RenderCoreMetalDX11)
endif ()
