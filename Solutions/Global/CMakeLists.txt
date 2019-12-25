cmake_minimum_required (VERSION 3.3)
project(Global CXX C)
include(../CMake/main.cmake RESULT_VARIABLE MAIN_CMAKE_LOCATION)
include(../CMake/modules.cmake)

add_subdirectory(../../Samples/ShaderScan Samples/ShaderScan)


