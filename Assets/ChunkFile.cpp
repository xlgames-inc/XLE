// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ChunkFile.h"
#include "Assets.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"

namespace Serialization { namespace ChunkFile
{
    using Assets::Exceptions::FormatError;

    std::vector<ChunkHeader> LoadChunkTable(BasicFile& file)
    {
        ChunkFileHeader fileHeader;
        if (file.Read(&fileHeader, sizeof(ChunkFileHeader), 1) != 1) {
            throw FormatError("Incomplete file header");
        }

        if (fileHeader._magic != MagicHeader) {
            throw FormatError("Unrecognised format");
        }

        if (fileHeader._fileVersionNumber != ChunkFileVersion) {
            throw FormatError("Bad chunk file format");
        }

        std::vector<ChunkHeader> result;
        result.resize(fileHeader._chunkCount);
        auto readCount = file.Read(AsPointer(result.begin()), sizeof(ChunkHeader), fileHeader._chunkCount);
        if (readCount != fileHeader._chunkCount) {
            throw FormatError("Incomplete file header");
        }

        return result;
    }

    Serialization::ChunkFile::ChunkHeader FindChunk(
        const char filename[],
        std::vector<Serialization::ChunkFile::ChunkHeader>& hdrs,
        Serialization::ChunkFile::TypeIdentifier chunkType,
        unsigned expectedVersion)
    {
        Serialization::ChunkFile::ChunkHeader scaffoldChunk;
        for (auto i=hdrs.begin(); i!=hdrs.end(); ++i) {
            if (i->_type == chunkType) {
                scaffoldChunk = *i;
                break;
            }
        }

        if (!scaffoldChunk._fileOffset) {
            throw FormatError("Missing could not find chunk in chunk file: %s", filename);
        }

        if (scaffoldChunk._chunkVersion != expectedVersion) {
            throw FormatError("Incorrect chunk version: %s", filename);
        }

        return scaffoldChunk;
    }

    std::unique_ptr<uint8[]> RawChunkAsMemoryBlock(
            const char filename[],
            Serialization::ChunkFile::TypeIdentifier chunkType,
            unsigned expectedVersion)
    {
        BasicFile file(filename, "rb");
        auto chunks = Serialization::ChunkFile::LoadChunkTable(file);

        auto scaffoldChunk = FindChunk(filename, chunks, chunkType, expectedVersion);
        auto rawMemoryBlock = std::make_unique<uint8[]>(scaffoldChunk._size);
        file.Seek(scaffoldChunk._fileOffset, SEEK_SET);
        file.Read(rawMemoryBlock.get(), 1, scaffoldChunk._size);
        return std::move(rawMemoryBlock);
    }


///////////////////////////////////////////////////////////////////////////////////////////////////
    SimpleChunkFileWriter::SimpleChunkFileWriter(
        unsigned chunkCount, const char filename[], const char openMode[], ShareMode::BitField shareMode,
        const char buildVersionString[], const char buildDateString[])
        : BasicFile(filename, openMode, shareMode)
        , _chunkCount(chunkCount)
    {
        using namespace Serialization::ChunkFile;

        _activeChunkStart = 0;
        _hasActiveChunk = false;
        _activeChunkIndex = 0;

        ChunkFileHeader fileHeader;
        XlZeroMemory(fileHeader);
        fileHeader._magic = MagicHeader;
        fileHeader._fileVersionNumber = 0;
        XlCopyString(fileHeader._buildVersion, dimof(fileHeader._buildVersion), buildVersionString);
        XlCopyString(fileHeader._buildDate, dimof(fileHeader._buildDate), buildDateString);
        fileHeader._chunkCount = chunkCount;

        Write(&fileHeader, sizeof(fileHeader), 1);
        for (unsigned c=0; c<chunkCount; ++c) {
            ChunkHeader t;
            Write(&t, sizeof(ChunkHeader), 1);
        }
    }

    SimpleChunkFileWriter::~SimpleChunkFileWriter()
    {
        if (_hasActiveChunk) {
            FinishCurrentChunk();
        }
        assert(_activeChunkIndex == _chunkCount);
    }

    void SimpleChunkFileWriter::BeginChunk(   Serialization::ChunkFile::TypeIdentifier type,
                                        unsigned version, const char name[])
    {
        if (_hasActiveChunk) {
            FinishCurrentChunk();
        }

        _activeChunk._type = type;
        _activeChunk._chunkVersion = version;
        XlCopyString(_activeChunk._name, name);
        _activeChunkStart = TellP();
        _activeChunk._fileOffset = (ChunkFile::SizeType)_activeChunkStart;
        _activeChunk._size = 0; // unknown currently

        _hasActiveChunk = true;
    }

    void SimpleChunkFileWriter::FinishCurrentChunk()
    {
        using namespace Serialization::ChunkFile;
        auto oldLoc = TellP();
        auto chunkHeaderLoc = sizeof(ChunkFileHeader) + _activeChunkIndex * sizeof(ChunkHeader);
        Seek(chunkHeaderLoc, SEEK_SET);
        _activeChunk._size = (ChunkFile::SizeType)std::max(size_t(0), oldLoc - _activeChunkStart);
        Write(&_activeChunk, sizeof(ChunkHeader), 1);
        Seek(oldLoc, SEEK_SET);
        ++_activeChunkIndex;
        _hasActiveChunk = false;
    }

}}

