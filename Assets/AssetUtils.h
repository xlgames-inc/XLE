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

        void ResolveFile(
            ResChar destination[], unsigned destinationCount, 
            const ResChar baseName[]) const;
        void ResolveDirectory(
            ResChar destination[], unsigned destinationCount, 
            const ResChar baseName[]) const;
        bool HasDirectory(StringSection<ResChar> dir);
		std::vector<std::basic_string<ResChar>> FindFiles(StringSection<char> wildcardSearch) const;

        template<int Count>
            void ResolveFile(ResChar (&destination)[Count], const ResChar baseName[]) const
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

        ////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Records the status of asynchronous operation, very much like a std::promise<AssetState></summary>
    class PendingOperationMarker : public std::enable_shared_from_this<PendingOperationMarker>
    {
    public:
        AssetState		GetAssetState() const { return _state; }
        AssetState		StallWhilePending() const;
        const char*     Initializer() const;  // "initializer" interface only provided in debug builds, and only intended for debugging

        PendingOperationMarker(AssetState state = AssetState::Pending);
        ~PendingOperationMarker();

		PendingOperationMarker(PendingOperationMarker&&) = delete;
		PendingOperationMarker& operator=(PendingOperationMarker&&) = delete;
		PendingOperationMarker(const PendingOperationMarker&) = delete;
		PendingOperationMarker& operator=(const PendingOperationMarker&) = delete;

		void	SetState(AssetState newState);
		void	SetInitializer(const ResChar initializer[]);

	private:
		AssetState _state;
		DEBUG_ONLY(ResChar _initializer[MaxPath];)
    };

		////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Container for a asset filename in string format<summary>
    /// Just a simple generalisation of a path and file name in char array form.
    /// Avoids scattering references to ResChar and MaxPath about
    /// the code (and provide some future-proof-ness).
    ///
    /// Note that in this form there is a strict limit on the max length of 
    /// and asset file name. This is in line with the MAX_PATH soft limit
    /// on some filesystems and standard library implementations... But most
    /// filesystems can actually support much longer path names (even if an
    /// individual directory name or filename is limited)
    class ResolvedAssetFile
    {
    public:
        ResChar _fn[MaxPath];

        const ResChar* get() const  { return _fn; }
        const bool IsGood() const   { return _fn[0] != '\0'; }

        ResolvedAssetFile() { _fn[0] = '\0'; }
    };

    /// @{
    /// Converts an input filename to a form that is best suited for the assets system.
    /// This includes converting absolute filenames into relative format (relative to the
    /// primary mount point).
    /// This is intended for GUI tools that allow the user to enter filenames of any form.
    void MakeAssetName(ResolvedAssetFile& dest, const StringSection<ResChar> src);
    void MakeAssetName(ResolvedAssetFile& dest, const StringSection<utf8> src);
    /// @}

        ////////////////////////////////////////////////////////////////////////////////////////////////

    void Dependencies_Shutdown();
}

