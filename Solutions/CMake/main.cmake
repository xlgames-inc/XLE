# setup basic XLE_DIR & FOREIGN_DIR macros
get_filename_component(XLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)
get_filename_component(FOREIGN_DIR "${XLE_DIR}/Foreign/" ABSOLUTE)
get_filename_component(XLE_MAIN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

macro(configure_xle_library LibName)
    if (MSVC)
        set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${XLE_MAIN_CMAKE_DIR}/Main.props")
    endif ()
    target_include_directories(${LibName} PRIVATE ${XLE_DIR})
    target_compile_features(${LibName} PUBLIC cxx_std_17)
    set_property(TARGET ${LibName} PROPERTY CXX_EXTENSIONS OFF)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${LibName} PUBLIC -DXL_DEBUG -D_DEBUG)
    elseif ( (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        target_compile_definitions(${LibName} PUBLIC -DXL_RELEASE -DNDEBUG)
    endif ()
endmacro()

macro(configure_xle_executable ExeName)
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
    target_compile_features(${ExeName} PUBLIC cxx_std_17)
    set_property(TARGET ${ExeName} PROPERTY CXX_EXTENSIONS OFF)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${ExeName} PUBLIC -DXL_DEBUG -D_DEBUG)
    elseif ( (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        target_compile_definitions(${ExeName} PUBLIC -DXL_RELEASE -DNDEBUG)
    endif ()
endmacro()

macro(configure_xle_dll DllName)
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
    target_compile_features(${DllName} PUBLIC cxx_std_17)
    set_property(TARGET ${DllName} PROPERTY CXX_EXTENSIONS OFF)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${DllName} PUBLIC -DXL_DEBUG -D_DEBUG)
    elseif ( (CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        target_compile_definitions(${DllName} PUBLIC -DXL_RELEASE -DNDEBUG)
    endif ()
endmacro()

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

