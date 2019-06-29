if(NOT CLIENT_HAS_LUA)
    file(GLOB LuaSrc "Lua/src/*.c" "Lua/src/*.h")
endif()
file(GLOB HashSrc "Hash/*.cpp" "Hash/*.h")
file(GLOB FmtSrc "fmt/*.cc")
if (WIN32)
    set(StackWalkerSrc "StackWalker/StackWalker.cpp")
endif()
BasicLibrary(ForeignMisc "${LuaSrc};${HashSrc};${FmtSrc};${StackWalkerSrc}")

add_subdirectory("FreeType")

target_compile_options(ForeignMisc PRIVATE -Wno-parentheses-equality -Wno-incompatible-pointer-types-discards-qualifiers)
