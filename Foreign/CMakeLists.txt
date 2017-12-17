file(GLOB LuaSrc "Lua/src/*.c" "Lua/src/*.h")
file(GLOB HashSrc "Hash/*.cpp" "Hash/*.h")
file(GLOB AntlrSrc "Antlr-3.4/libantlr3c-3.4/src/*.c")
BasicLibrary(ForeignMisc "${LuaSrc};${HashSrc};${AntlrSrc}")

include_directories(Antlr-3.4/libantlr3c-3.4 Antlr-3.4/libantlr3c-3.4/include)
target_compile_options(ForeignMisc PRIVATE -Wno-parentheses-equality -Wno-incompatible-pointer-types-discards-qualifiers)
