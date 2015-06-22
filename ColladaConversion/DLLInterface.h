// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Assets/ChunkFile.h"
#include "../Assets/BlockSerializer.h"
#include <vector>
#include <memory>

#if defined(PROJECT_COLLADA_CONVERSION)
    #define CONVERSION_API dll_export
#else
    #define CONVERSION_API dll_import
#endif

namespace RenderCore { namespace ColladaConversion
{
    class NascentChunk2
    {
    public:
        Serialization::ChunkFile::ChunkHeader _hdr;
        std::vector<uint8> _data;

        NascentChunk2(
            const Serialization::ChunkFile::ChunkHeader& hdr, 
            std::vector<uint8>&& data)
            : _hdr(hdr), _data(std::forward<std::vector<uint8>>(data)) {}
        NascentChunk2(NascentChunk2&& moveFrom)
        : _hdr(moveFrom._hdr)
        , _data(std::move(moveFrom._data))
        {}
        NascentChunk2() {}
        NascentChunk2& operator=(NascentChunk2&& moveFrom)
        {
            _hdr = moveFrom._hdr;
            _data = std::move(moveFrom._data);
            return *this;
        }
    };

    class ColladaScaffold;
    class WorkingAnimationSet;

    using NascentChunkArray2 = std::shared_ptr<std::vector<NascentChunk2>>;

    CONVERSION_API std::shared_ptr<ColladaScaffold> CreateColladaScaffold(const ::Assets::ResChar identifier[]);
    CONVERSION_API NascentChunkArray2 SerializeSkin2(const ColladaScaffold& model);
    CONVERSION_API NascentChunkArray2 SerializeSkeleton2(const ColladaScaffold& model);
    CONVERSION_API NascentChunkArray2 SerializeMaterials2(const ColladaScaffold& model);

    CONVERSION_API std::shared_ptr<WorkingAnimationSet> CreateAnimationSet(const char name[]);
    CONVERSION_API void ExtractAnimations(WorkingAnimationSet& dest, const ColladaScaffold& model, const char animName[]);
    CONVERSION_API NascentChunkArray2 SerializeAnimationSet2(const WorkingAnimationSet& animset);

    typedef std::shared_ptr<ColladaScaffold> CreateColladaScaffoldFn(const ::Assets::ResChar identifier[]);
    typedef NascentChunkArray2 Model2SerializeFn(const ColladaScaffold&);

    typedef std::shared_ptr<WorkingAnimationSet> CreateAnimationSetFn();
    typedef void ExtractAnimationsFn(WorkingAnimationSet&, const ColladaScaffold&, const char[]);
    typedef NascentChunkArray2 SerializeAnimationSet2Fn(const WorkingAnimationSet&);
}}
