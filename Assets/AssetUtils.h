// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/StringUtils.h"

namespace Assets
{
    class DirectorySearchRules
    {
    public:
        void AddSearchDirectory(StringSection<ResChar> dir);
        void AddSearchDirectoryFromFilename(StringSection<ResChar> filename);
        std::string AnySearchDirectory() const;
		void SetBaseFile(StringSection<ResChar> file);

        void ResolveFile(
            ResChar destination[], unsigned destinationCount, 
            StringSection<ResChar> baseName) const;
        void ResolveDirectory(
            ResChar destination[], unsigned destinationCount, 
            StringSection<ResChar> baseName) const;
        bool HasDirectory(StringSection<ResChar> dir);
		std::vector<std::basic_string<ResChar>> FindFiles(StringSection<char> wildcardSearch) const;

        template<int Count>
            void ResolveFile(ResChar (&destination)[Count], StringSection<ResChar> baseName) const
                { ResolveFile(destination, Count, baseName); }

        void Merge(const DirectorySearchRules& mergeFrom);

        DirectorySearchRules();
        DirectorySearchRules(const DirectorySearchRules&);
        DirectorySearchRules& operator=(const DirectorySearchRules&);
		DirectorySearchRules(DirectorySearchRules&&) never_throws;
        DirectorySearchRules& operator=(DirectorySearchRules&&) never_throws;
    protected:
        ResChar _buffer[512];
        std::vector<ResChar> _bufferOverflow;
        unsigned _startOffsets[8];
    
        unsigned _bufferUsed;
        unsigned _startPointCount;

		ResChar _baseFile[MaxPath];
    };

    DirectorySearchRules DefaultDirectorySearchRules(StringSection<ResChar> baseFile);

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DependentFileState
    {
    public:
        std::basic_string<ResChar> _filename;
        uint64 _timeMarker;

        enum class Status { Normal, Shadowed };
        Status _status;

        DependentFileState() : _timeMarker(0ull), _status(Status::Normal) {}
        DependentFileState(StringSection<ResChar> filename, uint64 timeMarker)
        : _filename(filename.AsString()), _timeMarker(timeMarker), _status(Status::Normal) {}
		DependentFileState(const std::basic_string<ResChar>& filename, uint64 timeMarker)
		: _filename(filename), _timeMarker(timeMarker), _status(Status::Normal) {}
    };

	DepValPtr AsDepVal(IteratorRange<const DependentFileState*> deps);

        ////////////////////////////////////////////////////////////////////////////////////////////////

    void Dependencies_Shutdown();
}

