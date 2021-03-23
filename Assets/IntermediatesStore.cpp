// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IntermediatesStore.h"
#include "LooseFilesCache.h"
#include "IArtifact.h"
#include "IFileSystem.h"
#include "DepVal.h"
#include "AssetUtils.h"
#include "ChunkFileContainer.h"
#include "BlockSerializer.h"
#include "MemoryFile.h"
#include "../OSServices/Log.h"
#include "../OSServices/RawFS.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Streams/SerializationUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StreamUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/FastParseValue.h"
#include <filesystem>
#include <memory>

namespace Assets
{
	static const auto ChunkType_Log = ConstHash64<'Log'>::Value;

	class IntermediatesStore::Pimpl
	{
	public:
		mutable std::string _resolvedBaseDirectory;
		mutable std::unique_ptr<IFileInterface> _markerFile;

		struct ConstructorOptions
		{
			::Assets::rstring _baseDir;
			::Assets::rstring _versionString;
			::Assets::rstring _configString;
		};
		ConstructorOptions _constructorOptions;

		struct Group
		{
			std::shared_ptr<LooseFilesStorage> _looseFilesStorage;
		};

		std::unordered_map<uint64_t, Group> _groups;

		std::shared_ptr<StoreReferenceCounts> _storeRefCounts;

		void ResolveBaseDirectory() const;
		uint64_t MakeHashCode(
			StringSection<> archivableName,
			CompileProductsGroupId groupId) const;
	};

	static ResChar ConvChar(ResChar input) 
	{
		return (ResChar)((input == '\\')?'/':tolower(input));
	}

	static std::string MakeSafeName(StringSection<> input)
	{
		auto result = input.AsString();
		for (auto&b:result)
			if (b == ':' || b == '*') b = '-';
		return result;
	}

	uint64_t IntermediatesStore::Pimpl::MakeHashCode(
		StringSection<> archivableName,
		CompileProductsGroupId groupId) const
	{
		return Hash64(archivableName.begin(), archivableName.end(), groupId);
	}

	class RetainedFileRecord : public DependencyValidation
	{
	public:
		DependentFileState _state;

		void OnChange()
		{
				// on change, update the modification time record
			_state._timeMarker = MainFileSystem::TryGetDesc(_state._filename)._modificationTime;
			DependencyValidation::OnChange();
		}

		RetainedFileRecord(StringSection<ResChar> filename)
		: _state(filename, 0ull) {}
	};

	static std::vector<std::pair<uint64, std::shared_ptr<RetainedFileRecord>>> RetainedRecords;
	static Threading::Mutex RetainedRecordsLock;

	static std::shared_ptr<RetainedFileRecord> GetRetainedFileRecord(StringSection<ResChar> filename)
	{
		// Use HashFilename to avoid case sensitivity/slash direction problems
		// todo --	we could also consider a system where we could use MainFileSystem::TryTranslate
		//			to transform the filename into some more definitive representation
		auto hash = HashFilename(filename);
		{
			ScopedLock(RetainedRecordsLock);
			auto i = LowerBound(RetainedRecords, hash);
			if (i!=RetainedRecords.end() && i->first == hash)
				return i->second;
		}

		// We (probably) have to create a new marker... Do it outside of the mutex lock, because it
		// can be expensive.
		// We should call "AttachFileSystemMonitor" before we query for the
		// file's current modification time.
		auto newRecord = std::make_shared<RetainedFileRecord>(filename);
		RegisterFileDependency(newRecord, filename);
		newRecord->_state._timeMarker = MainFileSystem::TryGetDesc(filename)._modificationTime;

		{
			ScopedLock(RetainedRecordsLock);
			auto i = LowerBound(RetainedRecords, hash);
			// It's possible that another thread has just added the record... In that case, we abandon
			// the new marker we just made, and return the one already there.
			if (i!=RetainedRecords.end() && i->first == hash)
				return i->second;		

			RetainedRecords.insert(i, std::make_pair(hash, newRecord));
			return newRecord;
		}
	}

