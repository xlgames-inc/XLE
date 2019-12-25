set(Src
    AnimationVisualization.cpp
    DebuggingDisplay.cpp
    Font.cpp
    FontRectanglePacking.cpp
    FontRendering.cpp
    FT_Font.cpp
    FT_FontTexture.cpp
    HighlightEffects.cpp
    OverlayContext.cpp
    OverlayPrimitives.cpp
    OverlayUtils.cpp)
set(Headers
    AnimationVisualization.h
    DebuggingDisplay.h
    Font.h
    FontRectanglePacking.h
    FT_Font.h
    FT_FontTexture.h
    HighlightEffects.h
    IOverlayContext_Forward.h
    IOverlayContext.h
    OverlayContext.h
    OverlayPrimitives.h
    OverlayUtils.h)
set(OverlaysSrc
    Overlays/Browser.cpp
    Overlays/OceanSettings.cpp
    Overlays/ShadowFrustumDebugger.cpp
    Overlays/TestMaterialSettings.cpp
    Overlays/ToneMapSettings.cpp
    Overlays/VolFogSettings.cpp)
set(OverlaysHeaders 
    Overlays/Browser.h
    Overlays/OceanSettings.h
    Overlays/ShadowFrustumDebugger.h
    Overlays/TestMaterialSettings.h
    Overlays/ToneMapSettings.h
    Overlays/VolFogSettings.h)

BasicLibrary(RenderOverlays "${Src};${Headers};${OverlaysSrc};${OverlaysHeaders}")
target_link_libraries(RenderOverlays ForeignMisc)  # required to pull in interface include directories (etc)
target_include_directories(RenderOverlays PRIVATE ${FOREIGN_DIR}/FreeType/include)
