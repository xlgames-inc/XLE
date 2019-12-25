set(Src 
    # AssetUtils.cpp  (*1 some files not really required for processing dlls -- eg model conversion dlls -- so it's convenient to remove them here)
    LocalCompiledShaderSource.cpp
    MaterialCompiler.cpp
    MaterialScaffold.cpp
    ModelScaffold.cpp
    PredefinedCBLayout.cpp
    RawAnimationCurve.cpp
    RawMaterial.cpp
    # Services.cpp  (*1 see above)
    ShaderPatchCollection.cpp
    SimpleModelDeform.cpp
    # SimpleModelRenderer.cpp  (*1 see above)
    SkinDeformer.cpp
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
    ../GeoProc/NascentSkeletonMachine.cpp)
set(Headers
    AssetUtils.h
    LocalCompiledShaderSource.h
    MaterialCompiler.h
    MaterialScaffold.h
    ModelImmutableData.h
    ModelScaffold.h
    PredefinedCBLayout.h
    RawAnimationCurve.h
    RawMaterial.h
    Services.h
    ShaderPatchCollection.h
    SimpleModelDeform.h
    SimpleModelRenderer.h
    SkeletonScaffoldInternal.h
    SkinDeformer.h
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

BasicLibrary(RenderCoreAssets "${Src};${Headers}")
target_link_libraries(RenderCoreAssets ForeignMisc)  # required to pull in interface include directories (etc)

BasicLibrary(RenderCoreAssetsServices "AssetUtils.cpp;Services.cpp;SimpleModelRenderer.cpp")
target_link_libraries(RenderCoreAssetsServices ForeignMisc)  # required to pull in interface include directories (etc)