	bool IntermediatesStore::TryRegisterDependency(
		const std::shared_ptr<DependencyValidation>& target,
		const DependentFileState& fileState,
		const StringSection<> assetName)
	{
		auto record = GetRetainedFileRecord(fileState._filename);

		RegisterAssetDependency(target, record);

		if (record->_state._status == DependentFileState::Status::Shadowed) {
			Log(Verbose) << "Asset (" << assetName << ") is invalidated because dependency (" << fileState._filename << ") is marked shadowed" << std::endl;
			return false;
		}

		if (!record->_state._timeMarker) {
			Log(Verbose)
				<< "Asset (" << assetName
				<< ") is invalidated because of missing dependency (" << fileState._filename << ")" << std::endl;
			return false;
		} else if (record->_state._timeMarker != fileState._timeMarker) {
			Log(Verbose)
				<< "Asset (" << assetName
				<< ") is invalidated because of file data on dependency (" << fileState._filename << ")" << std::endl;
			return false;
		}

		return true;
	}

	DependentFileState IntermediatesStore::GetDependentFileState(StringSection<ResChar> filename)
	{
		return GetRetainedFileRecord(filename)->_state;
	}

	void IntermediatesStore::ShadowFile(StringSection<ResChar> filename)
	{
		auto record = GetRetainedFileRecord(filename);
		record->_state._status = DependentFileState::Status::Shadowed;

			// propagate change messages...
			// (duplicating processing from RegisterFileDependency)
		MainFileSystem::TryFakeFileChange(filename);
		record->OnChange();
	}

	std::shared_ptr<IArtifactCollection> IntermediatesStore::RetrieveCompileProducts(
		StringSection<> archivableName,
		CompileProductsGroupId groupId)
	{
		auto hashCode = _pimpl->MakeHashCode(archivableName, groupId);
		{
			ScopedLock(_pimpl->_storeRefCounts->_lock);
			auto existing = _pimpl->_storeRefCounts->_storeOperationsInFlight.find(hashCode);
			if (existing != _pimpl->_storeRefCounts->_storeOperationsInFlight.end())
				Throw(std::runtime_error("Attempting to retrieve compile products while store in flight: " + archivableName.AsString()));
			auto read = LowerBound(_pimpl->_storeRefCounts->_readReferenceCount, hashCode);
			if (read != _pimpl->_storeRefCounts->_readReferenceCount.end() && read->first == hashCode) {
				++read->second;
			} else
				_pimpl->_storeRefCounts->_readReferenceCount.insert(read, std::make_pair(hashCode, 1));
		}
		auto cleanup = AutoCleanup(
			[hashCode, this]() {
				ScopedLock(this->_pimpl->_storeRefCounts->_lock);
				auto read = LowerBound(this->_pimpl->_storeRefCounts->_readReferenceCount, hashCode);
				if (read != this->_pimpl->_storeRefCounts->_readReferenceCount.end() && read->first == hashCode) {
					assert(read->second > 0);
					--read->second;
				} else {
					Log(Error) << "Missing _readReferenceCount marker during cleanup op in RetrieveCompileProducts" << std::endl;
				}
			});
		(void)cleanup;

		auto groupi = _pimpl->_groups.find(groupId);
		if (groupi == _pimpl->_groups.end())
			Throw(std::runtime_error("GroupId has not be registered in intermediates store during retrieve operation"));

		return groupi->second._looseFilesStorage->RetrieveCompileProducts(archivableName, _pimpl->_storeRefCounts, hashCode);
	}

	void IntermediatesStore::StoreCompileProducts(
		StringSection<> archivableName,
		CompileProductsGroupId groupId,
		IteratorRange<const ICompileOperation::SerializedArtifact*> artifacts,
		::Assets::AssetState state,
		IteratorRange<const DependentFileState*> dependencies)
	{
		//
		//		Maintain _pimpl->_storeOperationsInFlight, which just records what
		//		store operations are currently in flight
		//

		auto hashCode = _pimpl->MakeHashCode(archivableName, groupId);
		{
			ScopedLock(_pimpl->_storeRefCounts->_lock);
			auto existing = _pimpl->_storeRefCounts->_storeOperationsInFlight.find(hashCode);
			if (existing != _pimpl->_storeRefCounts->_storeOperationsInFlight.end())
				Throw(std::runtime_error("Multiple stores in flight for the same compile product: " + archivableName.AsString()));
			auto read = LowerBound(_pimpl->_storeRefCounts->_readReferenceCount, hashCode);
			if (read != _pimpl->_storeRefCounts->_readReferenceCount.end() && read->first == hashCode && read->second != 0)
				Throw(std::runtime_error("Attempting to store compile product while still reading from it: " + archivableName.AsString()));
			_pimpl->_storeRefCounts->_storeOperationsInFlight.insert(hashCode);
		}

		auto cleanup = AutoCleanup(
			[hashCode, this]() {
				ScopedLock(this->_pimpl->_storeRefCounts->_lock);
				auto existing = this->_pimpl->_storeRefCounts->_storeOperationsInFlight.find(hashCode);
				if (existing != this->_pimpl->_storeRefCounts->_storeOperationsInFlight.end()) {
					this->_pimpl->_storeRefCounts->_storeOperationsInFlight.erase(existing);
				} else {
					Log(Error) << "Missing _storeOperationsInFlight marker during cleanup op in StoreCompileProducts" << std::endl;
				}
			});
		(void)cleanup;

		//
		// 		Now write out the compile products
		//

		auto groupi = _pimpl->_groups.find(groupId);
		if (groupi == _pimpl->_groups.end())
			Throw(std::runtime_error("GroupId has not be registered in intermediates store during retrieve operation"));

		groupi->second._looseFilesStorage->StoreCompileProducts(archivableName, artifacts, state, dependencies);
	}

