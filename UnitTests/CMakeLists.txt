
set(Src
    Utilities.cpp
    BasicMaths.cpp
    MathSerialization.cpp
    StreamFormatterTests.cpp
    ConversionPatterns.cpp
    ClassAccessorsTests.cpp
    OSServicesAsync.cpp)

add_executable(UnitTests ${Src})
configure_xle_executable(UnitTests)

target_link_libraries(UnitTests PRIVATE Catch2WithMain)
target_link_libraries(UnitTests PRIVATE Assets OSServices Math Utility ForeignMisc )
