# setup basic XLE_DIR & FOREIGN_DIR macros
get_filename_component(XLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)
get_filename_component(FOREIGN_DIR "${XLE_DIR}/Foreign/" ABSOLUTE)
get_filename_component(XLE_MAIN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# See https://stackoverflow.com/questions/375913/how-can-i-profile-c-code-running-on-linux for some examples of methods to profile
# on linux, including tools that can interpret the -pg instrumentation output
# The profiling built into -pg seems to add a fair bit of overhead to the executable -- so I'm not sure how effective
# it is for profiling at a very fine grained level
set(XLE_LINUX_GPROF OFF CACHE BOOL "Enable the -pg clang command line option; this tells the compiler to add profiling instrumentation to the code.")

# Clang can add debugging instrumentation into the code to check for errors related to memory usage and threading
# These are called sanitizers, such as the address sanitizer, memory sanitizer and thread sanitizer
# They each add quite a lot of overhead, so generally it's best to only use one at a time
set(XLE_ADDRESS_SANITIZER OFF CACHE BOOL "Enable the address sanitizer when compiling with clang")
set(XLE_MEMORY_SANITIZER OFF CACHE BOOL "Enable the memory sanitizer when compiling with clang")
set(XLE_THREAD_SANITIZER OFF CACHE BOOL "Enable the thread sanitizer when compiling with clang")

# clang is quite lenient with shared libraries, and will allow symbols to be implicitly imported from shared libraries
# This works both ways -- ie, an executable might resolve a symbol by importing it from a shared library it links it.
# But also the also the shared library itself might import symbols from the loading executable.
#
# Microsoft's compilers have traditionally been much more explicit about this interface -- ie, you would specifically
# mark symbols that you wanted to import or export. In principle, this helps us be more careful about what symbols
# are used from each module. That can sometimes help avoid issues, particularly if we're worried about compatibility
# issues and dealing with libraries that were compiled at different times.
#
# Anyway, the default approach with XLE is the explicit/Microsoft way; but that may be partially just a result of
# the history of the project.
set(XLE_IMPLICIT_SHARED_LIBRARIES OFF CACHE BOOL "Set to allow clang to implicitly export and import symbols across the DLL interface. Normally this is set to OFF with XLE, as interface symbols are explicitly marked in the code.")

macro(xle_internal_configure_compiler TargetName)
    target_compile_features(${TargetName} PUBLIC cxx_std_17)
    set_property(TARGET ${TargetName} PROPERTY CXX_EXTENSIONS OFF)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${TargetName} PUBLIC XL_DEBUG _DEBUG)
    elseif ( (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        target_compile_definitions(${TargetName} PUBLIC XL_RELEASE NDEBUG)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # We must use -fPIC to enable relocation for any code that might be linked into a shared library
        # In theory, this might only need to be done for the code that ends up in a shared library. But in
        # practice it's only really practical to enable it for everything
        target_compile_options(${TargetName} PRIVATE -fPIC)

        if (NOT XLE_IMPLICIT_SHARED_LIBRARIES)
            target_compile_options(${TargetName} PRIVATE -fvisibility=hidden)
        endif()

        if (XLE_LINUX_GPROF)
            target_compile_options(${TargetName} PRIVATE -pg -g)
        endif()

        if (XLE_ADDRESS_SANITIZER)
            target_compile_options(${TargetName} PRIVATE -g -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${TargetName} PRIVATE -fsanitize=address)
        endif()

        if (XLE_MEMORY_SANITIZER)
            target_compile_options(${TargetName} PRIVATE -g -fsanitize=memory -fno-omit-frame-pointer)
            target_link_options(${TargetName} PRIVATE -fsanitize=memory)
        endif()

        if (XLE_THREAD_SANITIZER)
            target_compile_options(${TargetName} PRIVATE -g -fsanitize=thread)
            target_link_options(${TargetName} PRIVATE -fsanitize=thread)
        endif()
    endif()
endmacro()

macro(xle_configure_library LibName)
    if (MSVC)
        set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${XLE_MAIN_CMAKE_DIR}/Main.props")
    endif ()
    target_include_directories(${LibName} PRIVATE ${XLE_DIR})
    xle_internal_configure_compiler(${LibName})
endmacro()

macro(xle_configure_executable ExeName)
    if (MSVC)
        # The CMake Visual Studio generator has a hack that disables the LinkLibraryDependencies setting in
        # the output project (see cmGlobalVisualStudio8Generator::NeedLinkLibraryDependencies) unless there are
        # external project dependencies. It's frustrating because we absolutely need that on. To get around the
        # problem, we'll link in a dummy external project that just contains nothing. This causes cmake to
        # enable the LinkLibraryDependencies flag, and hopefully has no other side effects.
        include_external_msproject(generator_dummy ${XLE_MAIN_CMAKE_DIR}/generator_dummy.vcxproj)
        add_dependencies(${ExeName} generator_dummy)
        set_target_properties(${ExeName} PROPERTIES VS_USER_PROPS "${XLE_MAIN_CMAKE_DIR}/Main.props")
    endif (MSVC)

    target_include_directories(${ExeName} PRIVATE ${XLE_DIR})

    xle_internal_configure_compiler(${ExeName})

    if (NOT XLE_IMPLICIT_SHARED_LIBRARIES)
        if (NOT XLE_MEMORY_SANITIZER AND NOT XLE_ADDRESS_SANITIZER AND NOT XLE_THREAD_SANITIZER)
            target_link_options(${ExeName} PRIVATE -Wl,--unresolved-symbols=report-all)
        endif()
    endif()
endmacro()

macro(xle_configure_dll DllName)
    if (MSVC)
        # The CMake Visual Studio generator has a hack that disables the LinkLibraryDependencies setting in
        # the output project (see cmGlobalVisualStudio8Generator::NeedLinkLibraryDependencies) unless there are
        # external project dependencies. It's frustrating because we absolutely need that on. To get around the
        # problem, we'll link in a dummy external project that just contains nothing. This causes cmake to
        # enable the LinkLibraryDependencies flag, and hopefully has no other side effects.
        include_external_msproject(generator_dummy ${XLE_MAIN_CMAKE_DIR}/generator_dummy.vcxproj)
        add_dependencies(${DllName} generator_dummy)
        set_target_properties(${DllName} PROPERTIES VS_USER_PROPS "${XLE_MAIN_CMAKE_DIR}/Main.props")
    endif (MSVC)

    target_include_directories(${DllName} PRIVATE ${XLE_DIR})

    xle_internal_configure_compiler(${DllName})

    if (NOT XLE_IMPLICIT_SHARED_LIBRARIES)
        if (NOT XLE_MEMORY_SANITIZER AND NOT XLE_ADDRESS_SANITIZER AND NOT XLE_THREAD_SANITIZER)
            target_link_options(${DllName} PRIVATE -Wl,--unresolved-symbols=report-all)
        endif ()
    endif ()
endmacro()

macro(xle_select_default_rendercore_metal TargetName)
    if (WIN32)
        target_compile_definitions(${TargetName} PUBLIC SELECT_DX)
    else()
        target_compile_definitions(${TargetName} PUBLIC SELECT_OPENGL)
    endif()
endmacro()

list(APPEND MetalSelectMacros SELECT_OPENGL SELECT_VULKAN)
list(APPEND MetalSelectName OpenGLES Vulkan)

if (WIN32)
    list(APPEND MetalSelectMacros SELECT_DX)
    list(APPEND MetalSelectName DX11)
endif()

if (APPLE)
    list(APPEND MetalSelectMacros SELECT_APPLEMETAL)
    list(APPEND MetalSelectName AppleMetal)
endif()

macro (FindProjectFiles retVal)
    file(GLOB prefilteredFiles *.cpp *.h)
	set(${retVal})
	foreach(f ${prefilteredFiles})
		if (NOT f MATCHES ".*_WinAPI.*" OR WIN32)
		    list(APPEND ${retVal} ${f})
		endif ()
    endforeach ()
endmacro ()

if (MSVC)
    if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set (VS_CONFIGURATION "x64")
    else ()
        set (VS_CONFIGURATION "Win32")
    endif ()
endif ()

