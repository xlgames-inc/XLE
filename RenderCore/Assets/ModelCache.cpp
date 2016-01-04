// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelCache.h"
#include "ModelRunTime.h"
#include "Material.h"
#include "SharedStateSet.h"
#include "Services.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include <map>

namespace RenderCore { namespace Assets
{
    typedef std::pair<Float3, Float3> BoundingBox;
    class ModelCache::Pimpl
    {
    public:
        std::map<uint64, BoundingBox> _boundingBoxes;

        LRUCache<ModelScaffold>     _modelScaffolds;
        LRUCache<MaterialScaffold>  _materialScaffolds;
        LRUCache<ModelRenderer>     _modelRenderers;

        std::shared_ptr<RenderCore::Assets::IModelFormat> _format;
        std::unique_ptr<SharedStateSet> _sharedStateSet;

        Pimpl(const ModelCache::Config& cfg);
        ~Pimpl();

        LRUCache<ModelSupplementScaffold>   _supplements;
        std::vector<const ModelSupplementScaffold*> 
            LoadSupplementScaffolds(
                const ResChar modelFilename[], 
                const ResChar materialFilename[],
                IteratorRange<const SupplementGUID*> supplements);
    };
        
    ModelCache::Pimpl::Pimpl(const ModelCache::Config& cfg)
    : _modelScaffolds(cfg._modelScaffoldCount)
    , _materialScaffolds(cfg._materialScaffoldCount)
    , _modelRenderers(cfg._rendererCount)
    , _supplements(100)
    {
    }

    ModelCache::Pimpl::~Pimpl() {}

    namespace Internal
    {
        static std::shared_ptr<ModelScaffold> CreateModelScaffold(
            const ::Assets::ResChar filename[], 
            RenderCore::Assets::IModelFormat& modelFormat)
        {
            auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            auto marker = compilers.PrepareAsset(
                ModelScaffold::CompileProcessType, 
                (const char**)&filename, 1, store);
            if (!marker) return nullptr;
            return std::make_shared<ModelScaffold>(std::move(marker));
        }

        static std::shared_ptr<MaterialScaffold> CreateMaterialScaffold(
            const ::Assets::ResChar model[], 
            const ::Assets::ResChar material[], 
            RenderCore::Assets::IModelFormat& modelFormat)
        {
                // note --  we need to remove any parameters after ':' in the model name
                //          these are references to sub-nodes within the model hierarchy
                //          (which are irrelevant when dealing with materials, since the
                //          materials are shared for the entire model file)
            ::Assets::ResChar temp[MaxPath];
            auto splitter = MakeFileNameSplitter(model);
            if (!splitter.ParametersWithDivider().Empty()) {
                XlCopyString(temp, splitter.AllExceptParameters());
                model = temp;
            }

            auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            const ::Assets::ResChar* inits[] = { material, model };
            auto marker = compilers.PrepareAsset(
                MaterialScaffold::CompileProcessType, 
                inits, dimof(inits), store);
            if (!marker) return nullptr;
            return std::make_shared<MaterialScaffold>(std::move(marker));
        }
    }

    auto ModelCache::GetScaffolds(
        const ResChar modelFilename[], 
        const ResChar materialFilename[]) -> Scaffolds
    {
        Scaffolds result;
        result._hashedModelName = Hash64(modelFilename);
        result._model = _pimpl->_modelScaffolds.Get(result._hashedModelName).get();
        if (!result._model || result._model->GetDependencyValidation()->GetValidationIndex() > 0) {
            auto model = Internal::CreateModelScaffold(modelFilename, *_pimpl->_format);
            _pimpl->_modelScaffolds.Insert(result._hashedModelName, model);
            result._model = model.get();
        }

            // We can't build the material properly until the material scaffold is ready
            // So don't even try unless we get a successful resolve
        auto resolveResult = result._model->TryResolve();
        if (resolveResult == ::Assets::AssetState::Ready) {
            result._hashedMaterialName = HashCombine(Hash64(materialFilename), result._hashedModelName);
            auto matNamePtr = materialFilename;

            result._material = _pimpl->_materialScaffolds.Get(result._hashedMaterialName).get();
            if (!result._material || result._material->GetDependencyValidation()->GetValidationIndex() > 0) {
                auto mat = Internal::CreateMaterialScaffold(modelFilename, matNamePtr, *_pimpl->_format);
                _pimpl->_materialScaffolds.Insert(result._hashedMaterialName, mat);
                result._material = mat.get();
            }
        } else if (resolveResult == ::Assets::AssetState::Invalid) {
            Throw(::Assets::Exceptions::InvalidAsset(modelFilename, "Scaffolds invalid in ModelCache"));
        } else {
            result._material = nullptr;
        }

        return result;
    }

