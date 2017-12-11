file(GLOB LuaSrc "Lua/src/*.c" "Lua/src/*.h")
file(GLOB HashSrc "Hash/*.cpp" "Hash/*.h")
BasicLibrary(ForeignMisc "${LuaSrc};${HashSrc}")
