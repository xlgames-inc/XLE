#pragma once

#include "../Assets/Assets.h"

namespace RenderCore { namespace Assets 
{ 
    namespace Simple {
        class ModelScaffold;
        class MaterialScaffold;
        class ModelRenderer;
    }
    class SharedStateSet;
}}

namespace SceneEngine
{
    class ModelFormat
    {
    public:
        typedef RenderCore::Assets::Simple::ModelScaffold ModelScaffold;
        typedef RenderCore::Assets::Simple::MaterialScaffold MaterialScaffold;
        typedef RenderCore::Assets::Simple::ModelRenderer ModelRenderer;
        typedef RenderCore::Assets::SharedStateSet SharedStateSet;

        std::shared_ptr<ModelScaffold>      CreateModel(const Assets::ResChar initializer[])       { (void)initializer; return std::make_shared<ModelScaffold>(); }
        std::shared_ptr<MaterialScaffold>   CreateMaterial(const Assets::ResChar initializer[])    { (void)initializer; return std::make_shared<MaterialScaffold>(); }
        std::shared_ptr<ModelRenderer>      CreateRenderer(
            ModelScaffold& scaffold, MaterialScaffold& material,
            SharedStateSet& sharedStateSet, unsigned levelOfDetail)
        {
            return std::make_shared<ModelRenderer>(scaffold, material, sharedStateSet, levelOfDetail);
        }

        std::string DefaultMaterialName(const ModelScaffold&) { return std::string(); }
    };
}