    namespace Internal
    {
        static std::shared_ptr<ModelSupplementScaffold> CreateSupplement(
            uint64 compilerHash,
            const ::Assets::ResChar modelFilename[],
            const ::Assets::ResChar materialFilename[])
        {
            auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
            auto& store = ::Assets::Services::GetAsyncMan().GetIntermediateStore();
            const ::Assets::ResChar* inits[] = { modelFilename, materialFilename };
            auto marker = compilers.PrepareAsset(
                compilerHash, // ConstHash64<'PER_', 'VERT', 'EX_A', 'O'>::Value, 
                inits, dimof(inits), store);
            if (!marker) return nullptr;
            return std::make_shared<ModelSupplementScaffold>(std::move(marker));
        }
    }

    std::vector<const ModelSupplementScaffold*> ModelCache::Pimpl::LoadSupplementScaffolds(
        const ResChar modelFilename[], const ResChar materialFilename[],
        IteratorRange<const SupplementGUID*> supplements)
    {
        std::vector<const ModelSupplementScaffold*> result;
        for (auto s=supplements.cbegin(); s!=supplements.cend(); ++s) {
            auto hashName = HashCombine(HashCombine(Hash64(modelFilename), Hash64(materialFilename)), *s);
            auto supp = _supplements.Get(hashName);
            if (!supp || supp->GetDependencyValidation()->GetValidationIndex() > 0) {
                supp = Internal::CreateSupplement(*s, modelFilename, materialFilename);
                if (supp)
                    _supplements.Insert(hashName, supp);
            }
            if (supp)
                result.push_back(supp.get());
        }
        return std::move(result);
    }

    auto ModelCache::GetModel(
        const ResChar modelFilename[], const ResChar materialFilename[],
        IteratorRange<const SupplementGUID*> supplements,
        unsigned LOD) -> Model
    {
        auto scaffold = GetScaffolds(modelFilename, materialFilename);
        if (!scaffold._model)
            Throw(::Assets::Exceptions::PendingAsset(modelFilename, "Scaffolds still pending in ModelCache"));

        auto maxLOD = scaffold._model->GetMaxLOD();
        LOD = std::min(LOD, maxLOD);

            // we also need to load supplements, for any that are requested
        std::vector<const ModelSupplementScaffold*> supplementScaffolds;

        uint64 hashedModel = HashCombine(HashCombine(Hash64(modelFilename), Hash64(materialFilename)), LOD);
        for (auto s=supplements.begin(); s!=supplements.end(); ++s)
            hashedModel = HashCombine(hashedModel, *s);

        auto renderer = _pimpl->_modelRenderers.Get(hashedModel);
        if (!renderer || renderer->GetDependencyValidation()->GetValidationIndex() > 0) {
            auto searchRules = ::Assets::DefaultDirectorySearchRules(modelFilename);
            searchRules.AddSearchDirectoryFromFilename(materialFilename);
            auto suppScaff = _pimpl->LoadSupplementScaffolds(modelFilename, materialFilename, supplements);
            renderer = std::make_shared<ModelRenderer>(
                std::ref(*scaffold._model), std::ref(*scaffold._material), 
                MakeIteratorRange(suppScaff),
                std::ref(*_pimpl->_sharedStateSet), &searchRules, LOD);

            _pimpl->_modelRenderers.Insert(hashedModel, renderer);
        }

            // cache the bounding box, because it's an expensive operation to recalculate
        BoundingBox boundingBox;
        auto boundingBoxI = _pimpl->_boundingBoxes.find(scaffold._hashedModelName);
        if (boundingBoxI== _pimpl->_boundingBoxes.end()) {
            boundingBox = scaffold._model->GetStaticBoundingBox(0);
            _pimpl->_boundingBoxes.insert(std::make_pair(scaffold._hashedModelName, boundingBox));
        } else {
            boundingBox = boundingBoxI->second;
        }

        Model result;
        result._renderer = renderer.get();
        result._sharedStateSet = _pimpl->_sharedStateSet.get();
        result._model = scaffold._model;
        result._boundingBox = boundingBox;
        result._hashedModelName = scaffold._hashedModelName;
        result._hashedMaterialName = scaffold._hashedMaterialName;
        result._selectedLOD = LOD;
        result._maxLOD = maxLOD;
        return result;
    }

    ModelScaffold* ModelCache::GetModelScaffold(const ResChar modelFilename[])
    {
        auto hashedModelName = Hash64(modelFilename);
        auto* result = _pimpl->_modelScaffolds.Get(hashedModelName).get();
        if (!result || result->GetDependencyValidation()->GetValidationIndex() > 0) {
            auto model = Internal::CreateModelScaffold(modelFilename, *_pimpl->_format);
            _pimpl->_modelScaffolds.Insert(hashedModelName, model);
            result = model.get();
        }
        return result;
    }

    SharedStateSet& ModelCache::GetSharedStateSet() { return *_pimpl->_sharedStateSet; }

    ModelCache::ModelCache(const Config& cfg, std::shared_ptr<RenderCore::Assets::IModelFormat> format)
    {
        _pimpl = std::make_unique<Pimpl>(cfg);
        _pimpl->_sharedStateSet = std::make_unique<SharedStateSet>(RenderCore::Assets::Services::GetTechniqueConfigDirs());
        _pimpl->_format = std::move(format);
    }

    ModelCache::~ModelCache()
    {}


}}
