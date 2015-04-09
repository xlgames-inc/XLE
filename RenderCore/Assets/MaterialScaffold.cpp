// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialScaffold.h"
#include "ModelRunTime.h"
#include "ModelRunTimeInternal.h"
#include "Material.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/IntermediateResources.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ExceptionLogging.h"

namespace RenderCore { extern char VersionString[]; extern char BuildDateString[]; }

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void CompileMaterialScaffold(const char source[], const char destination[], std::vector<::Assets::DependentFileState>* outDeps)
    {
            //  get associated "raw" material information. This is should contain the material information attached
            //  to the geometry export (eg, .dae file). Note -- maybe the name of the raw file should come
            //  from the .material name (ie, to make it easier to have multiple material files with the same dae file)
        ::Assets::ResChar concreteFilename[MaxPath];
        XlCopyString(concreteFilename, dimof(concreteFilename), source);
        XlChopExtension(concreteFilename);
        MakeConcreteRawMaterialFilename(concreteFilename, dimof(concreteFilename), concreteFilename);

        Serialization::Vector<std::string> configurations;

        {
                //  We need to load the "-rawmat" file first to get the list
                //  of configurations within
            size_t sourceFileSize = 0;
            auto sourceFile = LoadFileAsMemoryBlock(concreteFilename, &sourceFileSize);
            if (!sourceFile)
                ThrowException(::Assets::Exceptions::InvalidResource(source, 
                    StringMeld<128>() << "Missing or empty file: " << concreteFilename));

            Data data;
            data.Load((const char*)sourceFile.get(), (int)sourceFileSize);

            for (auto config=data.child; config; config=config->next) {
                if (!config->value) continue;
                configurations.push_back(config->value);
            }
        }

        auto searchRules = ::Assets::DefaultDirectorySearchRules(source);
        std::vector<::Assets::DependentFileState> deps;

            //  for each configuration, we want to build a resolved material
            //  Note that this is a bit crazy, because we're going to be loading
            //  and re-parsing the same files over and over again!
        Serialization::Vector<std::pair<MaterialGuid, ResolvedMaterial>> resolved;
        Serialization::Vector<std::pair<MaterialGuid, std::string>> resolvedNames;
        resolved.reserve(configurations.size());

        // StringMeld<MaxPath, ::Assets::ResChar> materialFilename;
        // materialFilename << source;
        // if (!XlExtension(source)) materialFilename << ".material";

        for (auto i=configurations.cbegin(); i!=configurations.cend(); ++i) {
            auto guid = MakeMaterialGuid(i->c_str());
            StringMeld<MaxPath, ::Assets::ResChar> matName;
            matName << concreteFilename << ":" << *i;
            TRY {
                auto& rawMat = ::Assets::GetAssetDep<RawMaterial>((const ::Assets::ResChar*)matName);
                ResolvedMaterial resMat;
                rawMat.Resolve(resMat, searchRules, &deps);
                resolved.push_back(std::make_pair(guid, std::move(resMat)));
                resolvedNames.push_back(std::make_pair(guid, std::string(StringMeld<MaxPath, ::Assets::ResChar>() << source << ":" << *i)));
            } CATCH (const ::Assets::Exceptions::InvalidResource& e) {
                LogWarning << "Got an invalid resource exception while compiling material scaffold for " << source;
                LogWarning << "Exception follows: " << e;
            } CATCH_END
        }

        std::sort(resolved.begin(), resolved.end(), CompareFirst<MaterialGuid, ResolvedMaterial>());
        std::sort(resolvedNames.begin(), resolvedNames.end(), CompareFirst<MaterialGuid, std::string>());

            // "resolved" is now actually the data we want to write out
        {
            Serialization::NascentBlockSerializer blockSerializer;
            Serialization::Serialize(blockSerializer, resolved);
            Serialization::Serialize(blockSerializer, resolvedNames);

            auto blockSize = blockSerializer.Size();
            auto block = blockSerializer.AsMemoryBlock();

            Serialization::ChunkFile::SimpleChunkFileWriter output(
                1, destination, "wb", 0,
                VersionString, BuildDateString);

            output.BeginChunk(ChunkType_ResolvedMat, 0, source);
            output.Write(block.get(), 1, blockSize);
            output.FinishCurrentChunk();
        }

        if (outDeps) {
            *outDeps = std::move(deps);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<::Assets::PendingCompileMarker> MaterialScaffoldCompiler::PrepareResource(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount,
        const ::Assets::IntermediateResources::Store& destinationStore)
    {
        if (!initializerCount || !initializers[0][0]) return nullptr;

        char outputName[MaxPath];
        destinationStore.MakeIntermediateName(outputName, dimof(outputName), initializers[0]);
        XlCatString(outputName, dimof(outputName), "-resmat");

        if (DoesFileExist(outputName)) {
                // MakeDependencyValidation returns an object only if dependencies are currently good
             auto depVal = destinationStore.MakeDependencyValidation(outputName);
             if (depVal) {
                    // already exists -- just return "ready"
                return std::make_unique<::Assets::PendingCompileMarker>(
                    ::Assets::AssetState::Ready, outputName, ~0ull, std::move(depVal));
             }
        }

        std::vector<::Assets::DependentFileState> deps;
        CompileMaterialScaffold(initializers[0], outputName, &deps);

        auto newDepVal = destinationStore.WriteDependencies(outputName, "", deps);
        return std::make_unique<::Assets::PendingCompileMarker>(
            ::Assets::AssetState::Ready, outputName, ~0ull, std::move(newDepVal));
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
        assert(marker && marker->GetState() == ::Assets::AssetState::Ready);

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

