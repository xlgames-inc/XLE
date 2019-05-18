set(Src
    "../RenderCore/GeoProc/GeometryAlgorithm.cpp" 
    "../RenderCore/GeoProc/GeoProcUtil.cpp" 
    "../RenderCore/GeoProc/MeshDatabase.cpp" 
    "../RenderCore/Assets/LocalCompiledShaderSource.cpp" 
    "../RenderCore/Assets/RawMaterial.cpp" 
    "../RenderCore/Techniques/PredefinedCBLayout.cpp" 
    "../RenderCore/Techniques/RenderPass.cpp" 
    "../RenderCore/Techniques/TechniqueMaterial.cpp"
    "../RenderCore/Techniques/TechniqueUtils.cpp"
    "../PlatformRig/DebuggingDisplays/HistoricalProfilerDisplay.cpp"
    "../PlatformRig/DebuggingDisplays/CPUProfileDisplay.cpp"
    "../PlatformRig/PlatformRigUtil.cpp"
    "../RenderOverlays/DebuggingDisplay.cpp"
    "../RenderOverlays/Font_Dummy.cpp"
    "../RenderOverlays/IOverlayContext.cpp"
    "../SceneEngine/DualContour.cpp"
    "../SceneEngine/FiniteResourceHeap.cpp"
    "../SceneEngine/GenericGridPartitioning.cpp"
    "../SceneEngine/GenericQuadTree.cpp"
)
if (WIN32)
    set(WinSrc
        "../PlatformRig/WinAPI/InputTranslator.cpp"
        "../PlatformRig/WinAPI/OverlappedWindow.cpp"
    )
endif()
BasicLibrary(XLESubset "${Src};${WinSrc}")

# Foreign Common 
target_include_directories(XLESubset PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-2_1_2 ${FOREIGN_DIR})
