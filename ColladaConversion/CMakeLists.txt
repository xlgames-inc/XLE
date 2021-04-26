
add_library(
    ColladaConversion SHARED
    ColladaConversion.cpp
    ColladaDocument.cpp
    ConversionConfig.cpp
    DLLInterface.cpp
    SAnimation.cpp
    ScaffoldParsingUtil.cpp
    SCommandStream.cpp
    SEffect.cpp
    SRawGeometry.cpp)
xle_configure_dll(ColladaConversion)
target_link_libraries(ColladaConversion PRIVATE RenderCoreAssets ConsoleRig Assets OSServices Utility ForeignMisc)

add_custom_command(
	TARGET ColladaConversion POST_BUILD 
	COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:ColladaConversion>"
     "${CMAKE_CURRENT_LIST_DIR}/../Finals_Debug64/$<TARGET_FILE_NAME:ColladaConversion>" 
	COMMENT "Copying to Finals directory")

