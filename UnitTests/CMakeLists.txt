
####################################################################################################
    ## UnitTests-Core ##
####################################################################################################
add_executable(
    UnitTests-Core
    Utility/Utilities.cpp
    Utility/StreamFormatterTests.cpp
    Utility/ConversionPatterns.cpp
    Utility/ClassAccessorsTests.cpp
    Math/BasicMaths.cpp
    Math/MathSerialization.cpp
    OSServices/OSServicesAsync.cpp
    ConsoleRig/DynLibraryBinding.cpp
    Assets/MountingTreeTests.cpp
    Assets/AssetCompilerTests.cpp
    )

xle_configure_executable(UnitTests-Core)
target_link_libraries(UnitTests-Core PRIVATE Catch2WithMain)
target_link_libraries(UnitTests-Core PRIVATE ConsoleRig Assets OSServices Math Utility tiny-process-library)

#
# Also add a dynamic library as a dependency. This will be loaded at runtime by some of the unit tests
# it's a dependency, but not used with target_link_libraries(). So it should compile, but won't
# appear on the linker command line
#
add_library(UnitTestDynLibrary SHARED ConsoleRig/UnitTestDynLibrary.cpp)
xle_configure_dll(UnitTestDynLibrary)
target_link_libraries(UnitTestDynLibrary PRIVATE ConsoleRig Assets OSServices Utility ForeignMisc)

add_dependencies(UnitTests-Core UnitTestDynLibrary)

####################################################################################################
    ## UnitTests-RenderCore ##
####################################################################################################

foreach (metal_macro metal_name IN ZIP_LISTS MetalSelectMacros MetalSelectName)

    add_executable(
        UnitTests-RenderCore-${metal_name}
        RenderCore/Metal/CoordinateSpaces.cpp
        RenderCore/Metal/InputLayout.cpp
        RenderCore/Metal/MetalUnitTest.cpp
        #RenderCore/Metal/QueryPool.cpp
        #RenderCore/Metal/ResourceUpdateAndReadBack.cpp
        #RenderCore/Metal/StreamOutput.cpp
        )

    xle_configure_executable(UnitTests-RenderCore-${metal_name})
    target_compile_definitions(UnitTests-RenderCore-${metal_name} PUBLIC ${metal_macro})
    target_link_libraries(UnitTests-RenderCore-${metal_name} PRIVATE Catch2WithMain)
    target_link_libraries(UnitTests-RenderCore-${metal_name} PRIVATE RenderCore)
    target_link_libraries(UnitTests-RenderCore-${metal_name} PRIVATE RenderCore_${metal_name})
    target_link_libraries(UnitTests-RenderCore-${metal_name} PRIVATE RenderCoreTechniques-${metal_name})

endforeach()

####################################################################################################
    ## UnitTests-RenderCoreAssetsTechniques ##
####################################################################################################

foreach (metal_macro metal_name IN ZIP_LISTS MetalSelectMacros MetalSelectName)

    add_executable(
        UnitTests-RenderCoreAssetsTechniques-${metal_name}
        RenderCore/Assets/ShaderParserTests.cpp
        RenderCore/Assets/ShaderPatchCollection.cpp
        RenderCore/Assets/TransformationMachineOpt.cpp
        RenderCore/Assets/RenderCoreCompilerTests.cpp
        RenderCore/Assets/FakeModelCompiler.cpp
		RenderCore/Assets/ShaderCompilation.cpp
        RenderCore/Metal/MetalUnitTest.cpp
        )

    xle_configure_executable(UnitTests-RenderCoreAssetsTechniques-${metal_name})
    target_compile_definitions(UnitTests-RenderCoreAssetsTechniques-${metal_name} PUBLIC ${metal_macro})
    target_link_libraries(UnitTests-RenderCoreAssetsTechniques-${metal_name} PRIVATE Catch2WithMain)
    target_link_libraries(UnitTests-RenderCoreAssetsTechniques-${metal_name} PRIVATE ShaderParser RenderCoreAssets)
    target_link_libraries(UnitTests-RenderCoreAssetsTechniques-${metal_name} PRIVATE RenderCore)
    target_link_libraries(UnitTests-RenderCoreAssetsTechniques-${metal_name} PRIVATE RenderCore_${metal_name})
    target_link_libraries(UnitTests-RenderCoreAssetsTechniques-${metal_name} PRIVATE RenderCoreTechniques-${metal_name})

endforeach()