	void IntermediatesStore::Pimpl::ResolveBaseDirectory() const
	{
		if (!_resolvedBaseDirectory.empty()) return;

			//  First, we need to find an output directory to use.
			//  We want a directory that isn't currently being used, and
			//  that matches the version string.

		auto cfgDir = _constructorOptions._baseDir + "/.int-" + _constructorOptions._configString;
		std::string goodBranchDir;

			//  Look for existing directories that could match the version
			//  string we have. 
		std::set<unsigned> indicesUsed;
		auto searchPath = MainFileSystem::BeginWalk(cfgDir);
		for (auto candidateDirectory=searchPath.begin_directories(); candidateDirectory!=searchPath.end_directories(); ++candidateDirectory) {
			auto candidateName = candidateDirectory.Name();
			
			unsigned asInt = 0;
			if (FastParseValue(MakeStringSection(candidateName), asInt) == AsPointer(candidateName.end()))
				indicesUsed.insert(asInt);

			auto markerFileName = cfgDir + "/" + candidateName + "/.store";
			std::unique_ptr<IFileInterface> markerFile;
			auto ioReason = MainFileSystem::TryOpen(markerFile, markerFileName, "rb", 0);
			if (ioReason != IFileSystem::IOReason::Success)
				continue;

			auto fileSize = markerFile->GetSize();
			if (fileSize != 0) {
				auto rawData = std::unique_ptr<char[]>(new char[int(fileSize)]);
				markerFile->Read(rawData.get(), 1, size_t(fileSize));

				InputStreamFormatter<> formatter(MakeStringSection(rawData.get(), PtrAdd(rawData.get(), fileSize)));
				StreamDOM<InputStreamFormatter<>> doc(formatter);

				auto compareVersion = doc.RootElement().Attribute("VersionString").Value();
				if (XlEqString(compareVersion, _constructorOptions._versionString)) {
					// this branch is already present, and is good... so use it
					goodBranchDir = cfgDir + "/" + candidateName;
					_markerFile = std::move(markerFile);
					break;
				}
			}
		}

		if (goodBranchDir.empty()) {
				// if we didn't find an existing folder we can use, we need to create a new one
				// search through to find the first unused directory
			for (unsigned d=0;;++d) {
				if (indicesUsed.find(d) != indicesUsed.end())
					continue;

				goodBranchDir = cfgDir + "/" + std::to_string(d);
				std::filesystem::create_directories(goodBranchDir);

				auto markerFileName = goodBranchDir + "/.store";

					// Opening without sharing to prevent other instances of XLE apps from using
					// the same directory.
				_markerFile = MainFileSystem::OpenFileInterface(markerFileName, "wb", 0);
				auto outStr = std::string("VersionString=") + _constructorOptions._versionString + "\n";
				_markerFile->Write(outStr.data(), 1, outStr.size());
				break;
			}
		}

		_resolvedBaseDirectory = goodBranchDir;
	}

	auto IntermediatesStore::RegisterCompileProductsGroup(StringSection<> name, const ConsoleRig::LibVersionDesc& compilerVersionInfo) -> CompileProductsGroupId
	{
		auto id = Hash64(name.begin(), name.end());
		auto existing = _pimpl->_groups.find(id);
		if (existing == _pimpl->_groups.end()) {
			auto safeGroupName = MakeSafeName(name);
			_pimpl->ResolveBaseDirectory();
			std::string looseFilesBase = Concatenate(_pimpl->_resolvedBaseDirectory, "/", safeGroupName, "/");
			Pimpl::Group newGroup;
			newGroup._looseFilesStorage = std::make_shared<LooseFilesStorage>(looseFilesBase, compilerVersionInfo);
			_pimpl->_groups.insert({id, std::move(newGroup)});
		}
		return id;
	}

