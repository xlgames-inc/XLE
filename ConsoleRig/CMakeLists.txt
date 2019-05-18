# file(GLOB Src "*.cpp")
# file(GLOB Headers "*.h")
# BasicLibrary(ConsoleRig "${Src}" "${Headers}")

FindProjectFiles(Src)
BasicLibrary(ConsoleRig "${Src}")

# Foreign Common
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-2_1_2 ${FOREIGN_DIR})
include_directories(${FOREIGN_DIR}/LuaBridge/Source/LuaBridge)
include_directories(${FOREIGN_DIR}/Lua/src)
