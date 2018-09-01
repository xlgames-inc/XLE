

####################################################################################################
set(CLANG_BASE_FLAGS "")
set(WARNING_FLAGS "")
set(CPP_BASE_FLAGS "")
set(OPTIMIZER_FLAGS "")

####################################################################################################
# Unset all of the default projection configuration settings provided by cmake
# Note that we have to explicitly set the exception handling settings for visual studio to get around
# a hack in the cmake generator for visual studio that always sets the exception handling key to
# something; even if we've got a better setting in one of our inherit properties files
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
    set(CMAKE_CXX_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
    set(LINK_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
    set(CMAKE_STATIC_LINKER_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Clear CMake Settings" FORCE)

    set(CMAKE_C_FLAGS "/EHsc" CACHE STRING "work around MSVC generator issue" FORCE)
    set(CMAKE_CXX_FLAGS "/EHsc" CACHE STRING "work around MSVC generator issue" FORCE)
endif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

####################################################################################################
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # When using clang, we must use the --whole-archive flag, to ensure that symbols that
    # are required by sibling projects (ie, if the Math library uses symbols from the Utility library)
    # can be found.
    # We also set --gc-sections, (and -ffunction-sections and -fdata-sections on the compiler)
    # to try to strip out as much as possible after linking
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections,--whole-archive,-z,muldefs" CACHE STRING "Linker flags for clang" FORCE)

    APPEND(CLANG_BASE_FLAGS " $<$<COMPILE_LANGUAGE:CXX>:-std=c++14> $<$<COMPILE_LANGUAGE:C>:-std=c99> -fapple-pragma-pack -fexceptions -frtti -ffunction-sections -fdata-sections  $<$<COMPILE_LANGUAGE:CXX>:-fcxx-exceptions>")
    if ((BUILD_TYPE_LOWER STREQUAL "release") OR (BUILD_TYPE_LOWER STREQUAL "staging"))
        set(OPTIMIZER_FLAGS "-Os -finline-functions")
        add_definitions("-DNDEBUG")
    else()
        set(OPTIMIZER_FLAGS "-O0 -ggdb -fno-inline")
        add_definitions("-DDEBUG=1" "-D_DEBUG")
    endif()

    APPEND(WARNING_FLAGS " -Wall -Wno-unknown-pragmas -Wno-unused-local-typedef -Wno-unused-const-variable -Wno-reorder -Wno-unused-variable -Wno-unused-function -Wno-char-subscripts")
    append(WARNING_FLAGS " -Wno-unknown-pragmas -Werror=implicit-function-declaration -Wno-unused-local-typedef")
endif ()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
