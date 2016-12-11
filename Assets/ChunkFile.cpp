// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ChunkFile.h"
#include "Assets.h"
#include "IFileSystem.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/Stream.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"
#include <tuple>

namespace Serialization { namespace ChunkFile
{
    using Assets::Exceptions::FormatError;

    ChunkFileHeader MakeChunkFileHeader(unsigned chunkCount, const char buildVersionString[], const char buildDateString[])
    {
        ChunkFileHeader header;
        XlZeroMemory(header);
        header._magic = MagicHeader;
        header._fileVersionNumber = ChunkFileVersion;
        XlCopyString(header._buildVersion, buildVersionString);
        XlCopyString(header._buildDate, buildDateString);
        header._chunkCount = chunkCount;
        return header;
    }

    std::vector<ChunkHeader> LoadChunkTable(::Assets::IFileInterface& file)
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
        const utf8 filename[],
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
        const utf8 filename[],
        Serialization::ChunkFile::TypeIdentifier chunkType,
        unsigned expectedVersion)
    {
        auto file = Assets::MainFileSystem::OpenFileInterface(filename, "rb");
        auto chunks = Serialization::ChunkFile::LoadChunkTable(*file);

        auto scaffoldChunk = FindChunk(filename, chunks, chunkType, expectedVersion);
        auto rawMemoryBlock = std::make_unique<uint8[]>(scaffoldChunk._size);
        file->Seek(scaffoldChunk._fileOffset);
        file->Read(rawMemoryBlock.get(), 1, scaffoldChunk._size);
        return std::move(rawMemoryBlock);
    }


///////////////////////////////////////////////////////////////////////////////////////////////////
    namespace Internal
    {
        template<typename Writer>
            SimpleChunkFileWriterT<Writer>::SimpleChunkFileWriterT(
				Writer&& writer,
                unsigned chunkCount, const char buildVersionString[], const char buildDateString[])
        : _writer(std::move(writer))
        , _chunkCount(chunkCount)
        {
            _activeChunkStart = 0;
            _hasActiveChunk = false;
            _activeChunkIndex = 0;
            
            ChunkFileHeader fileHeader = MakeChunkFileHeader(
                chunkCount, buildVersionString, buildDateString);
			_writer.Write(&fileHeader, sizeof(fileHeader), 1);
            for (unsigned c=0; c<chunkCount; ++c) {
                ChunkHeader t;
				_writer.Write(&t, sizeof(ChunkHeader), 1);
            }
        }

        template<typename Writer>
            SimpleChunkFileWriterT<Writer>::~SimpleChunkFileWriterT()
        {
            if (_hasActiveChunk) {
                FinishCurrentChunk();
            }
            assert(_activeChunkIndex == _chunkCount);
        }

        template<typename Writer>
            void SimpleChunkFileWriterT<Writer>::BeginChunk(   
                Serialization::ChunkFile::TypeIdentifier type,
                unsigned version, const char name[])
        {
            if (_hasActiveChunk) {
                FinishCurrentChunk();
            }

            _activeChunk._type = type;
            _activeChunk._chunkVersion = version;
            XlCopyString(_activeChunk._name, name);
            _activeChunkStart = _writer.TellP();
            _activeChunk._fileOffset = (ChunkFile::SizeType)_activeChunkStart;
            _activeChunk._size = 0; // unknown currently

            _hasActiveChunk = true;
        }

        template<typename Writer>
            void SimpleChunkFileWriterT<Writer>::FinishCurrentChunk()
        {
            using namespace Serialization::ChunkFile;
            auto oldLoc = _writer.TellP();
            auto chunkHeaderLoc = sizeof(ChunkFileHeader) + _activeChunkIndex * sizeof(ChunkHeader);
            _writer.Seek(chunkHeaderLoc);
            _activeChunk._size = (ChunkFile::SizeType)std::max(size_t(0), oldLoc - _activeChunkStart);
			_writer.Write(&_activeChunk, sizeof(ChunkHeader), 1);
            _writer.Seek(oldLoc);
            ++_activeChunkIndex;
            _hasActiveChunk = false;
        }

		template<typename Writer>
			size_t SimpleChunkFileWriterT<Writer>::Write(const void *buffer, size_t size, size_t count) never_throws
		{
			return _writer.Write(buffer, size, count);
		}

		template<typename Writer>
			size_t SimpleChunkFileWriterT<Writer>::Seek(size_t offset, FileSeekAnchor anchor) never_throws
		{
			return _writer.Seek(offset, anchor);
		}

		template<typename Writer>
			size_t SimpleChunkFileWriterT<Writer>::TellP() const never_throws
		{
			return _writer.TellP();
		}

		template<typename Writer>
			void SimpleChunkFileWriterT<Writer>::Flush() never_throws
		{
			return _writer.Flush();
		}
        
    }

    template SimpleChunkFileWriter;

}}

