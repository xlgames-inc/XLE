cmake_minimum_required (VERSION 3.3)
project(Global CXX C)
include(../CMake/main.cmake RESULT_VARIABLE MAIN_CMAKE_LOCATION)

add_subdirectory(../../Math Math)
add_subdirectory(../../Utility Utility)
add_subdirectory(../../Assets Assets)
add_subdirectory(../../PlatformRig PlatformRig)
add_subdirectory(../../ConsoleRig ConsoleRig)
add_subdirectory(../../RenderCore RenderCore)
add_subdirectory(../../RenderCore/Metal RenderCoreMetal)

add_subdirectory(../../RenderCore/Techniques RenderCoreTechniques)
add_subdirectory(../../RenderCore/Assets RenderCoreAssets)
add_subdirectory(../../RenderOverlays RenderOverlays)
add_subdirectory(../../SceneEngine SceneEngine)
add_subdirectory(../../BufferUploads BufferUploads)

add_subdirectory(../../Foreign ForeignMisc)

add_subdirectory(../../ShaderParser ShaderParser)
add_subdirectory(../../Samples/ShaderScan Samples/ShaderScan)


