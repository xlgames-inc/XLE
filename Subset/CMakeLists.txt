set(Src
    "../RenderCore/GeoProc/GeometryAlgorithm.cpp" 
    "../RenderCore/GeoProc/GeoProcUtil.cpp" 
    "../RenderCore/GeoProc/MeshDatabase.cpp" 
    "../RenderCore/Assets/RawMaterial.cpp" 
    "../RenderCore/Assets/MaterialScaffold.cpp" 
    "../RenderCore/Techniques/PredefinedCBLayout.cpp" 
    "../RenderCore/Techniques/RenderPass.cpp" 
    "../RenderCore/Techniques/TechniqueUtils.cpp"
    "../RenderCore/Techniques/Techniques.cpp"
    "../PlatformRig/DebuggingDisplays/HistoricalProfilerDisplay.cpp"
    "../PlatformRig/DebuggingDisplays/CPUProfileDisplay.cpp"
    "../PlatformRig/PlatformRigUtil.cpp"
    "../PlatformRig/InputListener.cpp"
    "../PlatformRig/DebugHotKeys.cpp"
    "../PlatformRig/OverlaySystem.cpp"
    "../RenderOverlays/DebuggingDisplay.cpp"
    "../RenderOverlays/IOverlayContext.cpp"
    "../SceneEngine/DualContour.cpp"
    "../SceneEngine/FiniteResourceHeap.cpp"
    "../SceneEngine/GenericGridPartitioning.cpp"
    "../SceneEngine/GenericQuadTree.cpp"
    "Stubs.cpp"
)
if (WIN32)
    set(WinSrc
        "../PlatformRig/WinAPI/InputTranslator.cpp"
        "../PlatformRig/WinAPI/OverlappedWindow.cpp"
        "../PlatformRig/WinAPI/RunLoop_WinAPI.cpp"
    )
endif()
BasicLibrary(XLESubset "${Src};${WinSrc}")
target_link_libraries(XLESubset ForeignMisc)  # required to pull in interface include directories (etc)
