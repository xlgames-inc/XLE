
set(Src
    Device_EGL.cpp
    EGLUtils.cpp
    Metal/Buffer.cpp
    Metal/DeviceContext.cpp
    Metal/FakeGLES.cpp
    Metal/Format.cpp
    Metal/FrameBuffer.cpp
    Metal/GPUSyncedAllocator.cpp
    Metal/InputLayout.cpp
    Metal/ObjectFactory.cpp
    Metal/PipelineLayout.cpp
    Metal/QueryPool.cpp
    Metal/Resource.cpp
    Metal/Shader.cpp
    Metal/ShaderIntrospection.cpp
    Metal/State.cpp
    Metal/TextureView.cpp)

add_library(RenderCore_OpenGLES STATIC ${Src} ${Headers})
xle_configure_library(RenderCore_OpenGLES)
target_link_libraries(RenderCore_OpenGLES PUBLIC RenderCore Utility ForeignMisc)

############################################################################################################################################
set(XLE_OPENGLES_INCLUDE_DIR "" CACHE PATH "Include directory to use for OpenGLES/EGL")
set(XLE_OPENGLES_LIBRARY_DIR "" CACHE PATH "Library directory to use for OpenGLES/EGL")

macro(find_opengles)

    # CMake never reruns find_path, find_library, etc, while there's still a value in the 
    # variable we're making. This is a problem because the result of those operations can
    # depend on the value of other variables. To get around this, we can reset the values
    # when we detect a change
    if ((NOT INTERNAL_LAST_XLE_OPENGLES_INCLUDE_DIR STREQUAL ${XLE_OPENGLES_INCLUDE_DIR})
        OR (NOT INTERNAL_LAST_XLE_OPENGLES_LIBRARY_DIR STREQUAL ${XLE_OPENGLES_LIBRARY_DIR}))

        message("Detected change in GLES configuration, searching again")

        set(INTERNAL_FOUND_EGL_LIBRARY)
        set(INTERNAL_FOUND_EGL_INCLUDE_DIR)
        set(INTERNAL_FOUND_OPENGLES_LIBRARY)
        set(INTERNAL_FOUND_OPENGLES_INCLUDE_DIR)

        set(INTERNAL_LAST_XLE_OPENGLES_INCLUDE_DIR ${XLE_OPENGLES_INCLUDE_DIR} CACHE PATH INTERNAL FORCE)
        set(INTERNAL_LAST_XLE_OPENGLES_LIBRARY_DIR ${XLE_OPENGLES_LIBRARY_DIR} CACHE PATH INTERNAL FORCE)
    endif ()

    # First find "EGL"
    find_path(INTERNAL_FOUND_EGL_INCLUDE_DIR EGL/egl.h HINT ${XLE_OPENGLES_INCLUDE_DIR})
    find_library(INTERNAL_FOUND_EGL_LIBRARY EGL HINTS ${XLE_OPENGLES_LIBRARY_DIR})

    # check for .dll alternative
    if (INTERNAL_FOUND_EGL_LIBRARY STREQUAL INTERNAL_FOUND_EGL_LIBRARY-NOTFOUND)
        find_library(INTERNAL_FOUND_EGL_LIBRARY EGL.dll HINTS ${XLE_OPENGLES_LIBRARY_DIR})
    endif ()

    # If we've failed, try to fill in using find_package(OpenGL)
    if (INTERNAL_FOUND_EGL_INCLUDE_DIR STREQUAL INTERNAL_FOUND_EGL_INCLUDE_DIR-NOTFOUND
        OR INTERNAL_FOUND_EGL_LIBRARY STREQUAL INTERNAL_FOUND_EGL_LIBRARY-NOTFOUND)
    
        SET(OpenGL_GL_PREFERENCE GLVND) # set to quiet CMP0072. We don't actually care -- we want EGL, but not OpenGL
        find_package(OpenGL)

        if (OPENGL_egl_LIBRARY AND INTERNAL_FOUND_EGL_LIBRARY STREQUAL INTERNAL_FOUND_EGL_LIBRARY-NOTFOUND)
            # OPENGL_egl_LIBRARY set by OpenGL package
            SET(INTERNAL_FOUND_EGL_LIBRARY ${OPENGL_egl_LIBRARY})
        endif()

        if (OPENGL_EGL_INCLUDE_DIRS AND INTERNAL_FOUND_EGL_INCLUDE_DIR STREQUAL INTERNAL_FOUND_EGL_INCLUDE_DIR-NOTFOUND)
            # OPENGL_EGL_INCLUDE_DIRS set by OpenGL package
            SET(INTERNAL_FOUND_EGL_INCLUDE_DIR ${OPENGL_EGL_INCLUDE_DIRS})
        endif()
    endif()

    # Now find GLES itself. Note that we need the gles3 headers, even if we drop
    # back to gles2 at runtime
    # There isn't a module built into cmake for this. So we have to do everything manually
    # Basic search for headers and libraries
    find_path(INTERNAL_FOUND_OPENGLES_INCLUDE_DIR GLES3/gl3.h HINT ${XLE_OPENGLES_INCLUDE_DIR})
    find_library(INTERNAL_FOUND_OPENGLES_LIBRARY GLESv2 ${XLE_OPENGLES_LIBRARY_DIR})

    # check for .dll alternative
    if (INTERNAL_FOUND_OPENGLES_LIBRARY STREQUAL INTERNAL_FOUND_OPENGLES_LIBRARY-NOTFOUND)
        find_library(INTERNAL_FOUND_OPENGLES_LIBRARY GLESv2.dll ${XLE_OPENGLES_LIBRARY_DIR})
    endif ()

    if (INTERNAL_FOUND_EGL_LIBRARY STREQUAL INTERNAL_FOUND_EGL_LIBRARY-NOTFOUND 
        OR INTERNAL_FOUND_EGL_INCLUDE_DIR STREQUAL INTERNAL_FOUND_EGL_INCLUDE_DIR-NOTFOUND 
        OR INTERNAL_FOUND_OPENGLES_LIBRARY STREQUAL INTERNAL_FOUND_OPENGLES_LIBRARY-NOTFOUND 
        OR INTERNAL_FOUND_OPENGLES_INCLUDE_DIR STREQUAL INTERNAL_FOUND_OPENGLES_INCLUDE_DIR-NOTFOUND)

        # If we fail, spit out an error message
        # Most of the time, the remedy will be to just set XLE_OPENGLES_INCLUDE_DIR and XLE_OPENGLES_LIBRARY_DIR in the cmake cache
        # note that on Windows you must use Angle project to provide an OpenGLES implementation.
        # Usually this will come from here: https://github.com/google/angle
        message("INTERNAL_FOUND_EGL_LIBRARY: ${INTERNAL_FOUND_EGL_LIBRARY}")
        message("INTERNAL_FOUND_EGL_INCLUDE_DIR: ${INTERNAL_FOUND_EGL_INCLUDE_DIR}")
        message("INTERNAL_FOUND_OPENGLES_LIBRARY: ${INTERNAL_FOUND_OPENGLES_LIBRARY}")
        message("INTERNAL_FOUND_OPENGLES_INCLUDE_DIR: ${INTERNAL_FOUND_OPENGLES_INCLUDE_DIR}")
        message("XLE_OPENGLES_INCLUDE_DIR: ${XLE_OPENGLES_INCLUDE_DIR}")
        message("XLE_OPENGLES_LIBRARY_DIR: ${XLE_OPENGLES_LIBRARY_DIR}")
        message(FATAL_ERROR "Could not find OpenGLES headers and libraries. Either compile without this target included, or set the XLE_OPENGLES_INCLUDE_DIR and XLE_OPENGLES_LIBRARY_DIR cache directories to the correct directories.")
    endif ()

    mark_as_advanced(INTERNAL_FOUND_EGL_LIBRARY)
    mark_as_advanced(INTERNAL_FOUND_EGL_INCLUDE_DIR)
    mark_as_advanced(INTERNAL_FOUND_OPENGLES_LIBRARY)
    mark_as_advanced(INTERNAL_FOUND_OPENGLES_INCLUDE_DIR)
    mark_as_advanced(INTERNAL_LAST_XLE_OPENGLES_INCLUDE_DIR)
    mark_as_advanced(INTERNAL_LAST_XLE_OPENGLES_LIBRARY_DIR)

