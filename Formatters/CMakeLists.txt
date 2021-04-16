add_library(
    Formatters STATIC 
    BinaryFormatter.cpp
    BinarySchemata.cpp)
xle_configure_library(Math)

target_link_libraries(Formatters PRIVATE Utility ForeignMisc)
