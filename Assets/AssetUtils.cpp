// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AssetUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include <vector>
#include <algorithm>

namespace Assets
{
    static Utility::Threading::RecursiveMutex           ResourceDependenciesLock;
    static std::vector<std::pair<const DependencyValidation*, std::shared_ptr<DependencyValidation>>>       ResourceDependencies;

    void    DependencyValidation::OnChange()
    { 
        ++_validationIndex;
        ResourceDependenciesLock.lock();
        auto range = std::equal_range(
            ResourceDependencies.begin(), ResourceDependencies.end(), 
            this, CompareFirst<const DependencyValidation*, std::shared_ptr<DependencyValidation>>());
        for (auto i=range.first; i!=range.second; ++i) {
            i->second->OnChange();
        }
        ResourceDependenciesLock.unlock();
    }

    void RegisterFileDependency(const std::shared_ptr<DependencyValidation>& validationIndex, const char filename[])
    {
        ResChar directoryName[MaxPath], baseName[MaxPath];
        XlNormalizePath(baseName, dimof(baseName), filename);
        XlDirname(directoryName, dimof(directoryName), baseName);
        auto len = XlStringLen(directoryName);
        if (len > 0 && (directoryName[len-1] == '\\' || directoryName[len-1] == '/')) {
            directoryName[len-1] = '\0'; 
        }
        XlBasename(baseName, dimof(baseName), baseName);
        if (!directoryName[0]) XlCopyString(directoryName, "./");
        Utility::AttachFileSystemMonitor(directoryName, baseName, validationIndex);
    }

    void RegisterAssetDependency(const std::shared_ptr<DependencyValidation>& dependentResource, const DependencyValidation* dependency)
    {
        assert(dependentResource && dependency);
        ResourceDependenciesLock.lock();
        auto i = std::lower_bound(
            ResourceDependencies.begin(), ResourceDependencies.end(), 
            dependency, CompareFirst<const OnChangeCallback*, std::shared_ptr<OnChangeCallback>>());
        ResourceDependencies.insert(i, std::make_pair(dependency, dependentResource));
        ResourceDependenciesLock.unlock();
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

    void DirectorySearchRules::ResolveFile(ResChar destination[], unsigned destinationCount, const ResChar baseName[]) const
    {
            // by definition, we always check the unmodified file name first
        if (!DoesFileExist(baseName)) {
            const ResChar* b = _buffer;
            if (!_bufferOverflow.empty()) {
                b = AsPointer(_bufferOverflow.begin());
            }

            for (unsigned c=0; c<_startPointCount; ++c) {
                XlConcatPath(destination, destinationCount, &b[_startOffsets[c]], baseName);
                if (DoesFileExist(destination)) {
                    return;
                }
            }
        }

        XlCopyString(destination, destinationCount, baseName);
    }

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
        char skinPath[MaxPath];
        XlDirname(skinPath, dimof(skinPath), baseFile);
        Assets::DirectorySearchRules searchRules;
        searchRules.AddSearchDirectory(skinPath);
        return searchRules;
    }

    namespace Exceptions
    {
        InvalidResource::InvalidResource(const char resourceId[], const char what[]) 
        : ::Exceptions::BasicLabel(what) 
        {
            XlCopyString(_resourceId, dimof(_resourceId), resourceId); 
        }

        PendingResource::PendingResource(const char resourceId[], const char what[]) 
        : ::Exceptions::BasicLabel(what) 
        {
            XlCopyString(_resourceId, dimof(_resourceId), resourceId); 
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
}

