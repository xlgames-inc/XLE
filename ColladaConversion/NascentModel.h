// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TableOfObjects.h"
#include "DLLInterface.h"
#include "../RenderCore/GeoProc/NascentCommandStream.h"
#include <tuple>

namespace RenderCore { namespace Techniques { class CameraDesc; }}

namespace RenderCore { namespace ColladaConversion
{
    class ExtraDataCallback;

    class NascentModel
    {
    public:
        unsigned                    CameraCount() const;
        Techniques::CameraDesc      Camera(unsigned index) const;

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
}}

