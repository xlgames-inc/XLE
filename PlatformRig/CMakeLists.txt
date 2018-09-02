file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

list(FILTER Src EXCLUDE REGEX ".*MeshNode.cpp$")
list(FILTER Src EXCLUDE REGEX ".*StatePropagation.cpp$")

file(GLOB DebuggingDisplaysSrc "DebuggingDisplays/*.cpp")
file(GLOB DebuggingDisplaysHeaders "DebuggingDisplays/*.h")

if (WIN32)
    file(GLOB WinSrc "WinAPI/*.cpp")
endif()

BasicLibrary(PlatformRig "${Src};${WinSrc};${DebuggingDisplaysSrc}" "${Headers};${DebuggingDisplaysHeaders}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR})
include_directories(${FOREIGN_DIR}/LuaBridge/Source/LuaBridge)
include_directories(${FOREIGN_DIR}/Lua/src)
