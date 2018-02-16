file(GLOB Src "*.cpp" "OpenGLES/Metal/*.cpp")
set(Src "${Src}" "GeoProc/GeometryAlgorithm.cpp" "GeoProc/GeoProcUtil.cpp" "GeoProc/MeshDatabase.cpp" "Assets/LocalCompiledShaderSource.cpp" "Assets/RawMaterial.cpp" "Techniques/PredefinedCBLayout.cpp" "Techniques/RenderPass.cpp" "Techniques/TechniqueMaterial.cpp" "OpenGLES/Metal/PipelineLayout.cpp" "OpenGLES/Metal/InputLayout.cpp")
file(GLOB Headers "*.h" "Techniques/*.h")

BasicLibrary(RenderCore "${Src}" "${Headers}")

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR})
