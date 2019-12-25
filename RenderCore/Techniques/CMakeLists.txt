set(Src
    BasicDelegates.cpp
    CommonResources.cpp
    CompiledShaderPatchCollection.cpp
    DeferredShaderResource.cpp
    DrawableMaterial.cpp
    Drawables.cpp
    ParsingContext.cpp
    RenderPass.cpp
    RenderPassUtils.cpp
    RenderStateResolver.cpp
    ShaderVariationSet.cpp
    TechniqueDelegates.cpp
    Techniques.cpp
    TechniqueUtils.cpp)
set(Headers
    BasicDelegates.h
    CommonBindings.h
    CommonResources.h
    CompiledRenderStateSet.h
    CompiledShaderPatchCollection.h
    DeferredShaderResource.h
    DrawableDelegates.h
    DrawableMaterial.h
    Drawables.h
    ParsingContext.h
    RenderPass.h
    RenderPassUtils.h
    RenderStateResolver.h
    ShaderVariationSet.h
    TechniqueDelegates.h
    TechniqueMaterial.h
    Techniques.h
    TechniqueUtils.h)

BasicLibrary(RenderCoreTechniques "${Src};${Headers}")
target_link_libraries(RenderCoreTechniques ForeignMisc)  # required to pull in interface include directories (etc)
