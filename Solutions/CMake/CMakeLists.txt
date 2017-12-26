cmake_minimum_required (VERSION 3.9)
project(GlobalCMake)

message("Build type: ${CMAKE_BUILD_TYPE}")

# Unset all of the default projection configuration settings provided by cmake
# Note that we have to explicitly set the exception handling settings for visual studio to get around 
# a hack in the cmake generator for visual studio that always sets the exception handling key to 
# something; even if we've got a better setting in one of our inherit properties files
set(CMAKE_C_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_CXX_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_FLAGS "/EHsc" CACHE STRING "work around MSVC generator issue" FORCE)
    set(CMAKE_CXX_FLAGS "/EHsc" CACHE STRING "work around MSVC generator issue" FORCE)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")       # Can be Clang or AppleClang
#    set(CMAKE_C_FLAGS "-std=c++14 -fapple-pragma-pack -fexceptions -frtti" CACHE STRING "clang command line" FORCE)
#    set(CMAKE_CXX_FLAGS "-std=c++14 -fapple-pragma-pack -fexceptions -frtti" CACHE STRING "clang command line" FORCE)
#    set(CMAKE_CXX_FLAGS_Debug "-D_DEBUG" CACHE STRING "clang command line" FORCE)
#    set(CMAKE_CXX_FLAGS_Release "" CACHE STRING "clang command line" FORCE)
endif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

set(CMAKE_STATIC_LINKER_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "Clear CMake Settings" FORCE)

set(LINK_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
set(LINK_FLAGS_DEBUG "" CACHE STRING "Clear CMake Settings" FORCE)
set(LINK_FLAGS_RELEASE "" CACHE STRING "Clear CMake Settings" FORCE)
 
get_filename_component(XLE_DIR "${CMAKE_SOURCE_DIR}/../../" ABSOLUTE)
get_filename_component(FOREIGN_DIR "${XLE_DIR}/Foreign/" ABSOLUTE)

macro(BasicLibrary LibName Src)
    project (LibName)
	source_group("" FILES ${Src})       # Push all files into the root folder
    add_library(${LibName} STATIC ${Src})
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/Main.props")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	    target_compile_options(${LibName} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14> -fapple-pragma-pack -fexceptions -frtti)
    endif ()
    include_directories(${XLE_DIR})
	set_property(TARGET ${LibName} APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG>)
endmacro()

macro (FindProjectFiles retVal)
    file(GLOB prefilteredFiles *.cpp *.h)
	set(${retVal})
	foreach(f ${prefilteredFiles})
		if (NOT f MATCHES ".*_WinAPI.*")
		    list(APPEND ${retVal} ${f})
		endif ()
    endforeach ()
endmacro ()

add_subdirectory(../../Math Math)
add_subdirectory(../../Utility Utility)
add_subdirectory(../../Assets Assets)
# add_subdirectory(../../PlatformRig PlatformRig)
add_subdirectory(../../ConsoleRig ConsoleRig)
add_subdirectory(../../RenderCore RenderCore)
add_subdirectory(../../Foreign ForeignMisc)
add_subdirectory(../../Samples/ShaderScan Samples/ShaderScan)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
# set(CMAKE_CXX_FLAGS_DEBUG-DX11 "" CACHE STRING "Debug-DX11 compiler flags" FORCE)
# set(CMAKE_CXX_FLAGS_RELEASE-DX11 "" CACHE STRING "Release-DX11 compiler flags" FORCE)

