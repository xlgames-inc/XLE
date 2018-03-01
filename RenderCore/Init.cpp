// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Init.h"

namespace RenderCore
{
    namespace ImplDX11      { std::shared_ptr<IDevice> CreateDevice(); }
    namespace ImplVulkan    { std::shared_ptr<IDevice> CreateDevice(); }
    namespace ImplOpenGLES  { std::shared_ptr<IDevice> CreateDevice(); }

    std::shared_ptr<IDevice>    CreateDevice(UnderlyingAPI api)
    {
        // switch (api) {
        // default:
        // case UnderlyingAPI::DX11: return ImplDX11::CreateDevice();
        // case UnderlyingAPI::Vulkan: return ImplVulkan::CreateDevice();
        // case UnderlyingAPI::OpenGLES: return ImplOpenGLES::CreateDevice();
        // }
        return nullptr;
    }
}

