// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "OverlaySystem.h"
#include "../Assets/AssetUtils.h"

namespace RenderCore { namespace Assets 
{
    class IModelFormat; 
    class ModelRenderer;
    class SharedStateSet;
}}

namespace SceneEngine { class ISceneParser; }

namespace PlatformRig
{
    class ModelVisCache
    {
    public:
        class Model
        {
        public:
            RenderCore::Assets::ModelRenderer* _renderer;
            RenderCore::Assets::SharedStateSet* _sharedStateSet;
            std::pair<Float3, Float3> _boundingBox;
            uint64 _hash;
        };

        Model GetModel(const Assets::ResChar filename[]);
        std::string HashToModelName(uint64 hash);

        ModelVisCache(std::shared_ptr<RenderCore::Assets::IModelFormat> format);
        ~ModelVisCache();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class ModelVisLayer : public IOverlaySystem
    {
    public:
        virtual std::shared_ptr<IInputListener> GetInputListener();

        virtual void RenderToScene(
            RenderCore::IThreadContext* context, 
            SceneEngine::LightingParserContext& parserContext); 
        virtual void RenderWidgets(
            RenderCore::IThreadContext* context, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc);
        virtual void SetActivationState(bool newState);

        ModelVisLayer(std::shared_ptr<ModelVisCache> cache);
        ~ModelVisLayer();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    std::unique_ptr<SceneEngine::ISceneParser> CreateModelScene(const ModelVisCache::Model& model);
}

