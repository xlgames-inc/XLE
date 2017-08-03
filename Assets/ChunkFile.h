// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/Streams/FileUtils.h"
#include "../Utility/StringUtils.h"
#include "../Core/Types.h"
#include "../Core/SelectConfiguration.h"
#include <algorithm>
#include <vector>

namespace Utility { class OutputStream; }
namespace Assets { class IFileInterface; }

namespace Serialization { namespace ChunkFile
{
    typedef uint64 TypeIdentifier;
    typedef uint32 SizeType;

    static const TypeIdentifier TypeIdentifier_Unknown = 0;

#pragma pack(push)
#pragma pack(1)
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
    }
	#if COMPILER_ACTIVE != COMPILER_TYPE_MSVC
		__attribute__((packed, aligned(1)))
	#endif
	;

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
    }
	#if COMPILER_ACTIVE != COMPILER_TYPE_MSVC
		__attribute__((packed, aligned(1)))
	#endif
	;
#pragma pack(pop)

    ChunkFileHeader MakeChunkFileHeader(unsigned chunkCount, const char buildVersionString[], const char buildDateString[]);
    std::vector<ChunkHeader> LoadChunkTable(::Assets::IFileInterface& file);

    ChunkHeader FindChunk(
        const utf8 filename[], std::vector<ChunkHeader>& hdrs,
        TypeIdentifier chunkType, unsigned expectedVersion);

    std::unique_ptr<uint8[]> RawChunkAsMemoryBlock(
        const utf8 filename[], TypeIdentifier chunkType, unsigned expectedVersion);

    namespace Internal
    {
        template<typename Writer>
            class SimpleChunkFileWriterT
        {
        public:
            SimpleChunkFileWriterT(
                Writer&& writer, 
				unsigned chunkCount,
                const char buildVersionString[], const char buildDateString[]);
            ~SimpleChunkFileWriterT();

            void BeginChunk(
                Serialization::ChunkFile::TypeIdentifier type,
                unsigned version, const char name[]);
            void FinishCurrentChunk();

			size_t Write(const void *buffer, size_t size, size_t count) never_throws;
			size_t Seek(size_t offset, FileSeekAnchor anchor = FileSeekAnchor::Start) never_throws;
			size_t TellP() const never_throws;
			void Flush() never_throws;

        protected:
			Writer _writer;
            Serialization::ChunkFile::ChunkHeader _activeChunk;
            size_t _activeChunkStart;
            bool _hasActiveChunk;
            unsigned _chunkCount;
            unsigned _activeChunkIndex;
        };
    }

    using SimpleChunkFileWriter = Internal::SimpleChunkFileWriterT<BasicFile>;

}}