	IntermediatesStore::IntermediatesStore(const ResChar baseDirectory[], const ResChar versionString[], const ResChar configString[], bool universal)
	{
		_pimpl = std::make_unique<Pimpl>();
		if (universal) {
			// This is the "universal" store directory. A single directory is used by all
			// versions of the game.
			ResChar buffer[MaxPath];
			snprintf(buffer, dimof(buffer), "%s/.int/u", baseDirectory);
			_pimpl->_resolvedBaseDirectory = buffer;
		} else {
			_pimpl->_constructorOptions._baseDir = baseDirectory;
			_pimpl->_constructorOptions._versionString = versionString;
			_pimpl->_constructorOptions._configString = configString;
		}
		_pimpl->_storeRefCounts = std::make_shared<StoreReferenceCounts>();
	}

	IntermediatesStore::~IntermediatesStore() 
	{
	}

			////////////////////////////////////////////////////////////

	IArtifactCollection::~IArtifactCollection() {}

	void ArtifactCollectionFuture::SetArtifactCollection(
		const std::shared_ptr<IArtifactCollection>& artifacts)
	{
		assert(!_artifactCollection);
		_artifactCollection = artifacts;
		SetState(_artifactCollection->GetAssetState());
	}

	const std::shared_ptr<IArtifactCollection>& ArtifactCollectionFuture::GetArtifactCollection()
	{
		return _artifactCollection;
	}

	Blob ArtifactCollectionFuture::GetErrorMessage()
	{
		if (!_artifactCollection)
			return nullptr;

		// Try to find an artifact named with the type "ChunkType_Log"
		ArtifactRequest requests[] = {
			ArtifactRequest { nullptr, ChunkType_Log, 0, ArtifactRequest::DataType::SharedBlob }
		};
		auto resRequests = _artifactCollection->ResolveRequests(MakeIteratorRange(requests));
		if (resRequests.empty())
			return nullptr;
		return resRequests[0]._sharedBlob;
	}

	ArtifactCollectionFuture::ArtifactCollectionFuture() {}
	ArtifactCollectionFuture::~ArtifactCollectionFuture()  {}


	::Assets::DepValPtr ChunkFileArtifactCollection::GetDependencyValidation() const { return _depVal; }
	StringSection<ResChar>	ChunkFileArtifactCollection::GetRequestParameters() const { return MakeStringSection(_requestParameters); }
	std::vector<ArtifactRequestResult> ChunkFileArtifactCollection::ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const
	{
		ChunkFileContainer chunkFile;
		return chunkFile.ResolveRequests(*_file, requests);
	}
	AssetState ChunkFileArtifactCollection::GetAssetState() const { return AssetState::Ready; }
	ChunkFileArtifactCollection::ChunkFileArtifactCollection(
		const std::shared_ptr<IFileInterface>& file, const ::Assets::DepValPtr& depVal, const std::string& requestParameters)
	: _file(file), _depVal(depVal), _requestParameters(requestParameters) {}
	ChunkFileArtifactCollection::~ChunkFileArtifactCollection() {}

	ArtifactRequestResult MakeArtifactRequestResult(ArtifactRequest::DataType dataType, const ::Assets::Blob& blob)
	{
		ArtifactRequestResult chunkResult;
		if (	dataType == ArtifactRequest::DataType::BlockSerializer
			||	dataType == ArtifactRequest::DataType::Raw) {
			uint8_t* mem = (uint8*)XlMemAlign(blob->size(), sizeof(uint64_t));
			chunkResult._buffer = std::unique_ptr<uint8_t[], PODAlignedDeletor>(mem);
			chunkResult._bufferSize = blob->size();
			std::memcpy(mem, blob->data(), blob->size());

			// initialize with the block serializer (if requested)
			if (dataType == ArtifactRequest::DataType::BlockSerializer)
				Block_Initialize(chunkResult._buffer.get());
		} else if (dataType == ArtifactRequest::DataType::ReopenFunction) {
			auto blobCopy = blob;
			chunkResult._reopenFunction = [blobCopy]() -> std::shared_ptr<IFileInterface> {
				return CreateMemoryFile(blobCopy);
			};
		} else if (dataType == ArtifactRequest::DataType::SharedBlob) {
			chunkResult._sharedBlob = blob;
		} else {
			assert(0);
		}
		return chunkResult;
	}

