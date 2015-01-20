#pragma once

#include "../../Assets/Assets.h"

namespace RenderCore { namespace Assets
{
    namespace Simple {
        class ModelScaffold;
        class MaterialScaffold;
        class ModelRenderer;
    }
    class SharedStateSet;

    class IModelFormat
    {
    public:
        typedef Simple::ModelScaffold ModelScaffold;
        typedef Simple::MaterialScaffold MaterialScaffold;
        typedef Simple::ModelRenderer ModelRenderer;

        virtual std::shared_ptr<ModelScaffold>      CreateModel(const ::Assets::ResChar initializer[]) = 0;
        virtual std::shared_ptr<MaterialScaffold>   CreateMaterial(const ::Assets::ResChar initializer[]) = 0;
        virtual std::shared_ptr<ModelRenderer>      CreateRenderer(
            ModelScaffold& scaffold, MaterialScaffold& material,
            SharedStateSet& sharedStateSet, unsigned levelOfDetail) = 0;

        virtual std::string DefaultMaterialName(const ModelScaffold&) = 0;
    };
}}



