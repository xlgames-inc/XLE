
set(Src
    Utilities.cpp)

add_executable(UnitTests ${Src})
configure_xle_executable(UnitTests)

target_link_libraries(UnitTests PRIVATE Catch2 Catch2WithMain)
target_link_libraries(UnitTests PRIVATE Utility ForeignMisc)