endmacro()
############################################################################################################################################

find_opengles()
target_link_libraries(RenderCore_OpenGLES PUBLIC ${INTERNAL_FOUND_EGL_LIBRARY} ${INTERNAL_FOUND_OPENGLES_LIBRARY})
target_include_directories(RenderCore_OpenGLES PUBLIC ${INTERNAL_FOUND_OPENGLES_INCLUDE_DIR} ${INTERNAL_FOUND_EGL_INCLUDE_DIR})

# If our library files are actually DLLs, we have to mark them as required DLLs (and any client projects must be careful to
# copy them to the correct location)
# We do this by setting a list called "RequiredDLLs" in the target properties for this target
# I'm not sure if there's a more straightforward way to mark this dependency and have it automatically handled by the final
# executable... We don't really want to use install steps or anything like that; 
string(FIND ${INTERNAL_FOUND_EGL_LIBRARY} ".dll.lib" DLLTEMP)
if (DLLTEMP)
    string(REPLACE ".dll.lib" ".dll" DLLTEMP ${INTERNAL_FOUND_EGL_LIBRARY})
    set_property(TARGET RenderCore_OpenGLES APPEND PROPERTY RequiredDLLs ${DLLTEMP})
endif ()
string(FIND ${INTERNAL_FOUND_OPENGLES_LIBRARY} ".dll.lib" DLLTEMP)
if (DLLTEMP)
    string(REPLACE ".dll.lib" ".dll" DLLTEMP ${INTERNAL_FOUND_OPENGLES_LIBRARY})
    set_property(TARGET RenderCore_OpenGLES APPEND PROPERTY RequiredDLLs ${DLLTEMP})
endif ()
unset(DLLTEMP)

# For Release Angle, we need libEGL.dll & libGLESv2.dll copied to the output directory
# for Debug Angle, we also need absl.dll, zlib.dll & libc++.dll (however, maybe we can avoid some of those dependencies by compiling in MSVC mode for windows, rather than clang)

# If everything is configured exactly right, it might be possible to define KHRONOS_STATIC here to
# use the static versions of project angle on windows
