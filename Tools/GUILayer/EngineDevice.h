// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include "../../BufferUploads/IBufferUploads_Forward.h"
#include <memory>

namespace Assets { class CompileAndAsyncManager; }
namespace ConsoleRig { class Console; }

namespace GUILayer
{
    class IWindowRig;

    class NativeEngineDevice
    {
    public:
        RenderCore::IDevice*        GetRenderDevice();
        BufferUploads::IManager*    GetBufferUploads();
        std::unique_ptr<IWindowRig> CreateWindowRig(const void* nativeWindowHandle);

        NativeEngineDevice();
        ~NativeEngineDevice();

    protected:
        std::shared_ptr<RenderCore::IDevice> _renderDevice;
        std::shared_ptr<RenderCore::IThreadContext> _immediateContext;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
        std::unique_ptr<ConsoleRig::Console> _console;
        std::shared_ptr<BufferUploads::IManager> _bufferUploads;
    };

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

        EngineDevice();
        ~EngineDevice();
    protected:
        clix::auto_ptr<NativeEngineDevice> _pimpl;

        static EngineDevice^ s_instance;
    };
}

