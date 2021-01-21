
set(Src
    Utility/Utilities.cpp
    Utility/StreamFormatterTests.cpp
    Utility/ConversionPatterns.cpp
    Utility/ClassAccessorsTests.cpp
    Math/BasicMaths.cpp
    Math/MathSerialization.cpp
    OSServices/OSServicesAsync.cpp
    ConsoleRig/DynLibraryBinding.cpp
    Assets/MountingTreeTests.cpp
    RenderCore/ShaderParserTests.cpp)

add_executable(UnitTests ${Src})
configure_xle_executable(UnitTests)

target_link_libraries(UnitTests PRIVATE Catch2WithMain)
target_link_libraries(UnitTests PRIVATE ShaderParser RenderCoreAssets RenderCore ConsoleRig Assets OSServices Math Utility ForeignMisc tiny-process-library)

#
# Also add a dynamic library as a dependency. This will be loaded at runtime by some of the unit tests
# it's a dependency, but not used with target_link_libraries(). So it should compile, but won't
# appear on the linker command line
#
add_library(UnitTestDynLibrary SHARED ConsoleRig/UnitTestDynLibrary.cpp)
configure_xle_dll(UnitTestDynLibrary)
target_link_libraries(UnitTestDynLibrary PRIVATE ConsoleRig Assets OSServices Utility ForeignMisc)

add_dependencies(UnitTests UnitTestDynLibrary)
