file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

list(FILTER Src EXCLUDE REGEX ".*FiniteResourceHeap.cpp$")

BasicLibrary(SceneEngine "${Src};${Headers}")

target_include_directories(SceneEngine PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen)
