file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

BasicExecutable(ShaderScan "${Src};${Headers}")

add_dependencies(ShaderScan Utility Math Assets ConsoleRig RenderCore ShaderParser ForeignMisc)
target_link_libraries(ShaderScan Utility Math Assets ConsoleRig RenderCore ShaderParser ForeignMisc)
