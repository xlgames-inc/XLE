# file(GLOB Src "*.cpp")
# file(GLOB Headers "*.h")
# BasicLibrary(Assets "${Src}" "${Headers}")

FindProjectFiles(Src)
BasicLibrary(Assets "${Src}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-1_0_2 ${FOREIGN_DIR})
