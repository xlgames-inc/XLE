// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "../../RenderCore/IDevice_Forward.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include <memory>

namespace Assets { class CompileAndAsyncManager; }
namespace ConsoleRig { class Console; }
namespace BufferUploads { class IManager; }

namespace GUILayer
{
    class EngineDeviceInternal
    {
    public:
        std::unique_ptr<RenderCore::IDevice> _renderDevice;
        std::shared_ptr<RenderCore::IThreadContext> _immediateContext;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
        std::unique_ptr<ConsoleRig::Console> _console;
        std::unique_ptr<BufferUploads::IManager> _bufferUploads;

        ~EngineDeviceInternal();
    };

    public ref class EngineDevice
    {
    public:
        RenderCore::IDevice* GetRenderDevice();
        static EngineDevice^ GetInstance() { return s_instance; }
        static void SetDefaultWorkingDirectory();

        EngineDevice();
        ~EngineDevice();
    protected:
        clix::auto_ptr<EngineDeviceInternal> _pimpl;

        static EngineDevice^ s_instance;
    };
}

