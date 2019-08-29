
file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")
file(GLOB NetworkingSrc "Networking/*.cpp")
file(GLOB StreamsSrc "Streams/*.cpp")
file(GLOB ThreadingSrc "Threading/*.cpp")
file(GLOB ProfilingSrc "Profiling/*.cpp")
file(GLOB MetaSrc "Meta/*.cpp")
if (WIN32)
    file(GLOB WinAPISrc "WinAPI/*.cpp" "Streams/WinAPI/*.cpp" "Threading/WinAPI/*.cpp")
elseif (NOT APPLE)
    file(GLOB LinuxSrc "Linux/*.cpp" "Streams/Linux/*.cpp")
endif()

BasicLibrary(Utility "${Src};${Headers};${NetworkingSrc};${StreamsSrc};${ThreadingSrc};${WinAPISrc};${ProfilingSrc};${MetaSrc};${LinuxSrc}")

target_include_directories(Utility PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-2_1_2 ${FOREIGN_DIR})
