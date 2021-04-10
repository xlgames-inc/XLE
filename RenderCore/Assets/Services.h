// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../IDevice_Forward.h"
#include <memory>
#include <vector>

namespace RenderCore { class ShaderService; }
namespace Assets { class IAssetCompiler; }

namespace RenderCore { namespace Assets
{
    class Services
    {
    public:
        static RenderCore::IDevice& GetDevice() { return *s_instance->_device; }
        static bool HasInstance() { return s_instance != nullptr; }

        Services(const std::shared_ptr<RenderCore::IDevice>& device);
        ~Services();

        void AttachCurrentModule();
        void DetachCurrentModule();

        Services(const Services&) = delete;
        const Services& operator=(const Services&) = delete;
    protected:
        std::shared_ptr<RenderCore::IDevice> _device;
        static Services* s_instance;
    };
}}

