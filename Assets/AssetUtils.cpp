// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetUtils.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"     // for XlGetCurrentDirectorys
#include <vector>
#include <algorithm>

namespace Assets
{
    static Utility::Threading::RecursiveMutex ResourceDependenciesLock;
    static std::vector<std::pair<const OnChangeCallback*, std::weak_ptr<DependencyValidation>>> ResourceDependencies;
    static unsigned ResourceDepsChangeId = 0;

    void Dependencies_Shutdown()
    {
        decltype(ResourceDependencies) temp;
        temp.swap(ResourceDependencies);
    }

    void    DependencyValidation::OnChange()
    { 
        ++_validationIndex;
        ResourceDependenciesLock.lock();

        auto range = std::equal_range(
            ResourceDependencies.begin(), ResourceDependencies.end(), 
            this, CompareFirst<const OnChangeCallback*, std::weak_ptr<DependencyValidation>>());

        unsigned changeIdStart = ResourceDepsChangeId;

        bool foundExpired = false;
        for (auto i=range.first; i!=range.second; ++i) {
            auto l = i->second.lock();
            if (l) { 
                l->OnChange();

                if (ResourceDepsChangeId != changeIdStart) {
                    // another object may have changed the list of objects 
                    // during the OnChange() call. If this happens,
                    // we need to search through and find the range again.
                    // then we need to set 'i' to the position of the 
                    // same element in the new range (it's guaranteed to
                    // be there, because we have a lock on it!
                    // Oh, it's a wierd hack... But it should work well.

                    range = std::equal_range(
                        ResourceDependencies.begin(), ResourceDependencies.end(), 
                        this, CompareFirst<const OnChangeCallback*, std::weak_ptr<DependencyValidation>>());

                    for (i=range.first;; ++i) {
                        assert(i!=range.second);
                        if (Equivalent(i->second, l)) break;
                    }

                    changeIdStart = ResourceDepsChangeId;
                }
            }
            else foundExpired = true;
        }

        if (foundExpired) {
                // Remove any pointers that have expired
                // (note that we only check matching pointers. Non-matching pointers
                // that have expired are untouched)
            ResourceDependencies.erase(
                std::remove_if(range.first, range.second, 
                    [](std::pair<const OnChangeCallback*, std::weak_ptr<DependencyValidation>>& i)
                    { return i.second.expired(); }),
                range.second);
            ++ResourceDepsChangeId; // signal to callers that this has changed
        }

        ResourceDependenciesLock.unlock();
    }

    void    DependencyValidation::RegisterDependency(const std::shared_ptr<Utility::OnChangeCallback>& dependency)
    {
        ResourceDependenciesLock.lock();
        auto i = LowerBound(ResourceDependencies, (const OnChangeCallback*)dependency.get());
        ResourceDependencies.insert(i, std::make_pair(dependency.get(), shared_from_this()));
        ResourceDependenciesLock.unlock();

            // We must hold a reference to the dependency -- otherwise it can be destroyed,
            // and links to downstream assets/files might be lost
            // It's a little awkward to hold it here, but it's the only way
            // to make sure that it gets destroyed when "this" gets destroyed

        for (unsigned c=0; c<dimof(_dependencies); ++c)
            if (!_dependencies[c]) { _dependencies[c] = dependency; return; }
        
        _dependenciesOverflow.push_back(dependency);
    }

    DependencyValidation::DependencyValidation(DependencyValidation&& moveFrom) never_throws
    {
        _validationIndex = moveFrom._validationIndex;
        for (unsigned c=0; c<dimof(_dependencies); ++c)
            _dependencies[c] = std::move(moveFrom._dependencies[c]);
        _dependenciesOverflow = std::move(moveFrom._dependenciesOverflow);
    }

    DependencyValidation& DependencyValidation::operator=(DependencyValidation&& moveFrom) never_throws
    {
        _validationIndex = moveFrom._validationIndex;
        for (unsigned c=0; c<dimof(_dependencies); ++c)
            _dependencies[c] = std::move(moveFrom._dependencies[c]);
        _dependenciesOverflow = std::move(moveFrom._dependenciesOverflow);
        return *this;
    }

    DependencyValidation::~DependencyValidation() {}

    void RegisterFileDependency(
        const std::shared_ptr<Utility::OnChangeCallback>& validationIndex, 
        const ResChar filename[])
    {
            // try to prevent unoptimisated path names from getting here!
        assert(!XlFindString(filename, "../") && !XlFindString(filename, "./"));

        ResChar directoryName[MaxPath];
        FileNameSplitter<ResChar> splitter(filename);
        SplitPath<ResChar>(splitter.DriveAndPath()).Simplify().Rebuild(directoryName);
        Utility::AttachFileSystemMonitor(
            StringSection<ResChar>(directoryName), splitter.FileAndExtension(), validationIndex);
    }

