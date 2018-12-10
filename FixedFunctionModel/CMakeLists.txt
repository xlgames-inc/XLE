
set(Src
    DelayedDrawCall.cpp
    ModelCache.cpp
    ModelRunTime.cpp
    ModelUtils.cpp
    ShaderVariationSet.cpp
    SharedStateSet.cpp
    SkinningRunTime.cpp)
set(Headers
    DelayedDrawCall.h
    IModelFormat.h
    ModelCache.h
    ModelRendererInternal.h
    ModelRunTime.h
    ModelUtils.h
    ShaderVariationSet.h
    SharedStateSet.h)

BasicLibrary(FixedFunctionModel "${Src};${Headers}")
