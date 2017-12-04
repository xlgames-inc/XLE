file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

BasicLibrary(RenderCore "${Src}" "${Headers}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR})
