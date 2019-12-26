if(NOT CLIENT_HAS_LUA)
    file(GLOB LuaSrc "Lua/src/*.c" "Lua/src/*.h")
endif()
file(GLOB HashSrc "Hash/*.cpp" "Hash/*.h")
file(GLOB FmtSrc "fmt/*.cc")
if (WIN32)
	file(GLOB StackWalkerSrc "StackWalker/*.cpp")
endif()
file(GLOB PlustacheSrc "plustache/*.cpp")
file(GLOB ZLibSrc "zlib/*.c" "zlib/*.h")
set(StamSimpleFluidSrc StamSimpleFluid/solver.c)

BasicLibrary(ForeignMisc "${LuaSrc};${HashSrc};${FmtSrc};${StackWalkerSrc};${PlustacheSrc};${StamSimpleFluidSrc};${ZLibSrc}")

# Ensure we export the necessary include folders from this target
# Most other targets list this target as a dependency, so they should inherit these include folders
target_include_directories(ForeignMisc PUBLIC Antlr-3.4/libantlr3c-3.4 Antlr-3.4/libantlr3c-3.4/include)
target_include_directories(ForeignMisc PUBLIC ${CMAKE_CURRENT_LIST_DIR})        # required to be public because of the way plustache includes it's headers
target_include_directories(ForeignMisc INTERFACE half-1.9.2/include eigen)
target_include_directories(ForeignMisc INTERFACE cml-1_0_2)
target_include_directories(ForeignMisc INTERFACE LuaBridge/Source/LuaBridge Lua/src)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ForeignMisc PRIVATE -Wno-parentheses-equality -Wno-incompatible-pointer-types-discards-qualifiers)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(ForeignMisc PRIVATE /wd4996 /wd4101)
endif()
