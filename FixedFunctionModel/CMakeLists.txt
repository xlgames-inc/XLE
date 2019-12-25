
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

BasicLibrary(FixedFunctionModel "${Src};${Headers}")
target_link_libraries(FixedFunctionModel ForeignMisc)  # required to pull in interface include directories (etc)
