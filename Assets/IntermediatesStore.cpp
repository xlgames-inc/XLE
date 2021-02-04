// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "IntermediatesStore.h"
#include "IArtifact.h"
#include "IFileSystem.h"
#include "DepVal.h"
#include "AssetUtils.h"
#include "ChunkFileContainer.h"
#include "BlockSerializer.h"
#include "MemoryFile.h"
#include "ChunkFile.h"
#include "../OSServices/Log.h"
#include "../OSServices/RawFS.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/OutputStreamFormatter.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/SerializationUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StreamUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/FastParseValue.h"
#include <filesystem>
#include <set>
#include <sstream>

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
	#include "../OSServices/WinAPI/IncludeWindows.h"
#endif
#include <memory>

namespace Assets
{
	static const auto ChunkType_Metrics = ConstHash64<'Metr', 'ics'>::Value;
	static const auto ChunkType_Log = ConstHash64<'Log'>::Value;
	static const auto ChunkType_Multi = ConstHash64<'Mult', 'iChu', 'nk'>::Value;

	class CompileProductsFile
	{
	public:
		struct Product
		{
			uint64_t _type;
			std::string _intermediateArtifact;
		};
		std::vector<Product> _compileProducts;
		std::vector<DependentFileState> _dependencies;

		::Assets::AssetState _state = AssetState::Ready;
		std::string _basePath;

		const Product* FindProduct(uint64_t type) const
		{
			for (const auto&p:_compileProducts)
				if (p._type == type)
					return &p;
			return nullptr;
		}
	};

	static void SerializationOperator(OutputStreamFormatter& formatter, const CompileProductsFile& compileProducts)
	{
		formatter.WriteKeyedValue("BasePath", compileProducts._basePath);
		formatter.WriteKeyedValue("Invalid", compileProducts._state == AssetState::Ready ? "0" : "1");

		for (const auto&product:compileProducts._compileProducts) {
			auto ele = formatter.BeginKeyedElement(std::to_string(product._type));
			formatter.WriteKeyedValue("Artifact", product._intermediateArtifact.c_str());
			formatter.EndElement(ele);
		}

		{
			auto ele = formatter.BeginKeyedElement("Dependencies");
			for (const auto&product:compileProducts._dependencies) {
				formatter.WriteKeyedValue(
					MakeStringSection(product._filename), 
					MakeStringSection(std::to_string(product._timeMarker)));
			}
			formatter.EndElement(ele);
		}
	}

	static void DeserializationOperator(InputStreamFormatter<utf8>& formatter, CompileProductsFile::Product& result)
	{
		while (formatter.PeekNext() == FormatterBlob::KeyedItem) {
			StringSection<utf8> name, value;
			if (!formatter.TryKeyedItem(name) || !formatter.TryValue(value))
				Throw(Utility::FormatException("Poorly formed attribute in CompileProductsFile", formatter.GetLocation()));
			if (XlEqString(name, "Artifact")) {
				result._intermediateArtifact = value.AsString();
			} else
				Throw(Utility::FormatException("Unknown attribute in CompileProductsFile", formatter.GetLocation()));
		}
	}

	static void DerializeDependencies(InputStreamFormatter<utf8>& formatter, CompileProductsFile& result)
	{
		while (formatter.PeekNext() == FormatterBlob::KeyedItem) {
			StringSection<utf8> name, value;
			if (!formatter.TryKeyedItem(name) || !formatter.TryValue(value))
				Throw(Utility::FormatException("Poorly formed attribute in CompileProductsFile", formatter.GetLocation()));
			result._dependencies.push_back(DependentFileState {
				name.AsString(),
				Conversion::Convert<uint64_t>(value)
			});
		}
	}

	static StringSection<utf8> DeserializeValue(InputStreamFormatter<utf8>& formatter)
	{
		StringSection<utf8> value;
		if (!formatter.TryValue(value))
			Throw(Utility::FormatException("Expecting value", formatter.GetLocation()));
		return value;
	}

