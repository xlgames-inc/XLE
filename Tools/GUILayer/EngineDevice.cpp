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
        std::shared_ptr<RenderCore::IThreadContext> _immediateContext;
        std::unique_ptr<::Assets::CompileAndAsyncManager> _asyncMan;
    };

    EngineDevice* EngineDevice::s_instance = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////
    RenderCore::IDevice* EngineDevice::GetRenderDevice()
    {
        return _pimpl->_renderDevice.get();
    }
    
    EngineDevice::EngineDevice()
    {
        assert(s_instance == nullptr);
        
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_renderDevice = RenderCore::CreateDevice();
        _pimpl->_immediateContext = _pimpl->_renderDevice->GetImmediateContext();
        _pimpl->_asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();

        s_instance = this;
    }

    EngineDevice::~EngineDevice()
    {
        assert(s_instance == this);
        s_instance = nullptr;
    }
}

