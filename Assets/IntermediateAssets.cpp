// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "IntermediateAssets.h"
#include "IArtifact.h"
#include "IFileSystem.h"
#include "DepVal.h"
#include "AssetUtils.h"

#include "../OSServices/Log.h"
#include "../OSServices/RawFS.h"
#include "../OSServices/LegacyFileStreams.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StreamUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/Streams/PathUtils.h"

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    #include "../OSServices/WinAPI/IncludeWindows.h"
#endif
#include <memory>

namespace Assets
{

    static ResChar ConvChar(ResChar input) 
    {
        return (ResChar)((input == '\\')?'/':tolower(input));
    }

    void    IntermediatesStore::MakeIntermediateName(ResChar buffer[], unsigned bufferMaxCount, StringSection<ResChar> firstInitializer) const
    {
		ResolveBaseDirectory();

            // calculate the intermediate file for this request name (normally just a file with the 
            //  same name in our branch directory
        if (buffer == firstInitializer.begin()) {
            assert(bufferMaxCount >= (_resolvedBaseDirectory.size()+1));
            auto length = firstInitializer.Length();
            auto moveSize = std::min(unsigned(length), unsigned(bufferMaxCount-1-(_resolvedBaseDirectory.size()+1)));
            XlMoveMemory(&buffer[_resolvedBaseDirectory.size()+1], buffer, moveSize);
            buffer[_resolvedBaseDirectory.size()+1+moveSize] = ResChar('\0');
            std::copy(_resolvedBaseDirectory.begin(), _resolvedBaseDirectory.end(), buffer);
            buffer[_resolvedBaseDirectory.size()] = ResChar('/');
        } else {
            XlCopyString(buffer, bufferMaxCount, _resolvedBaseDirectory.c_str());
            XlCatString(buffer, bufferMaxCount, "/");
            XlCatString(buffer, bufferMaxCount, firstInitializer);
        }
        
            // make filename that is safe for the filesystem.
            //      replace ':' (which is sometimes used to deliminate parameters)
            //      with '-'
        for (ResChar* b = buffer; *b; ++b)
            if (*b == ':') *b = '-';
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
        auto hash = HashFilename(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), s_defaultFilenameRules);
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

    void IntermediatesStore::ClearDependencyData() {
        ScopedLock(RetainedRecordsLock);
        RetainedRecords.clear();
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
        utf8 directoryName[MaxPath];
        FileNameSplitter<utf8> splitter(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()));
        SplitPath<utf8>(splitter.DriveAndPath()).Simplify().Rebuild(directoryName);
        
        assert(0);  // deprecated with changes to FileSystemMonitor
        // OSServices::FakeFileChange(MakeStringSection(directoryName), splitter.FileAndExtension());

