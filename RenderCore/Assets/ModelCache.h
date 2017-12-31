// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Assets/AssetFuture.h"
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

    class ModelCache
    {
    public:
        class Model
        {
        public:
			ModelRenderer*  _renderer;
            SharedStateSet* _sharedStateSet;
			ModelScaffold*  _model;
            std::pair<Float3, Float3> _boundingBox;
            uint64          _hashedModelName;
            uint64          _hashedMaterialName;
            unsigned        _selectedLOD;
            unsigned        _maxLOD;

            Model()
            : _renderer(nullptr), _sharedStateSet(nullptr)
            , _hashedModelName(0), _hashedMaterialName(0)
            , _selectedLOD(0), _maxLOD(0) {}
        };

        class Scaffolds
        {
        public:
			ModelScaffold*      _model = nullptr;
			MaterialScaffold*   _material = nullptr;
            uint64              _hashedModelName = 0;
            uint64              _hashedMaterialName = 0;
        };

        class Config
        {
        public:
            unsigned _modelScaffoldCount;
            unsigned _materialScaffoldCount;
            unsigned _rendererCount;

            Config()
            : _modelScaffoldCount(2000), _materialScaffoldCount(2000)
            , _rendererCount(200) {}
        };

        using ResChar = ::Assets::ResChar;
        using SupplementGUID = uint64;
        using SupplementRange = IteratorRange<const SupplementGUID*>;

        Model GetModel(
            StringSection<ResChar> modelFilename, 
            StringSection<ResChar> materialFilename,
            SupplementRange supplements = SupplementRange(),
            unsigned LOD = 0);
        Scaffolds GetScaffolds(
            StringSection<ResChar> modelFilename, 
            StringSection<ResChar> materialFilename);
        ::Assets::AssetState PrepareModel(
            StringSection<ResChar> modelFilename,
            StringSection<ResChar> materialFilename,
            SupplementRange supplements = SupplementRange(),
            unsigned LOD = 0); 

        auto				GetModelScaffold(StringSection<ResChar> modelFilename) -> ::Assets::FuturePtr<ModelScaffold>;
        SharedStateSet&     GetSharedStateSet();

        uint32              GetReloadId();

        ModelCache(const Config& cfg = Config());
        ~ModelCache();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}}
