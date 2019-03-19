// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IDevice.h"

namespace SceneEngine
{
    class SimplePatchBox
    {
    public:
        class Desc
        {
        public:
            Desc(unsigned width, unsigned height, bool flipAlternate);
            unsigned _width, _height;
            bool _flipAlternate;
        };

        SimplePatchBox(const Desc& desc);
        ~SimplePatchBox();

        RenderCore::IResourcePtr	_simplePatchIndexBuffer;
        unsigned					_simplePatchIndexCount;
    };
}


