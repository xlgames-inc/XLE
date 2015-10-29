// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "IntermediateAssets.h"

#include "CompileAndAsyncManager.h"     // for ~PendingCompileMarker -- remove

#include "../ConsoleRig/Log.h"
#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Data.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/IteratorUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/ExceptionLogging.h"

#include "../Core/WinAPI/IncludeWindows.h"
#include <memory>

namespace Assets { namespace IntermediateAssets
{

    static ResChar ConvChar(ResChar input) 
    {
        return (ResChar)((input == '\\')?'/':tolower(input));
    }

    void    Store::MakeIntermediateName(ResChar buffer[], unsigned bufferMaxCount, StringSection<ResChar> firstInitializer) const
    {
            // calculate the intermediate file for this request name (normally just a file with the 
            //  same name in our branch directory
        if (buffer == firstInitializer.begin()) {
            assert(bufferMaxCount >= (_baseDirectory.size()+1));
            auto length = firstInitializer.Length();
            auto moveSize = std::min(unsigned(length), unsigned(bufferMaxCount-1-(_baseDirectory.size()+1)));
            XlMoveMemory(&buffer[_baseDirectory.size()+1], buffer, moveSize);
            buffer[_baseDirectory.size()+1+moveSize] = ResChar('\0');
            std::copy(_baseDirectory.begin(), _baseDirectory.end(), buffer);
            buffer[_baseDirectory.size()] = ResChar('/');
        } else {
            XlCopyString(buffer, bufferMaxCount, _baseDirectory.c_str());
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
        static void MakeDepFileName(ResChar (&destination)[DestCount], const ResChar baseDirectory[], const ResChar depFileName[])
        {
                //  if the prefix of "baseDirectory" and "intermediateFileName" match, we should skip over that
            const ResChar* f = depFileName, *b = baseDirectory;
            while (ConvChar(*f) == ConvChar(*b) && *f != '\0') { ++f; ++b; }
            while (ConvChar(*f) == '/') { ++f; }
            _snprintf_s(destination, sizeof(ResChar)*DestCount, _TRUNCATE, "%s/.deps/%s", baseDirectory, f);
        }

    class RetainedFileRecord : public DependencyValidation
    {
    public:
        DependentFileState _state;

        void OnChange()
        {
                // on change, update the modification time record
            _state._timeMarker = GetFileModificationTime(_state._filename.c_str());
            DependencyValidation::OnChange();
        }

        RetainedFileRecord(const ResChar filename[])
        : _state(filename, 0ull) {}
    };

    static std::vector<std::pair<uint64, std::shared_ptr<RetainedFileRecord>>> RetainedRecords;
    static Threading::Mutex RetainedRecordsLock;

    static std::shared_ptr<RetainedFileRecord>& GetRetainedFileRecord(StringSection<ResChar> filename)
    {
            //  We should normalize to avoid problems related to
            //  case insensitivity and slash differences
        ResolvedAssetFile assetName;
        MakeAssetName(assetName, filename);
        auto hash = Hash64(assetName._fn);

        {
            ScopedLock(RetainedRecordsLock);
            auto i = LowerBound(RetainedRecords, hash);
            if (i!=RetainedRecords.end() && i->first == hash) {
                return i->second;
            }

                //  we should call "AttachFileSystemMonitor" before we query for the
                //  file's current modification time
            auto newRecord = std::make_shared<RetainedFileRecord>(assetName._fn);
            RegisterFileDependency(newRecord, assetName._fn);
            newRecord->_state._timeMarker = GetFileModificationTime(assetName._fn);

            return RetainedRecords.insert(i, std::make_pair(hash, std::move(newRecord)))->second;
        }
    }

    const DependentFileState& Store::GetDependentFileState(StringSection<ResChar> filename)
    {
        return GetRetainedFileRecord(filename)->_state;
    }

    void Store::ShadowFile(const ResChar filename[])
    {
        auto& record = GetRetainedFileRecord(filename);
        record->_state._status = DependentFileState::Status::Shadowed;

            // propagate change messages...
            // (duplicating processing from RegisterFileDependency)
        ResChar directoryName[MaxPath];
        FileNameSplitter<ResChar> splitter(filename);
        SplitPath<ResChar>(splitter.DriveAndPath()).Simplify().Rebuild(directoryName);
        
        FakeFileChange(StringSection<ResChar>(directoryName), splitter.FileAndExtension());

        record->OnChange();
    }

    std::shared_ptr<DependencyValidation> Store::MakeDependencyValidation(const ResChar intermediateFileName[]) const
    {
            //  When we process a file, we write a little text file to the
            //  ".deps" directory. This contains a list of dependency files, and
            //  the state of those files when this file was compiled.
            //  If the current files don't match the state that's recorded in
            //  the .deps file, then we can assume that it is out of date and
            //  must be recompiled.

        ResChar buffer[MaxPath];
        MakeDepFileName(buffer, _baseDirectory.c_str(), intermediateFileName);
        if (!DoesFileExist(buffer)) return nullptr;

        Data data;
        data.LoadFromFile(buffer);

        auto* basePath = data.StrAttribute("BasePath");
        auto validation = std::make_shared<DependencyValidation>();
        auto* dependenciesBlock = data.ChildWithValue("Dependencies");
        if (dependenciesBlock) {
            for (auto* dependency = dependenciesBlock->child; dependency; dependency = dependency->next) {
                auto* depName = dependency->value;
                if (!depName || !depName[0]) continue;

                auto dateLow = (unsigned)dependency->IntAttribute("ModTimeL");
                auto dateHigh = (unsigned)dependency->IntAttribute("ModTimeH");
                    
                const RetainedFileRecord* record;
                if (basePath && basePath[0]) {
                    XlConcatPath(buffer, dimof(buffer), basePath, depName, &depName[XlStringLen(depName)]);
                    auto& ptr = GetRetainedFileRecord(buffer);
                    RegisterAssetDependency(validation, ptr);
                    record = ptr.get();
                } else {
                    auto& ptr = GetRetainedFileRecord(depName);
                    RegisterAssetDependency(validation, ptr);
                    record = ptr.get();
                }

                if (record->_state._status == DependentFileState::Status::Shadowed) {
                    LogInfo << "Asset (" << intermediateFileName << ") is invalidated because dependency (" << depName << ") is marked shadowed";
                    return nullptr;
                }

                if (!record->_state._timeMarker) {
                    LogInfo
                        << "Asset (" << intermediateFileName 
                        << ") is invalidated because of missing dependency (" << depName << ")";
                    return nullptr;
                } else if (record->_state._timeMarker != ((uint64(dateHigh) << 32ull) | uint64(dateLow))) {
                    LogInfo
                        << "Asset (" << intermediateFileName 
                        << ") is invalidated because of file data on dependency (" << depName << ")";
                    return nullptr;
                }
            }
        }

        return validation;
    }

    std::shared_ptr<DependencyValidation> Store::WriteDependencies(
        const ResChar intermediateFileName[], 
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

        MakeDepFileName(buffer, _baseDirectory.c_str(), intermediateFileName);

            // first, create the directory if we need to
        char dirName[MaxPath];
        XlDirname(dirName, dimof(dirName), buffer);
        CreateDirectoryRecursive(dirName);

            // now, write -- 
        data.Save(buffer);

        return result;
    }

    Store::Store(const ResChar baseDirectory[], const ResChar versionString[])
    {
            //  First, we need to find an output directory to use.
            //  We want a directory that isn't currently being used, and
            //  that matches the version string.

        char buffer[MaxPath];
        _snprintf_s(buffer, _TRUNCATE, "%s/d*", baseDirectory);

        std::string goodBranchDir;

        {
                //  Look for existing directories that could match the version
                //  string we have. 
            WIN32_FIND_DATAA findData;
            XlZeroMemory(findData);
            HANDLE findHandle = FindFirstFileA(buffer, &findData);
            if (findHandle != INVALID_HANDLE_VALUE) {
                do {
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        _snprintf_s(buffer, _TRUNCATE, "%s/%s/.store", baseDirectory, findData.cFileName);
                        TRY 
                        {
                                // Note --  Ideally we want to prevent two different instances of the
                                //          same app from using the same intermediate assets store.
                                //          We can do this by use a "non-shareable" file mode when
                                //          we load these files. 
                            BasicFile markerFile(buffer, "r+b");
                            auto fileSize = markerFile.GetSize();
                            if (fileSize != 0) {
                                auto rawData = std::unique_ptr<uint8[]>(new uint8[int(fileSize)]);
                                markerFile.Read(rawData.get(), 1, size_t(fileSize));

                                Data data;
                                data.Load((const char*)rawData.get(), (int)fileSize);
                                auto* compareVersion = data.StrAttribute("VersionString");
                                if (!_stricmp(versionString, compareVersion)) {
                                    // this branch is already present, and is good... so use it
                                    goodBranchDir = std::string(baseDirectory) + "/" + findData.cFileName;
                                    break;
                                }

                                // it's a store for some other version of the executable. Try the next one
                                continue;
                            }
                        } 
                        CATCH (...) {}
                        CATCH_END
                    }
                } while (FindNextFileA(findHandle, &findData));

                FindClose(findHandle);
            }
        }

        if (goodBranchDir.empty()) {
                // if we didn't find an existing folder we can use, we need to create a new one
                // search through to find the first unused directory
            for (unsigned d=0;;++d) {
                _snprintf_s(buffer, _TRUNCATE, "%s/d%i", baseDirectory, d);
                DWORD dwAttrib = GetFileAttributes(buffer);
                if (dwAttrib != INVALID_FILE_ATTRIBUTES) {
                    continue;
                }

                CreateDirectoryRecursive(buffer);
                goodBranchDir = buffer;

                _snprintf_s(buffer, _TRUNCATE, "%s/d%i/.store", baseDirectory, d);

                Data newData;
                newData.SetAttribute("VersionString", versionString);
                newData.Save(buffer);
                break;
            }
        }

        _baseDirectory = goodBranchDir;
    }

    Store::~Store() 
    {
        decltype(RetainedRecords) temp;
        temp.swap(RetainedRecords);
    }

    ////////////////////////////////////////////////////////////////////////////////////////

    class CompilerSet::Pimpl 
    {
    public:
        std::vector<std::pair<uint64, std::shared_ptr<IAssetCompiler>>> _compilers;
    };

    void CompilerSet::AddCompiler(uint64 typeCode, const std::shared_ptr<IAssetCompiler>& processor)
    {
        auto i = LowerBound(_pimpl->_compilers, typeCode);
        if (i != _pimpl->_compilers.cend() && i->first == typeCode) {
            i->second = processor;
        } else {
            _pimpl->_compilers.insert(i, std::make_pair(typeCode, processor));
        }
    }

    std::shared_ptr<PendingCompileMarker> CompilerSet::PrepareAsset(
        uint64 typeCode, const ResChar* initializers[], unsigned initializerCount,
        Store& store)
    {
            // look for a "processor" object with the given type code, and rebuild the file
            // write the .deps file containing dependencies information
            //  Note that there's a slight race condition type problem here. We are querying
            //  the dependency files for their state after the processing has completed. So
            //  if the dependency file changes state during processing, we might not recognize
            //  that change properly. It's probably ignorable, however.

            // note that ideally we want to be able to schedule this in the background
        auto i = LowerBound(_pimpl->_compilers, typeCode);
        if (i != _pimpl->_compilers.cend() && i->first == typeCode) {

            TRY {
                return i->second->PrepareAsset(typeCode, initializers, initializerCount, store);
            } CATCH (const std::exception& e) {
                    // we must send back an Invalid marker if we hit an exception
                    // (otherwise we can end up in an infinite compile operation)
                LogAlwaysError << "Exception during processing of (" << initializers[0] << "). Exception details: (" << e.what() << ")";
                return std::make_shared<PendingCompileMarker>(AssetState::Invalid, nullptr, 0, nullptr);
            } CATCH (...) {
                LogAlwaysError << "Unknown exception during processing of (" << initializers[0] << ").";
                return std::make_shared<PendingCompileMarker>(AssetState::Invalid, nullptr, 0, nullptr);
            } CATCH_END

        } else {
            assert(0);  // couldn't find a processor for this asset type
        }

        return nullptr;
    }

    void CompilerSet::StallOnPendingOperations(bool cancelAll)
    {
        for (auto i=_pimpl->_compilers.cbegin(); i!=_pimpl->_compilers.cend(); ++i)
            i->second->StallOnPendingOperations(cancelAll);
    }

    CompilerSet::CompilerSet()
    {
        auto pimpl = std::make_unique<Pimpl>();
        _pimpl = std::move(pimpl);
    }

    CompilerSet::~CompilerSet()
    {
    }

    IAssetCompiler::~IAssetCompiler() {}
}}

            ////////////////////////////////////////////////////////////

namespace Assets
{
    PendingCompileMarker::PendingCompileMarker() : _sourceID1(0) 
    {
        DEBUG_ONLY(_initializer[0] = '\0');
    }

    PendingCompileMarker::PendingCompileMarker(AssetState state, const char sourceID0[], uint64 sourceID1, std::shared_ptr<Assets::DependencyValidation> depVal)
    : PendingOperationMarker(state, std::move(depVal)), _sourceID1(sourceID1)
    {
        if (sourceID0) {
            XlCopyString(_sourceID0, sourceID0);
        } else {
            _sourceID0[0] = '\0';
        }
    }

    PendingCompileMarker::~PendingCompileMarker() {}
}