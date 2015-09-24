// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include <utility>

namespace RenderCore { namespace Assets
{
    class ModelRenderer;
    class ModelScaffold;
    class MaterialScaffold;
    class SharedStateSet;
    class IModelFormat;

    class ModelCache
    {
    public:
        class Model
        {
        public:
            ModelRenderer* _renderer;
            SharedStateSet* _sharedStateSet;
            ModelScaffold* _model;
            std::pair<Float3, Float3> _boundingBox;
            uint64 _hashedModelName;
            uint64 _hashedMaterialName;
            unsigned _selectedLOD;
            unsigned _maxLOD;

            Model()
            : _renderer(nullptr), _sharedStateSet(nullptr)
            , _hashedModelName(0), _hashedMaterialName(0)
            , _selectedLOD(0), _maxLOD(0) {}
        };

        class Scaffolds
        {
        public:
            ModelScaffold* _model;
            MaterialScaffold* _material;
            uint64 _hashedModelName;
            uint64 _hashedMaterialName;
        };

        class Config
        {
        public:
            unsigned _modelScaffoldCount;
            unsigned _materialScaffoldCount;
            unsigned _rendererCount;

            Config() : _modelScaffoldCount(2000), _materialScaffoldCount(2000), _rendererCount(100) {}
        };

        using ResChar = ::Assets::ResChar;
        using SupplementGUID = uint64;

        Model GetModel(
            const ResChar modelFilename[], 
            const ResChar materialFilename[],
            IteratorRange<SupplementGUID*> supplements = IteratorRange<SupplementGUID*>(),
            unsigned LOD = 0);
        Scaffolds GetScaffolds(
            const ResChar modelFilename[], 
            const ResChar materialFilename[]);

        ModelScaffold* GetModelScaffold(const ResChar modelFilename[]);

        SharedStateSet& GetSharedStateSet();

        ModelCache(const Config& cfg = Config(), std::shared_ptr<IModelFormat> format = nullptr);
        ~ModelCache();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}
