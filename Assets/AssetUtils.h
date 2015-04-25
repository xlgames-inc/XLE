// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Assets.h"

namespace Assets
{
    class DirectorySearchRules
    {
    public:
        void AddSearchDirectory(const ResChar dir[]);
        void AddSearchDirectoryFromFilename(const ResChar filename[]);
        void ResolveFile(ResChar destination[], unsigned destinationCount, const ResChar baseName[]) const;
        bool HasDirectory(const ResChar dir[]);

        DirectorySearchRules();
        DirectorySearchRules(const DirectorySearchRules&);
        DirectorySearchRules& operator=(const DirectorySearchRules&);
    protected:
        ResChar _buffer[512];
        std::vector<ResChar> _bufferOverflow;
        unsigned _startOffsets[8];
    
        unsigned _bufferUsed;
        unsigned _startPointCount;
    };

    DirectorySearchRules DefaultDirectorySearchRules(const ResChar baseFile[]);

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DependentFileState
    {
    public:
        std::basic_string<ResChar> _filename;
        uint64 _timeMarker;

        enum class Status { Normal, Shadowed };
        Status _status;

        DependentFileState() : _timeMarker(0ull), _status(Status::Normal) {}
        DependentFileState(
            const std::basic_string<ResChar>& filename, 
            uint64 timeMarker)
        : _filename(filename), _timeMarker(timeMarker), _status(Status::Normal) {}
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace AssetState
    {
        enum Enum { Pending, Ready, Invalid };
    };

    void Dependencies_Shutdown();
}

