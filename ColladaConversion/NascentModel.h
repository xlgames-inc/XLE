// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TableOfObjects.h"
#include "ModelCommandStream.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/IDevice_Forward.h"
#include "../Assets/ChunkFile.h"
#include "../Assets/BlockSerializer.h"
#include "../Utility/Mixins.h"
#include "../Math/Transformations.h"
#include <tuple>

namespace RenderCore { namespace Techniques { class CameraDesc; }}

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
        NascentChunk(NascentChunk&& moveFrom)
        : _hdr(moveFrom._hdr)
        , _data(std::move(moveFrom._data))
        {}
        NascentChunk() {}
        NascentChunk& operator=(NascentChunk&& moveFrom)
        {
            _hdr = moveFrom._hdr;
            _data = std::move(moveFrom._data);
            return *this;
        }
    };

    class ExtraDataCallback;

    namespace Internal
    {
        class CrossDLLDeletor
        {
        public:
            typedef void DeleteFunction(const void*);
            typedef void Deallocator(const void*);
            DeleteFunction*     _deleteFunction;
            CrossDLLDeletor(DeleteFunction* fn) : _deleteFunction(fn) {}
            void operator()(const void* model) { (*_deleteFunction)(model); }
        };
    }

    typedef std::pair<std::unique_ptr<NascentChunk[], Internal::CrossDLLDeletor>, unsigned> NascentChunkArray;

    class NascentModel
    {
    public:
        NascentModel(const ResChar identifier[]);
        ~NascentModel();

        std::pair<Float3, Float3>   CalculateBoundingBox() const;
        unsigned                    CameraCount() const;
        Techniques::CameraDesc      Camera(unsigned index) const;

        CONVERSION_API NascentChunkArray    SerializeSkin() const;
        CONVERSION_API NascentChunkArray    SerializeAnimationSet() const;
        CONVERSION_API NascentChunkArray    SerializeSkeleton() const;

        CONVERSION_API void         MergeAnimationData(const NascentModel& source, const char animationName[]);

    protected:
        TableOfObjects               _objects;
        NascentModelCommandStream    _visualScene;
        NascentAnimationSet          _animationSet;
        NascentSkeleton              _skeleton;

        std::unique_ptr<ExtraDataCallback> _extraDataCallback;

        std::string _name;
    };

    CONVERSION_API std::unique_ptr<NascentModel, Internal::CrossDLLDeletor>     CreateModel(const ResChar identifier[]);
    CONVERSION_API std::pair<const char*, const char*>                          GetVersionInformation();

    typedef std::unique_ptr<NascentModel, Internal::CrossDLLDeletor> CreateModelFunction(const ResChar identifier[]);
    typedef NascentChunkArray (NascentModel::*ModelSerializeFunction)() const;
    typedef void (NascentModel::*MergeAnimationDataFunction)(const NascentModel& source, const char animationName[]);
}}