        record->OnChange();
    }

    std::shared_ptr<DependencyValidation> IntermediatesStore::MakeDependencyValidation(StringSection<ResChar> intermediateFileName) const
    {
            //  When we process a file, we write a little text file to the
            //  ".deps" directory. This contains a list of dependency files, and
            //  the state of those files when this file was compiled.
            //  If the current files don't match the state that's recorded in
            //  the .deps file, then we can assume that it is out of date and
            //  must be recompiled.

		ResolveBaseDirectory();

        ResChar buffer[MaxPath];
        MakeDepFileName(buffer, MakeStringSection(_resolvedBaseDirectory), intermediateFileName);
        if (MainFileSystem::TryGetDesc(buffer)._state != FileDesc::State::Normal) return nullptr;

        Data data;
		OSServices::BasicFile file;
		if (MainFileSystem::TryOpen(file, buffer, "rb") != IFileSystem::IOReason::Success)
			return nullptr;
		data.LoadFromFile(file);

        auto* basePath = data.StrAttribute("BasePath");
        auto validation = std::make_shared<DependencyValidation>();
        auto* dependenciesBlock = data.ChildWithValue("Dependencies");
        if (dependenciesBlock) {
            for (auto* dependency = dependenciesBlock->child; dependency; dependency = dependency->next) {
                auto* depName = dependency->value;
                if (!depName || !depName[0]) continue;

                auto dateLow = (unsigned)dependency->IntAttribute("ModTimeL");
                auto dateHigh = (unsigned)dependency->IntAttribute("ModTimeH");
                    
                std::shared_ptr<RetainedFileRecord> record;
                if (basePath && basePath[0]) {
                    Legacy::XlConcatPath(buffer, dimof(buffer), basePath, depName, XlStringEnd(depName));
                    record = GetRetainedFileRecord(buffer);
                } else
                    record = GetRetainedFileRecord(depName);

                RegisterAssetDependency(validation, record);

                if (record->_state._status == DependentFileState::Status::Shadowed) {
                    Log(Verbose) << "Asset (" << intermediateFileName << ") is invalidated because dependency (" << depName << ") is marked shadowed" << std::endl;
                    return nullptr;
                }

                if (!record->_state._timeMarker) {
                    Log(Verbose)
                        << "Asset (" << intermediateFileName 
                        << ") is invalidated because of missing dependency (" << depName << ")" << std::endl;
                    return nullptr;
                } else if (record->_state._timeMarker != ((uint64(dateHigh) << 32ull) | uint64(dateLow))) {
					Log(Verbose)
                        << "Asset (" << intermediateFileName 
                        << ") is invalidated because of file data on dependency (" << depName << ")" << std::endl;
                    return nullptr;
                }
            }
        }

        return validation;
    }

    std::shared_ptr<DependencyValidation> IntermediatesStore::WriteDependencies(
        StringSection<ResChar> intermediateFileName, 
        StringSection<ResChar> baseDir,
        IteratorRange<const DependentFileState*> deps,
        bool makeDepValidation) const
    {
        Data data;

        std::shared_ptr<DependencyValidation> result;
        if (makeDepValidation)
            result = std::make_shared<DependencyValidation>();

            //  we have to write the base directory to the dependencies file as well
            //  to keep it short, most filenames should be expressed as relative files
        char buffer[MaxPath];
        data.SetAttribute("BasePath", baseDir.AsString().c_str());

        SplitPath<ResChar> baseSplitPath(baseDir);

        auto dependenciesBlock = std::make_unique<Data>("Dependencies");
        for (auto& s:deps) {
            auto c = std::make_unique<Data>();

            auto relPath = MakeRelativePath(
                baseSplitPath, 
                SplitPath<ResChar>(s._filename));
            c->SetValue(relPath.c_str());

            if (s._status != DependentFileState::Status::Shadowed) {
                c->SetAttribute("ModTimeH", (int)(s._timeMarker>>32ull));
                c->SetAttribute("ModTimeL", (int)(s._timeMarker));
            }
            dependenciesBlock->Add(c.release());
            if (makeDepValidation) RegisterFileDependency(result, s._filename.c_str());
        }
        data.Add(dependenciesBlock.release());

		ResolveBaseDirectory();
        MakeDepFileName(buffer, _resolvedBaseDirectory.c_str(), intermediateFileName);

            // first, create the directory if we need to
        char dirName[MaxPath];
        Legacy::XlDirname(dirName, dimof(dirName), buffer);
        OSServices::CreateDirectoryRecursive(dirName);

            // now, write -- 
		OSServices::BasicFile file;
		if (MainFileSystem::TryOpen(file, buffer, "wb") != IFileSystem::IOReason::Success)
			return nullptr;
		auto stream = OSServices::Legacy::OpenFileOutput(std::move(file));
        data.SaveToOutputStream(*stream);

        return result;
    }

	void IntermediatesStore::ResolveBaseDirectory() const
	{
		if (!_resolvedBaseDirectory.empty()) return;

            //  First, we need to find an output directory to use.
            //  We want a directory that isn't currently being used, and
            //  that matches the version string.

        ResChar buffer[MaxPath];
        (void)buffer;

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
		_snprintf_s(buffer, _TRUNCATE, "%s/%s_*", _constructorOptions._baseDir.c_str(), _constructorOptions._configString.c_str());

		std::string goodBranchDir;

			//  Look for existing directories that could match the version
			//  string we have. 
		WIN32_FIND_DATAA findData;
		XlZeroMemory(findData);
		HANDLE findHandle = FindFirstFileA(buffer, &findData);
		if (findHandle != INVALID_HANDLE_VALUE) {
			do {
				if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					_snprintf_s(buffer, _TRUNCATE, "%s/%s/.store", _constructorOptions._baseDir.c_str(), findData.cFileName);
						// Note --  Ideally we want to prevent two different instances of the
						//          same app from using the same intermediate assets store.
						//          We can do this by use a "non-shareable" file mode when
						//          we load these files. 
					OSServices::BasicFile markerFile;
					auto ioReason = MainFileSystem::TryOpen(markerFile, buffer, "rb", 0);
					if (ioReason != IFileSystem::IOReason::Success)
						continue;

					auto fileSize = markerFile.GetSize();
					if (fileSize != 0) {
						auto rawData = std::unique_ptr<uint8[]>(new uint8[int(fileSize)]);
						markerFile.Read(rawData.get(), 1, size_t(fileSize));

                        InputStreamFormatter<utf8> formatter(
                            MemoryMappedInputStream(rawData.get(), PtrAdd(rawData.get(), (ptrdiff_t)fileSize)));
                        StreamDOM<InputStreamFormatter<utf8>> doc(formatter);

						auto compareVersion = doc.Attribute("VersionString").Value();
						if (XlEqString(compareVersion, (const utf8*)_constructorOptions._versionString.c_str())) {
							// this branch is already present, and is good... so use it
							goodBranchDir = _constructorOptions._baseDir + "/" + findData.cFileName;
                            _markerFile = std::make_unique<OSServices::BasicFile>(std::move(markerFile));
							break;
						}

						// it's a store for some other version of the executable. Try the next one
						continue;
					}
				}
			} while (FindNextFileA(findHandle, &findData));

			FindClose(findHandle);
		}

		if (goodBranchDir.empty()) {
				// if we didn't find an existing folder we can use, we need to create a new one
				// search through to find the first unused directory
			for (unsigned d=0;;++d) {
				_snprintf_s(buffer, _TRUNCATE, "%s/%s_%i", _constructorOptions._baseDir.c_str(), _constructorOptions._configString.c_str(), d);
				DWORD dwAttrib = GetFileAttributesA(buffer);
				if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
					continue;
				}

				OSServices::CreateDirectoryRecursive(buffer);
				goodBranchDir = buffer;

				_snprintf_s(buffer, _TRUNCATE, "%s/%s_%i/.store", _constructorOptions._baseDir.c_str(), _constructorOptions._configString.c_str(), d);

                    // Opening without sharing to prevent other instances of XLE apps from using
                    // the same directory.
                _markerFile = std::make_unique<OSServices::BasicFile>(MainFileSystem::OpenBasicFile(buffer, "wb", 0));
					
				auto outStr = std::string("VersionString=") + _constructorOptions._versionString + "\n";
				_markerFile->Write(outStr.data(), 1, outStr.size());
                    
				break;
			}
		}
