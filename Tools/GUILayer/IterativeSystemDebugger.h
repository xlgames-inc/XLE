// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "ManipulatorUtils.h"       // for IGetAndSetProperties

using namespace System;

namespace GUILayer
{
    ref class IOverlaySystem;

    class ErosionIterativeSystemPimpl;

    public ref class ErosionIterativeSystem
    {
    public:
        IOverlaySystem^ _overlay;
        IGetAndSetProperties^ _getAndSetProperties;

        ref class Settings
        {
            enum class Preview { Velocities, Heights };
            Preview _preview;
        };
        Settings^ _settings;

        void Tick();

        ErosionIterativeSystem(String^ sourceHeights);
        !ErosionIterativeSystem();
        ~ErosionIterativeSystem();

    private:
        clix::auto_ptr<ErosionIterativeSystemPimpl> _pimpl;
    };

}
