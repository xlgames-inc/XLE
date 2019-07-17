file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")
file(GLOB OverlaysSrc "Overlays/*.cpp")
file(GLOB OverlaysHeaders "Overlays/*.h")

list(FILTER Src EXCLUDE REGEX ".*IOverlayContext.cpp$")
list(FILTER Src EXCLUDE REGEX ".*Font_Dummy.cpp$")

BasicLibrary(RenderOverlays "${Src};${Headers};${OverlaysSrc};${OverlaysHeaders}")

include_directories(RenderOverlays PRIVATE ${FOREIGN_DIR}/FreeType/include)
