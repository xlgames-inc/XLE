// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LooseFilesCache.h"
#include "IntermediatesStore.h"
#include "AssetUtils.h"
#include "IFileSystem.h"
#include "ChunkFile.h"
#include "ChunkFileContainer.h"
#include "IArtifact.h"
#include "DepVal.h"
#include "../OSServices/Log.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/OutputStreamFormatter.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/SerializationUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include <filesystem>

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

	static std::shared_ptr<IArtifactCollection> MakeArtifactCollection(
		const CompileProductsFile& productsFile, 
		const ::Assets::DepValPtr& depVal,
		const std::shared_ptr<StoreReferenceCounts>& refCounts,
		uint64_t refCountHashCode);

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
				if (product._status == DependentFileState::Status::DoesNotExist) {
					formatter.WriteKeyedValue(
						MakeStringSection(product._filename), 
						"doesnotexist");
				} else if (product._status == DependentFileState::Status::Shadowed) {
					formatter.WriteKeyedValue(
						MakeStringSection(product._filename), 
						"shadowed");
				} else {
					formatter.WriteKeyedValue(
						MakeStringSection(product._filename), 
						MakeStringSection(std::to_string(product._timeMarker)));
				}
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
			if (XlEqString(value, "doesnotexist")) {
				result._dependencies.push_back(DependentFileState {
					name.AsString(),
					0, DependentFileState::Status::DoesNotExist
				});
			} else if (XlEqString(value, "shadowed")) {
			} else {
				result._dependencies.push_back(DependentFileState {
					name.AsString(),
					Conversion::Convert<uint64_t>(value)
				});
			}
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

	std::shared_ptr<IArtifactCollection> LooseFilesStorage::RetrieveCompileProducts(
		StringSection<> archivableName,
		const std::shared_ptr<StoreReferenceCounts>& storeRefCounts,
		uint64_t hashCode)
	{
		auto intermediateName = MakeProductsFileName(archivableName);
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
				if (!IntermediatesStore::TryRegisterDependency(depVal, adjustedDep, archivableName))
					return nullptr;
			} else {
				if (!IntermediatesStore::TryRegisterDependency(depVal, dep, archivableName))
					return nullptr;
			}
		}

		return MakeArtifactCollection(finalProductsFile, depVal, storeRefCounts, hashCode);
	}

	static std::string MakeSafeName(StringSection<> input)
	{
		auto result = input.AsString();
		for (auto&b:result)
			if (b == ':' || b == '*' || b == '/' || b == '\\') b = '-';
		return result;
	}

	void LooseFilesStorage::StoreCompileProducts(
		StringSection<> archivableName,
		IteratorRange<const ICompileOperation::SerializedArtifact*> artifacts,
		::Assets::AssetState state,
		IteratorRange<const DependentFileState*> dependencies)
	{
		CompileProductsFile compileProductsFile;
		compileProductsFile._state = state;

		compileProductsFile._dependencies.reserve(dependencies.size());
		for (const auto& s:dependencies) {
			auto adjustedDep = s;
			adjustedDep._filename = MakeSplitPath(s._filename).Simplify().Rebuild();
			assert(!adjustedDep._filename.empty());
			compileProductsFile._dependencies.push_back(adjustedDep);
		}

		auto productsName = MakeProductsFileName(archivableName);
		OSServices::CreateDirectoryRecursive(MakeFileNameSplitter(productsName).DriveAndPath());
		std::vector<std::pair<std::string, std::string>> renameOps;

		// Will we create one chunk file that will contain most of the artifacts
		// However, some special artifacts (eg, metric files), can become separate files
		std::vector<ICompileOperation::SerializedArtifact> chunksInMainFile;
		for (const auto&a:artifacts)
			if (a._type == ChunkType_Metrics) {
				std::string metricsName;
				if (!a._name.empty()) {
					metricsName = productsName + "-" + MakeSafeName(a._name) + ".metrics";
				} else 
					metricsName = productsName + ".metrics";
				auto outputFile = MainFileSystem::OpenFileInterface(metricsName + ".staging", "wb", 0);
				outputFile->Write((const void*)AsPointer(a._data->cbegin()), 1, a._data->size());
				compileProductsFile._compileProducts.push_back({a._type, metricsName});
				renameOps.push_back({metricsName + ".staging", metricsName});
			} else if (a._type == ChunkType_Log) {
				std::string metricsName;
				if (!a._name.empty()) {
					metricsName = productsName + "-" + MakeSafeName(a._name) + ".log";
				} else 
					metricsName = productsName + ".log";
				auto outputFile = MainFileSystem::OpenFileInterface(metricsName + ".staging", "wb", 0);
				outputFile->Write((const void*)AsPointer(a._data->cbegin()), 1, a._data->size());
				compileProductsFile._compileProducts.push_back({a._type, metricsName});
				renameOps.push_back({metricsName + ".staging", metricsName});
			} else {
				chunksInMainFile.push_back(a);
			}

		if (!chunksInMainFile.empty()) {
			auto mainBlobName = productsName + ".chunk";
			auto outputFile = MainFileSystem::OpenFileInterface(mainBlobName + ".staging", "wb", 0);
			ChunkFile::BuildChunkFile(*outputFile, MakeIteratorRange(chunksInMainFile), _compilerVersionInfo);
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
			std::shared_ptr<IFileInterface> productsFile = MainFileSystem::OpenFileInterface(productsName + ".staging", "wb", 0); // note -- no sharing allowed on this file. We take an exclusive lock on it
			FileOutputStream stream(productsFile);
			OutputStreamFormatter fmtter(stream);
			fmtter << compileProductsFile;
			renameOps.push_back({productsName + ".staging", productsName});
		}

#if defined(_DEBUG)
		// Check for duplicated names in renameOps. Any dupes will result in exceptions later
		for (auto i=renameOps.begin(); i!=renameOps.end(); ++i)
			for (auto i2=renameOps.begin(); i2!=i; ++i2) {
				if (i->first == i2->first)
					Throw(std::runtime_error("Duplicated rename op in LooseFilesStorage for intermediate: " + i->first));
				if (i->second == i2->second)
					Throw(std::runtime_error("Duplicated rename op in LooseFilesStorage for intermediate: " + i->second));
			}
#endif

		// If we get to here successfully, go ahead and rename all of the staging files to their final names 
		// This gives us a little bit of protection against exceptions while writing out the staging files
		for (const auto& renameOp:renameOps) {
			std::filesystem::remove(renameOp.second);
			std::filesystem::rename(renameOp.first, renameOp.second);
		}
	}

	std::string LooseFilesStorage::MakeProductsFileName(StringSection<> archivableName)
	{
		std::string result = _baseDirectory;
		result.reserve(result.size() + archivableName.size());
		for (auto b:archivableName)
			result.push_back((b != ':' && b != '*')?b:'-');
		return result;
	}

	LooseFilesStorage::LooseFilesStorage(StringSection<> baseDirectory, const ConsoleRig::LibVersionDesc& compilerVersionInfo)
	: _baseDirectory(baseDirectory.AsString())
	, _compilerVersionInfo(compilerVersionInfo)
	{}
	LooseFilesStorage::~LooseFilesStorage() {}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
					ChunkFileContainer temp(prod._intermediateArtifact);
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

}