	std::vector<ArtifactRequestResult> BlobArtifactCollection::ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const
	{
		// We need to look through the list of chunks and try to match the given requests
		// This is very similar to ChunkFileContainer::ResolveRequests

		std::vector<ArtifactRequestResult> result;
		result.reserve(requests.size());

			// First scan through and check to see if we
			// have all of the chunks we need
		for (auto r=requests.begin(); r!=requests.end(); ++r) {
			auto prevWithSameCode = std::find_if(requests.begin(), r, [r](const auto& t) { return t._type == r->_type; });
			if (prevWithSameCode != r)
				Throw(std::runtime_error("Type code is repeated multiple times in call to ResolveRequests"));

			auto i = std::find_if(
				_chunks.begin(), _chunks.end(), 
				[&r](const auto& c) { return c._type == r->_type; });
			if (i == _chunks.end())
				Throw(Exceptions::ConstructionError(
					Exceptions::ConstructionError::Reason::MissingFile,
					_depVal,
					StringMeld<128>() << "Missing chunk (" << r->_name << ") in collection " << _collectionName));

			if (r->_expectedVersion != ~0u && (i->_version != r->_expectedVersion))
				Throw(::Assets::Exceptions::ConstructionError(
					Exceptions::ConstructionError::Reason::UnsupportedVersion,
					_depVal,
					StringMeld<256>() 
						<< "Data chunk is incorrect version for chunk (" 
						<< r->_name << ") expected: " << r->_expectedVersion << ", got: " << i->_version
						<< " in collection " << _collectionName));
		}

		for (const auto& r:requests) {
			auto i = std::find_if(
				_chunks.begin(), _chunks.end(), 
				[&r](const auto& c) { return c._type == r._type; });
			assert(i != _chunks.end());
			result.emplace_back(MakeArtifactRequestResult(r._dataType, i->_data));
		}

		return result;
	}
	::Assets::DepValPtr BlobArtifactCollection::GetDependencyValidation() const { return _depVal; }
	StringSection<ResChar>	BlobArtifactCollection::GetRequestParameters() const { return MakeStringSection(_requestParams); }
	AssetState BlobArtifactCollection::GetAssetState() const 
	{
		if (_chunks.empty())
			return AssetState::Invalid;
		// If we have just a single chunk, of type "ChunkType_Log", we'll consider this collection invalid
		// most compilers will generate a "log" chunk on failure containing error information
		// This is just a simple way to propagate this "invalid" state -- it does mean that we can't have
		// any assets that are normally just a "log" chunk be considered valid
		if (_chunks.size() == 1 && _chunks[0]._type == ChunkType_Log)
			return AssetState::Invalid;
		return AssetState::Ready; 
	}
	BlobArtifactCollection::BlobArtifactCollection(
		IteratorRange<const ICompileOperation::SerializedArtifact*> chunks, 
		const ::Assets::DepValPtr& depVal, const std::string& collectionName, const rstring& requestParams)
	: _chunks(chunks.begin(), chunks.end()), _depVal(depVal), _collectionName(collectionName), _requestParams(requestParams) {}
	BlobArtifactCollection::~BlobArtifactCollection() {}

	std::vector<ArtifactRequestResult> CompilerExceptionArtifact::ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const
	{
		if (requests.size() == 1 && requests[0]._type == ChunkType_Log && requests[0]._dataType == ArtifactRequest::DataType::SharedBlob) {
			ArtifactRequestResult res;
			res._sharedBlob = _log;
			std::vector<ArtifactRequestResult> result;
			result.push_back(std::move(res));
			return result;
		}
		Throw(std::runtime_error("Compile operation failed with error: " + AsString(_log)));
	}
	::Assets::DepValPtr CompilerExceptionArtifact::GetDependencyValidation() const { return _depVal; }
	StringSection<::Assets::ResChar>	CompilerExceptionArtifact::GetRequestParameters() const { return {}; }
	AssetState CompilerExceptionArtifact::GetAssetState() const { return AssetState::Invalid; }
	CompilerExceptionArtifact::CompilerExceptionArtifact(const ::Assets::Blob& log, const ::Assets::DepValPtr& depVal) : _log(log), _depVal(depVal) {}
	CompilerExceptionArtifact::~CompilerExceptionArtifact() {}

}
