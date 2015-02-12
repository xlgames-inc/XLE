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
        void ResolveFile(ResChar destination[], unsigned destinationCount, const ResChar baseName[]) const;

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

    class FileAndTime
    {
    public:
        std::string _filename;
        uint64 _timeMarker;

        FileAndTime() : _timeMarker(0) {}
        FileAndTime(const std::string& filename, uint64 timeMarker) : _filename(filename), _timeMarker(timeMarker) {}
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    namespace AssetState
    {
        enum Enum { Pending, Ready, Invalid };
    };
}

