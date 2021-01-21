
set(Src
    Utility/Utilities.cpp
    Utility/StreamFormatterTests.cpp
    Utility/ConversionPatterns.cpp
    Utility/ClassAccessorsTests.cpp
    Math/BasicMaths.cpp
    Math/MathSerialization.cpp
    OSServices/OSServicesAsync.cpp
    Assets/MountingTreeTests.cpp
    RenderCore/ShaderParserTests.cpp)

add_executable(UnitTests ${Src})
configure_xle_executable(UnitTests)

target_link_libraries(UnitTests PRIVATE Catch2WithMain)
target_link_libraries(UnitTests PRIVATE ShaderParser RenderCoreAssets RenderCore Assets OSServices Math Utility ForeignMisc )
