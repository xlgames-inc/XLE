set(Src 
    # AssetUtils.cpp  (*1 some files not really required for processing dlls -- eg model conversion dlls -- so it's convenient to remove them here)
    LocalCompiledShaderSource.cpp
    MaterialCompiler.cpp
    MaterialScaffold.cpp
    ModelScaffold.cpp
    PredefinedCBLayout.cpp
    PredefinedDescriptorSetLayout.cpp
    RawAnimationCurve.cpp
    RawMaterial.cpp
    # Services.cpp  (*1 see above)
    ShaderPatchCollection.cpp
    SkinningScaffold.cpp
    TransformationCommands.cpp
    ../GeoProc/GeometryAlgorithm.cpp
    ../GeoProc/GeoProcUtil.cpp
    ../GeoProc/MeshDatabase.cpp
    ../GeoProc/NascentAnimController.cpp
    ../GeoProc/NascentCommandStream.cpp
    ../GeoProc/NascentModel.cpp
    ../GeoProc/NascentObjectsSerialize.cpp
    ../GeoProc/NascentRawGeometry.cpp
    ../GeoProc/NascentSkeletonMachine.cpp
	../GeoProc/DequantAnalysisTools.cpp)
set(Headers
    AssetUtils.h
    LocalCompiledShaderSource.h
    MaterialCompiler.h
    MaterialScaffold.h
    ModelImmutableData.h
    ModelScaffold.h
    PredefinedCBLayout.h
    PredefinedDescriptorSetLayout.h
    RawAnimationCurve.h
    RawMaterial.h
    Services.h
    ShaderPatchCollection.h
    SkeletonScaffoldInternal.h
    TransformationCommands.h
    ../GeoProc/GeometryAlgorithm.h
    ../GeoProc/GeoProcUtil.h
    ../GeoProc/MeshDatabase.h
    ../GeoProc/NascentAnimController.h
    ../GeoProc/NascentCommandStream.h
    ../GeoProc/NascentModel.h
    ../GeoProc/NascentObjectGuid.h
    ../GeoProc/NascentObjectsSerialize.h
    ../GeoProc/NascentRawGeometry.h
    ../GeoProc/NascentSkeletonMachine.h)

add_library(RenderCoreAssets STATIC ${Src} ${Headers})
xle_configure_library(RenderCoreAssets)
target_link_libraries(RenderCoreAssets PUBLIC RenderCore ConsoleRig Assets Math OSServices Utility ForeignMisc)

add_library(RenderCoreAssetsServices STATIC "AssetUtils.cpp;Services.cpp")
xle_configure_library(RenderCoreAssetsServices)
target_link_libraries(RenderCoreAssetsServices PUBLIC RenderCore ConsoleRig Assets Math OSServices Utility ForeignMisc)