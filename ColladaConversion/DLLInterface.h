// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Assets/ChunkFile.h"
#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../ConsoleRig/GlobalServices.h"
#include <vector>
#include <memory>

#if defined(PROJECT_COLLADA_CONVERSION)
    #define CONVERSION_API dll_export
#else
    #define CONVERSION_API dll_import
#endif

namespace RenderCore { namespace ColladaConversion
{
    class NascentChunk
    {
    public:
        Serialization::ChunkFile::ChunkHeader _hdr;
        std::vector<uint8> _data;

        NascentChunk(
            const Serialization::ChunkFile::ChunkHeader& hdr, 
            std::vector<uint8>&& data)
            : _hdr(hdr), _data(std::forward<std::vector<uint8>>(data)) {}
        NascentChunk(NascentChunk&& moveFrom) never_throws
        : _hdr(moveFrom._hdr)
        , _data(std::move(moveFrom._data))
        {}
        NascentChunk& operator=(NascentChunk&& moveFrom) never_throws
        {
            _hdr = moveFrom._hdr;
            _data = std::move(moveFrom._data);
            return *this;
        }
		NascentChunk(const NascentChunk& copyFrom) : _hdr(copyFrom._hdr), _data(copyFrom._data) {}
		NascentChunk& operator=(const NascentChunk& copyFrom)
		{
			_hdr = copyFrom._hdr;
			_data = copyFrom._data;
			return *this;
		}
		NascentChunk() {}
    };

    class ColladaScaffold;
    class WorkingAnimationSet;

    using NascentChunkArray = std::shared_ptr<std::vector<NascentChunk>>;

    CONVERSION_API std::shared_ptr<ColladaScaffold> CreateColladaScaffold(const ::Assets::ResChar identifier[]);
    CONVERSION_API NascentChunkArray SerializeSkin(const ColladaScaffold& model, const char startingNode[]);
    CONVERSION_API NascentChunkArray SerializeSkeleton(const ColladaScaffold& model, const char startingNode[]);
    CONVERSION_API NascentChunkArray SerializeMaterials(const ColladaScaffold& model, const char startingNode[]);

    CONVERSION_API std::shared_ptr<WorkingAnimationSet> CreateAnimationSet(const char name[]);
    CONVERSION_API void ExtractAnimations(WorkingAnimationSet& dest, const ColladaScaffold& model, const char animName[]);
    CONVERSION_API NascentChunkArray SerializeAnimationSet(const WorkingAnimationSet& animset);

    typedef std::shared_ptr<ColladaScaffold> CreateColladaScaffoldFn(const ::Assets::ResChar identifier[]);
    typedef NascentChunkArray ModelSerializeFn(const ColladaScaffold&, const char[]);

    typedef std::shared_ptr<WorkingAnimationSet> CreateAnimationSetFn(const char name[]);
    typedef void ExtractAnimationsFn(WorkingAnimationSet&, const ColladaScaffold&, const char[]);
    typedef NascentChunkArray SerializeAnimationSetFn(const WorkingAnimationSet&);
}}

extern "C" CONVERSION_API ConsoleRig::LibVersionDesc GetVersionInformation();
extern "C" CONVERSION_API void AttachLibrary(ConsoleRig::GlobalServices&);
extern "C" CONVERSION_API void DetachLibrary();
