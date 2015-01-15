// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringUtils.h"
#include "../Core/Types.h"
#include <algorithm>
#include <vector>

namespace Utility { class BasicFile; }

namespace Serialization { namespace ChunkFile
{
    typedef uint64 TypeIdentifier;
    typedef uint32 SizeType;

    static const TypeIdentifier TypeIdentifier_Unknown = 0;

    class ChunkHeader
    {
    public:
        TypeIdentifier  _type;
        unsigned        _chunkVersion;
        char            _name[32];      // fixed size for serialisation convenience
        SizeType        _fileOffset;
        SizeType        _size;

        ChunkHeader()
        {
            _type = TypeIdentifier_Unknown;
            _chunkVersion = 0;
            std::fill(_name, &_name[dimof(_name)], 0);
            _fileOffset = _size = 0;
        }

        ChunkHeader(TypeIdentifier type, unsigned version, 
                    const char name[], SizeType size)
        {
            _type = type;
            _chunkVersion = version;
            XlCopyString(_name, name);
            _fileOffset = 0;        // (not yet decided)
            _size = size;
        }
    };

    static const unsigned MagicHeader = uint32('X') | (uint32('L') << 8) | (uint32('E') << 16) | (uint32('~') << 24);
    static const unsigned ChunkFileVersion = 0;

    class ChunkFileHeader
    {
    public:
        uint32      _magic;
        unsigned    _fileVersionNumber;
        char        _buildVersion[64];
        char        _buildDate[64];
        unsigned    _chunkCount;
    };

    std::vector<ChunkHeader> LoadChunkTable(Utility::BasicFile& file);

    ChunkHeader FindChunk(
        const char filename[], std::vector<ChunkHeader>& hdrs,
        TypeIdentifier chunkType, unsigned expectedVersion);

    std::unique_ptr<uint8[]> RawChunkAsMemoryBlock(
        const char filename[], TypeIdentifier chunkType, unsigned expectedVersion);

    class SimpleChunkFileWriter : public Utility::BasicFile
    {
    public:
        SimpleChunkFileWriter(
            unsigned chunkCount, const char filename[], const char openMode[], ShareMode::BitField shareMode,
            const char buildVersionString[], const char buildDateString[]);
        ~SimpleChunkFileWriter();

        void BeginChunk(    
            Serialization::ChunkFile::TypeIdentifier type,
            unsigned version, const char name[]);
        void FinishCurrentChunk();

    protected:
        Serialization::ChunkFile::ChunkHeader _activeChunk;
        size_t _activeChunkStart;
        bool _hasActiveChunk;
        unsigned _chunkCount;
        unsigned _activeChunkIndex;
    };

}}