#else
        auto goodBranchDir = _constructorOptions._baseDir;
#endif

		_resolvedBaseDirectory = goodBranchDir;
    }

	IntermediatesStore::IntermediatesStore(const ResChar baseDirectory[], const ResChar versionString[], const ResChar configString[], bool universal)
    {
		if (universal) {
			// This is the "universal" store directory. A single directory is used by all
			// versions of the game.
			ResChar buffer[MaxPath];
			snprintf(buffer, dimof(buffer), "%s/u", baseDirectory);
			_resolvedBaseDirectory = buffer;
		} else {
			_constructorOptions._baseDir = baseDirectory;
			_constructorOptions._versionString = versionString;
			_constructorOptions._configString = configString;
		}
	}

    IntermediatesStore::~IntermediatesStore() 
    {
    }

            ////////////////////////////////////////////////////////////

	IArtifact::~IArtifact() {}
	IArtifactCompileMarker::~IArtifactCompileMarker() {}

	void ArtifactFuture::AddArtifact(const std::string& name, const std::shared_ptr<IArtifact>& artifact)
	{
		_artifacts.push_back(std::make_pair(name, artifact));
	}

	ArtifactFuture::ArtifactFuture() {}
    ArtifactFuture::~ArtifactFuture()  {}


	auto FileArtifact::GetBlob() const -> Blob
	{
		auto splitter = MakeFileNameSplitter(_filename);
		auto file = ::Assets::MainFileSystem::OpenFileInterface(splitter.AllExceptParameters(), "rb");
		auto size = file->GetDesc()._size;
		auto result = std::make_shared<std::vector<uint8>>((size_t)size);
		file->Read(AsPointer(result->begin()), result->size());
		return result;
	}

	::Assets::DepValPtr FileArtifact::GetDependencyValidation() const { return _depVal; }
	StringSection<ResChar>	FileArtifact::GetRequestParameters() const { return MakeFileNameSplitter(_filename).Parameters(); }
	FileArtifact::FileArtifact(const ::Assets::rstring& filename, const ::Assets::DepValPtr& depVal)
	: _filename(filename), _depVal(depVal) {}
	FileArtifact::~FileArtifact() {}


	auto BlobArtifact::GetBlob() const -> Blob { return _blob; }
	::Assets::DepValPtr BlobArtifact::GetDependencyValidation() const { return _depVal; }
	StringSection<ResChar>	BlobArtifact::GetRequestParameters() const { return MakeStringSection(_requestParams); }
	BlobArtifact::BlobArtifact(const Blob& blob, const ::Assets::DepValPtr& depVal, const rstring& requestParams)
	: _blob(blob), _depVal(depVal), _requestParams(requestParams) {}
	BlobArtifact::~BlobArtifact() {}

	auto CompilerExceptionArtifact::GetBlob() const -> ::Assets::Blob { return _log; }
	::Assets::DepValPtr CompilerExceptionArtifact::GetDependencyValidation() const { return _depVal; }
	StringSection<::Assets::ResChar>	CompilerExceptionArtifact::GetRequestParameters() const { return {}; }
	CompilerExceptionArtifact::CompilerExceptionArtifact(const ::Assets::Blob& log, const ::Assets::DepValPtr& depVal) : _log(log), _depVal(depVal) {}
	CompilerExceptionArtifact::~CompilerExceptionArtifact() {}

}
