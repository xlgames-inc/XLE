// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TableOfObjects.h"
#include "NascentCommandStream.h"
#include "DLLInterface.h"
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

namespace RenderCore { namespace ColladaConversion
{
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

    class NascentModel
    {
    public:
        unsigned                    CameraCount() const;
        Techniques::CameraDesc      Camera(unsigned index) const;

        CONVERSION_API NascentChunkArray    SerializeSkin() const;
        CONVERSION_API NascentChunkArray    SerializeAnimationSet() const;
        CONVERSION_API NascentChunkArray    SerializeSkeleton() const;
        CONVERSION_API NascentChunkArray    SerializeMaterials() const;

        CONVERSION_API void     MergeAnimationData(const NascentModel& source, const char animationName[]);

        NascentModel(const ::Assets::ResChar identifier[]);
        ~NascentModel();

    protected:
        TableOfObjects               _objects;
        NascentModelCommandStream    _visualScene;
        NascentAnimationSet          _animationSet;
        NascentSkeleton              _skeleton;

        std::unique_ptr<ExtraDataCallback> _extraDataCallback;

        std::string _name;
    };

    CONVERSION_API std::unique_ptr<NascentModel, Internal::CrossDLLDeletor> OCCreateModel(const ::Assets::ResChar identifier[]);

    typedef std::unique_ptr<NascentModel, Internal::CrossDLLDeletor> OCCreateModelFunction(const ::Assets::ResChar identifier[]);
    typedef NascentChunkArray (NascentModel::*OCModelSerializeFunction)() const;
    typedef void (NascentModel::*OCMergeAnimationDataFunction)(const NascentModel& source, const char animationName[]);
}}

