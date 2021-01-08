
set(Src
    Utilities.cpp
    BasicMaths.cpp
    MathSerialization.cpp
    StreamFormatterTests.cpp
    ConversionPatterns.cpp)

add_executable(UnitTests ${Src})
configure_xle_executable(UnitTests)

target_link_libraries(UnitTests PRIVATE Catch2 Catch2WithMain)
target_link_libraries(UnitTests PRIVATE Utility Math ForeignMisc)
