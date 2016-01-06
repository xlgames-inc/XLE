// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialCompiler.h"
#include "ModelImmutableData.h"     // for MaterialImmutableData
#include "Material.h"
#include "CompilationThread.h"
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
	static const unsigned ResolvedMat_ExpectedVersion = 1;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RawMatConfigurations
    {
    public:
        std::vector<std::basic_string<utf8>> _configurations;

        RawMatConfigurations(
            const ::Assets::IntermediateAssetLocator& locator, 
            const ::Assets::ResChar initializer[]);

        static const auto CompileProcessType = ConstHash64<'RawM', 'at'>::Value;

        auto GetDependencyValidation() const -> const std::shared_ptr<::Assets::DependencyValidation>& { return _validationCallback; }
    protected:
        std::shared_ptr<::Assets::DependencyValidation> _validationCallback;
    };

    RawMatConfigurations::RawMatConfigurations(const ::Assets::IntermediateAssetLocator& locator, const ::Assets::ResChar initializer[])
    {
            //  Get associated "raw" material information. This is should contain the material information attached
            //  to the geometry export (eg, .dae file).

        size_t sourceFileSize = 0;
        auto sourceFile = LoadFileAsMemoryBlock(locator._sourceID0, &sourceFileSize);
        if (!sourceFile || sourceFileSize == 0)
            Throw(::Assets::Exceptions::InvalidAsset(
                initializer, 
                StringMeld<128>() << "Missing or empty file: " << locator._sourceID0));

        InputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(sourceFile.get(), PtrAdd(sourceFile.get(), sourceFileSize)));
        Document<decltype(formatter)> doc(formatter);
            
        for (auto config=doc.FirstChild(); config; config=config.NextSibling()) {
            auto name = config.Name();
            if (name.Empty()) continue;
            _configurations.push_back(name.AsString());
        }

        _validationCallback = locator._dependencyValidation;
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
        const ::Assets::ResChar sourceMaterial[], const ::Assets::ResChar sourceModel[],
        const ::Assets::ResChar destination[])
    {
            // Parameters must be stripped off the source model filename before we get here.
            // the parameters are irrelevant to the compiler -- so if they stay on the request
            // name, will we end up with multiple assets that are equivalent
        assert(MakeFileNameSplitter(sourceModel).ParametersWithDivider().Empty());

            // note -- we can throw pending & invalid from here...
        auto& modelMat = ::Assets::GetAssetComp<RawMatConfigurations>(sourceModel);
            
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

        AddDep(deps, sourceModel);        // we need need a dependency (even if it's a missing file)

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
            
            TRY {
                    // resolve in model:configuration
                auto configName = Conversion::Convert<::Assets::rstring>(*i);
                Meld meld; meld << sourceModel << ":" << configName;
                resName << meld;
                auto& rawMat = RawMaterial::GetAsset(meld);
                rawMat._asset.Resolve(resMat, searchRules, &deps);
            } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
            } CATCH_END

            if (resolvedSourceMaterial[0] != '\0') {
                AddDep(deps, resolvedSourceMaterial);        // we need need a dependency (even if it's a missing file)

                TRY {
                        // resolve in material:*
                    Meld meld; meld << resolvedSourceMaterial << ":*";
                    resName << ";" << meld;
                    auto& rawMat = RawMaterial::GetAsset(meld);
                    rawMat._asset.Resolve(resMat, searchRules, &deps);
                } CATCH (const ::Assets::Exceptions::InvalidAsset&) {
                } CATCH_END

                TRY {
                        // resolve in material:configuration
                    Meld meld; meld << resolvedSourceMaterial << ":" << Conversion::Convert<::Assets::rstring>(*i);
                    resName << ";" << meld;
                    auto& rawMat = RawMaterial::GetAsset(meld);
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

            output.BeginChunk(ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion, Meld() << sourceModel << "&" << sourceMaterial);
            output.Write(block.get(), 1, blockSize);
            output.FinishCurrentChunk();
        }

        return ::Assets::CompilerHelper::CompileResult { std::move(deps), std::string() };
    }

    static void DoCompileMaterialScaffold(QueuedCompileOperation& op)
    {
        TRY
        {
            auto compileResult = CompileMaterialScaffold(op._initializer0, op._initializer1, op.GetLocator()._sourceID0);
            op.GetLocator()._dependencyValidation = op._destinationStore->WriteDependencies(
                op.GetLocator()._sourceID0, MakeStringSection(compileResult._baseDir), 
                MakeIteratorRange(compileResult._dependencies));
            assert(op.GetLocator()._dependencyValidation);

            op.SetState(::Assets::AssetState::Ready);
        } CATCH(const ::Assets::Exceptions::PendingAsset&) {
            throw;
        } CATCH(...) {
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialScaffoldCompiler::Pimpl
    {
    public:
        Threading::Mutex _threadLock;
        std::unique_ptr<CompilationThread> _thread;
    };

    class MatCompilerMarker : public ::Assets::ICompileMarker
    {
    public:
        ::Assets::IntermediateAssetLocator GetExistingAsset() const;
        std::shared_ptr<::Assets::PendingCompileMarker> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        MatCompilerMarker(
            ::Assets::rstring materialFilename, ::Assets::rstring modelFilename,
            const ::Assets::IntermediateAssets::Store& store,
            std::shared_ptr<MaterialScaffoldCompiler> compiler);
        ~MatCompilerMarker();
    private:
        std::weak_ptr<MaterialScaffoldCompiler> _compiler;
        ::Assets::rstring _materialFilename, _modelFilename;
        const ::Assets::IntermediateAssets::Store* _store;

        void GetIntermediateName(::Assets::ResChar destination[], size_t destinationCount) const;
    };

    void MatCompilerMarker::GetIntermediateName(::Assets::ResChar destination[], size_t destinationCount) const
    {
        _store->MakeIntermediateName(destination, (unsigned)destinationCount, _materialFilename.c_str());
        StringMeldAppend(destination, &destination[destinationCount])
            << "-" << MakeFileNameSplitter(_modelFilename).FileAndExtension().AsString() << "-resmat";
    }

    ::Assets::IntermediateAssetLocator MatCompilerMarker::GetExistingAsset() const
    {
        ::Assets::IntermediateAssetLocator result;
        GetIntermediateName(result._sourceID0, dimof(result._sourceID0));
        result._dependencyValidation = _store->MakeDependencyValidation(result._sourceID0);
        return result;
    }

    std::shared_ptr<::Assets::PendingCompileMarker> MatCompilerMarker::InvokeCompile() const
    {
        auto c = _compiler.lock();
        if (!c) return nullptr;

        using namespace ::Assets;
        StringMeld<256,ResChar> debugInitializer;
        debugInitializer<< _materialFilename << "(material scaffold)";

        auto backgroundOp = std::make_shared<QueuedCompileOperation>();
        backgroundOp->SetInitializer(debugInitializer);
        XlCopyString(backgroundOp->_initializer0, _materialFilename);
        XlCopyString(backgroundOp->_initializer1, _modelFilename);
        backgroundOp->_destinationStore = _store;
        GetIntermediateName(backgroundOp->GetLocator()._sourceID0, dimof(backgroundOp->GetLocator()._sourceID0));

		{
			ScopedLock(c->_pimpl->_threadLock);
			if (!c->_pimpl->_thread)
				c->_pimpl->_thread = std::make_unique<CompilationThread>(
					[](QueuedCompileOperation& op) { DoCompileMaterialScaffold(op); });
		}
		c->_pimpl->_thread->Push(backgroundOp);

        return std::move(backgroundOp);
    }

    StringSection<::Assets::ResChar> MatCompilerMarker::Initializer() const
    {
        return MakeStringSection(_materialFilename);
    }

    MatCompilerMarker::MatCompilerMarker(
        ::Assets::rstring materialFilename, ::Assets::rstring modelFilename,
        const ::Assets::IntermediateAssets::Store& store,
        std::shared_ptr<MaterialScaffoldCompiler> compiler)
    : _materialFilename(materialFilename), _modelFilename(modelFilename), _compiler(std::move(compiler)), _store(&store) {}
    MatCompilerMarker::~MatCompilerMarker() {}

    std::shared_ptr<::Assets::ICompileMarker> MaterialScaffoldCompiler::PrepareAsset(
        uint64 typeCode, 
        const ::Assets::ResChar* initializers[], unsigned initializerCount,
        const ::Assets::IntermediateAssets::Store& store)
    {
        if (initializerCount != 2 || !initializers[0][0] || !initializers[1][0]) 
            Throw(::Exceptions::BasicLabel("Expecting exactly 2 initializers in MaterialScaffoldCompiler. Material filename first, then model filename"));

        const auto* materialFilename = initializers[0], *modelFilename = initializers[1];
        return std::make_shared<MatCompilerMarker>(materialFilename, modelFilename, store, shared_from_this());
    }

    void MaterialScaffoldCompiler::StallOnPendingOperations(bool cancelAll)
    {
        {
            ScopedLock(_pimpl->_threadLock);
            if (!_pimpl->_thread) return;
        }
        _pimpl->_thread->StallOnPendingOperations(cancelAll);
    }

    MaterialScaffoldCompiler::MaterialScaffoldCompiler()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    MaterialScaffoldCompiler::~MaterialScaffoldCompiler()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    const MaterialImmutableData& MaterialScaffold::ImmutableData() const
    {
        Resolve(); 
        return *(const MaterialImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const MaterialImmutableData*   MaterialScaffold::TryImmutableData() const
    {
        if (!_rawMemoryBlock) return nullptr;
        return (const MaterialImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ResolvedMaterial* MaterialScaffold::GetMaterial(MaterialGuid guid) const
    {
        const auto& data = ImmutableData();
        auto i = LowerBound(data._materials, guid);
        if (i!=data._materials.end() && i->first==guid)
            return &i->second;
        return nullptr;
    }

    const char* MaterialScaffold::GetMaterialName(MaterialGuid guid) const
    {
        const auto& data = ImmutableData();
        auto i = LowerBound(data._materialNames, guid);
        if (i!=data._materialNames.end() && i->first==guid)
            return i->second.c_str();
        return nullptr;
    }
    
    static const ::Assets::AssetChunkRequest MaterialScaffoldChunkRequests[]
    {
        ::Assets::AssetChunkRequest {
            "Scaffold", ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion, 
            ::Assets::AssetChunkRequest::DataType::BlockSerializer 
        }
    };

    MaterialScaffold::MaterialScaffold(std::shared_ptr<::Assets::ICompileMarker>&& marker)
        : ChunkFileAsset("MaterialScaffold")
    {
        Prepare(*marker, ResolveOp{MakeIteratorRange(MaterialScaffoldChunkRequests), &Resolver}); 
    }

    MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom) never_throws
    :   ::Assets::ChunkFileAsset(std::move(moveFrom))
    ,   _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
    {}

    MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom) never_throws
    {
        ::Assets::ChunkFileAsset::operator=(std::move(moveFrom));
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
        return *this;
    }

    MaterialScaffold::~MaterialScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~MaterialImmutableData();
    }

    void MaterialScaffold::Resolver(void* obj, IteratorRange<::Assets::AssetChunkResult*> chunks)
    {
        assert(chunks.size() == 1);
        ((MaterialScaffold*)obj)->_rawMemoryBlock = std::move(chunks[0]._buffer);
    }

}}