    void RegisterAssetDependency(
        const std::shared_ptr<DependencyValidation>& dependentResource, 
        const std::shared_ptr<Utility::OnChangeCallback>& dependency)
    {
        assert(dependentResource && dependency);
        dependentResource->RegisterDependency(dependency);
    }

    void DirectorySearchRules::AddSearchDirectory(const ResChar dir[])
    {
            //  Attempt to fit this directory into our buffer.
            //  note that we have limited space in the buffer, but we can't really
            //  rely on all directory names remaining small and convenient... If we 
            //  overflow our fixed size buffer, we can use the dynamically 
            //  allocated "_bufferOverflow"
        assert((_startPointCount+1) <= dimof(_startOffsets));
        if ((_startPointCount+1) > dimof(_startOffsets)) {
                //  limited number of directories that can be pushed into a single "search rules"
                //  this allows us to avoid a little bit of awkward dynamic memory allocation
            return; 
        }

            // Check for duplicates
            //  Duplicates are bad because they will increase the number of search operations
        if (HasDirectory(dir)) return;

        unsigned allocationLength = (unsigned)XlStringLen(dir) + 1;
        if (_bufferOverflow.empty() && (_bufferUsed + allocationLength <= dimof(_buffer))) {
                // just append this new string to our buffer, and add a new start offset
            XlCopyMemory(&_buffer[_bufferUsed], dir, allocationLength * sizeof(ResChar));
        } else {
            if (_bufferOverflow.empty()) {
                _bufferOverflow.resize(_bufferUsed + allocationLength);
                XlCopyMemory(AsPointer(_bufferOverflow.begin()), _buffer, _bufferUsed * sizeof(ResChar));
                XlCopyMemory(PtrAdd(AsPointer(_bufferOverflow.begin()), _bufferUsed * sizeof(ResChar)), dir, allocationLength * sizeof(ResChar));
            } else {
                assert(_bufferOverflow.size() == allocationLength);
                _bufferOverflow.insert(_bufferOverflow.end(), dir, &dir[allocationLength]);
            }
        }

        _startOffsets[_startPointCount++] = _bufferUsed;
        _bufferUsed += allocationLength;
    }

    void DirectorySearchRules::AddSearchDirectoryFromFilename(const ResChar filename[])
    {
        char path[MaxPath];
        XlDirname(path, dimof(path), filename);
        AddSearchDirectory(path);
    }

    bool DirectorySearchRules::HasDirectory(const ResChar dir[])
    {
        const ResChar* b = _buffer;
        if (!_bufferOverflow.empty()) {
            b = AsPointer(_bufferOverflow.begin());
        }

            // note --  just doing a string insensitive compare here...
            //          we should really do a more sophisticated path compare
            //          to get a more accurate result
            //          Actually, it might be better to store the paths in some
            //          format that is more convenient for comparisons and combining
            //          paths.
        for (unsigned c=0; c<_startPointCount; ++c)
            if (!XlCompareStringI(&b[_startOffsets[c]], dir)) 
                return true;

        return false;
    }

    void DirectorySearchRules::ResolveFile(ResChar destination[], unsigned destinationCount, const ResChar baseName[]) const
    {
        ResChar tempBuffer[MaxPath];

        const auto* colon = XlFindChar(baseName, ':');
        bool baseFileExist = false;
        if (colon) {
            XlCopyNString(tempBuffer, dimof(tempBuffer), baseName, colon - baseName);
            baseFileExist = DoesFileExist(tempBuffer);
        } else {
            baseFileExist = DoesFileExist(baseName);
            colon = &baseName[XlStringLen(baseName)];
        }

            // by definition, we always check the unmodified file name first
        if (!baseFileExist) {
            const ResChar* b = _buffer;
            if (!_bufferOverflow.empty()) {
                b = AsPointer(_bufferOverflow.begin());
            }

                // We want to support the case were destination == baseName
                // But that cases requires another temporary buffer, because we
                // don't want to trash "baseName" while searching for matches
            ResChar* workingBuffer = (baseName!=destination) ? destination : tempBuffer;
            unsigned workingBufferSize = (baseName!=destination) ? destinationCount : unsigned(dimof(tempBuffer));

            for (unsigned c=0; c<_startPointCount; ++c) {
                XlConcatPath(workingBuffer, workingBufferSize, &b[_startOffsets[c]], baseName, colon);
                if (DoesFileExist(workingBuffer)) {
                    SplitPath<ResChar>(workingBuffer).Simplify().Rebuild(workingBuffer, workingBufferSize);
                    if (workingBuffer != destination) {
                        auto workingBufferLen = std::min((ptrdiff_t)XlStringLen(workingBuffer), ptrdiff_t(destinationCount) - 1);
                        auto colonLen = (ptrdiff_t)XlStringLen(colon);
                        auto colonCopy = std::min(ptrdiff_t(destinationCount) - workingBufferLen - 1, colonLen);
                        assert((workingBufferLen + colonCopy) < ptrdiff_t(destinationCount));
                        if (colonCopy > 0)
                            XlMoveMemory(&destination[workingBufferLen], colon, colonCopy);
                        destination[workingBufferLen + colonCopy] = '\0';
                        assert(workingBufferLen < (ptrdiff_t(destinationCount)-1));
                        XlCopyMemory(destination, workingBuffer, workingBufferLen);
                    } else {
                        XlCatString(destination, destinationCount, colon);
                    }
                    return;
                }
            }
        }

        if (baseName != destination)
            XlCopyString(destination, destinationCount, baseName);
        SplitPath<ResChar>(destination).Simplify().Rebuild(destination, destinationCount);
    }

