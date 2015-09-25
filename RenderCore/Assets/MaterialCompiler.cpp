// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialCompiler.h"
#include "ModelImmutableData.h"     // for MaterialImmutableData
#include "Material.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/CompilerHelper.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ExceptionLogging.h"
#include "../../Utility/Conversion.h"

namespace RenderCore { extern char VersionString[]; extern char BuildDateString[]; }

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawMatConfigurations
    {
    public:
        SerializableVector<std::basic_string<utf8>> _configurations;
        ::Assets::ResChar _rawModelMaterial[MaxPath];

        RawMatConfigurations(const ::Assets::ResChar sourceModel[]);
    };

    RawMatConfigurations::RawMatConfigurations(const ::Assets::ResChar sourceModel[])
    {
            //  get associated "raw" material information. This is should contain the material information attached
            //  to the geometry export (eg, .dae file). Note -- maybe the name of the raw file should come
            //  from the .material name (ie, to make it easier to have multiple material files with the same dae file)
        XlCopyString(_rawModelMaterial, dimof(_rawModelMaterial), sourceModel);
        XlChopExtension(_rawModelMaterial);
        MakeConcreteRawMaterialFilename(_rawModelMaterial, dimof(_rawModelMaterial), _rawModelMaterial);

        {
                //  We need to load the "-rawmat" file first to get the list
                //  of configurations within
            size_t sourceFileSize = 0;
            auto sourceFile = LoadFileAsMemoryBlock(_rawModelMaterial, &sourceFileSize);
            if (!sourceFile)
                Throw(::Assets::Exceptions::InvalidAsset(sourceModel, 
                    StringMeld<128>() << "Missing or empty file: " << _rawModelMaterial));

            InputStreamFormatter<utf8> formatter(
                MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), sourceFileSize)));
            Document<decltype(formatter)> doc(formatter);
            
            for (auto config=doc.FirstChild(); config; config=config.NextSibling()) {
                auto name = config.Name();
                if (name.empty()) continue;
                _configurations.push_back(name);
            }
        }
    }

    static void AddDep(
        std::vector<::Assets::DependentFileState>& deps,
        const char newDep[])
    {
            // we need to call "GetDependentFileState" first, because this can change the
            // format of the filename. String compares alone aren't working well for us here
        auto depState = ::Assets::IntermediateAssets::Store::GetDependentFileState(newDep);
        auto existing = std::find_if(deps.cbegin(), deps.cend(),
            [&](const ::Assets::DependentFileState& test) 
            {
                return !XlCompareStringI(test._filename.c_str(), depState._filename.c_str());
            });
        if (existing == deps.cend()) {
            deps.push_back(depState);
        }
    }

    static ::Assets::CompilerHelper::CompileResult CompileMaterialScaffold(
        const char sourceMaterial[], const char sourceModel[],
        const char destination[])
    {
        RawMatConfigurations modelMat(sourceModel);
            
        std::vector<::Assets::DependentFileState> deps;

            //  for each configuration, we want to build a resolved material
            //  Note that this is a bit crazy, because we're going to be loading
            //  and re-parsing the same files over and over again!
        SerializableVector<std::pair<MaterialGuid, ResolvedMaterial>> resolved;
        SerializableVector<std::pair<MaterialGuid, std::string>> resolvedNames;
        resolved.reserve(modelMat._configurations.size());

        auto searchRules = ::Assets::DefaultDirectorySearchRules(sourceModel);
        ::Assets::ResChar resolvedSourceMaterial[MaxPath];
        ResolveMaterialFilename(resolvedSourceMaterial, dimof(resolvedSourceMaterial), searchRules, sourceMaterial);
        searchRules.AddSearchDirectoryFromFilename(resolvedSourceMaterial);

        using Meld = StringMeld<MaxPath, ::Assets::ResChar>;
        for (auto i=modelMat._configurations.cbegin(); i!=modelMat._configurations.cend(); ++i) {

            ResolvedMaterial resMat;
            std::basic_stringstream<::Assets::ResChar> resName;
            auto guid = MakeMaterialGuid(AsPointer(i->cbegin()), AsPointer(i->cend()));

                // Our resolved material comes from 3 separate inputs:
                //  1) model:configuration
                //  2) material:*
                //  3) material:configuration
                //
                // Some material information is actually stored in the model
                // source data. This is just for art-pipeline convenience --
                // generally texture assignments (and other settings) are 
                // set in the model authoring tool (eg, 3DS Max). The .material
                // files actually only provide overrides for settings that can't
                // be set within 3rd party tools.
                // 
                // We don't combine the model and material information until
                // this step -- this gives us some flexibility to use the same
                // model with different material files. The material files can
                // also override settings from 3DS Max (eg, change texture assignments
                // etc). This provides a path for reusing the same model with
                // different material settings (eg, when we want one thing to have
                // a red version and a blue version)

            AddDep(deps, modelMat._rawModelMaterial);        // we need need a dependency (even if it's a missing file)
            
            TRY {
                    // resolve in model:configuration
                auto configName = Conversion::Convert<::Assets::rstring>(*i);
                resName << sourceModel << ":" << configName;
                auto& rawMat = ::Assets::GetAssetDep<::Assets::ConfigFileListContainer<RawMaterial>>((Meld() << modelMat._rawModelMaterial << ":" << configName).get());
                rawMat._asset.Resolve(resMat, searchRules, &deps);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END

            if (resolvedSourceMaterial[0] != '\0') {
                AddDep(deps, resolvedSourceMaterial);        // we need need a dependency (even if it's a missing file)

                TRY {
                        // resolve in material:*
                    Meld meld; meld << resolvedSourceMaterial << ":*";
                    resName << ";" << meld;
                    auto& rawMat = ::Assets::GetAssetDep<::Assets::ConfigFileListContainer<RawMaterial>>(meld.get());
                    rawMat._asset.Resolve(resMat, searchRules, &deps);
                } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                } CATCH_END

                TRY {
                        // resolve in material:configuration
                    Meld meld; meld << resolvedSourceMaterial << ":" << Conversion::Convert<::Assets::rstring>(*i);
                    resName << ";" << meld;
                    auto& rawMat = ::Assets::GetAssetDep<::Assets::ConfigFileListContainer<RawMaterial>>(meld.get());
                    rawMat._asset.Resolve(resMat, searchRules, &deps);
                } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                } CATCH_END
            }

            resolved.push_back(std::make_pair(guid, std::move(resMat)));
            resolvedNames.push_back(std::make_pair(guid, resName.str()));
        }

        std::sort(resolved.begin(), resolved.end(), CompareFirst<MaterialGuid, ResolvedMaterial>());
        std::sort(resolvedNames.begin(), resolvedNames.end(), CompareFirst<MaterialGuid, std::string>());

            // "resolved" is now actually the data we want to write out
        {
            Serialization::NascentBlockSerializer blockSerializer;
            ::Serialize(blockSerializer, resolved);
            ::Serialize(blockSerializer, resolvedNames);

            auto blockSize = blockSerializer.Size();
            auto block = blockSerializer.AsMemoryBlock();

            Serialization::ChunkFile::SimpleChunkFileWriter output(
                1, VersionString, BuildDateString, 
                std::make_tuple(destination, "wb", 0));

            output.BeginChunk(ChunkType_ResolvedMat, 0, Meld() << sourceModel << "&" << sourceMaterial);
            output.Write(block.get(), 1, blockSize);
            output.FinishCurrentChunk();
        }

        return ::Assets::CompilerHelper::CompileResult
            {
                std::move(deps),
                std::string()
            };
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<::Assets::PendingCompileMarker> MaterialScaffoldCompiler::PrepareAsset(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount,
        const ::Assets::IntermediateAssets::Store& store)
    {
        if (initializerCount != 2 || !initializers[0][0] || !initializers[1][0]) 
            Throw(::Exceptions::BasicLabel("Expecting exactly 2 initializers in MaterialScaffoldCompiler. Material filename first, then model filename"));

        const auto* materialFilename = initializers[0], *modelFilename = initializers[1];

        using namespace ::Assets;
        ResChar intermediateName[MaxPath];
        store.MakeIntermediateName(intermediateName, materialFilename);
        StringMeldAppend(intermediateName)
            << "-" << MakeFileNameSplitter(modelFilename).FileAndExtension().AsString() << "-resmat";

            // now either return an existing asset, or compile a new one
        auto marker = CompilerHelper::CheckExistingAsset(store, intermediateName);
        if (marker) return marker;

        auto deps = CompileMaterialScaffold(materialFilename, modelFilename, intermediateName);
        return CompilerHelper::PrepareCompileMarker(store, intermediateName, deps);
    }

    void MaterialScaffoldCompiler::StallOnPendingOperations(bool cancelAll)
    {
        // processing occurs in the foreground. So nothing to stall on
    }

    MaterialScaffoldCompiler::MaterialScaffoldCompiler()
    {}

    MaterialScaffoldCompiler::~MaterialScaffoldCompiler()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    const ResolvedMaterial* MaterialScaffold::GetMaterial(MaterialGuid guid) const
    {
        auto i = LowerBound(_data->_materials, guid);
        if (i!=_data->_materials.end() && i->first==guid){
            return &i->second;
        }
        return nullptr;
    }

    const char* MaterialScaffold::GetMaterialName(MaterialGuid guid) const
    {
        auto i = LowerBound(_data->_materialNames, guid);
        if (i!=_data->_materialNames.end() && i->first==guid){
            return i->second.c_str();
        }
        return nullptr;
    }

    MaterialScaffold::MaterialScaffold(std::shared_ptr<::Assets::PendingCompileMarker>&& marker)
    {
        if (!marker || marker->GetState() == ::Assets::AssetState::Invalid) {
            Throw(::Assets::Exceptions::InvalidAsset("", "MaterialScaffold not ready"));
        }

        const auto* filename = marker->_sourceID0;
        BasicFile file(filename, "rb");

        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

        Serialization::ChunkFile::ChunkHeader scaffoldChunk;
        for (auto i=chunks.begin(); i!=chunks.end(); ++i) {
            if (i->_type == ChunkType_ResolvedMat) {
                scaffoldChunk = *i;
                break;
            }
        }

        if (!scaffoldChunk._fileOffset)
            throw ::Assets::Exceptions::FormatError("Missing material scaffold chunk: %s", filename);

        _rawMemoryBlock = std::make_unique<uint8[]>(scaffoldChunk._size);
        file.Seek(scaffoldChunk._fileOffset, SEEK_SET);
        file.Read(_rawMemoryBlock.get(), 1, scaffoldChunk._size);

        Serialization::Block_Initialize(_rawMemoryBlock.get());        
        _data = (const MaterialImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());

        if (marker->_dependencyValidation) {
            _validationCallback = marker->_dependencyValidation;
        } else {
            _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        }
    }

    MaterialScaffold::~MaterialScaffold()
    {
        _data->~MaterialImmutableData();
    }

}}

