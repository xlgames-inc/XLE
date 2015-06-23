// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/IThreadContext_Forward.h"
#include "CLIXAutoPtr.h"
#include <memory>

namespace GUILayer
{
    class NativeEngineDevice;

    /// <summary>CLI layer to represent a rendering device</summary>
    ///
    /// This class manages the construction/destruction and access of some global 
    /// engine resources.
    ///
    /// It must be a managed classed, so that it can be accessed from a C# layer.
    /// We generally want to avoid putting a lot of functionality in "ref class"
    /// CLI objects -- but we do need them to provide interfaces that can be used
    /// from GUI elements. This creates a kind of balancing act between what should
    /// go in "ref class" objects and plain native objects.
    public ref class EngineDevice
    {
    public:
        static EngineDevice^ GetInstance() { return s_instance; }
        static void SetDefaultWorkingDirectory();
        NativeEngineDevice& GetNative() { return *_pimpl; }

        RenderCore::IThreadContext* GetNativeImmediateContext();

        void AttachDefaultCompilers();

        EngineDevice();
        ~EngineDevice();
    protected:
        clix::auto_ptr<NativeEngineDevice> _pimpl;

        static EngineDevice^ s_instance;
    };
}