	static void DeserializationOperator(InputStreamFormatter<utf8>& formatter, CompileProductsFile& result)
	{
		while (formatter.PeekNext() == FormatterBlob::KeyedItem) {
			InputStreamFormatter<utf8>::InteriorSection name;
			if (!formatter.TryKeyedItem(name))
				Throw(Utility::FormatException("Poorly formed item in CompileProductsFile", formatter.GetLocation()));

			if (XlEqString(name, "Dependencies")) {
				RequireBeginElement(formatter);
				DerializeDependencies(formatter, result);
				RequireEndElement(formatter);
			} else if (XlEqString(name, "BasePath")) {
				result._basePath = DeserializeValue(formatter).AsString();
			} else if (XlEqString(name, "Invalid")) {
				if (XlEqString(DeserializeValue(formatter), "1")) {
					result._state = AssetState::Invalid;
				} else
					result._state = AssetState::Ready;
			} else if (formatter.PeekNext() == FormatterBlob::BeginElement) {
				RequireBeginElement(formatter);
				CompileProductsFile::Product product;
				formatter >> product;
				product._type = Conversion::Convert<uint64_t>(name);
				result._compileProducts.push_back(product);
				RequireEndElement(formatter);
			} else
				Throw(Utility::FormatException("Unknown attribute in CompileProductsFile", formatter.GetLocation()));
		}
	}

	class StoreReferenceCounts
	{
	public:
		Threading::Mutex _lock;
		std::set<uint64_t> _storeOperationsInFlight;
		std::vector<std::pair<uint64_t, unsigned>> _readReferenceCount;
	};

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

		std::unordered_map<uint64_t, std::string> _groupIdToDirName;

		std::shared_ptr<StoreReferenceCounts> _storeRefCounts;

