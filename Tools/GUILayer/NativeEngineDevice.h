// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EngineForward.h"
#include <memory>

namespace GUILayer
{
    class IWindowRig;

    class NativeEngineDevice
    {
    public:
        RenderCore::IDevice*        GetRenderDevice() { return _renderDevice.get(); }
        BufferUploads::IManager*    GetBufferUploads() { return _bufferUploads.get(); }
        ::Assets::Services*         GetAssetServices() { return _assetServices.get(); }
        std::unique_ptr<IWindowRig> CreateWindowRig(const void* nativeWindowHandle);
        void                        AttachDefaultCompilers();

        NativeEngineDevice();
        ~NativeEngineDevice();

    protected:
        std::shared_ptr<RenderCore::IDevice> _renderDevice;
        std::shared_ptr<RenderCore::IThreadContext> _immediateContext;
        std::unique_ptr<::Assets::Services> _assetServices;
        std::unique_ptr<ConsoleRig::Console> _console;
        std::shared_ptr<BufferUploads::IManager> _bufferUploads;
    };
}
