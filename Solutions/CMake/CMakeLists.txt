cmake_minimum_required (VERSION 3.10)
project(GlobalCMake)

# Unset all of the default projection configuration settings provided by cmake
# Note that we have to explicitly set the exception handling settings for visual studio to get around 
# a hack in the cmake generator for visual studio that always sets the exception handling key to 
# something; even if we've got a better setting in one of our inherit properties files
if (MSVC)
    set(CMAKE_C_FLAGS "/EHsc" CACHE STRING "Clear CMake Settings" FORCE)
    set(CMAKE_CXX_FLAGS "/EHsc" CACHE STRING "Clear CMake Settings" FORCE)
else (MSVC)
    set(CMAKE_C_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
    set(CMAKE_CXX_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
endif (MSVC)
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "Clear CMake Settings" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "Clear CMake Settings" FORCE)

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

macro(BasicLibrary LibName Src Headers)
    project (LibName)
    source_group("" FILES ${Src} ${Headers})       # Push all files into the root folder
    add_library(${LibName} STATIC ${Src} ${Headers})
    set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/Main.props")
    include_directories(${XLE_DIR})
endmacro()

add_subdirectory(../../Math Math)
add_subdirectory(../../Utility Utility)
add_subdirectory(../../Assets Assets)
add_subdirectory(../../PlatformRig PlatformRig)
add_subdirectory(../../ConsoleRig ConsoleRig)
add_subdirectory(../../RenderCore RenderCore)
add_subdirectory(../../Samples/ShaderScan Samples/ShaderScan)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG-DX11 "" CACHE STRING "Debug-DX11 compiler flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE-DX11 "" CACHE STRING "Release-DX11 compiler flags" FORCE)