		void ResolveBaseDirectory() const;
		std::string MakeStoreName(
			StringSection<> archivableName,
			CompileProductsGroupId groupId) const;
		uint64_t MakeHashCode(
			StringSection<> archivableName,
			CompileProductsGroupId groupId) const;
	};

	static ResChar ConvChar(ResChar input) 
	{
		return (ResChar)((input == '\\')?'/':tolower(input));
	}

	std::string IntermediatesStore::Pimpl::MakeStoreName(
		StringSection<> archivableName,
		CompileProductsGroupId groupId) const
	{
		ResolveBaseDirectory();

		std::stringstream str;
		str << _resolvedBaseDirectory << "/";
		assert(_groupIdToDirName.find(groupId) != _groupIdToDirName.end());
		str << _groupIdToDirName.find(groupId)->second << "/";
		str << archivableName;

		auto result = str.str();
		for (auto&b:result)
			if (b == ':') b = '-';
		return result;
	}

	uint64_t IntermediatesStore::Pimpl::MakeHashCode(
		StringSection<> archivableName,
		CompileProductsGroupId groupId) const
	{
		return Hash64(archivableName.begin(), archivableName.end(), groupId);
	}

	template <int DestCount>
		static void MakeDepFileName(ResChar (&destination)[DestCount], StringSection<ResChar> baseDirectory, StringSection<ResChar> depFileName)
		{
				//  if the prefix of "baseDirectory" and "intermediateFileName" match, we should skip over that
			const ResChar* f = depFileName.begin(), *b = baseDirectory.begin();

			while (f != depFileName.end() && b != baseDirectory.end() && ConvChar(*f) == ConvChar(*b)) { ++f; ++b; }
			while (f != depFileName.end() && ConvChar(*f) == '/') { ++f; }

			static_assert(DestCount > 0, "Attempting to use MakeDepFileName with zero length array");
			auto* dend = &destination[DestCount-1];
			auto* d = destination;
			auto* s = baseDirectory.begin();
			while (d != dend && s != baseDirectory.end()) *d++ = *s++;
			s = "/.deps/";
			while (d != dend && *s != '\0') *d++ = *s++;
			s = f;
			while (d != dend && s != depFileName.end()) *d++ = *s++;
			*d = '\0';
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

	static std::string DescriptiveName(const StringSection<ResChar> initializers[], unsigned initializerCount)
	{
		if (!initializerCount) return "<<unnamed>>";
		std::stringstream str;
		for (unsigned i=0; i<initializerCount; ++i) {
			if (i != 0) str << "-";
			str << initializers[i];
		}
		return str.str();
	}

	class CompileProductsArtifactCollection : public IArtifactCollection
	{
	public:
		std::vector<ArtifactRequestResult> ResolveRequests(IteratorRange<const ArtifactRequest*> requests) const override
		{
			// look for the main chunk file in the compile products -- we'll use this for resolving requests
			for (const auto&prod:_productsFile._compileProducts)
				if (prod._type == ChunkType_Multi) {
					// open with no sharing
					auto mainChunkFile = MainFileSystem::OpenFileInterface(prod._intermediateArtifact, "rb", 0);
					ChunkFileContainer temp;
					return temp.ResolveRequests(*mainChunkFile, requests);
				}
			return {};
		}

		DepValPtr GetDependencyValidation() const override { return _depVal; }
		StringSection<ResChar> GetRequestParameters() const override { return {}; }
		AssetState GetAssetState() const override { return _productsFile._state; }
		CompileProductsArtifactCollection(
			const CompileProductsFile& productsFile, 
			const ::Assets::DepValPtr& depVal,
			const std::shared_ptr<StoreReferenceCounts>& refCounts,
			uint64_t refCountHashCode)
		: _productsFile(productsFile), _depVal(depVal)
		, _refCounts(refCounts), _refCountHashCode(refCountHashCode)
		{
			ScopedLock(_refCounts->_lock);
			auto read = LowerBound(_refCounts->_readReferenceCount, _refCountHashCode);
			if (read != _refCounts->_readReferenceCount.end() && read->first == _refCountHashCode) {
				++read->second;
			} else
				_refCounts->_readReferenceCount.insert(read, std::make_pair(_refCountHashCode, 1));
		}

		~CompileProductsArtifactCollection() 
		{
			ScopedLock(_refCounts->_lock);
			auto read = LowerBound(_refCounts->_readReferenceCount, _refCountHashCode);
			if (read != _refCounts->_readReferenceCount.end() && read->first == _refCountHashCode) {
				assert(read->second > 0);
				--read->second;
			} else {
				Log(Error) << "Missing _readReferenceCount marker during cleanup op in RetrieveCompileProducts" << std::endl;
			}
		}

		CompileProductsArtifactCollection(const CompileProductsArtifactCollection&) = delete;
		CompileProductsArtifactCollection& operator=(const CompileProductsArtifactCollection&) = delete;
	private:
		CompileProductsFile _productsFile;
		DepValPtr _depVal;
		std::shared_ptr<StoreReferenceCounts> _refCounts;
		uint64_t _refCountHashCode;
	};

	static std::shared_ptr<IArtifactCollection> MakeArtifactCollection(
		const CompileProductsFile& productsFile, 
		const ::Assets::DepValPtr& depVal,
		const std::shared_ptr<StoreReferenceCounts>& refCounts,
		uint64_t refCountHashCode)
	{
		return std::make_shared<CompileProductsArtifactCollection>(productsFile, depVal, refCounts, refCountHashCode);
	}

	bool TryRegisterDependency(
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

			//  When we process a file, we write a little text file to the
			//  ".deps" directory. This contains a list of dependency files, and
			//  the state of those files when this file was compiled.
			//  If the current files don't match the state that's recorded in
			//  the .deps file, then we can assume that it is out of date and
			//  must be recompiled.

		auto intermediateName = _pimpl->MakeStoreName(archivableName, groupId);
		std::unique_ptr<IFileInterface> productsFile;
		auto ioResult = MainFileSystem::TryOpen(productsFile, intermediateName.c_str(), "rb", 0);
		if (ioResult != ::Assets::IFileSystem::IOReason::Success || !productsFile)
			return nullptr;
	
		size_t size = productsFile->GetSize();
		auto productsFileData = std::make_unique<char[]>(size);
		productsFile->Read(productsFileData.get(), 1, size);

		InputStreamFormatter<> formatter(
			MakeStringSection(productsFileData.get(), PtrAdd(productsFileData.get(), size)));

		CompileProductsFile finalProductsFile;
		formatter >> finalProductsFile;

		auto depVal = std::make_shared<DependencyValidation>();

		for (const auto&dep:finalProductsFile._dependencies) {
			if (!finalProductsFile._basePath.empty()) {
				auto adjustedDep = dep;
				char buffer[MaxPath];
				Legacy::XlConcatPath(buffer, dimof(buffer), finalProductsFile._basePath.c_str(), AsPointer(dep._filename.begin()), AsPointer(dep._filename.end()));
				adjustedDep._filename = buffer;
				if (!TryRegisterDependency(depVal, adjustedDep, archivableName))
					return nullptr;
			} else {
				if (!TryRegisterDependency(depVal, dep, archivableName))
					return nullptr;
			}
		}

		return MakeArtifactCollection(finalProductsFile, depVal, _pimpl->_storeRefCounts, hashCode);
	}

	void IntermediatesStore::StoreCompileProducts(
		StringSection<> archivableName,
		CompileProductsGroupId groupId,
		IteratorRange<const ICompileOperation::SerializedArtifact*> artifacts,
		::Assets::AssetState state,
		IteratorRange<const DependentFileState*> dependencies,
		const ConsoleRig::LibVersionDesc& compilerVersionInfo)
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

		bool successfulStore = false;
		auto cleanup = AutoCleanup(
			[&successfulStore, hashCode, this]() {
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

		CompileProductsFile compileProductsFile;
		compileProductsFile._state = state;

		for (const auto& s:dependencies) {
			auto filename = MakeSplitPath(s._filename).Simplify().Rebuild();
			compileProductsFile._dependencies.push_back({filename, s._timeMarker});
		}

		auto intermediateName = _pimpl->MakeStoreName(archivableName, groupId);
		OSServices::CreateDirectoryRecursive(MakeFileNameSplitter(intermediateName).DriveAndPath());
		std::vector<std::pair<std::string, std::string>> renameOps;

		// Will we create one chunk file that will contain most of the artifacts
		// However, some special artifacts (eg, metric files), can become separate files
		std::vector<ICompileOperation::SerializedArtifact> chunksInMainFile;
		for (const auto&a:artifacts)
			if (a._type == ChunkType_Metrics) {
				std::string metricsName;
				if (!a._name.empty()) {
					metricsName = intermediateName + "-" + a._name + ".metrics";
				} else 
					metricsName = intermediateName + ".metrics";
				auto outputFile = MainFileSystem::OpenFileInterface(metricsName + ".staging", "wb", 0);
				outputFile->Write((const void*)AsPointer(a._data->cbegin()), 1, a._data->size());
				compileProductsFile._compileProducts.push_back({a._type, metricsName});
				renameOps.push_back({metricsName + ".staging", metricsName});
			} else if (a._type == ChunkType_Log) {
				std::string metricsName;
				if (!a._name.empty()) {
					metricsName = intermediateName + "-" + a._name + ".log";
				} else 
					metricsName = intermediateName + ".log";
				auto outputFile = MainFileSystem::OpenFileInterface(metricsName + ".log", "wb", 0);
				outputFile->Write((const void*)AsPointer(a._data->cbegin()), 1, a._data->size());
				compileProductsFile._compileProducts.push_back({a._type, metricsName});
				renameOps.push_back({metricsName + ".log", metricsName});
			} else {
				chunksInMainFile.push_back(a);
			}

		if (!chunksInMainFile.empty()) {
			auto mainBlobName = intermediateName + ".chunk";
			auto outputFile = MainFileSystem::OpenFileInterface(mainBlobName + ".staging", "wb", 0);
			ChunkFile::BuildChunkFile(*outputFile, MakeIteratorRange(chunksInMainFile), compilerVersionInfo);
			compileProductsFile._compileProducts.push_back({ChunkType_Multi, mainBlobName});
			renameOps.push_back({mainBlobName + ".staging", mainBlobName});
		}

		// note -- we can set compileProductsFile._basePath here, and then make the dependencies
		// 			within the compiler products file into relative filenames
		/*
			auto basePathSplitPath = MakeSplitPath(compileProductsFile._basePath);
			if (!compileProductsFile._basePath.empty()) {
				filename = MakeRelativePath(basePathSplitPath, MakeSplitPath(filename));
			} else {
		*/

		{
			std::shared_ptr<IFileInterface> productsFile = MainFileSystem::OpenFileInterface(intermediateName + ".staging", "wb", 0); // note -- no sharing allowed on this file. We take an exclusive lock on it
			FileOutputStream stream(productsFile);
			OutputStreamFormatter fmtter(stream);
			fmtter << compileProductsFile;
			renameOps.push_back({intermediateName + ".staging", intermediateName});
		}

#if defined(_DEBUG)
		// Check for duplicated names in renameOps. Any dupes will result in exceptions later
		for (auto i=renameOps.begin(); i!=renameOps.end(); ++i)
			for (auto i2=renameOps.begin(); i2!=i; ++i2) {
				if (i->first == i2->first)
					Throw(std::runtime_error("Duplicated rename op in IntermediatesStore for intermediate: " + i->first));
				if (i->second == i2->second)
					Throw(std::runtime_error("Duplicated rename op in IntermediatesStore for intermediate: " + i->second));
			}
#endif

		// If we get to here successfully, go ahead and rename all of the staging files to their final names 
		// This gives us a little bit of protection against exceptions while writing out the staging files
		for (const auto& renameOp:renameOps) {
			std::filesystem::remove(renameOp.second);
			std::filesystem::rename(renameOp.first, renameOp.second);
		}

		successfulStore = false;		
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

	auto IntermediatesStore::RegisterCompileProductsGroup(StringSection<> name) -> CompileProductsGroupId
	{
		auto id = Hash64(name.begin(), name.end());
		auto existing = _pimpl->_groupIdToDirName.find(id);
		if (existing == _pimpl->_groupIdToDirName.end()) {
			_pimpl->_groupIdToDirName.insert({id, name.AsString()});
		} else {
			assert(existing->second == name.AsString());
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
	AssetState BlobArtifactCollection::GetAssetState() const { return AssetState::Ready; }
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