    void DirectorySearchRules::ResolveDirectory(
            ResChar destination[], unsigned destinationCount, 
            const ResChar baseName[]) const
    {
            //  We have a problem with basic paths (like '../')
            //  These will match for most directories -- which means that
            //  there is some ambiguity. Let's prefer to use the first
            //  registered path for simple relative paths like this.
        bool useBaseName = 
            (baseName[0] != '.' && DoesDirectoryExist(baseName));

        if (!useBaseName) {
            const ResChar* b = _buffer;
            if (!_bufferOverflow.empty()) {
                b = AsPointer(_bufferOverflow.begin());
            }

            const auto* baseEnd = &baseName[XlStringLen(baseName)];
            
            ResChar tempBuffer[MaxPath];
            ResChar* workingBuffer = (baseName!=destination) ? destination : tempBuffer;
            unsigned workingBufferSize = (baseName!=destination) ? destinationCount : unsigned(dimof(tempBuffer));

            for (unsigned c=0; c<_startPointCount; ++c) {
                XlConcatPath(workingBuffer, workingBufferSize, &b[_startOffsets[c]], baseName, baseEnd);
                if (DoesDirectoryExist(workingBuffer)) {
                    if (workingBuffer != destination)
                        XlCopyString(destination, destinationCount, workingBuffer);
                    return;
                }
            }
        }

        if (baseName != destination)
            XlCopyString(destination, destinationCount, baseName);
    }

    void DirectorySearchRules::Merge(const DirectorySearchRules& mergeFrom)
    {
            // Merge in the settings from the given search rules (if the directories
            // don't already exist here)
            // We should really do a better job of comparing directories. Where strings
            // resolve to the same directory, we should consider them identical
        const ResChar* b = mergeFrom._buffer;
        if (!mergeFrom._bufferOverflow.empty())
            b = AsPointer(mergeFrom._bufferOverflow.begin());

        for (unsigned c=0; c<mergeFrom._startPointCount; ++c)
            AddSearchDirectory(&b[mergeFrom._startOffsets[c]]);
    }

    const ResChar* DirectorySearchRules::GetFirstSearchDir() const { return _buffer; }

    DirectorySearchRules::DirectorySearchRules()
    {
        _buffer[0] = '\0';
        _startPointCount = 0;
        _bufferUsed = 0;
        std::fill(_startOffsets, &_startOffsets[dimof(_startOffsets)], 0);
    }

    DirectorySearchRules::DirectorySearchRules(const DirectorySearchRules& copyFrom)
    : _bufferOverflow(copyFrom._bufferOverflow)
    {
        std::copy(copyFrom._buffer, &copyFrom._buffer[dimof(_buffer)], _buffer);
        std::copy(copyFrom._startOffsets, &copyFrom._startOffsets[dimof(_startOffsets)], _startOffsets);
        _bufferUsed = copyFrom._bufferUsed;
        _startPointCount = copyFrom._startPointCount;
    }

    DirectorySearchRules& DirectorySearchRules::operator=(const DirectorySearchRules& copyFrom)
    {
        std::copy(copyFrom._buffer, &copyFrom._buffer[dimof(_buffer)], _buffer);
        std::copy(copyFrom._startOffsets, &copyFrom._startOffsets[dimof(_startOffsets)], _startOffsets);
        _bufferOverflow = copyFrom._bufferOverflow;
        _bufferUsed = copyFrom._bufferUsed;
        _startPointCount = copyFrom._startPointCount;
        return *this;
    }

