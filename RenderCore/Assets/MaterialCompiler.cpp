// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialCompiler.h"
#include "RawMaterial.h"
#include "MaterialScaffold.h"
#include "../../Assets/CompilationThread.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/Assets.h"
#include "../../Assets/NascentChunk.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/MemoryFile.h"
#include "../../Assets/AssetServices.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Streams/StreamDOM.h"
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/StringFormat.h"

namespace RenderCore { namespace Assets
{
///////////////////////////////////////////////////////////////////////////////////////////////////

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

    static void CompileMaterialScaffold(
        StringSection<::Assets::ResChar> sourceMaterial, StringSection<::Assets::ResChar> sourceModel,
        StringSection<::Assets::ResChar> destinationFile,
		::Assets::ArtifactFuture& compileMarker,
		const ::Assets::IntermediateAssets::Store& destinationStore)
    {
		std::vector<::Assets::DependentFileState> deps;
		TRY
		{

				// Parameters must be stripped off the source model filename before we get here.
				// the parameters are irrelevant to the compiler -- so if they stay on the request
				// name, will we end up with multiple assets that are equivalent
			assert(MakeFileNameSplitter(sourceModel).ParametersWithDivider().IsEmpty());

			auto modelMatFuture = ::Assets::MakeAsset<RawMatConfigurations>(sourceModel);
			auto modelMatState = modelMatFuture->StallWhilePending();
			if (modelMatState == ::Assets::AssetState::Invalid) {
				Throw(::Assets::Exceptions::ConstructionError(
					::Assets::Exceptions::ConstructionError::Reason::FormatNotUnderstood,
					modelMatFuture->GetDependencyValidation(),
					"Failed while loading material information from source model (%s)", sourceModel.AsString().c_str()));
			}

			auto modelMat = modelMatFuture->Actualize();

				//  for each configuration, we want to build a resolved material
				//  Note that this is a bit crazy, because we're going to be loading
				//  and re-parsing the same files over and over again!
			SerializableVector<std::pair<MaterialGuid, MaterialScaffold::Material>> resolved;
			SerializableVector<std::pair<MaterialGuid, SerializableVector<char>>> resolvedNames;
			std::vector<ShaderPatchCollection> patchCollections;
			resolved.reserve(modelMat->_configurations.size());
			patchCollections.reserve(modelMat->_configurations.size());

			auto searchRules = ::Assets::DefaultDirectorySearchRules(sourceModel);
			::Assets::ResChar resolvedSourceMaterial[MaxPath];
			ResolveMaterialFilename(resolvedSourceMaterial, dimof(resolvedSourceMaterial), searchRules, sourceMaterial);
			searchRules.AddSearchDirectoryFromFilename(resolvedSourceMaterial);

			AddDep(deps, sourceModel);        // we need need a dependency (even if it's a missing file)

			using Meld = StringMeld<MaxPath, ::Assets::ResChar>;
			for (const auto& cfg:modelMat->_configurations) {

				MaterialScaffold::Material resMat;
				ShaderPatchCollection patchCollection;
				std::basic_stringstream<::Assets::ResChar> resName;
				auto guid = MakeMaterialGuid(MakeStringSection(cfg));

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
				Meld meld; meld << sourceModel << ":" << Conversion::Convert<::Assets::rstring>(cfg);
				resName << meld;

				MergeIn_Stall(resMat, patchCollection, meld.AsStringSection(), searchRules, deps);

				if (resolvedSourceMaterial[0] != '\0') {
						// resolve in material:*
					Meld starInit; starInit << resolvedSourceMaterial << ":*";
					Meld configInit; configInit << resolvedSourceMaterial << ":" << Conversion::Convert<::Assets::rstring>(cfg);
                
					MergeIn_Stall(resMat, patchCollection, starInit.AsStringSection(), searchRules, deps);
					MergeIn_Stall(resMat, patchCollection, configInit.AsStringSection(), searchRules, deps);

					resName << ";" << starInit << ";" << configInit;
				}

				resolved.push_back(std::make_pair(guid, std::move(resMat)));

				auto resNameStr = resName.str();
				SerializableVector<char> resNameVec(resNameStr.begin(), resNameStr.end());
				resolvedNames.push_back(std::make_pair(guid, std::move(resNameVec)));
				patchCollections.emplace_back(std::move(patchCollection));
			}

			std::sort(resolved.begin(), resolved.end(), CompareFirst<MaterialGuid, MaterialScaffold::Material>());
			std::sort(resolvedNames.begin(), resolvedNames.end(), CompareFirst<MaterialGuid, SerializableVector<char>>());

				// "resolved" is now actually the data we want to write out
			Serialization::NascentBlockSerializer blockSerializer;
			::Serialize(blockSerializer, resolved);
			::Serialize(blockSerializer, resolvedNames);

			MemoryOutputStream<utf8> patchCollectionStrm;
			{
				OutputStreamFormatter fmtter(patchCollectionStrm);
				SerializeShaderPatchCollectionSet(fmtter, MakeIteratorRange(patchCollections));
			}

			::Assets::NascentChunk chunks[] = {
				{
					Serialization::ChunkFile::ChunkHeader{ ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion, Meld() << sourceModel << "&" << sourceMaterial },
					::Assets::AsBlob(blockSerializer)
				},
				{
					Serialization::ChunkFile::ChunkHeader{ ChunkType_PatchCollections, ResolvedMat_ExpectedVersion, Meld() << sourceModel << "&" << sourceMaterial },
					::Assets::AsBlob(MakeIteratorRange(patchCollectionStrm.GetBuffer().Begin(), patchCollectionStrm.GetBuffer().End()))
				}
			};

			auto mainBlob = std::make_shared<std::vector<uint8_t>>();
			auto mainFile = ::Assets::CreateMemoryFile(mainBlob);

			::Assets::BuildChunkFile(
				*mainFile, MakeIteratorRange(chunks),
				ConsoleRig::GetLibVersionDesc());

			{
				auto dst = ::Assets::MainFileSystem::OpenFileInterface(destinationFile, "wb");
				dst->Write(mainBlob->data(), mainBlob->size());
			}

			auto depVal = destinationStore.WriteDependencies(destinationFile, {}, MakeIteratorRange(deps));
			compileMarker.AddArtifact(
				"main",
				std::make_shared<::Assets::BlobArtifact>(mainBlob, depVal));

			compileMarker.SetState(::Assets::AssetState::Ready);

		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, ::Assets::AsDepVal(MakeIteratorRange(deps))));
		} CATCH(const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, ::Assets::AsDepVal(MakeIteratorRange(deps))));
		} CATCH(...) {
			Throw(::Assets::Exceptions::ConstructionError(
				::Assets::Exceptions::ConstructionError::Reason::Unknown,
				::Assets::AsDepVal(MakeIteratorRange(deps)),
				"%s", "unknown exception"));
		} CATCH_END
        
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MatCompilerMarker : public ::Assets::IArtifactCompileMarker
    {
    public:
        std::shared_ptr<::Assets::IArtifact> GetExistingAsset() const;
        std::shared_ptr<::Assets::ArtifactFuture> InvokeCompile() const;
        StringSection<::Assets::ResChar> Initializer() const;

        MatCompilerMarker(
            ::Assets::rstring materialFilename, ::Assets::rstring modelFilename,
            const ::Assets::IntermediateAssets::Store& store);
        ~MatCompilerMarker();
    private:
        std::weak_ptr<::Assets::CompilationThread> _thread;
        ::Assets::rstring _materialFilename, _modelFilename;
        const ::Assets::IntermediateAssets::Store* _store;

        void GetIntermediateName(::Assets::ResChar destination[], size_t destinationCount) const;
    };

    void MatCompilerMarker::GetIntermediateName(::Assets::ResChar destination[], size_t destinationCount) const
    {
        _store->MakeIntermediateName(destination, (unsigned)destinationCount, MakeStringSection(_materialFilename));
        StringMeldAppend(destination, &destination[destinationCount])
            << "-" << MakeFileNameSplitter(_modelFilename).FileAndExtension().AsString() << "-resmat";
    }

    std::shared_ptr<::Assets::IArtifact> MatCompilerMarker::GetExistingAsset() const
    {
		::Assets::ResChar intermediateName[MaxPath];
        GetIntermediateName(intermediateName, dimof(intermediateName));
        auto depVal = _store->MakeDependencyValidation(intermediateName);
        return std::make_shared<::Assets::FileArtifact>(intermediateName, depVal);
    }

    std::shared_ptr<::Assets::ArtifactFuture> MatCompilerMarker::InvokeCompile() const
    {
        using namespace ::Assets;
        StringMeld<256,ResChar> debugInitializer;
        debugInitializer<< _materialFilename << "(material scaffold)";

        auto backgroundOp = std::make_shared<::Assets::ArtifactFuture>();
        backgroundOp->SetInitializer(debugInitializer);

		::Assets::ResChar intermediateName[MaxPath];
		GetIntermediateName(intermediateName, dimof(intermediateName));

		::Assets::rstring destinationFile = intermediateName;
		auto materialFilename = _materialFilename;
		auto modelFilename = _modelFilename;
		auto* store = _store;
		QueueCompileOperation(
			backgroundOp,
			[materialFilename, modelFilename, destinationFile, store](::Assets::ArtifactFuture& op) {
				CompileMaterialScaffold(
					MakeStringSection(materialFilename), MakeStringSection(modelFilename), MakeStringSection(destinationFile),
					op, *store);
			});

        return backgroundOp;
    }

    StringSection<::Assets::ResChar> MatCompilerMarker::Initializer() const
    {
        return MakeStringSection(_materialFilename);
    }

    MatCompilerMarker::MatCompilerMarker(
        ::Assets::rstring materialFilename, ::Assets::rstring modelFilename,
        const ::Assets::IntermediateAssets::Store& store)
    : _materialFilename(materialFilename), _modelFilename(modelFilename), _store(&store) {}
    MatCompilerMarker::~MatCompilerMarker() {}

    std::shared_ptr<::Assets::IArtifactCompileMarker> MaterialScaffoldCompiler::Prepare(
        uint64 typeCode, 
        const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
    {
		if (typeCode != RenderCore::Assets::MaterialScaffold::CompileProcessType) return nullptr;

        if (initializerCount != 2 || initializers[0].IsEmpty() || initializers[1].IsEmpty())
            Throw(::Exceptions::BasicLabel("Expecting exactly 2 initializers in MaterialScaffoldCompiler. Material filename first, then model filename"));

        const auto materialFilename = initializers[0], modelFilename = initializers[1];
        return std::make_shared<MatCompilerMarker>(materialFilename.AsString(), modelFilename.AsString(), *::Assets::Services::GetAsyncMan().GetIntermediateStore());
    }

	std::vector<uint64_t> MaterialScaffoldCompiler::GetTypesForAsset(const StringSection<::Assets::ResChar> initializers[], unsigned initializerCount)
	{
		return {};
	}

	std::vector<std::pair<std::string, std::string>> MaterialScaffoldCompiler::GetExtensionsForType(uint64_t typeCode)
	{
		return {};
	}

	void MaterialScaffoldCompiler::StallOnPendingOperations(bool cancelAll) {}

	MaterialScaffoldCompiler::MaterialScaffoldCompiler()
    {}

    MaterialScaffoldCompiler::~MaterialScaffoldCompiler()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

}}

