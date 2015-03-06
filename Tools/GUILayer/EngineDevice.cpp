// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EngineDevice.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/Shader.h"

namespace GUILayer
{
    class EngineDevice::Pimpl
    {
    public:
        std::unique_ptr<RenderCore::IDevice> _renderDevice;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
    };

    EngineDevice::EngineDevice()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_renderDevice = RenderCore::CreateDevice();
        _pimpl->_asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
    }

    EngineDevice::~EngineDevice()
    {

    }
}

