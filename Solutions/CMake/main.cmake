# setup basic XLE_DIR & FOREIGN_DIR macros
get_filename_component(XLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)
get_filename_component(FOREIGN_DIR "${XLE_DIR}/Foreign/" ABSOLUTE)

macro(BasicLibrary LibName Src)
	source_group("" FILES ${Src})       # Push all files into the root folder
    add_library(${LibName} STATIC ${Src})
    if (MSVC)
        set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${MAIN_CMAKE_DIR}/Main.props")
    endif ()
    target_include_directories(${LibName} PRIVATE ${XLE_DIR})
endmacro()

macro(BasicExecutable ExeName Src)
    source_group("" FILES ${Src})       # Push all files into the root folder
    add_executable(${ExeName} ${Src})

    if (MSVC)
        # The CMake Visual Studio generator has a hack that disables the LinkLibraryDependencies setting in
        # the output project (see cmGlobalVisualStudio8Generator::NeedLinkLibraryDependencies) unless there are
        # external project dependencies. It's frustrating because we absolutely need that on. To get around the
        # problem, we'll link in a dummy external project that just contains nothing. This causes cmake to
        # enable the LinkLibraryDependencies flag, and hopefully has no other side effects.
        include_external_msproject(generator_dummy ${MAIN_CMAKE_DIR}/generator_dummy.vcxproj)
        add_dependencies(${ExeName} generator_dummy)
        set_target_properties(${ExeName} PROPERTIES VS_USER_PROPS "${MAIN_CMAKE_DIR}/Main.props")
    endif (MSVC)

    target_include_directories(${ExeName} PRIVATE ${XLE_DIR})

    if (NOT WIN32)
        target_link_libraries(${ExeName} pthread)
    else ()
        target_link_libraries(${ExeName} wsock32.lib ws2_32.lib)
    endif ()
endmacro()

macro(BasicDLL DllName Src)
    source_group("" FILES ${Src})       # Push all files into the root folder
    add_library(${DllName} SHARED ${Src})

    if (MSVC)
        # The CMake Visual Studio generator has a hack that disables the LinkLibraryDependencies setting in
        # the output project (see cmGlobalVisualStudio8Generator::NeedLinkLibraryDependencies) unless there are
        # external project dependencies. It's frustrating because we absolutely need that on. To get around the
        # problem, we'll link in a dummy external project that just contains nothing. This causes cmake to
        # enable the LinkLibraryDependencies flag, and hopefully has no other side effects.
        include_external_msproject(generator_dummy ${MAIN_CMAKE_DIR}/generator_dummy.vcxproj)
        add_dependencies(${DllName} generator_dummy)
        set_target_properties(${DllName} PROPERTIES VS_USER_PROPS "${MAIN_CMAKE_DIR}/Main.props")
    endif (MSVC)

    target_include_directories(${DllName} PRIVATE ${XLE_DIR})

    if (NOT WIN32)
        target_link_libraries(${DllName} pthread)
    else ()
        target_link_libraries(${DllName} wsock32.lib ws2_32.lib)
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

