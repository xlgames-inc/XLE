// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace GUILayer
{

    class EngineDevice
    {
    public:

        EngineDevice();
        ~EngineDevice();
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
}

