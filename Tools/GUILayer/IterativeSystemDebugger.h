// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "ManipulatorUtils.h"       // for IGetAndSetProperties

using namespace System;
using namespace System::ComponentModel;

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
        public:
            enum class Preview { WaterVelocity, HardMaterials, SoftMaterials };
            
            [Browsable(true)]
            [Category("Preview")] [Description("Rendering mode for the preview window")]
            property Preview ActivePreview;

            Settings();
        };
        Settings^ _settings;

        void Tick();

        ErosionIterativeSystem(String^ sourceHeights);
        !ErosionIterativeSystem();
        ~ErosionIterativeSystem();

    private:
        clix::auto_ptr<ErosionIterativeSystemPimpl> _pimpl;
    };


    class CFDRefIterativeSystemPimpl;

    public ref class CFDRefIterativeSystem
    {
    public:
        IOverlaySystem^ _overlay;
        IGetAndSetProperties^ _getAndSetProperties;

        ref class Settings
        {
        public:
            enum class Preview { Density, Velocity };
            
            [Browsable(true)]
            [Category("Preview")] [Description("Rendering mode for the preview window")]
            property Preview ActivePreview;

            Settings();
        };
        Settings^ _settings;

        void Tick();
        void OnMouseDown(float x, float y, float velX, float velY, unsigned mouseButton);

        CFDRefIterativeSystem(unsigned size);
        !CFDRefIterativeSystem();
        ~CFDRefIterativeSystem();

    private:
        clix::auto_ptr<CFDRefIterativeSystemPimpl> _pimpl;
    };

}