    DirectorySearchRules DefaultDirectorySearchRules(const ResChar baseFile[])
    {
        Assets::DirectorySearchRules searchRules;
        searchRules.AddSearchDirectoryFromFilename(baseFile);
        return searchRules;
    }

    namespace Exceptions
    {
        InvalidAsset::InvalidAsset(const char initializer[], const char what[]) 
        : ::Exceptions::BasicLabel(what) 
        {
            XlCopyString(_initializer, dimof(_initializer), initializer); 
        }

        bool InvalidAsset::CustomReport() const
        {
            LogAlwaysError << "Invalid asset: " << _initializer << ". More information:";
            LogAlwaysError << what();
            return true;
        }

        PendingAsset::PendingAsset(const char initializer[], const char what[]) 
        : ::Exceptions::BasicLabel(what) 
        {
            XlCopyString(_initializer, dimof(_initializer), initializer); 
        }

        bool PendingAsset::CustomReport() const
        {
            LogAlwaysWarning << "Pending asset: " << _initializer;
            return true;
        }

        FormatError::FormatError(const char format[], ...) never_throws
        {
            va_list args;
            va_start(args, format);
            XlFormatStringV(_buffer, dimof(_buffer), format, args);
            va_end(args);
        }

        UnsupportedFormat::UnsupportedFormat(const char format[], ...) never_throws
        {
            va_list args;
            va_start(args, format);
            XlFormatStringV(_buffer, dimof(_buffer), format, args);
            va_end(args);
        }
    }


    PendingOperationMarker::PendingOperationMarker() : _state(AssetState::Pending)
    {
        DEBUG_ONLY(_initializer[0] = '\0');
    }

    PendingOperationMarker::PendingOperationMarker(AssetState state, std::shared_ptr<DependencyValidation> depVal) 
    : _state(state), _dependencyValidation(std::move(depVal))
    {
        DEBUG_ONLY(_initializer[0] = '\0');
    }

    PendingOperationMarker::~PendingOperationMarker() {}

    const char* PendingOperationMarker::Initializer() const
    {
        #if defined(_DEBUG)
            return _initializer;
        #else
            return "";
        #endif
    }

    void PendingOperationMarker::SetInitializer(const char initializer[])
    {
        DEBUG_ONLY(XlCopyString(_initializer, initializer));
    }

    void PendingOperationMarker::SetState(AssetState newState)
    {
        _state = newState;
    }

    AssetState PendingOperationMarker::StallWhilePending() const
    {
            // Stall until the _state variable changes
            // in another thread.
            // there is no semaphore, so we must poll
            // note -- we should start a progress bar here...
        volatile AssetState* state = const_cast<AssetState*>(&_state);
        while (*state == AssetState::Pending)
            Threading::YieldTimeSlice();

        return *state;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const SplitPath<ResChar>& BaseDir()
    {
            // hack -- find the base directory we'll use to build relative names from 
            //  Note that this is going to call XlGetCurrentDirectory at some unpredictable
            //  time; so we're assuming that the current directory is set at app start, and
            //  remains constant
        static ResolvedAssetFile buffer;
        static SplitPath<ResChar> result;
        static bool init = false;
        if (!init) {
            XlGetCurrentDirectory(dimof(buffer._fn), buffer._fn);
            SplitPath<ResChar>(buffer._fn).Rebuild(buffer._fn, dimof(buffer._fn));
            result = SplitPath<ResChar>(buffer._fn);
            init = true;
        }
        return result;
    }

    void MakeAssetName(ResolvedAssetFile& dest, const ::Assets::ResChar src[])
    {
        auto rules = s_defaultFilenameRules;
        FileNameSplitter<ResChar> srcSplit(src);
        auto relPath = 
            MakeRelativePath(
                BaseDir(),
                SplitPath<ResChar>(srcSplit.DriveAndPath()),
                rules);

        XlCopyString(dest._fn, relPath.c_str());

        auto* i = &dest._fn[XlStringLen(dest._fn)];
        auto* iend = ArrayEnd(dest._fn);
        auto fileAndExtension = srcSplit.FileAndExtension();
        auto* s = fileAndExtension._start;
        auto* send = fileAndExtension._end;

        while (i!=iend && s!=send) { *i = ConvertPathChar(*s, rules); ++i; ++s; }
        
        if (!srcSplit.ParametersWithDivider().Empty()) {
            s = srcSplit.ParametersWithDivider()._start;
            send = srcSplit.ParametersWithDivider()._end;
            while (i!=iend && s!=send) { *i = *s; ++i; ++s; }
        }

        *std::min(i, iend-1) = '\0';
    }

    void MakeAssetName(ResolvedAssetFile& dest, const utf8 src[])
    {
        MakeAssetName(dest, (const ::Assets::ResChar*)src);
    }

}

