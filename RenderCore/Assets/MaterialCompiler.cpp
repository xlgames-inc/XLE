// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialCompiler.h"
#include "ModelImmutableData.h"     // for MaterialImmutableData
#include "Material.h"
#include "../../Assets/CompilationThread.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/ChunkFileContainer.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/CompilerHelper.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/DeferredConstruction.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ExceptionLogging.h"
#include "../../Utility/Conversion.h"
#include "../../Utility/IteratorUtils.h"

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
            const ::Assets::IArtifact& locator, 
            StringSection<::Assets::ResChar> initializer);

        static const auto CompileProcessType = ConstHash64<'RawM', 'at'>::Value;

        auto GetDependencyValidation() const -> const std::shared_ptr<::Assets::DependencyValidation>& { return _validationCallback; }

		static std::shared_ptr<::Assets::DeferredConstruction> BeginDeferredConstruction(
			const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
		{
			return ::Assets::DefaultBeginDeferredConstruction<RawMatConfigurations>(initializers, initializerCount);
		}
    protected:
        std::shared_ptr<::Assets::DependencyValidation> _validationCallback;
    };

    RawMatConfigurations::RawMatConfigurations(const ::Assets::IArtifact& locator, StringSection<::Assets::ResChar> initializer)
    {
            //  Get associated "raw" material information. This is should contain the material information attached
            //  to the geometry export (eg, .dae file).

		auto blob = locator.GetBlob();
        if (!blob || blob->size() == 0)
            Throw(::Assets::Exceptions::InvalidAsset(initializer, "Missing or empty file"));

        InputStreamFormatter<utf8> formatter(
            MemoryMappedInputStream(AsPointer(blob->begin()), AsPointer(blob->end())));
        Document<decltype(formatter)> doc(formatter);
            
        for (auto config=doc.FirstChild(); config; config=config.NextSibling()) {
            auto name = config.Name();
            if (name.IsEmpty()) continue;
            _configurations.push_back(name.AsString());
        }

        _validationCallback = locator.GetDependencyValidation();
    }


    static void AddDep(
        std::vector<::Assets::DependentFileState>& deps,
        StringSection<::Assets::ResChar> newDep)
    {
            // we need to call "GetDependentFileState" first, because this can change the
            // format of the filename. String compares alone aren't working well for us here
        auto depState = ::Assets::IntermediateAssets::Store::GetDependentFileState(newDep);
        auto existing = std::find_if(
            deps.cbegin(), deps.cend(),
            [&](const ::Assets::DependentFileState& test) { return test._filename == depState._filename; });
        if (existing == deps.cend())
            deps.push_back(depState);
    }

    static ::Assets::CompilerHelper::CompileResult CompileMaterialScaffold(
        StringSection<::Assets::ResChar> sourceMaterial, StringSection<::Assets::ResChar> sourceModel,
        StringSection<::Assets::ResChar> destination)
    {
            // Parameters must be stripped off the source model filename before we get here.
            // the parameters are irrelevant to the compiler -- so if they stay on the request
            // name, will we end up with multiple assets that are equivalent
        assert(MakeFileNameSplitter(sourceModel).ParametersWithDivider().IsEmpty());

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
            auto guid = MakeMaterialGuid(MakeStringSection(*i));

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
            
                // resolve in model:configuration
            auto configName = Conversion::Convert<::Assets::rstring>(*i);
            Meld meld; meld << sourceModel << ":" << configName;
            resName << meld;
            auto& rawMat = RawMaterial::GetAsset(meld);
            auto subMatState = rawMat.TryResolve(resMat, searchRules, &deps);

			// Allow both "invalid" and "ready" assets to continue on. Invalid assets are
			// treated as empty materials
			if (subMatState == ::Assets::AssetState::Pending)
				Throw(::Assets::Exceptions::PendingAsset(meld, "Sub material is pending"));

            if (resolvedSourceMaterial[0] != '\0') {
                AddDep(deps, resolvedSourceMaterial);        // we need need a dependency (even if it's a missing file)

                    // resolve in material:*
                Meld starInit; starInit << resolvedSourceMaterial << ":*";
                resName << ";" << starInit;
                auto& starMat = RawMaterial::GetAsset(starInit);
				subMatState = starMat.TryResolve(resMat, searchRules, &deps);
				if (subMatState == ::Assets::AssetState::Pending)
					Throw(::Assets::Exceptions::PendingAsset(starInit, "Sub material is pending"));

                    // resolve in material:configuration
                Meld configInit; configInit << resolvedSourceMaterial << ":" << Conversion::Convert<::Assets::rstring>(*i);
                resName << ";" << configInit;
                auto& configMat = RawMaterial::GetAsset(configInit);
				subMatState = configMat.TryResolve(resMat, searchRules, &deps);
				if (subMatState == ::Assets::AssetState::Pending)
					Throw(::Assets::Exceptions::PendingAsset(configInit, "Sub material is pending"));
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
				::Assets::MainFileSystem::OpenBasicFile(destination, "wb"),
                1, VersionString, BuildDateString);

            output.BeginChunk(ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion, Meld() << sourceModel << "&" << sourceMaterial);
            output.Write(block.get(), 1, blockSize);
            output.FinishCurrentChunk();
        }

        return ::Assets::CompilerHelper::CompileResult { std::move(deps), std::string() };
    }

    static void DoCompileMaterialScaffold(::Assets::QueuedCompileOperation& op)
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
        } CATCH(const std::exception& e) {
			LogWarning << "Got exception while compiling material scaffold (" << e.what() << ")";
			op.SetState(::Assets::AssetState::Invalid);
		} CATCH(...) {
			LogWarning << "Got unknown exception while compiling material scaffold";
            op.SetState(::Assets::AssetState::Invalid);
        } CATCH_END
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MaterialScaffoldCompiler::Pimpl
    {
    public:
        Threading::Mutex _threadLock;
        std::unique_ptr<::Assets::CompilationThread> _thread;
    };

    class MatCompilerMarker : public ::Assets::ICompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
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

    std::shared_ptr<::Assets::IArtifact> MatCompilerMarker::GetExistingAsset() const
    {
		::Assets::ResChar intermediateName[MaxPath];
        GetIntermediateName(intermediateName, dimof(intermediateName));
        auto depVal = _store->MakeDependencyValidation(intermediateName);
        return std::make_shared<::Assets::FileArtifact(MakeStringSection(intermediateName), depVal));
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
        const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount,
        const ::Assets::IntermediateAssets::Store& store)
    {
        if (initializerCount != 2 || !initializers[0][0] || !initializers[1][0]) 
            Throw(::Exceptions::BasicLabel("Expecting exactly 2 initializers in MaterialScaffoldCompiler. Material filename first, then model filename"));

        const auto materialFilename = initializers[0], modelFilename = initializers[1];
        return std::make_shared<MatCompilerMarker>(materialFilename.AsString(), modelFilename.AsString(), store, shared_from_this());
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

	void MaterialScaffold::Resolve() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				Throw(::Assets::Exceptions::PendingAsset(MakeStringSection(_filename), "Pending deferred construction"));

			auto* mutableThis = const_cast<MaterialScaffold*>(this);
			auto constructor = std::move(mutableThis->_deferredConstructor);
			assert(!mutableThis->_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*mutableThis = std::move(*constructor->PerformConstructor<MaterialScaffold>());
			} else {
				assert(state == ::Assets::AssetState::Invalid);
			}
		}
		if (!_rawMemoryBlock)
			Throw(::Assets::Exceptions::InvalidAsset(MakeStringSection(_filename), "Missing data"));
	}
	
	::Assets::AssetState MaterialScaffold::TryResolve() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->GetAssetState();
			if (state == ::Assets::AssetState::Pending)
				return state;

			auto* mutableThis = const_cast<MaterialScaffold*>(this);
			auto constructor = std::move(mutableThis->_deferredConstructor);
			assert(!mutableThis->_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*mutableThis = std::move(*constructor->PerformConstructor<MaterialScaffold>());
			} // (else fall through);
		}

		return _rawMemoryBlock ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
	}

	::Assets::AssetState MaterialScaffold::StallWhilePending() const
	{
		if (_deferredConstructor) {
			auto state = _deferredConstructor->StallWhilePending();
			auto* mutableThis = const_cast<MaterialScaffold*>(this);
			auto constructor = std::move(mutableThis->_deferredConstructor);
			assert(!mutableThis->_deferredConstructor);
			if (state == ::Assets::AssetState::Ready) {
				*mutableThis = std::move(*constructor->PerformConstructor<MaterialScaffold>());
			} // (else fall through);
		}

		return _rawMemoryBlock ? ::Assets::AssetState::Ready : ::Assets::AssetState::Invalid;
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

    MaterialScaffold::MaterialScaffold(const ::Assets::ChunkFileContainer& chunkFile)
	: _filename(chunkFile.Filename())
	, _depVal(chunkFile.GetDependencyValidation())
    {
		auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(MaterialScaffoldChunkRequests));
		assert(chunks.size() == 1);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
    }

	MaterialScaffold::MaterialScaffold(const std::shared_ptr<::Assets::DeferredConstruction>& deferredConstruction)
	: _deferredConstructor(deferredConstruction)
	, _depVal(deferredConstruction->GetDependencyValidation())
	{}

    MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
	, _deferredConstructor(std::move(moveFrom._deferredConstructor))
	, _filename(std::move(moveFrom._filename))
	, _depVal(moveFrom._depVal)
    {}

    MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom) never_throws
    {
		assert(!_rawMemoryBlock);		// (not thread safe to use this operator after we've hit "ready" status
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_deferredConstructor = std::move(moveFrom._deferredConstructor);
		_filename = std::move(moveFrom._filename);
		_depVal = moveFrom._depVal;
        return *this;
    }

    MaterialScaffold::~MaterialScaffold()
    {
        auto* data = TryImmutableData();
        if (data)
            data->~MaterialImmutableData();
    }

	std::shared_ptr<::Assets::DeferredConstruction> MaterialScaffold::BeginDeferredConstruction(
		const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
	{
		return ::Assets::DefaultBeginDeferredConstruction<MaterialScaffold>(initializers, initializerCount);
	}

}}

