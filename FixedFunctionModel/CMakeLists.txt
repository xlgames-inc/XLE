
set(Src
    DelayedDrawCall.cpp
    ModelCache.cpp
    ModelRunTime.cpp
    ModelUtils.cpp
    PreboundShaders.cpp
    SharedStateSet.cpp
    SkinningRunTime.cpp)
set(Headers
    DelayedDrawCall.h
    IModelFormat.h
    ModelCache.h
    ModelRendererInternal.h
    ModelRunTime.h
    ModelUtils.h
    PreboundShaders.h
    SharedStateSet.h)

add_library(FixedFunctionModel STATIC ${Src} ${Headers})
configure_xle_library(FixedFunctionModel)
target_link_libraries(FixedFunctionModel PRIVATE ForeignMisc)